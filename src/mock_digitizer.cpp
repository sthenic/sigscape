#include "mock_digitizer.h"

#include <cstring>
#include <sstream>
#include <type_traits>
#include <cstring>

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
noise standard deviation:
    0.1, 0.02
)""";

const std::string MockDigitizer::DEFAULT_CLOCK_SYSTEM_PARAMETERS =
R"""(CLOCK SYSTEM
sampling frequency:
    500e6, 500e6
)""";

MockDigitizer::MockDigitizer(const std::string &serial_number, int nof_channels)
    : m_constant{}
    , m_generators{}
{
    m_constant.id = ADQ_PARAMETER_ID_CONSTANT;
    m_constant.magic = ADQ_PARAMETERS_MAGIC;
    m_constant.nof_channels = nof_channels;
    std::strncpy(m_constant.serial_number, serial_number.c_str(),
                 std::min(sizeof(m_constant.serial_number), serial_number.size()));

    for (int ch = 0; ch < nof_channels; ++ch)
    {
        m_constant.channel[ch].label[0] = "ABCDEFGH"[ch];
        m_generators.push_back(std::make_unique<Generator>());
    }
}

int MockDigitizer::SetupDevice()
{
    /* FIXME: Anything here? */
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return 1;
}

int MockDigitizer::StartDataAcquisition()
{
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
    {
        printf("Channel is %d.\n", *channel);
        return ADQ_EINVAL;
    }

    ADQGen4Record *lbuffer = NULL;
    int result = m_generators[*channel]->WaitForBuffer(lbuffer, timeout);
    *buffer = lbuffer;

    /* FIXME: Error code space etc.*/
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
    else
    {
        return ADQ_EUNSUPPORTED;
    }
}

int MockDigitizer::InitializeParametersString(enum ADQParameterId id, char *const string, size_t length, int format)
{
    (void)format;

    if (id == ADQ_PARAMETER_ID_TOP)
    {
        std::strncpy(string, DEFAULT_TOP_PARAMETERS.c_str(), length);
        return static_cast<int>(sizeof(DEFAULT_TOP_PARAMETERS));
    }
    else if (id == ADQ_PARAMETER_ID_CLOCK_SYSTEM)
    {
        std::strncpy(string, DEFAULT_CLOCK_SYSTEM_PARAMETERS.c_str(), length);
        return static_cast<int>(sizeof(DEFAULT_CLOCK_SYSTEM_PARAMETERS));
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
        if (ADQR_EOK != ParseLine(2, parameters_str, frequency))
            return ADQR_EINVAL;
        size_t nof_generators = frequency.size();

        std::vector<double> amplitude;
        if (ADQR_EOK != ParseLine(4, parameters_str, amplitude))
            return ADQR_EINVAL;
        nof_generators = std::min(nof_generators, amplitude.size());

        std::vector<int> record_length;
        if (ADQR_EOK != ParseLine(6, parameters_str, record_length))
            return ADQR_EINVAL;
        nof_generators = std::min(nof_generators, record_length.size());

        std::vector<double> trigger_frequency;
        if (ADQR_EOK != ParseLine(8, parameters_str, trigger_frequency))
            return ADQR_EINVAL;
        nof_generators = std::min(nof_generators, trigger_frequency.size());

        std::vector<int> harmonic_distortion;
        if (ADQR_EOK != ParseLine(10, parameters_str, harmonic_distortion))
            return ADQR_EINVAL;
        nof_generators = std::min(nof_generators, harmonic_distortion.size());

        std::vector<double> noise_std_dev;
        if (ADQR_EOK != ParseLine(12, parameters_str, noise_std_dev))
            return ADQR_EINVAL;
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
            parameters.back().sine.noise_std_dev = noise_std_dev[i];
        }

        for (size_t i = 0; (i < parameters.size()) && (i < m_generators.size()); ++i)
            m_generators[i]->SetParameters(parameters[i]);

        /* Emulate reconfiguration time. */
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    else if (parameters_str.rfind("CLOCK SYSTEM", 0) == 0)
    {
        std::vector<double> sampling_frequency;
        if (ADQR_EOK != ParseLine(2, parameters_str, sampling_frequency))
            return ADQR_EINVAL;

        for (size_t i = 0; (i < sampling_frequency.size()) && (i < m_generators.size()); ++i)
            m_generators[i]->SetSamplingFrequency(sampling_frequency[i]);

        /* Emulate reconfiguration time. */
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    else
    {
        printf("Unrecognized parameter set.\n");
        return ADQ_EINVAL;
    }

    /* FIXME: Return value */
    return static_cast<int>(length);
}

int MockDigitizer::GetParametersString(enum ADQParameterId id, char *const string, size_t length, int format)
{
    /* FIXME: Implement */
    (void)id;
    (void)string;
    (void)length;
    (void)format;
    return ADQ_EUNSUPPORTED;
}

int MockDigitizer::ValidateParametersString( const char *const string, size_t length)
{
    /* FIXME: Implement */
    (void)string;
    (void)length;
    return ADQ_EUNSUPPORTED;
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
                    return ADQR_EINVAL;
                }
                catch (const std::out_of_range &)
                {
                    return ADQR_EINVAL;
                }
            }

            return ADQR_EOK;
        }
        ++idx;
    }

    return ADQR_EINVAL;
}
