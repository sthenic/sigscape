/* This file implements control of a digitizer. It's essentially a threaded
   state machine wrapped around a collection of objects that together make up
   the functionality that we need. These include:

     - Identification information for the digitizer.
     - Channel-specific threads to read and process data.
     - Two file watchers, dedicated to observing the top and clock system
       parameter files for any changes.

   The external world communicates with the digitizer by sending and receiving
   messages. These messages often correspond to an action initiated by the user,
   like starting the acquisition or applying a new set of parameters. The
   messages are processed by a handler that's specific to the current state. */

#include "digitizer.h"
#include "fmt/core.h"
#include "fmt/format.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

/* A private exception type that we use for simplified control flow within this class. */
class DigitizerException : public std::runtime_error
{
public:
    DigitizerException(const std::string &str) : std::runtime_error(str) {};
};

template<>
struct fmt::formatter<DigitizerMessageId> : formatter<string_view>
{
    template <typename FmtContext>
    auto format(DigitizerMessageId id, FmtContext &ctx)
    {
        string_view name = "unknown";
        switch (id)
        {
        case DigitizerMessageId::DIRTY_TOP_PARAMETERS:
            name = "DIRTY_TOP_PARAMETERS";
            break;
        case DigitizerMessageId::DIRTY_CLOCK_SYSTEM_PARAMETERS:
            name = "DIRTY_CLOCK_SYSTEM_PARAMETERS";
            break;
        case DigitizerMessageId::CLEAN_TOP_PARAMETERS:
            name = "CLEAN_TOP_PARAMETERS";
            break;
        case DigitizerMessageId::CLEAN_CLOCK_SYSTEM_PARAMETERS:
            name = "CLEAN_CLOCK_SYSTEM_PARAMETERS";
            break;
        case DigitizerMessageId::IDENTIFIER:
            name = "IDENTIFIER";
            break;
        case DigitizerMessageId::NOF_CHANNELS:
            name = "NOF_CHANNELS";
            break;
        case DigitizerMessageId::STATE:
            name = "STATE";
            break;
        case DigitizerMessageId::ERR:
            name = "ERROR";
            break;
        case DigitizerMessageId::CLEAR:
            name = "CLEAR";
            break;
        case DigitizerMessageId::CONFIGURATION:
            name = "CONFIGURATION";
            break;
        case DigitizerMessageId::INITIALIZE_WOULD_OVERWRITE:
            name = "INITIALIZE_WOULD_OVERWRITE";
            break;
        case DigitizerMessageId::SET_INTERNAL_REFERENCE:
            name = "SET_INTERNAL_REFERENCE";
            break;
        case DigitizerMessageId::SET_EXTERNAL_REFERENCE:
            name = "SET_EXTERNAL_REFERENCE";
            break;
        case DigitizerMessageId::SET_EXTERNAL_CLOCK:
            name = "SET_EXTERNAL_CLOCK";
            break;
        case DigitizerMessageId::DEFAULT_ACQUISITION:
            name = "DEFAULT_ACQUISITION";
            break;
        case DigitizerMessageId::START_ACQUISITION:
            name = "START_ACQUISITION";
            break;
        case DigitizerMessageId::STOP_ACQUISITION:
            name = "STOP_ACQUISITION";
            break;
        case DigitizerMessageId::SET_PARAMETERS:
            name = "SET_PARAMETERS";
            break;
        case DigitizerMessageId::GET_PARAMETERS:
            name = "GET_PARAMETERS";
            break;
        case DigitizerMessageId::VALIDATE_PARAMETERS:
            name = "VALIDATE_PARAMETERS";
            break;
        case DigitizerMessageId::INITIALIZE_PARAMETERS:
            name = "INITIALIZE_PARAMETERS";
            break;
        case DigitizerMessageId::INITIALIZE_PARAMETERS_FORCE:
            name = "INITIALIZE_PARAMETERS_FORCE";
            break;
        case DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
            name = "SET_CLOCK_SYSTEM_PARAMETERS";
            break;
        case DigitizerMessageId::SET_WINDOW_TYPE:
            name = "SET_WINDOW_TYPE";
            break;
        }

        return fmt::formatter<string_view>::format(name, ctx);
    }
};

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

void Digitizer::MainLoop()
{
    /* When the main loop is started, we assume ownership of the digitizer with
       the identifier we were given when this object was constructed. We begin
       by enumerating the digitizer, completing its initial setup procedure. */
    m_read_queue.EmplaceWrite(DigitizerMessageId::IDENTIFIER,
                              std::make_shared<std::string>("Unknown"));
    SetState(DigitizerState::ENUMERATION);

    /* Performing this operation in a thread safe manner requires that
       ADQControlUnit_OpenDeviceInterface() has been called (and returned
       successfully) in a single thread process. */
    int result = ADQControlUnit_SetupDevice(m_id.handle, m_id.index - 1);
    if (result != 1)
    {
        m_thread_exit_code = ADQR_EINTERNAL;
        SetState(DigitizerState::NOT_ENUMERATED);
        return;
    }

    /* Read the digitizer's constant parameters, then start the file watchers
       for the parameter sets. */
    struct ADQConstantParameters constant;
    result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CONSTANT, &constant);
    if (result != sizeof(constant))
    {
        m_thread_exit_code = ADQR_EINTERNAL;
        SetState(DigitizerState::NOT_ENUMERATED);
        return;
    }
    InitializeFileWatchers(constant);

    /* Instantiate one data processing thread for each digitizer channel. */
    for (int ch = 0; ch < constant.nof_channels; ++ch)
    {
        std::string label = fmt::format("{} CH{}", constant.serial_number,
                                                   constant.channel[ch].label);
        m_processing_threads.emplace_back(
            std::make_unique<DataProcessing>(m_id.handle, m_id.index, ch, label, constant)
        );
    }

    /* Signal that the digitizer was set up correctly, that we're entering the
       IDLE state and enter the main loop. */
    m_read_queue.EmplaceWrite(DigitizerMessageId::IDENTIFIER,
                              std::make_shared<std::string>(constant.serial_number));
    m_read_queue.EmplaceWrite(DigitizerMessageId::NOF_CHANNELS, constant.nof_channels);
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

    StopDataAcquisition();
}

void Digitizer::ProcessMessages()
{
    DigitizerMessage message;
    while (ADQR_EOK == m_write_queue.Read(message, 0))
        HandleMessageInState(message);
}

void Digitizer::ProcessWatcherMessages()
{
    ProcessWatcherMessages(m_watchers.top, m_parameters.top,
                           DigitizerMessageId::DIRTY_TOP_PARAMETERS);
    ProcessWatcherMessages(m_watchers.clock_system, m_parameters.clock_system,
                           DigitizerMessageId::DIRTY_CLOCK_SYSTEM_PARAMETERS);
}

void Digitizer::ProcessWatcherMessages(const std::unique_ptr<FileWatcher> &watcher,
                                       std::shared_ptr<std::string> &str,
                                       DigitizerMessageId dirty_id)
{
    FileWatcherMessage message;
    while (ADQR_EOK == watcher->WaitForMessage(message, 0))
    {
        switch (message.id)
        {
        case FileWatcherMessageId::FILE_CREATED:
        case FileWatcherMessageId::FILE_UPDATED:
            str = message.contents;
            m_read_queue.Write(dirty_id);
            break;

        case FileWatcherMessageId::FILE_DELETED:
            /* FIXME: What to do here? */
            break;

        case FileWatcherMessageId::UPDATE_FILE:
        default:
            break;
        }
    }
}

void Digitizer::StartDataAcquisition()
{
    try
    {
        struct ADQAnalogFrontendParameters afe;
        int result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_ANALOG_FRONTEND, &afe);
        if (result != sizeof(afe))
            throw DigitizerException(fmt::format("ADQ_GetParameters failed, result {}.", result));

        for (size_t i = 0; i < m_processing_threads.size(); ++i)
            m_processing_threads[i]->SetAnalogFrontendParameters(afe.channel[i]);

        for (const auto &t : m_processing_threads)
        {
            if (ADQR_EOK != t->Start())
                throw DigitizerException("Failed to start one of the data processing threads.");
        }

        result = ADQ_StartDataAcquisition(m_id.handle, m_id.index);
        if (result != ADQ_EOK)
            throw DigitizerException(fmt::format("ADQ_GetParameters failed, result {}.", result));
    }
    catch (const DigitizerException &e)
    {
        StopDataAcquisition();
        throw;
    }
}

void Digitizer::StopDataAcquisition()
{
    /* Ignoring the error code here is intentional. We want to attempt to stop
       all processing threads, regardless of the outcome of the other actions. */
    for (const auto &t : m_processing_threads)
        t->Stop();
    ADQ_StopDataAcquisition(m_id.handle, m_id.index);
}

void Digitizer::SetState(DigitizerState state)
{
    m_state = state;
    m_read_queue.EmplaceWrite(DigitizerMessageId::STATE, state);
}

void Digitizer::HandleMessageInNotEnumerated(const struct DigitizerMessage &message)
{
    /* TODO: Nothing to do? */
    (void)message;
}

void Digitizer::HandleMessageInEnumeration(const struct DigitizerMessage &message)
{
    /* TODO: Nothing to do? */
    (void)message;
}

void Digitizer::HandleMessageInIdle(const struct DigitizerMessage &message)
{
    /* In the IDLE state we just let the exceptions propagate to the upper
       layers to create an error message. */
    switch (message.id)
    {
    case DigitizerMessageId::START_ACQUISITION:
        StartDataAcquisition();
        SetState(DigitizerState::ACQUISITION);
        break;

    case DigitizerMessageId::SET_PARAMETERS:
        SetParameters(m_parameters.top, DigitizerMessageId::CLEAN_TOP_PARAMETERS);
        break;

    case DigitizerMessageId::SET_INTERNAL_REFERENCE:
        ConfigureInternalReference();
        SetParameters(m_parameters.top, DigitizerMessageId::CLEAN_TOP_PARAMETERS);
        break;

    case DigitizerMessageId::SET_EXTERNAL_REFERENCE:
        ConfigureExternalReference();
        SetParameters(m_parameters.top, DigitizerMessageId::CLEAN_TOP_PARAMETERS);
        break;

    case DigitizerMessageId::SET_EXTERNAL_CLOCK:
        ConfigureExternalClock();
        SetParameters(m_parameters.top, DigitizerMessageId::CLEAN_TOP_PARAMETERS);
        break;

    case DigitizerMessageId::DEFAULT_ACQUISITION:
        ConfigureDefaultAcquisition();
        break;

    case DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
        SetParameters(m_parameters.clock_system, DigitizerMessageId::CLEAN_CLOCK_SYSTEM_PARAMETERS);
        SetParameters(m_parameters.top, DigitizerMessageId::CLEAN_TOP_PARAMETERS);
        break;

    case DigitizerMessageId::GET_PARAMETERS:
        GetParameters(ADQ_PARAMETER_ID_TOP, m_watchers.top);
        GetParameters(ADQ_PARAMETER_ID_CLOCK_SYSTEM, m_watchers.clock_system);
        break;

    case DigitizerMessageId::INITIALIZE_PARAMETERS:
        if (m_parameters.top->size() != 0 || m_parameters.clock_system->size() != 0)
        {
            m_read_queue.EmplaceWrite(DigitizerMessageId::INITIALIZE_WOULD_OVERWRITE);
            break;
        }
        /* FALLTHROUGH */
    case DigitizerMessageId::INITIALIZE_PARAMETERS_FORCE:
        InitializeParameters(ADQ_PARAMETER_ID_TOP, m_watchers.top);
        InitializeParameters(ADQ_PARAMETER_ID_CLOCK_SYSTEM, m_watchers.clock_system);
        break;

    case DigitizerMessageId::SET_WINDOW_TYPE:
        for (const auto &t : m_processing_threads)
            t->SetWindowType(message.window_type);
        break;

    default:
        throw DigitizerException(fmt::format("Unsupported action '{}'.", message.id));
    }
}

void Digitizer::HandleMessageInAcquisition(const struct DigitizerMessage &message)
{
    /* If we encounter an exception in the ACQUISITION state, have to abort and
       return to the IDLE state. We pass on the exception to the upper layers to
       create an error message. */
    switch (message.id)
    {
    case DigitizerMessageId::STOP_ACQUISITION:
        StopDataAcquisition();
        SetState(DigitizerState::IDLE);
        break;

    case DigitizerMessageId::DEFAULT_ACQUISITION:
        try
        {
            StopDataAcquisition();
            ConfigureDefaultAcquisition();
            StartDataAcquisition();
        }
        catch (const DigitizerException &e)
        {
            SetState(DigitizerState::IDLE);
            throw;
        }
        break;

    case DigitizerMessageId::SET_PARAMETERS:
        try
        {
            StopDataAcquisition();
            SetParameters(m_parameters.top, DigitizerMessageId::CLEAN_TOP_PARAMETERS);
            StartDataAcquisition();
        }
        catch (const DigitizerException &e)
        {
            SetState(DigitizerState::IDLE);
            throw;
        }
        break;

    case DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
        try
        {
            StopDataAcquisition();
            SetParameters(m_parameters.clock_system, DigitizerMessageId::CLEAN_CLOCK_SYSTEM_PARAMETERS);
            SetParameters(m_parameters.top, DigitizerMessageId::CLEAN_TOP_PARAMETERS);
            StartDataAcquisition();
        }
        catch (const DigitizerException &e)
        {
            SetState(DigitizerState::IDLE);
            throw;
        }
        break;

    case DigitizerMessageId::SET_WINDOW_TYPE:
        for (const auto &t : m_processing_threads)
            t->SetWindowType(message.window_type);
        break;

    default:
        throw DigitizerException(fmt::format("Unsupported action '{}'.", message.id));
    }
}

void Digitizer::HandleMessageInState(const struct DigitizerMessage &message)
{
    try
    {
        switch (m_state)
        {
        case DigitizerState::NOT_ENUMERATED:
            HandleMessageInNotEnumerated(message);
            break;

        case DigitizerState::ENUMERATION:
            HandleMessageInEnumeration(message);
            break;

        case DigitizerState::IDLE:
            HandleMessageInIdle(message);
            break;

        case DigitizerState::ACQUISITION:
            HandleMessageInAcquisition(message);
            break;
        }

        /* If the message was processed successfully, we send the 'all clear'. */
        m_read_queue.EmplaceWrite(DigitizerMessageId::CLEAR);
    }
    catch (const DigitizerException &e)
    {
        /* TODO: Propagate message in some other way? We just write it to stderr for now. */
        fprintf(stderr, "%s", fmt::format("ERROR: {}\n", e.what()).c_str());
        m_read_queue.EmplaceWrite(DigitizerMessageId::ERR);
    }
}

void Digitizer::ConfigureInternalReference()
{
#ifdef NO_ADQAPI
    throw DigitizerException("ConfigureInternalReference() not implemented.");
#else
    struct ADQClockSystemParameters clock_system;
    int result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CLOCK_SYSTEM,
                                          &clock_system);
    if (result != sizeof(clock_system))
        throw DigitizerException(fmt::format("Failed to initialize clock system parameters, result {}.", result));

    clock_system.clock_generator = ADQ_CLOCK_GENERATOR_INTERNAL_PLL;
    clock_system.reference_source = ADQ_CLOCK_REFERENCE_SOURCE_INTERNAL;

    result = ADQ_SetParameters(m_id.handle, m_id.index, &clock_system);
    if (result != sizeof(clock_system))
        throw DigitizerException(fmt::format("Failed to set clock system parameters, result {}.", result));
#endif
}

void Digitizer::ConfigureExternalReference()
{
#ifdef NO_ADQAPI
    throw DigitizerException("ConfigureExternalReference() not implemented.");
#else
    struct ADQClockSystemParameters clock_system;
    int result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CLOCK_SYSTEM,
                                          &clock_system);
    if (result != sizeof(clock_system))
        throw DigitizerException(fmt::format("Failed to initialize clock system parameters, result {}.", result));

    clock_system.clock_generator = ADQ_CLOCK_GENERATOR_INTERNAL_PLL;
    clock_system.reference_source = ADQ_CLOCK_REFERENCE_SOURCE_PORT_CLK;
    clock_system.reference_frequency = 10e6;
    clock_system.low_jitter_mode_enabled = 1;

    result = ADQ_SetParameters(m_id.handle, m_id.index, &clock_system);
    if (result != sizeof(clock_system))
        throw DigitizerException(fmt::format("Failed to set clock system parameters, result {}.", result));
#endif
}

void Digitizer::ConfigureExternalClock()
{
#ifdef NO_ADQAPI
    throw DigitizerException("ConfigureExternalClock() not implemented.");
#else
    struct ADQClockSystemParameters clock_system;
    int result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CLOCK_SYSTEM,
                                          &clock_system);
    if (result != sizeof(clock_system))
        throw DigitizerException(fmt::format("Failed to initialize clock system parameters, result {}.", result));

    clock_system.clock_generator = ADQ_CLOCK_GENERATOR_EXTERNAL_CLOCK;

    result = ADQ_SetParameters(m_id.handle, m_id.index, &clock_system);
    if (result != sizeof(clock_system))
        throw DigitizerException(fmt::format("Failed to set clock system parameters, result {}.", result));
#endif
}

void Digitizer::ConfigureDefaultAcquisition()
{
#ifdef NO_ADQAPI
    throw DigitizerException("ConfigureDefaultAcquisition() not implemented.");
#else
    struct ADQConstantParameters constant;
    int result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CONSTANT, &constant);
    if (result != sizeof(constant))
        throw DigitizerException(fmt::format("Failed to get constant parameters, result {}.", result));

    struct ADQEventSourcePeriodicParameters periodic;
    result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_EVENT_SOURCE_PERIODIC, &periodic);
    if (result != sizeof(periodic))
        throw DigitizerException(fmt::format("Failed to get periodic parameters, result {}.", result));

    struct ADQDataAcquisitionParameters acquisition;
    result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_ACQUISITION, &acquisition);
    if (result != sizeof(acquisition))
        throw DigitizerException(fmt::format("Failed to get acquisition parameters, result {}.", result));

    struct ADQDataTransferParameters transfer;
    result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_TRANSFER, &transfer);
    if (result != sizeof(transfer))
        throw DigitizerException(fmt::format("Failed to get transfer parameters, result {}.", result));

    /* The default acquisition parameters is an infinite stream of records from
       each available channel triggered by the periodic event generator. */
    for (int i = 0; i < constant.nof_channels; ++i)
    {
        acquisition.channel[i].nof_records = ADQ_INFINITE_NOF_RECORDS;
        acquisition.channel[i].record_length = 64 * 1024;
        acquisition.channel[i].horizontal_offset = 0;
        acquisition.channel[i].trigger_source = ADQ_EVENT_SOURCE_PERIODIC;
        acquisition.channel[i].trigger_edge = ADQ_EDGE_RISING;
        acquisition.channel[i].trigger_blocking_source = ADQ_FUNCTION_INVALID;

        transfer.channel[i].metadata_enabled = 1;
        transfer.channel[i].nof_buffers = ADQ_MAX_NOF_BUFFERS;
        transfer.channel[i].record_size = 2 * acquisition.channel[i].record_length;
        transfer.channel[i].record_buffer_size = transfer.channel[i].record_size;
        transfer.channel[i].metadata_buffer_size = sizeof(ADQGen4RecordHeader);
    }

    /* Default trigger frequency of 5 Hz. */
    periodic.frequency = 5.0;

    result = ADQ_SetParameters(m_id.handle, m_id.index, &periodic);
    if (result != sizeof(periodic))
        throw DigitizerException(fmt::format("Failed to set periodic parameters, result {}.", result));

    result = ADQ_SetParameters(m_id.handle, m_id.index, &acquisition);
    if (result != sizeof(acquisition))
        throw DigitizerException(fmt::format("Failed to set acquisition parameters, result {}.", result));

    result = ADQ_SetParameters(m_id.handle, m_id.index, &transfer);
    if (result != sizeof(transfer))
        throw DigitizerException(fmt::format("Failed to set transfer parameters, result {}.", result));
#endif
}

void Digitizer::SetParameters(const std::shared_ptr<std::string> &str, DigitizerMessageId clean_id)
{
    m_read_queue.EmplaceWrite(DigitizerMessageId::CONFIGURATION);
    int result = ADQ_SetParametersString(m_id.handle, m_id.index, str->c_str(), str->size());
    if (result > 0)
        m_read_queue.Write(clean_id);
    else
        throw DigitizerException(fmt::format("ADQ_SetParametersString() failed, result {}.", result));
}

void Digitizer::InitializeParameters(enum ADQParameterId id, const std::unique_ptr<FileWatcher> &watcher)
{
    auto parameters = std::make_shared<std::string>();
    parameters->resize(64 * 1024);
    int result = ADQ_InitializeParametersString(m_id.handle, m_id.index, id, parameters->data(),
                                                parameters->size(), 1);
    if (result > 0)
    {
        /* The return value includes the null terminator so we have to remove it from the string. */
        parameters->resize(result - 1);
        watcher->PushMessage({FileWatcherMessageId::UPDATE_FILE, parameters});
    }
    else
    {
        throw DigitizerException(
            fmt::format("ADQ_InitializeParametersString failed, result {}.", result));
    }
}

void Digitizer::GetParameters(enum ADQParameterId id, const std::unique_ptr<FileWatcher> &watcher)
{
    /* Write directly to a std::string. */
    auto parameters = std::make_shared<std::string>();
    parameters->resize(64 * 1024);
    int result = ADQ_GetParametersString(m_id.handle, m_id.index, id, parameters->data(),
                                         parameters->size(), 1);
    if (result > 0)
    {
        /* The return value includes the null terminator so we have to remove it from the string. */
        parameters->resize(result - 1);
        watcher->PushMessage({FileWatcherMessageId::UPDATE_FILE, parameters});
    }
    else
    {
        throw DigitizerException(fmt::format("ADQ_GetParametersString failed, result {}.", result));
    }
}

void Digitizer::InitializeFileWatchers(const struct ADQConstantParameters &constant)
{
    auto MakeLowercase = [](unsigned char c){ return static_cast<char>(std::tolower(c)); };

    std::string filename = fmt::format("./parameters_top_{}.json", constant.serial_number);
    std::transform(filename.begin(), filename.end(), filename.begin(), MakeLowercase);
    m_watchers.top = std::make_unique<FileWatcher>(filename);

    filename = fmt::format("./parameters_clock_system_{}.json", constant.serial_number);
    std::transform(filename.begin(), filename.end(), filename.begin(), MakeLowercase);
    m_watchers.clock_system = std::make_unique<FileWatcher>(filename);

    m_parameters.top = std::make_shared<std::string>("");
    m_parameters.clock_system = std::make_shared<std::string>("");

    m_watchers.top->Start();
    m_watchers.clock_system->Start();
}
