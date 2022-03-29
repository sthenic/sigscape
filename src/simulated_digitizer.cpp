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
}

int SimulatedDigitizer::Initialize()
{
    /* FIXME: Implement */
    return ADQR_EOK;
}

void SimulatedDigitizer::MainLoop()
{
    m_read_queue.Write({MESSAGE_ID_NEW_STATE, STATE_NOT_ENUMERATED, NULL});
    m_read_queue.Write({MESSAGE_ID_SETUP_STARTING, 0, NULL});

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    m_read_queue.Write({MESSAGE_ID_SETUP_OK, 0, NULL});

    m_thread_exit_code = ADQR_EOK;
    for (;;)
    {
        /* FIXME: Refactor into its own function. */
        struct DigitizerMessage msg;
        int result = m_write_queue.Read(msg, 0);
        if (result == ADQR_EOK)
        {
            result = HandleMessage(msg);
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
    case MESSAGE_ID_START_ACQUISITION:
    {
        printf("Entering acquisition.\n");
        Simulator::SineWave sine;
        m_simulator[0]->Initialize(18000, 30.0, sine);
        sine.frequency = 9e6;
        sine.amplitude = 0.8;
        sine.noise_std_dev = 0.02;
        m_simulator[1]->Initialize(18000, 15.0, sine);
        m_processing_threads[0]->Start();
        m_processing_threads[1]->Start();
        m_state = STATE_ACQUISITION;
        m_read_queue.Write({MESSAGE_ID_NEW_STATE, STATE_ACQUISITION, NULL});
        break;
    }

    case MESSAGE_ID_STOP_ACQUISITION:
    {
        printf("Entering configuration.\n");
        m_processing_threads[0]->Stop();
        m_processing_threads[1]->Stop();
        m_state = STATE_CONFIGURATION;
        m_read_queue.Write({MESSAGE_ID_NEW_STATE, STATE_CONFIGURATION, NULL});
        break;
    }

    case MESSAGE_ID_SETUP_STARTING:
    case MESSAGE_ID_SETUP_OK:
    case MESSAGE_ID_SETUP_FAILED:
    case MESSAGE_ID_PARAMETER_SET_UPDATED_FROM_FILE:
    case MESSAGE_ID_PARAMETER_SET_UPDATED_FROM_GUI:
    case MESSAGE_ID_NEW_STATE:
    default:
        printf("Unknown message id %d.\n", msg.id);
        return ADQR_EINTERNAL;
    }

    return ADQR_EOK;
}
