#include "simulated_digitizer.h"

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

    m_watcher_parameters = std::make_unique<FileWatcher>("./simulated_digitizer.json");
    m_watcher_clock_system_parameters = std::make_unique<FileWatcher>("./simulated_digitizer_clock_system.json");
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
        /* FIXME: Refactor into its own function. */
        struct DigitizerMessage message;
        int result = m_write_queue.Read(message, 0);
        if (result == ADQR_EOK)
        {
            result = HandleMessage(message);
            if (result != ADQR_EOK)
            {
                printf("Failed to handle a message, result %d.\n", result);
                m_thread_exit_code = ADQR_EINTERNAL;
                break;
            }
        }
        else if ((result == ADQR_EINTERRUPTED) || (result == ADQR_ENOTREADY))
        {
            break;
        }
        else if (result != ADQR_EAGAIN)
        {
            printf("Failed to read an inbound message, result %d.\n", result);
            m_thread_exit_code = ADQR_EINTERNAL;
            break;
        }

        /* FIXME: Probably zip these up in pairs? */
        ProcessWatcherMessages(m_watcher_parameters, m_parameters);
        ProcessWatcherMessages(m_watcher_clock_system_parameters, m_clock_system_parameters);

        /* We implement the sleep using the stop event to be able to immediately
            react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            break;
    }
}

int SimulatedDigitizer::HandleMessage(const struct DigitizerMessage &msg)
{
    switch (msg.id)
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

    case DigitizerMessageId::SETUP_STARTING:
    case DigitizerMessageId::SETUP_OK:
    case DigitizerMessageId::SETUP_FAILED:
    case DigitizerMessageId::NEW_STATE:
    default:
        printf("Unknown message id %d.\n", static_cast<int>(msg.id));
        return ADQR_EINTERNAL;
    }

    return ADQR_EOK;
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
