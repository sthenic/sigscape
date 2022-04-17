#include "digitizer.h"

#include <sstream>
#include <algorithm>

Digitizer::Digitizer(void *handle, int index)
    : m_state(DigitizerState::NOT_ENUMERATED)
    , m_id{handle, index}
    , m_watchers{}
    , m_parameters{}
    , m_processing_threads{}
{
    /* FIXME: Move this to initialization in mainloop */
    for (int i = 0; i < ADQ_MAX_NOF_CHANNELS; ++i)
        m_processing_threads.push_back(std::make_unique<DataProcessing>(m_id.handle, m_id.index, i));
}

Digitizer::~Digitizer()
{
    Stop();
}

/* Interface to the digitizer's data processing threads, one per channel. */
int Digitizer::WaitForProcessedRecord(int channel, std::shared_ptr<ProcessedRecord> &record)
{
    if ((channel < 0) || (channel >= static_cast<int>(m_processing_threads.size())))
        return ADQR_EINVAL;

    return m_processing_threads[channel]->WaitForBuffer(record, 0);
}

/* TODO: We can probably do the same generalization for the MessageThread
          with a persistent read queue and expose that directly. */
int Digitizer::WaitForParameters(std::shared_ptr<std::string> &str)
{
    if (m_parameters.top == NULL)
        return ADQR_ENOTREADY;

    return m_parameters.top->Read(str, 0);
}

int Digitizer::WaitForClockSystemParameters(std::shared_ptr<std::string> &str)
{
    if (m_parameters.clock_system == NULL)
        return ADQR_ENOTREADY;

    return m_parameters.clock_system->Read(str, 0);
}

void Digitizer::MainLoop()
{
    /* When the main loop is started, we assume ownership of the digitizer with
       the identifier we were given when this object was constructed. We begin
       by enumerating the digitizer, completing its initial setup procedure. */
    m_read_queue.Write({DigitizerMessageId::NEW_STATE, DigitizerState::NOT_ENUMERATED});
    m_read_queue.Write({DigitizerMessageId::ENUMERATING});

    /* Performing this operation in a thread safe manner requires that
       ADQControlUnit_OpenDeviceInterface() has been called (and returned
       successfully) in a single thread process. */
    int result = ADQControlUnit_SetupDevice(m_id.handle, m_id.index);
    if (result != 1)
    {
        m_read_queue.Write({DigitizerMessageId::SETUP_FAILED});
        m_thread_exit_code = ADQR_EINTERNAL;
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    /* Read the digitizer's constant parameters, then start the file watchers
       for the parameter sets. */
    struct ADQConstantParameters constant;
    result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CONSTANT, &constant);
    if (result != sizeof(constant))
    {
        m_read_queue.Write({DigitizerMessageId::SETUP_FAILED});
        m_thread_exit_code = ADQR_EINTERNAL;
        return;
    }
    InitializeFileWatchers(constant);

    /* Signal that the digitizer was set up correctly and enter the main loop. */
    m_read_queue.Write({DigitizerMessageId::SETUP_OK});
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

    for (const auto &t : m_processing_threads)
        t->Stop();
    ADQ_StopDataAcquisition(m_id.handle, m_id.index);
}

void Digitizer::ProcessMessages()
{
    DigitizerMessage message;
    while (ADQR_EOK == m_write_queue.Read(message, 0))
    {
        switch (message.id)
        {
        case DigitizerMessageId::START_ACQUISITION:
        {
            for (const auto &t : m_processing_threads)
                t->Start();
            ADQ_StartDataAcquisition(m_id.handle, m_id.index);

            m_state = DigitizerState::ACQUISITION;
            m_read_queue.Write({DigitizerMessageId::NEW_STATE, DigitizerState::ACQUISITION});
            break;
        }

        case DigitizerMessageId::STOP_ACQUISITION:
        {
            for (const auto &t : m_processing_threads)
                t->Stop();
            ADQ_StopDataAcquisition(m_id.handle, m_id.index);

            m_state = DigitizerState::CONFIGURATION;
            m_read_queue.Write({DigitizerMessageId::NEW_STATE, DigitizerState::CONFIGURATION});
            break;
        }

        case DigitizerMessageId::SET_PARAMETERS:
        {
            /* FIXME: Not ideal to stop the threads. */
            for (const auto &t : m_processing_threads)
                t->Stop();
            SetParameters();
            for (const auto &t : m_processing_threads)
                t->Start();
            break;
        }

        case DigitizerMessageId::VALIDATE_PARAMETERS:
            /* FIXME: Implement something */
            break;

        case DigitizerMessageId::GET_PARAMETERS:
            /* The file holds the state so there's nothing to do. */
            break;

        case DigitizerMessageId::INITIALIZE_PARAMETERS:
        {
            char parameters_str[16384];
            int result = ADQ_InitializeParametersString(m_id.handle, m_id.index,
                                                        ADQ_PARAMETER_ID_TOP, parameters_str,
                                                        sizeof(parameters_str), 1);
            if (result > 0)
            {
                m_watchers.top->PushMessage({FileWatcherMessageId::UPDATE_FILE,
                                             std::make_shared<std::string>(parameters_str)});
            }

            result = ADQ_InitializeParametersString(m_id.handle, m_id.index,
                                                    ADQ_PARAMETER_ID_CLOCK_SYSTEM, parameters_str,
                                                    sizeof(parameters_str), 1);
            if (result > 0)
            {
                m_watchers.clock_system->PushMessage({FileWatcherMessageId::UPDATE_FILE,
                                                     std::make_shared<std::string>(parameters_str)});
            }
            break;
        }

        case DigitizerMessageId::ENUMERATING:
        case DigitizerMessageId::SETUP_OK:
        case DigitizerMessageId::SETUP_FAILED:
        case DigitizerMessageId::NEW_STATE:
        default:
            break;
        }
    }
}

void Digitizer::ProcessWatcherMessages(const std::unique_ptr<FileWatcher> &watcher,
                                       const std::unique_ptr<ParameterQueue> &queue)
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

int Digitizer::SetParameters()
{
    /* FIXME: A very rough implementation. */
    /* FIXME: Split this up so top/clock systems are set by different functions. */
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

    ADQ_SetParametersString(m_id.handle, m_id.index, parameters_str->c_str(), parameters_str->size());
    return ADQR_EOK;
}

void Digitizer::InitializeFileWatchers(const struct ADQConstantParameters &constant)
{
    std::stringstream ss;
    ss << "./parameters_top_" << constant.serial_number << ".txt";
    std::string filename = ss.str();
    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
    m_watchers.top = std::make_unique<FileWatcher>(filename);
    ss.str("");
    ss.clear();

    ss << "./parameters_clock_system_" << constant.serial_number << ".txt";
    filename = ss.str();
    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
    m_watchers.clock_system = std::make_unique<FileWatcher>(filename);

    m_parameters.top = std::make_unique<ThreadSafeQueue<std::shared_ptr<std::string>>>(0, true);
    m_parameters.clock_system = std::make_unique<ThreadSafeQueue<std::shared_ptr<std::string>>>(0, true);

    m_parameters.top->Start();
    m_parameters.clock_system->Start();

    m_watchers.top->Start();
    m_watchers.clock_system->Start();
}
