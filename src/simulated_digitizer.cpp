#include "simulated_digitizer.h"

#include <sstream>
#include <type_traits>

const std::string SimulatedDigitizer::DEFAULT_PARAMETERS =
R"""(frequency:
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

const std::string SimulatedDigitizer::DEFAULT_CLOCK_SYSTEM_PARAMETERS =
R"""(sampling frequency:
    500e6, 500e6
)""";

SimulatedDigitizer::SimulatedDigitizer()
    : Digitizer()
    , m_simulator{}
{
    for (int i = 0; i < ADQ_MAX_NOF_CHANNELS; ++i)
    {
        auto simulator = std::make_shared<DataAcquisitionSimulator>();
        m_simulator.push_back(simulator);
        m_processing_threads.push_back(std::make_unique<DataProcessing>(simulator));
    }

    m_watchers.top = std::make_unique<FileWatcher>("./simulated_digitizer.txt");
    m_watchers.clock_system = std::make_unique<FileWatcher>("./simulated_digitizer_clock_system.txt");
    m_parameters.top = std::make_unique<ThreadSafeQueue<std::shared_ptr<std::string>>>(0, true);
    m_parameters.clock_system = std::make_unique<ThreadSafeQueue<std::shared_ptr<std::string>>>(0, true);
}

void SimulatedDigitizer::MainLoop()
{
    m_parameters.top->Start();
    m_parameters.clock_system->Start();

    m_watchers.top->Start();
    m_watchers.clock_system->Start();

    m_simulator[0]->Initialize();
    m_simulator[1]->Initialize();

    m_read_queue.Write(DigitizerMessage(DigitizerMessageId::NEW_STATE,
                                        DigitizerState::NOT_ENUMERATED));
    m_read_queue.Write(DigitizerMessage(DigitizerMessageId::ENUMERATING));

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    m_read_queue.Write(DigitizerMessage(DigitizerMessageId::SETUP_OK));

    m_thread_exit_code = ADQR_EOK;
    for (;;)
    {
        ProcessMessages();
        ProcessWatcherMessages();

        /* We implement the sleep using the stop event to be able to immediately
            react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            break;
    }
}

void SimulatedDigitizer::ProcessMessages()
{
    DigitizerMessage message;
    while (ADQR_EOK == m_write_queue.Read(message, 0))
    {
        switch (message.id)
        {
        case DigitizerMessageId::START_ACQUISITION:
        {
            printf("Entering acquisition.\n");
            m_processing_threads[0]->Start();
            m_processing_threads[1]->Start();
            m_state = DigitizerState::ACQUISITION;
            m_read_queue.Write(DigitizerMessage(DigitizerMessageId::NEW_STATE,
                                                DigitizerState::ACQUISITION));
            break;
        }

        case DigitizerMessageId::STOP_ACQUISITION:
        {
            printf("Entering configuration.\n");
            m_processing_threads[0]->Stop();
            m_processing_threads[1]->Stop();
            m_state = DigitizerState::CONFIGURATION;
            m_read_queue.Write(DigitizerMessage(DigitizerMessageId::NEW_STATE,
                                                DigitizerState::CONFIGURATION));
            break;
        }

        case DigitizerMessageId::SET_PARAMETERS:
        {
            /* TODO: We temporarily suspend the  */
            m_processing_threads[0]->Stop();
            m_processing_threads[1]->Stop();
            SetParameters();
            m_processing_threads[0]->Start();
            m_processing_threads[1]->Start();
            break;
        }

        case DigitizerMessageId::VALIDATE_PARAMETERS:
            /* FIXME: Implement something */
            break;

        case DigitizerMessageId::GET_PARAMETERS:
            /* The file holds the state so there's nothing to do. */
            break;

        case DigitizerMessageId::INITIALIZE_PARAMETERS:
            m_watchers.top->PushMessage(
                {FileWatcherMessageId::UPDATE_FILE,
                 std::make_shared<std::string>(DEFAULT_PARAMETERS)});
            m_watchers.clock_system->PushMessage(
                {FileWatcherMessageId::UPDATE_FILE,
                 std::make_shared<std::string>(DEFAULT_CLOCK_SYSTEM_PARAMETERS)});
            break;

        case DigitizerMessageId::ENUMERATING:
        case DigitizerMessageId::SETUP_OK:
        case DigitizerMessageId::SETUP_FAILED:
        case DigitizerMessageId::NEW_STATE:
        default:
            break;
        }
    }
}

template<typename T>
int SimulatedDigitizer::ParseLine(int line_idx, const std::string &str, std::vector<T> &values)
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

int SimulatedDigitizer::SetParameters()
{
    /* FIXME: A very rough implementation. */
    std::shared_ptr<std::string> parameters_str;
    std::shared_ptr<std::string> clock_system_parameters_str;

    int result = ADQR_ELAST;
    while (result == ADQR_ELAST)
        result = m_parameters.top->Read(parameters_str, 0);

    if (result != ADQR_EOK)
        return ADQR_EINTERNAL;

    result = ADQR_ELAST;
    while (result == ADQR_ELAST)
        result = m_parameters.clock_system->Read(clock_system_parameters_str, 0);

    if (result != ADQR_EOK)
        return ADQR_EINTERNAL;

    std::vector<double> frequency;
    if (ADQR_EOK != ParseLine(1, *parameters_str, frequency))
        return ADQR_EINVAL;

    std::vector<double> amplitude;
    if (ADQR_EOK != ParseLine(3, *parameters_str, amplitude))
        return ADQR_EINVAL;

    std::vector<int> record_length;
    if (ADQR_EOK != ParseLine(5, *parameters_str, record_length))
        return ADQR_EINVAL;

    std::vector<double> trigger_frequency;
    if (ADQR_EOK != ParseLine(7, *parameters_str, trigger_frequency))
        return ADQR_EINVAL;

    std::vector<int> harmonic_distortion;
    if (ADQR_EOK != ParseLine(9, *parameters_str, harmonic_distortion))
        return ADQR_EINVAL;

    std::vector<double> noise_std_dev;
    if (ADQR_EOK != ParseLine(11, *parameters_str, noise_std_dev))
        return ADQR_EINVAL;

    std::vector<double> sampling_frequency;
    if (ADQR_EOK != ParseLine(1, *clock_system_parameters_str, sampling_frequency))
        return ADQR_EINVAL;

    /* FIXME: Don't do manual unrolling. Also some access checks would probably help. */
    Generator::Parameters parameters;
    parameters.record_length = record_length[0];
    parameters.trigger_frequency = trigger_frequency[0];
    parameters.sine.frequency = frequency[0];
    parameters.sine.amplitude = amplitude[0];
    parameters.sine.harmonic_distortion = harmonic_distortion[0] > 0;
    parameters.sine.noise_std_dev = noise_std_dev[0];
    parameters.sine.sampling_frequency = sampling_frequency[0];
    m_simulator[0]->Initialize(parameters);

    parameters = Generator::Parameters();
    parameters.record_length = record_length[1];
    parameters.trigger_frequency = trigger_frequency[1];
    parameters.sine.frequency = frequency[1];
    parameters.sine.amplitude = amplitude[1];
    parameters.sine.harmonic_distortion = harmonic_distortion[1] > 0;
    parameters.sine.noise_std_dev = noise_std_dev[1];
    parameters.sine.sampling_frequency = sampling_frequency[1];
    m_simulator[1]->Initialize(parameters);

    return ADQR_EOK;
}
