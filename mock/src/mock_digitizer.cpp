#include "mock_digitizer.h"
#include "nlohmann/json.hpp"

#include <cstring>
#include <sstream>

static const std::string DEFAULT_TOP_PARAMETERS =
R"""({"top": [
    {
        "record_length": 18000,
        "trigger_frequency": 5.0,
        "frequency": 1e6,
        "amplitude": 1.0,
        "noise": 0.1,
        "harmonic_distortion": false,
        "interleaving_distortion": false
    },
    {
        "record_length": 18000,
        "trigger_frequency": 15.0,
        "frequency": 9e6,
        "amplitude": 0.8,
        "noise": 0.02,
        "harmonic_distortion": true,
        "interleaving_distortion": true
    }
]})""";

static const std::string DEFAULT_CLOCK_SYSTEM_PARAMETERS =
R"""({"clock_system": {
    "sampling_frequency": 500e6
}})""";

MockDigitizer::MockDigitizer(const ADQConstantParameters &constant)
    : m_constant(constant)
    , m_afe{}
    , m_clock_system{}
    , m_transfer{}
    , m_dram_status{}
    , m_overflow_status{}
    , m_generators(constant.nof_acquisition_channels)
    , m_sysman{}
    , m_top_parameters(DEFAULT_TOP_PARAMETERS)
    , m_clock_system_parameters(DEFAULT_CLOCK_SYSTEM_PARAMETERS)
{
    m_afe.id = ADQ_PARAMETER_ID_ANALOG_FRONTEND;
    m_afe.magic = ADQ_PARAMETERS_MAGIC;

    /* Pretend we're able to run whatever frequency using the internal reference. */
    m_clock_system.id = ADQ_PARAMETER_ID_CLOCK_SYSTEM;
    m_clock_system.clock_generator = ADQ_CLOCK_GENERATOR_INTERNAL_PLL;
    m_clock_system.reference_source = ADQ_REFERENCE_CLOCK_SOURCE_INTERNAL;
    m_clock_system.sampling_frequency = 500e6;
    m_clock_system.reference_frequency = 10e6;
    m_clock_system.magic = ADQ_PARAMETERS_MAGIC;

    for (int ch = 0; ch < constant.nof_channels; ++ch)
    {
        /* 'Activate' the first input range entry. */
        m_afe.channel[ch].input_range = constant.channel[ch].input_range[0];
        m_afe.channel[ch].dc_offset = 0;
    }

    for (int ch = 0; ch < constant.nof_transfer_channels; ++ch)
        m_transfer.channel[ch].nof_buffers = 2;
}

int MockDigitizer::SetupDevice()
{
    /* Start the system manager and emulate the rest as a delay. */
    m_sysman.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return 1;
}

int MockDigitizer::StartDataAcquisition()
{
    m_dram_status = {};
    m_overflow_status = {};

    for (auto &generator : m_generators)
        generator.Start();

    return ADQ_EOK;
}

int MockDigitizer::StopDataAcquisition()
{
    for (auto &generator : m_generators)
        generator.Stop();

    return ADQ_EOK;
}

int64_t MockDigitizer::WaitForRecordBuffer(int *channel, void **buffer, int timeout,
                                           ADQDataReadoutStatus *status)
{
    (void)status;

    if (buffer == NULL)
        return ADQ_EINVAL;
    if (*channel == -1)
        return ADQ_EUNSUPPORTED;
    if (*channel < 0 || static_cast<size_t>(*channel) >= m_generators.size())
        return ADQ_EINVAL;

    std::shared_ptr<ADQGen4Record> smart_buffer;
    int result = m_generators[*channel].WaitForBuffer(smart_buffer, timeout);
    if (result < 0)
    {
        /* FIXME: Error code space etc. */
        return result;
    }
    else
    {
        /* Since our goal is to emulate a C API, we have to extract and pass on
           the raw pointer. By virtue of the generator being configured in the
           "preserve" mode, the buffer memory is kept alive until it's returned
           through `ReturnRecordBuffer`. Otherwise, we'd have a problem. */
        *buffer = smart_buffer.get();

        /* FIXME: Would be better if the generator returned length. */
        return smart_buffer->header->record_length * sizeof(int16_t);
    }
}

int MockDigitizer::ReturnRecordBuffer(int channel, void *buffer)
{
    if (buffer == NULL)
        return ADQ_EINVAL;
    if (channel == -1)
        return ADQ_EUNSUPPORTED;
    if (channel < 0 || static_cast<size_t>(channel) >= m_generators.size())
        return ADQ_EINVAL;

    /* FIXME: Error space */
    return m_generators[channel].ReturnBuffer(static_cast<ADQGen4Record *>(buffer));
}

int MockDigitizer::GetParameters(enum ADQParameterId id, void *const parameters)
{
    if (parameters == NULL)
        return ADQ_EINVAL;

    switch (id)
    {
    case ADQ_PARAMETER_ID_CONSTANT:
        m_constant.clock_system = m_clock_system;
        std::memcpy(parameters, &m_constant, sizeof(m_constant));
        return sizeof(m_constant);

    case ADQ_PARAMETER_ID_ANALOG_FRONTEND:
        std::memcpy(parameters, &m_afe, sizeof(m_afe));
        return sizeof(m_afe);

    case ADQ_PARAMETER_ID_CLOCK_SYSTEM:
        std::memcpy(parameters, &m_clock_system, sizeof(m_clock_system));
        return sizeof(m_clock_system);

    case ADQ_PARAMETER_ID_DATA_TRANSFER:
        std::memcpy(parameters, &m_transfer, sizeof(m_transfer));
        return sizeof(m_transfer);

    default:
        return ADQ_EUNSUPPORTED;
    }
}

int MockDigitizer::GetStatus(enum ADQStatusId id, void *const status)
{
    if (status == NULL)
        return ADQ_EINVAL;

    switch (id)
    {
    case ADQ_STATUS_ID_OVERFLOW:
        std::memcpy(status, &m_overflow_status, sizeof(m_overflow_status));
        return sizeof(m_overflow_status);

    case ADQ_STATUS_ID_DRAM:
        /* TODO: For now, just increase by 512 MiB for each call. */
        if (m_dram_status.fill < m_constant.dram_size)
        {
            m_dram_status.fill += 512 * 1024 * 1024;
            m_dram_status.fill_max = m_dram_status.fill;
            if (m_dram_status.fill >= m_constant.dram_size)
                m_overflow_status.overflow = 1;
        }

        std::memcpy(status, &m_dram_status, sizeof(m_dram_status));
        return sizeof(m_dram_status);

    case ADQ_STATUS_ID_RESERVED:
    case ADQ_STATUS_ID_ACQUISITION:
    case ADQ_STATUS_ID_TEMPERATURE:
    case ADQ_STATUS_ID_CLOCK_SYSTEM:
    default:
        return ADQ_EUNSUPPORTED;
    }
}

int MockDigitizer::InitializeParametersString(enum ADQParameterId id, char *const string, size_t length, int format)
{
    (void)format;

    if (id == ADQ_PARAMETER_ID_TOP)
    {
        std::strncpy(string, DEFAULT_TOP_PARAMETERS.c_str(), length);
        return static_cast<int>(std::min(DEFAULT_TOP_PARAMETERS.size() + 1, length));
    }
    else if (id == ADQ_PARAMETER_ID_CLOCK_SYSTEM)
    {
        std::strncpy(string, DEFAULT_CLOCK_SYSTEM_PARAMETERS.c_str(), length);
        return static_cast<int>(std::min(DEFAULT_CLOCK_SYSTEM_PARAMETERS.size() + 1, length));
    }
    else
    {
        return ADQ_EINVAL;
    }
}

int MockDigitizer::SetParametersString(const char *const string, size_t length)
{
    try
    {
        const auto json = nlohmann::json::parse(std::stringstream(string));
        if (json.contains("top"))
        {
            size_t i = 0;
            for (const auto &object : json["top"])
            {
                SineGeneratorParameters parameters{};
                parameters.record_length = object["record_length"];
                parameters.trigger_frequency = object["trigger_frequency"];
                parameters.amplitude = object["amplitude"];
                parameters.frequency = object["frequency"];
                parameters.amplitude = object["amplitude"];
                parameters.noise = object["noise"];
                parameters.harmonic_distortion = object["harmonic_distortion"];
                parameters.interleaving_distortion = object["interleaving_distortion"];

                if (i < m_generators.size())
                    m_generators[i++].PushMessage(parameters);
            }
        }
        else if (json.contains("clock_system"))
        {
            const double frequency = json["clock_system"]["sampling_frequency"];
            for (auto &generator: m_generators)
                generator.PushMessage(frequency);
        }
        else
        {
            fprintf(stderr, "Unrecognized parameter set.\n");
            return ADQ_EINVAL;
        }
    }
    catch (const nlohmann::json::exception &e)
    {
        fprintf(stderr, "Failed to parse the parameter set %s.\n", e.what());
        return ADQ_EINVAL;
    }

    return static_cast<int>(length);
}

int MockDigitizer::GetParametersString(enum ADQParameterId id, char *const string, size_t length, int format)
{
    (void)format;
    if (id == ADQ_PARAMETER_ID_TOP)
    {
        std::strncpy(string, m_top_parameters.c_str(), length);
        return static_cast<int>(std::min(m_top_parameters.size() + 1, length));
    }
    else if (id == ADQ_PARAMETER_ID_CLOCK_SYSTEM)
    {
        std::strncpy(string, m_clock_system_parameters.c_str(), length);
        return static_cast<int>(std::min(m_clock_system_parameters.size() + 1, length));
    }
    else
    {
        return ADQ_EINVAL;
    }
}

int MockDigitizer::ValidateParametersString(const char *const string, size_t length)
{
    (void)string;
    (void)length;
    return ADQ_EUNSUPPORTED;
}

int MockDigitizer::SmTransaction(uint16_t cmd, void *wr_buf, size_t wr_buf_len, void *rd_buf,
                                 size_t rd_buf_len)
{
    if (wr_buf_len > 0 && wr_buf == NULL)
        return ADQ_EINVAL;
    if (rd_buf_len > 0 && rd_buf == NULL)
        return ADQ_EINVAL;

    /* Add write message. */
    int result = m_sysman.EmplaceMessage(static_cast<SystemManagerCommand>(cmd), wr_buf, wr_buf_len);
    if (result != ADQ_EOK)
        return result;

    /* Wait for the reply. */
    SystemManagerMessage reply;
    result = m_sysman.WaitForMessage(reply, 100);
    if (result != ADQ_EOK)
        return result;

    /* Early exit if there was an error. */
    if (reply.result != 0)
        return reply.result;

    /* Write the response data to the read buffer. */
    if (rd_buf_len > 0)
    {
        if (reply.data.size() != rd_buf_len)
        {
            fprintf(stderr, "System manager read length mismatch, got %zu, expected %zu.\n",
                    reply.data.size(), rd_buf_len);
            return ADQ_EINTERNAL;
        }

        std::memcpy(rd_buf, reply.data.data(), reply.data.size());
    }

    return 0;
}

int MockDigitizer::SmTransactionImmediate(uint16_t cmd, void *wr_buf, size_t wr_buf_len,
                                          void *rd_buf, size_t rd_buf_len)
{
    /* Exactly the same implementation since everything is emulated in software. */
    return SmTransaction(cmd, wr_buf, wr_buf_len, rd_buf, rd_buf_len);
}
