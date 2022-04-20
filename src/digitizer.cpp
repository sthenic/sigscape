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
}

Digitizer::~Digitizer()
{
    /* FIXME: Really needed? */
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
    SetState(DigitizerState::NOT_ENUMERATED);
    m_read_queue.Write({DigitizerMessageId::ENUMERATING});

    /* Performing this operation in a thread safe manner requires that
       ADQControlUnit_OpenDeviceInterface() has been called (and returned
       successfully) in a single thread process. */
    int result = ADQControlUnit_SetupDevice(m_id.handle, m_id.index - 1);
    if (result != 1)
    {
        m_read_queue.Write({DigitizerMessageId::SETUP_FAILED});
        m_thread_exit_code = ADQR_EINTERNAL;
        return;
    }

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

    /* Instantiate one data processing thread for each digitizer channel. */
    for (int ch = 0; ch < constant.nof_channels; ++ch)
    {
        std::stringstream ss;
        ss << constant.serial_number << " CH" << constant.channel[ch].label;
        m_processing_threads.push_back(
            std::make_unique<DataProcessing>(m_id.handle, m_id.index, ch, ss.str())
        );
    }

    /* Signal that the digitizer was set up correctly, that we're entering the
       IDLE state and enter the main loop. */
    m_read_queue.Write({DigitizerMessageId::SETUP_OK,
                        std::make_shared<std::string>(constant.serial_number)});
    SetState(DigitizerState::IDLE);

    m_thread_exit_code = ADQR_EOK;
    for (;;)
    {
        ProcessMessages();
        ProcessWatcherMessages();

        /* We implement the sleep using the stop event to be able to immediately
            react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready)
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
            /* FIXME: Check return value */
            StartDataAcquisition();
            break;
        }

        case DigitizerMessageId::STOP_ACQUISITION:
        {
            /* FIXME: Check return value */
            StopDataAcquisition();
            break;
        }

        case DigitizerMessageId::SET_PARAMETERS:
            SetParameters(false);
            break;

        case DigitizerMessageId::VALIDATE_PARAMETERS:
            /* FIXME: Implement */
            break;

        case DigitizerMessageId::GET_PARAMETERS:
            /* FIXME: Implement */
            break;

        case DigitizerMessageId::INITIALIZE_PARAMETERS:
        {
            InitializeParameters(ADQ_PARAMETER_ID_TOP, m_watchers.top);
            InitializeParameters(ADQ_PARAMETER_ID_CLOCK_SYSTEM, m_watchers.clock_system);
            break;
        }

        case DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
            SetParameters(true);
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

int Digitizer::StartDataAcquisition()
{
    for (const auto &t : m_processing_threads)
        t->Start();

    /* FIXME: Return value? Probably as a message. */
    int result = ADQ_StartDataAcquisition(m_id.handle, m_id.index);
    if (result == ADQ_EOK)
        SetState(DigitizerState::ACQUISITION);
    return result;
}

int Digitizer::StopDataAcquisition()
{
    for (const auto &t : m_processing_threads)
        t->Stop();

    /* FIXME: Return value? Probably as a message. */
    ADQ_StopDataAcquisition(m_id.handle, m_id.index);
    SetState(DigitizerState::IDLE);
    return ADQR_EOK;
}

void Digitizer::SetState(DigitizerState state)
{
    m_state = state;
    m_read_queue.Write({DigitizerMessageId::NEW_STATE, state});
}

int Digitizer::SetParameters(bool clock_system)
{
    /* If we're in the acquisition state, we temporarily stop when applying the
       new parameters. */
    DigitizerState previous_state = m_state;

    if (previous_state == DigitizerState::ACQUISITION)
        StopDataAcquisition();

    SetState(DigitizerState::CONFIGURATION);

    /* FIXME: Check return value, emit error etc. */
    if (clock_system)
        SetParameters(m_parameters.clock_system);
    SetParameters(m_parameters.top);

    if (previous_state == DigitizerState::ACQUISITION)
        StartDataAcquisition();

    SetState(previous_state);

    /* FIXME: Probably as a void, errors should propagate via messages. */
    return ADQR_EOK;
}

int Digitizer::SetParameters(const std::unique_ptr<ParameterQueue> &queue)
{
    std::shared_ptr<std::string> parameters_str;

    int result = ADQR_ELAST;
    while (result == ADQR_ELAST)
        result = queue->Read(parameters_str, 0);
    if (result != ADQR_EOK)
        return ADQR_EINTERNAL;

    result = ADQ_SetParametersString(m_id.handle, m_id.index, parameters_str->c_str(),
                                     parameters_str->size());
    if (result > 0)
        return ADQR_EOK;
    else
        return result;
}

int Digitizer::InitializeParameters(enum ADQParameterId id,
                                    const std::unique_ptr<FileWatcher> &watcher)
{
    /* Heap allocation */
    static constexpr size_t SIZE = 32768;
    auto parameters_str = std::unique_ptr<char[]>( new char[SIZE] );
    int result = ADQ_InitializeParametersString(m_id.handle, m_id.index, id, parameters_str.get(),
                                                SIZE, 1);
    if (result > 0)
    {
        watcher->PushMessage({FileWatcherMessageId::UPDATE_FILE,
                              std::make_shared<std::string>(parameters_str.get())});
        return ADQR_EOK;
    }
    else
    {
        return result;
    }
}

void Digitizer::InitializeFileWatchers(const struct ADQConstantParameters &constant)
{
    std::stringstream ss;
    ss << "./parameters_top_" << constant.serial_number << ".json";
    std::string filename = ss.str();
    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
    m_watchers.top = std::make_unique<FileWatcher>(filename);
    ss.str("");
    ss.clear();

    ss << "./parameters_clock_system_" << constant.serial_number << ".json";
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