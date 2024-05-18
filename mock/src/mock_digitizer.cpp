#include "mock_digitizer.h"
#include "pulse_generator.h"
#include "sine_generator.h"
#include "nlohmann/json.hpp"

#include <cstring>
#include <sstream>

MockDigitizer::MockDigitizer(const ADQConstantParameters &constant)
    : m_constant(constant)
    , m_afe{}
    , m_clock_system{}
    , m_transfer{}
    , m_dram_status{}
    , m_overflow_status{}
    , m_generators{}
    , m_sysman{}
{
    if (constant.nof_channels <= 0)
        throw std::runtime_error("Invalid nof_channels");

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

        if (constant.firmware.type == ADQ_FIRMWARE_TYPE_FWPD)
            m_generators.emplace_back(std::make_unique<PulseGenerator>());
        else
            m_generators.emplace_back(std::make_unique<SineGenerator>());
    }

    /* Check that the channel configuration makes sense for the generators we just added. */
    if (constant.nof_transfer_channels != GetNofTransferChannels())
        throw std::runtime_error("Invalid nof_transfer_channels for generator configuration.");

    for (int ch = 0; ch < constant.nof_transfer_channels; ++ch)
        m_transfer.channel[ch].nof_buffers = 2;
}

int MockDigitizer::SetupDevice()
{
    /* Start the system manager and emulate the rest as a delay. */
    m_sysman.Start();
    for (auto &generator : m_generators)
        generator->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return 1;
}

int MockDigitizer::StartDataAcquisition()
{
    m_dram_status = {};
    m_overflow_status = {};

    for (auto &generator : m_generators)
        generator->PushMessageWaitForResponse({GeneratorMessageId::ENABLE});

    return ADQ_EOK;
}

int MockDigitizer::StopDataAcquisition()
{
    for (auto &generator : m_generators)
        generator->PushMessageWaitForResponse({GeneratorMessageId::DISABLE});

    return ADQ_EOK;
}

int64_t MockDigitizer::WaitForRecordBuffer(int *channel, void **buffer, int timeout,
                                           ADQDataReadoutStatus */* status */)
{
    if (buffer == NULL || channel == NULL)
        return ADQ_EINVAL;
    if (*channel == -1)
        return ADQ_EUNSUPPORTED;
    if (*channel < 0 || *channel >= m_constant.nof_transfer_channels)
        return ADQ_EINVAL;

    std::shared_ptr<ADQGen4Record> smart_buffer;
    const auto [gidx, cidx] = MapChannelIndex(*channel);
    int result = m_generators[gidx]->WaitForBuffer(smart_buffer, timeout, cidx);
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
    if (channel < 0 || channel >= m_constant.nof_transfer_channels)
        return ADQ_EINVAL;

    /* FIXME: Error space */
    const auto [gidx, _] = MapChannelIndex(channel);
    return m_generators[gidx]->ReturnBuffer(static_cast<ADQGen4Record *>(buffer));
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

int MockDigitizer::InitializeParametersString(
    enum ADQParameterId id, char *const string, size_t length, int format)
{
    try
    {
        if (id == ADQ_PARAMETER_ID_TOP)
        {
            nlohmann::json json{
                {"top", {}},
            };

            for (auto &generator : m_generators)
            {
                GeneratorMessage response;
                GeneratorMessage message{GeneratorMessageId::GET_TOP_PARAMETERS};
                if (SCAPE_EOK != generator->PushMessageWaitForResponse(message, response))
                    return ADQ_EINVAL;
                json["top"].emplace_back(response.json);
            }

            const auto str = json.dump(format ? 4 : -1);
            std::strncpy(string, str.c_str(), length);
            return static_cast<int>(std::min(str.size() + 1, length));
        }
        else if (id == ADQ_PARAMETER_ID_CLOCK_SYSTEM)
        {
            /* The clock system parameters should the be same for all channels
               we only retrieve it for the first one. */
            GeneratorMessage response;
            GeneratorMessage message{GeneratorMessageId::GET_CLOCK_SYSTEM_PARAMETERS};
            if (SCAPE_EOK != m_generators.front()->PushMessageWaitForResponse(message, response))
                return ADQ_EINVAL;

            nlohmann::json json{
                {"clock_system", response.json},
            };

            const auto str = json.dump(format ? 4 : -1);
            std::strncpy(string, str.c_str(), length);
            return static_cast<int>(std::min(str.size() + 1, length));
        }
        else
        {
            fprintf(stderr, "Invalid parameter id %d.\n", id);
            return ADQ_EINVAL;
        }
    }
    catch (const nlohmann::json::exception &e)
    {
        fprintf(stderr, "Failed to parse the parameter set %s.\n", e.what());
        return ADQ_EINVAL;
    }
}

int MockDigitizer::SetParametersString(const char *const string, size_t length)
{
    try
    {
        const auto json = nlohmann::json::parse(std::string(string));
        if (json.contains("top"))
        {
            size_t i = 0;
            for (const auto &object : json["top"])
            {
                if (i < m_generators.size())
                {
                    m_generators[i]->PushMessageWaitForResponse(
                        {GeneratorMessageId::SET_TOP_PARAMETERS, object});
                }
                ++i;
            }

            /* Emulate reconfiguration time. */
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        else if (json.contains("clock_system"))
        {
            for (auto &generator: m_generators)
            {
                generator->PushMessageWaitForResponse(
                    {GeneratorMessageId::SET_CLOCK_SYSTEM_PARAMETERS, json["clock_system"]});
            }

            /* Emulate reconfiguration time. */
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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

int MockDigitizer::GetParametersString(
    enum ADQParameterId id, char *const string, size_t length, int format)
{
    return InitializeParametersString(id, string, length, format);
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

    /* FIXME: Replace with MessageChannel::PushMessage(in, out) */
    /* Add write message. */
    SystemManagerMessage response;
    int result = m_sysman.PushMessageWaitForResponse(
        {static_cast<SystemManagerCommand>(cmd), wr_buf, wr_buf_len}, response);
    if (result != ADQ_EOK)
        return result;

    /* Early exit if there was an error. */
    if (response.result != 0)
        return response.result;

    /* Write the response data to the read buffer. */
    if (rd_buf_len > 0)
    {
        if (response.data.size() != rd_buf_len)
        {
            fprintf(stderr, "System manager read length mismatch, got %zu, expected %zu.\n",
                    response.data.size(), rd_buf_len);
            return ADQ_EINTERNAL;
        }

        std::memcpy(rd_buf, response.data.data(), response.data.size());
    }

    return 0;
}

int MockDigitizer::SmTransactionImmediate(uint16_t cmd, void *wr_buf, size_t wr_buf_len,
                                          void *rd_buf, size_t rd_buf_len)
{
    /* Exactly the same implementation since everything is emulated in software. */
    return SmTransaction(cmd, wr_buf, wr_buf_len, rd_buf, rd_buf_len);
}

int MockDigitizer::GetNofTransferChannels() const
{
    /* The number of transfer channels is equal to the sum of the output
       channels for the generators. */
    size_t result = 0;
    for (const auto &generator : m_generators)
        result += generator->GetNofChannels();
    return static_cast<int>(result);
}

std::tuple<size_t, size_t> MockDigitizer::MapChannelIndex(int index) const
{
    /* TODO: For now, we assume that a generator may only have two channels, and
       that the generator channels are grouped together. So that index wise, the
       first channel of all the generators come before the second channel. */
    if (index < m_constant.nof_acquisition_channels)
        return {index, 0};
    else
        return {index % m_constant.nof_acquisition_channels, 1};
}
