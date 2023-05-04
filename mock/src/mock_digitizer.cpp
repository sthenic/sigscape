#include "mock_digitizer.h"

#include <cstring>
#include <sstream>
#include <type_traits>

const std::string MockDigitizer::DEFAULT_TOP_PARAMETERS =
R"""(TOP
frequency:
    1e6, 9e6
amplitude:
    1.0, 0.8
record length:
    18000, 18000
trigger frequency:
    5.0, 15.0
harmonic distortion:
    0, 1
interleaving distortion:
    0, 1
noise standard deviation:
    0.1, 0.02
)""";

const std::string MockDigitizer::DEFAULT_CLOCK_SYSTEM_PARAMETERS =
R"""(CLOCK SYSTEM
sampling frequency:
    500e6
)""";

MockDigitizer::MockDigitizer(const struct ADQConstantParameters &constant)
    : m_constant(constant)
    , m_afe{}
    , m_clock_system{}
    , m_dram_status{}
    , m_overflow_status{}
    , m_generators{}
    , m_sysman(std::make_unique<MockSystemManager>())
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
        m_generators.push_back(std::make_unique<Generator>());
    }
}

int MockDigitizer::SetupDevice()
{
    /* Start the system manager and emulate the rest as a delay. */
    m_sysman->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return 1;
}

int MockDigitizer::StartDataAcquisition()
{
    m_dram_status = {};
    m_overflow_status = {};

    for (const auto &g : m_generators)
        g->Start();

    return ADQ_EOK;
}

int MockDigitizer::StopDataAcquisition()
{
    for (const auto &g : m_generators)
        g->Stop();

    return ADQ_EOK;
}

int64_t MockDigitizer::WaitForRecordBuffer(int *channel, void **buffer, int timeout,
                                           struct ADQDataReadoutStatus *status)
{
    (void)status;

    if (buffer == NULL)
        return ADQ_EINVAL;
    if (*channel == -1)
        return ADQ_EUNSUPPORTED;
    if ((*channel < 0) || (static_cast<size_t>(*channel) >= m_generators.size()))
        return ADQ_EINVAL;

    ADQGen4Record *lbuffer = NULL;
    int result = m_generators[*channel]->WaitForBuffer(lbuffer, timeout);
    *buffer = lbuffer;

    /* FIXME: Error code space etc. */
    /* FIXME: Would be better if the generator return length. */
    if (result < 0)
        return result;
    else
        return lbuffer->header->record_length * sizeof(int16_t);
}

int MockDigitizer::ReturnRecordBuffer(int channel, void *buffer)
{

    if (buffer == NULL)
        return ADQ_EINVAL;
    if (channel == -1)
        return ADQ_EUNSUPPORTED;
    if ((channel < 0) || (static_cast<size_t>(channel) >= m_generators.size()))
        return ADQ_EINVAL;

    /* FIXME: Error space */
    return m_generators[channel]->ReturnBuffer(static_cast<ADQGen4Record *>(buffer));
}

int MockDigitizer::GetParameters(enum ADQParameterId id, void *const parameters)
{
    if (parameters == NULL)
        return ADQ_EINVAL;

    if (id == ADQ_PARAMETER_ID_CONSTANT)
    {
        std::memcpy(parameters, &m_constant, sizeof(m_constant));
        return sizeof(m_constant);
    }
    else if (id == ADQ_PARAMETER_ID_ANALOG_FRONTEND)
    {
        std::memcpy(parameters, &m_afe, sizeof(m_afe));
        return sizeof(m_afe);
    }
    else if (id == ADQ_PARAMETER_ID_CLOCK_SYSTEM)
    {
        std::memcpy(parameters, &m_clock_system, sizeof(m_clock_system));
        return sizeof(m_clock_system);
    }
    else
    {
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
    (void)length;
    std::string parameters_str(string);
    if (parameters_str.rfind("TOP", 0) == 0)
    {
        std::vector<double> frequency;
        if (SCAPE_EOK != ParseLine(2, parameters_str, frequency))
            return SCAPE_EINVAL;
        size_t nof_generators = frequency.size();

        std::vector<double> amplitude;
        if (SCAPE_EOK != ParseLine(4, parameters_str, amplitude))
            return SCAPE_EINVAL;
        nof_generators = std::min(nof_generators, amplitude.size());

        std::vector<int> record_length;
        if (SCAPE_EOK != ParseLine(6, parameters_str, record_length))
            return SCAPE_EINVAL;
        nof_generators = std::min(nof_generators, record_length.size());

        std::vector<double> trigger_frequency;
        if (SCAPE_EOK != ParseLine(8, parameters_str, trigger_frequency))
            return SCAPE_EINVAL;
        nof_generators = std::min(nof_generators, trigger_frequency.size());

        std::vector<int> harmonic_distortion;
        if (SCAPE_EOK != ParseLine(10, parameters_str, harmonic_distortion))
            return SCAPE_EINVAL;
        nof_generators = std::min(nof_generators, harmonic_distortion.size());

        std::vector<int> interleaving_distortion;
        if (SCAPE_EOK != ParseLine(12, parameters_str, interleaving_distortion))
            return SCAPE_EINVAL;
        nof_generators = std::min(nof_generators, interleaving_distortion.size());

        std::vector<double> noise_std_dev;
        if (SCAPE_EOK != ParseLine(14, parameters_str, noise_std_dev))
            return SCAPE_EINVAL;
        nof_generators = std::min(nof_generators, noise_std_dev.size());

        std::vector<Generator::Parameters> parameters;
        for (size_t i = 0; i < nof_generators; ++i)
        {
            parameters.push_back(Generator::Parameters());
            parameters.back().record_length = record_length[i];
            parameters.back().trigger_frequency = trigger_frequency[i];
            parameters.back().sine.frequency = frequency[i];
            parameters.back().sine.amplitude = amplitude[i];
            parameters.back().sine.harmonic_distortion = harmonic_distortion[i] > 0;
            parameters.back().sine.interleaving_distortion = interleaving_distortion[i] > 0;
            parameters.back().sine.noise_std_dev = noise_std_dev[i];
            parameters.back().sine.offset = 0.0;
            parameters.back().sine.phase = 0.1;
        }

        for (size_t i = 0; (i < parameters.size()) && (i < m_generators.size()); ++i)
            m_generators[i]->SetParameters(parameters[i]);

        /* Keep track of the string to be able to respond to get requests. */
        m_top_parameters = string;

        /* Emulate reconfiguration time. */
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    else if (parameters_str.rfind("CLOCK SYSTEM", 0) == 0)
    {
        std::vector<double> sampling_frequency;
        if (SCAPE_EOK != ParseLine(2, parameters_str, sampling_frequency))
            return SCAPE_EINVAL;

        if (sampling_frequency.size() > 0)
        {
            for (auto &generator : m_generators)
                generator->SetSamplingFrequency(sampling_frequency[0]);

            /* Keep track of the string to be able to respond to get requests. */
            m_clock_system_parameters = string;

            /* Update the clock system parameters. */
            m_clock_system.sampling_frequency = sampling_frequency[0];

            /* Emulate reconfiguration time. */
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    else
    {
        fprintf(stderr, "Unrecognized parameter set.\n");
        return ADQ_EINVAL;
    }

    /* FIXME: Return value */
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
    /* FIXME: Implement */
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
    int result = m_sysman->EmplaceMessage(static_cast<SystemManagerCommand>(cmd), wr_buf, wr_buf_len);
    if (result != ADQ_EOK)
        return result;

    /* Wait for the reply. */
    SystemManagerMessage reply;
    result = m_sysman->WaitForMessage(reply, 100);
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

template<typename T>
int MockDigitizer::ParseLine(int line_idx, const std::string &str, std::vector<T> &values)
{
    /* Iterate through the string until we get to the target line, then attempt
       to read a comma-separated list of elements of the target type. */
    std::stringstream ss(str);
    std::string line;
    int idx = 0;

    while (std::getline(ss, line, '\n'))
    {
        if (idx == line_idx)
        {
            std::stringstream lss(line);
            std::string str_value;
            while (std::getline(lss, str_value, ','))
            {
                try
                {
                    if (std::is_floating_point<T>::value)
                        values.push_back(std::stod(str_value));
                    else
                        values.push_back(std::stoi(str_value));
                }
                catch (const std::invalid_argument &)
                {
                    return SCAPE_EINVAL;
                }
                catch (const std::out_of_range &)
                {
                    return SCAPE_EINVAL;
                }
            }

            return SCAPE_EOK;
        }
        ++idx;
    }

    return SCAPE_EINVAL;
}
