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
        m_processing_threads.push_back(std::make_unique<DataProcessing>(*simulator));
    }

    m_watcher_parameters = std::make_unique<FileWatcher>("./simulated_digitizer.txt");
    m_watcher_clock_system_parameters = std::make_unique<FileWatcher>("./simulated_digitizer_clock_system.txt");
    m_parameters = std::make_unique<ThreadSafeQueue<std::shared_ptr<std::string>>>(0, true);
    m_clock_system_parameters = std::make_unique<ThreadSafeQueue<std::shared_ptr<std::string>>>(0, true);
}

int SimulatedDigitizer::Initialize()
{
    /* FIXME: Implement */
    return ADQR_EOK;
}

void SimulatedDigitizer::MainLoop()
{
    m_parameters->Start();
    m_clock_system_parameters->Start();

    m_watcher_parameters->Start();
    m_watcher_clock_system_parameters->Start();

    m_read_queue.Write(DigitizerMessage(DigitizerMessageId::NEW_STATE,
                                        DigitizerState::NOT_ENUMERATED));
    m_read_queue.Write(DigitizerMessage(DigitizerMessageId::SETUP_STARTING));

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    m_read_queue.Write(DigitizerMessage(DigitizerMessageId::SETUP_OK));

    m_thread_exit_code = ADQR_EOK;
    for (;;)
    {
        ProcessMessages();

        /* FIXME: Probably zip these up in pairs? */
        ProcessWatcherMessages(m_watcher_parameters, m_parameters);
        ProcessWatcherMessages(m_watcher_clock_system_parameters, m_clock_system_parameters);

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
            Simulator::SineWave sine;
            m_simulator[0]->Initialize(18000, 30.0, sine);
            sine.frequency = 9e6;
            sine.amplitude = 0.8;
            sine.noise_std_dev = 0.02;
            sine.harmonic_distortion = true;
            m_simulator[1]->Initialize(18000, 15.0, sine);
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
            /* FIXME: Implement */
            std::shared_ptr<std::string> parameters;
            int result = m_parameters->Read(parameters, 0);
            if (result == ADQR_EOK)
            {
                std::vector<double> frequency;
                result = ParseLine(1, *parameters, frequency);
                if (result == ADQR_EOK)
                    printf("Success\n");
                else
                    printf("Failed\n");
            }
            break;
        }

        case DigitizerMessageId::VALIDATE_PARAMETERS:
            /* FIXME: Implement something */
            break;

        case DigitizerMessageId::GET_PARAMETERS:
            /* The file holds the state so there's nothing to do. */
            break;

        case DigitizerMessageId::INITIALIZE_PARAMETERS:
            m_watcher_parameters->PushMessage(
                {FileWatcherMessageId::UPDATE_FILE,
                 std::make_shared<std::string>(DEFAULT_PARAMETERS)});
            m_watcher_clock_system_parameters->PushMessage(
                {FileWatcherMessageId::UPDATE_FILE,
                 std::make_shared<std::string>(DEFAULT_CLOCK_SYSTEM_PARAMETERS)});
            break;

        case DigitizerMessageId::SETUP_STARTING:
        case DigitizerMessageId::SETUP_OK:
        case DigitizerMessageId::SETUP_FAILED:
        case DigitizerMessageId::NEW_STATE:
        default:
            break;
        }
    }
}

void SimulatedDigitizer::ProcessWatcherMessages(
    const std::unique_ptr<FileWatcher> &watcher,
    const std::unique_ptr<ThreadSafeQueue<std::shared_ptr<std::string>>> &queue)
{
    FileWatcherMessage message;
    while (ADQR_EOK == watcher->WaitForMessage(message, 0))
    {
        switch (message.id)
        {
        case FileWatcherMessageId::FILE_CREATED:
        case FileWatcherMessageId::FILE_UPDATED:
        case FileWatcherMessageId::FILE_DELETED:
            queue->Write(message.contents);
            break;

        case FileWatcherMessageId::UPDATE_FILE:
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
        }
        ++idx;
    }

    return ADQR_EOK;
}
