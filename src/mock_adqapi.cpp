#include "mock_adqapi.h"

#include <cstring>
#include <sstream>
#include <type_traits>

const std::string MockAdqApi::DEFAULT_PARAMETERS =
R"""(PARAMETERS
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

const std::string MockAdqApi::DEFAULT_CLOCK_SYSTEM_PARAMETERS =
R"""(CLOCK SYSTEM
sampling frequency:
    500e6, 500e6
)""";

void MockAdqApi::Initialize(const std::vector<Generator::Parameters> &parameters)
{
    m_generators.clear();
    for (const auto &p : parameters)
    {
        m_generators.push_back(std::make_unique<Generator>());
        m_generators.back()->Initialize(p);
    }
}

int MockAdqApi::StartDataAcquisition(int adq_num)
{
    (void)adq_num;
    for (const auto &g : m_generators)
        g->Start();

    return ADQ_EOK;
}

int MockAdqApi::StopDataAcquisition(int adq_num)
{
    (void)adq_num;
    for (const auto &g : m_generators)
        g->Stop();

    return ADQ_EOK;
}

int64_t MockAdqApi::WaitForRecordBuffer(int adq_num, int *channel, void **buffer, int timeout,
                                        struct ADQDataReadoutStatus *status)
{
    (void)adq_num;
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

int MockAdqApi::ReturnRecordBuffer(int adq_num, int channel, void *buffer)
{
    (void)adq_num;

    if (buffer == NULL)
        return ADQ_EINVAL;
    if (channel == -1)
        return ADQ_EUNSUPPORTED;
    if ((channel < 0) || (static_cast<size_t>(channel) >= m_generators.size()))
        return ADQ_EINVAL;

    /* FIXME: Error space */
    return m_generators[channel]->ReturnBuffer(static_cast<ADQGen4Record *>(buffer));
}

int MockAdqApi::InitializeParametersString(int adq_num, enum ADQParameterId id, char *const string, size_t length, int format)
{
    (void)adq_num;
    (void)format;

    switch (id)
    {
    case ADQ_PARAMETER_ID_TOP:
        std::strncpy(string, DEFAULT_PARAMETERS.c_str(), length);
        return static_cast<int>(sizeof(DEFAULT_PARAMETERS));

    case ADQ_PARAMETER_ID_CLOCK_SYSTEM:
        std::strncpy(string, DEFAULT_CLOCK_SYSTEM_PARAMETERS.c_str(), length);
        return static_cast<int>(sizeof(DEFAULT_CLOCK_SYSTEM_PARAMETERS));

    case ADQ_PARAMETER_ID_RESERVED:
    case ADQ_PARAMETER_ID_DATA_ACQUISITION:
    case ADQ_PARAMETER_ID_DATA_TRANSFER:
    case ADQ_PARAMETER_ID_DATA_READOUT:
    case ADQ_PARAMETER_ID_CONSTANT:
    case ADQ_PARAMETER_ID_DIGITAL_GAINANDOFFSET:
    case ADQ_PARAMETER_ID_EVENT_SOURCE_LEVEL:
    case ADQ_PARAMETER_ID_DBS:
    case ADQ_PARAMETER_ID_SAMPLE_SKIP:
    case ADQ_PARAMETER_ID_TEST_PATTERN:
    case ADQ_PARAMETER_ID_EVENT_SOURCE_PERIODIC:
    case ADQ_PARAMETER_ID_EVENT_SOURCE_TRIG:
    case ADQ_PARAMETER_ID_EVENT_SOURCE_SYNC:
    case ADQ_PARAMETER_ID_ANALOG_FRONTEND:
    case ADQ_PARAMETER_ID_PATTERN_GENERATOR0:
    case ADQ_PARAMETER_ID_PATTERN_GENERATOR1:
    case ADQ_PARAMETER_ID_EVENT_SOURCE:
    case ADQ_PARAMETER_ID_SIGNAL_PROCESSING:
    case ADQ_PARAMETER_ID_FUNCTION:
    case ADQ_PARAMETER_ID_PORT_TRIG:
    case ADQ_PARAMETER_ID_PORT_SYNC:
    case ADQ_PARAMETER_ID_PORT_SYNCO:
    case ADQ_PARAMETER_ID_PORT_SYNCI:
    case ADQ_PARAMETER_ID_PORT_CLK:
    case ADQ_PARAMETER_ID_PORT_CLKI:
    case ADQ_PARAMETER_ID_PORT_CLKO:
    case ADQ_PARAMETER_ID_PORT_GPIOA:
    case ADQ_PARAMETER_ID_PORT_GPIOB:
    case ADQ_PARAMETER_ID_PORT_PXIE:
    case ADQ_PARAMETER_ID_PORT_MTCA:
    case ADQ_PARAMETER_ID_PULSE_GENERATOR0:
    case ADQ_PARAMETER_ID_PULSE_GENERATOR1:
    case ADQ_PARAMETER_ID_PULSE_GENERATOR2:
    case ADQ_PARAMETER_ID_PULSE_GENERATOR3:
    case ADQ_PARAMETER_ID_TIMESTAMP_SYNCHRONIZATION:
    case ADQ_PARAMETER_ID_MAX_VAL:
    default:
        return ADQ_EINVAL;
    }
}

int MockAdqApi::SetParametersString(int adq_num, const char *const string, size_t length)
{
    /* FIXME: Implement clock system */
    (void)adq_num;
    (void)length;

    std::string parameters_str(string);
    std::vector<Generator::Parameters> parameters;

    if (parameters_str.rfind("PARAMETERS", 0) == 0)
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

        for (size_t i = 0; i < nof_generators; ++i)
        {
            parameters.push_back(Generator::Parameters());
            parameters.back().record_length = record_length[i];
            parameters.back().trigger_frequency = trigger_frequency[i];
            parameters.back().sine.frequency = frequency[i];
            parameters.back().sine.amplitude = amplitude[i];
            parameters.back().sine.harmonic_distortion = harmonic_distortion[i] > 0;
            parameters.back().sine.noise_std_dev = noise_std_dev[i];
            printf("Pushed a parameter set\n");
        }
    }
    else if (parameters_str.rfind("CLOCK SYSTEM", 0) == 0)
    {
        /* FIXME: Clock system separately? */
    }
    else
    {
        printf("Unrecognized parameter set.\n");
        return ADQ_EINVAL;
    }

    StopDataAcquisition(adq_num); /* TODO: We can quite easily support live updates I think. */
    for (size_t i = 0; i < parameters.size(); ++i)
        m_generators[i]->Initialize(parameters[i]);
    StartDataAcquisition(adq_num);

    return ADQ_EUNSUPPORTED;
}

int MockAdqApi::GetParametersString(int adq_num, enum ADQParameterId id, char *const string, size_t length, int format)
{
    /* FIXME: Implement */
    (void)adq_num;
    (void)id;
    (void)string;
    (void)length;
    (void)format;
    return ADQ_EUNSUPPORTED;
}

int MockAdqApi::ValidateParametersString(int adq_num,  const char *const string, size_t length)
{
    /* FIXME: Implement */
    (void)adq_num;
    (void)string;
    (void)length;
    return ADQ_EUNSUPPORTED;
}

template<typename T>
int MockAdqApi::ParseLine(int line_idx, const std::string &str, std::vector<T> &values)
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

/* ADQ_ interface */
int ADQ_StartDataAcquisition(void *adq_cu, int adq_num)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->StartDataAcquisition(adq_num);
}

int ADQ_StopDataAcquisition(void *adq_cu, int adq_num)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->StopDataAcquisition(adq_num);
}

int64_t ADQ_WaitForRecordBuffer(void *adq_cu, int adq_num, int *channel, void **buffer, int timeout,
                                struct ADQDataReadoutStatus *status)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->WaitForRecordBuffer(adq_num, channel, buffer, timeout,
                                                                  status);
}

int ADQ_ReturnRecordBuffer(void *adq_cu, int adq_num, int channel, void *buffer)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->ReturnRecordBuffer(adq_num, channel, buffer);
}

int ADQ_InitializeParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->InitializeParametersString(adq_num, id, string, length, format);
}

int ADQ_SetParametersString(void *adq_cu, int adq_num, const char *const string, size_t length)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->SetParametersString(adq_num, string, length);
}

int ADQ_GetParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->GetParametersString(adq_num, id, string, length, format);
}

int ADQ_ValidateParametersString(void *adq_cu, int adq_num,  const char *const string, size_t length)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->ValidateParametersString(adq_num, string, length);
}

