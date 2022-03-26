#include "simulated_digitizer.h"

SimulatedDigitizer::SimulatedDigitizer()
    : Digitizer()
    , m_simulator()
    , m_data_processing(m_simulator)
    , m_data_read_queue(0, true)
{}

SimulatedDigitizer::~SimulatedDigitizer()
{
    Stop();
}

int SimulatedDigitizer::Initialize()
{
    /* FIXME: Implement */
    return ADQR_EOK;
}

int SimulatedDigitizer::Start()
{
    m_data_read_queue.Start();
    return MessageThread::Start();
}

int SimulatedDigitizer::Stop()
{
    m_data_read_queue.Stop();
    m_data_read_queue.Free();
    return MessageThread::Stop();
}

void SimulatedDigitizer::MainLoop()
{
    m_data_read_queue.Start();

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

        if (m_state == STATE_ACQUISITION)
        {
            result = DoAcquisition();
            if (result != ADQR_EOK)
            {
                printf("Acquisition failed, result %d.\n", result);
                m_thread_exit_code = ADQR_EINTERNAL;
                break;
            }
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
        printf("Entering acquisition.\n");
        m_simulator.Initialize(10000, 60.0, Simulator::SineWave());
        m_data_processing.Start();
        m_state = STATE_ACQUISITION;
        m_read_queue.Write({MESSAGE_ID_NEW_STATE, STATE_ACQUISITION, NULL});
        break;

    case MESSAGE_ID_STOP_ACQUISITION:
        printf("Entering configuration.\n");
        m_data_processing.Stop();
        m_state = STATE_CONFIGURATION;
        m_read_queue.Write({MESSAGE_ID_NEW_STATE, STATE_CONFIGURATION, NULL});
        break;

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

int SimulatedDigitizer::DoAcquisition()
{
    /* FIXME: Support several channels. */
    struct ProcessedRecord *processed_record = NULL;
    int result = m_data_processing.WaitForBuffer(processed_record, 0);
    if (result == ADQR_EAGAIN)
    {
        return ADQR_EOK;
    }
    else if (result < 0)
    {
        printf("Failed to get a processed record buffer %d.\n", result);
        return result;
    }

    /* We have a buffer whose contents we copy into persistent storage used to
       plot the data. The plot process may hold the lock, in which case we
       discard the data. FIXME: Perhaps two modes: discard or not? */
    struct ProcessedRecord *copy = new ProcessedRecord(*processed_record);
    result = m_data_read_queue.Write(copy);

    m_data_processing.ReturnBuffer(processed_record);
    return ADQR_EOK;
}
