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
#include "log.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <ctime>

/* A private exception type that we use for simplified control flow within this class. */
class DigitizerException : public std::runtime_error
{
public:
    DigitizerException(const std::string &str) : std::runtime_error(str) {}
};

Digitizer::Digitizer(void *handle, int init_index, int index,
                     const std::filesystem::path &configuration_directory,
                     std::shared_ptr<EmbeddedPythonThread> python)
    : m_state(DigitizerState::NOT_INITIALIZED)
    , m_id{handle, init_index, index}
    , m_configuration_directory{configuration_directory}
    , m_python{python}
    , m_constant{}
    , m_watchers{}
    , m_parameters{}
    , m_processing_threads{}
    , m_no_activity_threshold_ms(DEFAULT_ACTIVITY_THRESHOLD_MS)
    , m_notified_no_activity(false)
    , m_sensor_records{}
    , m_sensor_record_queue()
    , m_sensor_last_record_timestamp(std::chrono::high_resolution_clock::now())
    , m_last_status_timestamp(std::chrono::high_resolution_clock::now())
{
    m_sensor_record_queue.Start();
}

Digitizer::~Digitizer()
{
    Stop();
}

/* Interface to the digitizer's data processing threads, one per channel. */
int Digitizer::WaitForProcessedRecord(int channel, std::shared_ptr<ProcessedRecord> &record)
{
    if (channel < 0 || channel >= static_cast<int>(m_processing_threads.size()))
        return SCAPE_EINVAL;

    return m_processing_threads[channel]->WaitForBuffer(record, 0);
}

int Digitizer::WaitForSensorRecords(std::shared_ptr<std::vector<SensorRecord>> &records)
{
    return m_sensor_record_queue.Read(records, 0);
}

void Digitizer::MainLoop()
{
    try
    {
        MainInitialization();
        SetState(DigitizerState::IDLE);
    }
    catch (const DigitizerException &e)
    {
        SetState(DigitizerState::NOT_INITIALIZED);
        SignalError(e.what());
        return;
    }

    m_thread_exit_code = SCAPE_EOK;
    for (;;)
    {
        try
        {
            /* Can't go faster than 10 Hz unless we change the wait but that should suffice. */
            ProcessMessages();
            ProcessWatcherMessages();
            UpdateSystemManagerObjects();
            CheckActivity();
            CheckStatus();
        }
        catch (const DigitizerException &e)
        {
            SignalError(e.what());
        }

        /* We implement the sleep using the stop event to be able to immediately
           react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            break;
    }

    StopDataAcquisition();
}

void Digitizer::MainInitialization()
{
    /* When the main loop is started, we assume ownership of the digitizer with
       the identifier we were given when this object was constructed. We begin
       by initializing the digitizer, completing its initial setup procedure. */
    SetState(DigitizerState::INITIALIZATION);
    Log::log->info("Starting initialization of digitizer {}.", m_id.index);

    /* Performing this operation in a thread safe manner requires that
       ADQControlUnit_OpenDeviceInterface() has been called (and returned
       successfully) in a single thread process. */
    int result = ADQControlUnit_SetupDevice(m_id.handle, m_id.init_index);
    if (result != 1)
    {
        m_thread_exit_code = SCAPE_EINTERNAL;
        ThrowDigitizerException("Failed to initialize the digitizer, status {}.", result);
    }

    /* Read the digitizer's constant parameters. */
    result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CONSTANT, &m_constant);
    if (result != sizeof(m_constant))
    {
        m_thread_exit_code = SCAPE_EINTERNAL;
        ThrowDigitizerException("Failed to get the constant parameters, status {}.", result);
    }

    Log::log->info("Digitizer {} is {}.", m_id.index, m_constant.serial_number);

    /* Instantiate one data processing thread for each transfer channel. */
    for (int ch = 0; ch < m_constant.nof_transfer_channels; ++ch)
    {
        const auto label = fmt::format("{} {} {}", m_constant.product_name,
                                       m_constant.serial_number, m_constant.channel[ch].label);
        m_processing_threads.emplace_back(
            std::make_unique<DataProcessing>(m_id.handle, m_id.index, ch, std::move(label), m_constant)
        );
    }

    /* Initialize the file watchers now that we know the identifying properties
       of the digitizer. */
    InitializeFileWatchers();

    /* Send the post-initialization message, triggering the GUI initialization. */
    _EmplaceMessage(DigitizerMessageId::INITIALIZED, m_constant);

    /* Initialize the objects associated with the system manager, like sensors
       and boot status. */
    InitializeSystemManagerObjects();
}

void Digitizer::SignalError(const std::string &message)
{
    Log::log->error(message);
    _EmplaceMessage(DigitizerMessageId::EVENT_ERROR);
}

void Digitizer::ProcessMessages()
{
    DigitizerMessage message;
    while (SCAPE_EOK == _WaitForMessage(message, 100))
        HandleMessageInState(message);
}

void Digitizer::ProcessWatcherMessages()
{
    ProcessWatcherMessages(m_watchers.top, m_parameters.top,
                           DigitizerMessageId::CHANGED_TOP_PARAMETERS, ADQ_PARAMETER_ID_TOP);
    ProcessWatcherMessages(m_watchers.clock_system, m_parameters.clock_system,
                           DigitizerMessageId::CHANGED_CLOCK_SYSTEM_PARAMETERS,
                           ADQ_PARAMETER_ID_CLOCK_SYSTEM);
}

void Digitizer::ProcessWatcherMessages(const std::unique_ptr<FileWatcher> &watcher,
                                       std::shared_ptr<std::string> &str,
                                       DigitizerMessageId dirty_id, ADQParameterId parameter_id)
{
    FileWatcherMessage message;
    while (SCAPE_EOK == watcher->WaitForMessage(message, 0))
    {
        switch (message.id)
        {
        case FileWatcherMessageId::FILE_CREATED:
        case FileWatcherMessageId::FILE_UPDATED:
            str = message.contents;
            _EmplaceMessage(dirty_id);
            break;

        case FileWatcherMessageId::FILE_DOES_NOT_EXIST:
            InitializeParameters(parameter_id, watcher);
            ConfigureDefaultAcquisition();
            break;

        case FileWatcherMessageId::FILE_DELETED:
            /* TODO: What to do here? */
            break;

        case FileWatcherMessageId::UPDATE_FILE:
        default:
            break;
        }
    }
}

void Digitizer::InitializeSystemManagerBootStatus()
{
    std::vector<BootEntry> boot_entries;

    uint32_t nof_entries;
    int result = ADQ_SmTransactionImmediate(m_id.handle, m_id.index,
                                            SystemManagerCommand::BOOT_GET_NOF_ENTRIES, NULL, 0,
                                            &nof_entries, sizeof(nof_entries));
    if (result != ADQ_EOK)
        ThrowDigitizerException("BOOT_GET_NOF_ENTRIES failed, status {}.", result);

    std::vector<uint32_t> boot_map(nof_entries + 1); /* +1 for EOM */
    result = ADQ_SmTransactionImmediate(m_id.handle, m_id.index,
                                        SystemManagerCommand::BOOT_GET_MAP,
                                        NULL, 0, boot_map.data(),
                                        sizeof(boot_map[0]) * boot_map.size());
    if (result != ADQ_EOK)
        ThrowDigitizerException("BOOT_GET_MAP failed, status {}.", result);

    boot_map.resize(nof_entries);
    for (auto &boot_id : boot_map)
    {
        struct SystemManagerBootInformation boot_information;
        result = ADQ_SmTransactionImmediate(m_id.handle, m_id.index,
                                            SystemManagerCommand::BOOT_GET_INFO, &boot_id,
                                            sizeof(boot_id), &boot_information,
                                            sizeof(boot_information));
        if (result != ADQ_EOK)
            ThrowDigitizerException("BOOT_GET_INFO, status {}.", result);

        boot_entries.emplace_back(boot_id, boot_information.label, boot_information.status);

        /* TODO: Error code to text description? */
        if (boot_information.status != 0)
            boot_entries.back().note = "I'm a descriptive error message!";
    }

    int32_t state;
    result = ADQ_SmTransactionImmediate(m_id.handle, m_id.index, SystemManagerCommand::GET_STATE,
                                        NULL, 0, &state, sizeof(state));
    if (result != ADQ_EOK)
        ThrowDigitizerException("GET_STATE, status {}.", result);

    struct SystemManagerStateInformation state_information;
    result = ADQ_SmTransactionImmediate(m_id.handle, m_id.index,
                                        SystemManagerCommand::GET_STATE_INFO, &state, sizeof(state),
                                        &state_information, sizeof(state_information));
    if (result != ADQ_EOK)
        ThrowDigitizerException("GET_STATE_INFO, status {}.", result);

    _EmplaceMessage(
        DigitizerMessageId::BOOT_STATUS, state, state_information.label, std::move(boot_entries));
}

void Digitizer::InitializeSystemManagerSensors()
{
    /* Populate a sensor tree w/ the static information and set up the sensor readings. */
    SensorTree sensor_tree;
    m_sensor_records.clear();

    uint32_t nof_sensors;
    int result = ADQ_SmTransactionImmediate(m_id.handle, m_id.index,
                                            SystemManagerCommand::SENSOR_GET_NOF_SENSORS, NULL,
                                            0, &nof_sensors, sizeof(nof_sensors));
    if (result != ADQ_EOK)
        ThrowDigitizerException("SENSOR_GET_NOF_SENSORS failed, status {}.", result);

    std::vector<uint32_t> sensor_map(nof_sensors + 1); /* +1 for EOM */
    result = ADQ_SmTransactionImmediate(m_id.handle, m_id.index,
                                        SystemManagerCommand::SENSOR_GET_MAP,
                                        NULL, 0, sensor_map.data(),
                                        sizeof(sensor_map[0]) * sensor_map.size());
    if (result != ADQ_EOK)
        ThrowDigitizerException("SENSOR_GET_MAP failed, status {}.", result);

    /* Resize to remove EOM, then walk through the sensor map, retrieving the
       static information about each sensor. While we have a flat sequence of
       numbers, they appear clustered with the other sensors in their sensor
       group. Thus, we can also discover the groups by detecting the first time
       we read a sensor from a different group. */

    sensor_map.resize(nof_sensors);
    uint32_t group_id = 0;
    for (auto &sensor_id : sensor_map)
    {
        struct SystemManagerSensorInformation information;
        result = ADQ_SmTransactionImmediate(m_id.handle, m_id.index,
                                            SystemManagerCommand::SENSOR_GET_INFO, &sensor_id,
                                            sizeof(sensor_id), &information, sizeof(information));
        if (result != ADQ_EOK)
            ThrowDigitizerException("SENSOR_GET_INFO, status {}.", result);

        /* If we encounter a new group, read its information and create new entry. */
        if (information.group_id != group_id)
        {
            struct SystemManagerSensorGroupInformation group_information;
            result = ADQ_SmTransactionImmediate(m_id.handle, m_id.index,
                                                SystemManagerCommand::SENSOR_GET_GROUP_INFO,
                                                &information.group_id, sizeof(information.group_id),
                                                &group_information, sizeof(group_information));
            if (result != ADQ_EOK)
                ThrowDigitizerException("SENSOR_GET_GROUP_INFO, status {}.", result);

            sensor_tree.emplace_back(group_information.id, group_information.label);
            group_id = group_information.id;
        }

        sensor_tree.back().sensors.emplace_back(information.id, information.group_id,
                                                information.label);
        m_sensor_records.emplace_back(information.id, information.group_id, information.unit);
    }

    _EmplaceMessage(DigitizerMessageId::SENSOR_TREE, std::move(sensor_tree));
}

void Digitizer::InitializeSystemManagerObjects()
{
    InitializeSystemManagerBootStatus();
    InitializeSystemManagerSensors();
}

void Digitizer::UpdateSystemManagerObjects()
{
    const auto now = std::chrono::high_resolution_clock::now();
    if ((now - m_sensor_last_record_timestamp).count() / 1e6 >= SENSOR_SAMPLING_PERIOD_MS)
    {
        /* Update the timestamp. */
        m_sensor_last_record_timestamp = now;

        for (auto &sensor : m_sensor_records)
        {
            sensor.step = SENSOR_SAMPLING_PERIOD_MS / 1e3;
            struct ArgSensorGetValue arg
            {
                static_cast<uint32_t>(sensor.id), SENSOR_FORMAT_FLOAT
            };

            float value = 0.0f;
            sensor.status = ADQ_SmTransaction(m_id.handle, m_id.index,
                                              SystemManagerCommand::SENSOR_GET_VALUE,
                                              &arg, sizeof(arg), &value, sizeof(value));

            if (sensor.status == ADQ_EINTERNAL)
            {
                ThrowDigitizerException("Internal error when reading sensor {}.", sensor.id);
            }
            else if (sensor.status != 0)
            {
                /* FIXME: Error code to text description? */
                sensor.note = "I'm a descriptive error message!";
                continue;
            }

            /* TODO: Maybe a configurable value? */
            /* Storing 10 hours of sensor data (1 Hz sampling rate) will mean
               that a single sensor consumes at most about 0.5 MB of memory. I
               think that's reasonable if you let it run that long. */
            if (sensor.y.size() > 60 * 60 * 10)
            {
                sensor.y.erase(sensor.y.begin());
                sensor.x.erase(sensor.x.begin());
            }

            sensor.y.emplace_back(value);
            double last = (sensor.x.size() > 0) ? sensor.x.back()
                                                : static_cast<double>(std::time(NULL));
            sensor.x.emplace_back(last + sensor.step);
        }

        /* Make a copy of the current sensor records and push a shared pointer
           to the outbound queue. */
        m_sensor_record_queue.Write(std::make_shared<std::vector<SensorRecord>>(m_sensor_records));
    }
}

void Digitizer::CheckActivity()
{
    /* Loop through the processing threads, querying the time since the last
       record was passed along. If we haven't seen any data for a while, we send
       a 'no activity' message to the UI and also take the opportunity to update
       the threshold based on the current value. This regulates itself to a slow
       trigger rate. */
    int milliseconds_max = -1;
    for (auto &thread : m_processing_threads)
    {
        int milliseconds;
        if (SCAPE_EOK != thread->GetTimeSinceLastActivity(milliseconds))
            continue;
        milliseconds_max = std::max(milliseconds_max, milliseconds);
    }

    if (milliseconds_max > m_no_activity_threshold_ms + ACTIVITY_HYSTERESIS_MS)
    {
        _EmplaceMessage(DigitizerMessageId::EVENT_NO_ACTIVITY);
        m_no_activity_threshold_ms = milliseconds_max;
        m_notified_no_activity = true;
    }
    else if (m_notified_no_activity && milliseconds_max + ACTIVITY_HYSTERESIS_MS < m_no_activity_threshold_ms)
    {
        _EmplaceMessage(DigitizerMessageId::EVENT_CLEAR);
        m_notified_no_activity = false;
    }
}

void Digitizer::CheckStatus()
{
    /* Only provide status updates when we're acquiring data. */
    if (m_state != DigitizerState::ACQUISITION)
        return;

    /* Interrogate the digitizer's various statuses available through
       GetStatus(). We emit a relevant message if we find an unexpected status
       value, e.g. overflow. */
    const auto now = std::chrono::high_resolution_clock::now();
    if ((now - m_last_status_timestamp).count() / 1e6 >= STATUS_SAMPLING_PERIOD_MS)
    {
        /* Update the timestamp. */
        m_last_status_timestamp = now;

        /* Skip DRAM status if FWATD. */
        if (m_constant.firmware.type != ADQ_FIRMWARE_TYPE_FWATD)
        {
            ADQDramStatus dram_status;
            if (sizeof(dram_status) ==
                ADQ_GetStatus(m_id.handle, m_id.index, ADQ_STATUS_ID_DRAM, &dram_status))
            {
                double fill_percent = static_cast<double>(dram_status.fill) /
                                      static_cast<double>(m_constant.dram_size);
                _EmplaceMessage(DigitizerMessageId::DRAM_FILL, fill_percent);
            }
        }

        ADQOverflowStatus overflow_status;
        if (sizeof(overflow_status) ==
            ADQ_GetStatus(m_id.handle, m_id.index, ADQ_STATUS_ID_OVERFLOW, &overflow_status))
        {
            if (overflow_status.overflow)
                _EmplaceMessage(DigitizerMessageId::EVENT_OVERFLOW);
        }
    }
}

void Digitizer::StartDataAcquisition()
{
    try
    {
        ADQAnalogFrontendParameters afe;
        int result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_ANALOG_FRONTEND, &afe);
        if (result != sizeof(afe))
            ThrowDigitizerException("ADQ_GetParameters failed, result {}.", result);

        ADQDataTransferParameters transfer;
        result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_TRANSFER, &transfer);
        if (result != sizeof(transfer))
            ThrowDigitizerException("ADQ_GetParameters failed, result {}.", result);

        /* TODO: Temporary workaround until the `time_unit` precision is fixed. */
        ADQClockSystemParameters clock_system;
        result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CLOCK_SYSTEM, &clock_system);
        if (result != sizeof(clock_system))
            ThrowDigitizerException("ADQ_GetParameters failed, result {}.", result);

        for (size_t i = 0; i < m_processing_threads.size(); ++i)
        {
            /* We only start processing threads for enabled channels. */
            if (transfer.channel[i].nof_buffers == 0)
                continue;

            m_processing_threads[i]->EmplaceMessage(
                DataProcessingMessageId::SET_AFE_PARAMETERS, afe.channel[i]);

            if (SCAPE_EOK != m_processing_threads[i]->Start())
            {
                ThrowDigitizerException(
                    "Failed to start data processing thread for channel {}.",
                    m_constant.channel[i].label);
            }
        }

        result = ADQ_StartDataAcquisition(m_id.handle, m_id.index);
        if (result != ADQ_EOK)
            ThrowDigitizerException("ADQ_GetParameters failed, result {}.", result);

        /* Reset the activity monitoring and enter the acquisition state. */
        m_no_activity_threshold_ms = DEFAULT_ACTIVITY_THRESHOLD_MS;
        m_notified_no_activity = false;
        SetState(DigitizerState::ACQUISITION);
    }
    catch (const DigitizerException &)
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
    SetState(DigitizerState::IDLE);
}

void Digitizer::SetState(DigitizerState state)
{
    m_state = state;
    _EmplaceMessage(DigitizerMessageId::STATE, state);
}

void Digitizer::HandleMessageInNotInitialized(const DigitizerMessage &message)
{
    ThrowDigitizerException("Unsupported action in NOT INITIALIZED '{}'.", message.id);
}

void Digitizer::HandleMessageInInitialization(const DigitizerMessage &message)
{
    ThrowDigitizerException("Unsupported action in INITIALIZATION '{}'.", message.id);
}

void Digitizer::HandleMessageInIdle(const DigitizerMessage &message)
{
    /* Any exception propagates to the upper layers to create an error message. */
    switch (message.id)
    {
    case DigitizerMessageId::START_ACQUISITION:
        StartDataAcquisition();
        break;

    case DigitizerMessageId::SET_TOP_PARAMETERS:
        SetParameters(m_parameters.top);
        break;

    case DigitizerMessageId::SET_INTERNAL_REFERENCE:
        ConfigureInternalReference();
        SetParameters(m_parameters.top);
        EmitConstantParameters();
        break;

    case DigitizerMessageId::SET_EXTERNAL_REFERENCE:
        ConfigureExternalReference();
        SetParameters(m_parameters.top);
        EmitConstantParameters();
        break;

    case DigitizerMessageId::SET_EXTERNAL_CLOCK:
        ConfigureExternalClock();
        SetParameters(m_parameters.top);
        EmitConstantParameters();
        break;

    case DigitizerMessageId::DEFAULT_ACQUISITION:
        ConfigureDefaultAcquisition();
        break;

    case DigitizerMessageId::SCALE_RECORD_LENGTH_DOUBLE:
        ScaleRecordLength(2.0);
        break;

    case DigitizerMessageId::SCALE_RECORD_LENGTH_HALF:
        ScaleRecordLength(0.5);
        break;

    case DigitizerMessageId::FORCE_ACQUISITION:
        ForceAcquisition();
        break;

    case DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
        SetParameters(m_parameters.clock_system);
        SetParameters(m_parameters.top);
        EmitConstantParameters();
        break;

    case DigitizerMessageId::GET_TOP_PARAMETERS:
        GetParameters(ADQ_PARAMETER_ID_TOP, m_parameters.top, m_watchers.top);
        GetParameters(ADQ_PARAMETER_ID_CLOCK_SYSTEM, m_parameters.clock_system,
                      m_watchers.clock_system);
        break;

    case DigitizerMessageId::INITIALIZE_PARAMETERS:
        if (m_parameters.top->size() != 0 || m_parameters.clock_system->size() != 0)
        {
            _EmplaceMessage(DigitizerMessageId::INITIALIZE_WOULD_OVERWRITE);
            break;
        }
        /* FALLTHROUGH */
    case DigitizerMessageId::INITIALIZE_PARAMETERS_FORCE:
        InitializeParameters(ADQ_PARAMETER_ID_TOP, m_watchers.top);
        InitializeParameters(ADQ_PARAMETER_ID_CLOCK_SYSTEM, m_watchers.clock_system);
        break;

    case DigitizerMessageId::SET_PROCESSING_PARAMETERS:
        for (const auto &t : m_processing_threads)
        {
            t->EmplaceMessage(DataProcessingMessageId::SET_PROCESSING_PARAMETERS,
                              std::move(message.processing_parameters));
        }
        break;

    case DigitizerMessageId::CLEAR_PROCESSING_MEMORY:
        for (const auto &t : m_processing_threads)
            t->EmplaceMessage(DataProcessingMessageId::CLEAR_PROCESSING_MEMORY);
        break;

    case DigitizerMessageId::GET_TOP_PARAMETERS_FILENAME:
        _EmplaceMessage(
            DigitizerMessageId::PARAMETERS_FILENAME, m_watchers.top->GetPath().string());
        break;

    case DigitizerMessageId::GET_CLOCK_SYSTEM_PARAMETERS_FILENAME:
        _EmplaceMessage(
            DigitizerMessageId::PARAMETERS_FILENAME, m_watchers.clock_system->GetPath().string());
        break;

    case DigitizerMessageId::CALL_PYTHON:
        CallPython(message.str);
        break;

    default:
        ThrowDigitizerException("Unsupported action in IDLE '{}'.", message.id);
    }
}

void Digitizer::HandleMessageInAcquisition(const DigitizerMessage &message)
{
    /* Any exception propagates to the upper layers to create an error message. */
    switch (message.id)
    {
    case DigitizerMessageId::STOP_ACQUISITION:
        StopDataAcquisition();
        break;

    case DigitizerMessageId::DEFAULT_ACQUISITION:
        StopDataAcquisition();
        ConfigureDefaultAcquisition();
        StartDataAcquisition();
        break;

    case DigitizerMessageId::SCALE_RECORD_LENGTH_DOUBLE:
        StopDataAcquisition();
        ScaleRecordLength(2.0);
        StartDataAcquisition();
        break;

    case DigitizerMessageId::SCALE_RECORD_LENGTH_HALF:
        StopDataAcquisition();
        ScaleRecordLength(0.5);
        StartDataAcquisition();
        break;

    case DigitizerMessageId::FORCE_ACQUISITION:
        StopDataAcquisition();
        ForceAcquisition();
        break;

    case DigitizerMessageId::SET_TOP_PARAMETERS:
        StopDataAcquisition();
        SetParameters(m_parameters.top);
        StartDataAcquisition();
        break;

    case DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
        StopDataAcquisition();
        SetParameters(m_parameters.clock_system);
        SetParameters(m_parameters.top);
        EmitConstantParameters();
        StartDataAcquisition();
        break;

    case DigitizerMessageId::SET_INTERNAL_REFERENCE:
        StopDataAcquisition();
        ConfigureInternalReference();
        SetParameters(m_parameters.top);
        EmitConstantParameters();
        StartDataAcquisition();
        break;

    case DigitizerMessageId::SET_EXTERNAL_REFERENCE:
        StopDataAcquisition();
        ConfigureExternalReference();
        SetParameters(m_parameters.top);
        EmitConstantParameters();
        StartDataAcquisition();
        break;

    case DigitizerMessageId::SET_EXTERNAL_CLOCK:
        StopDataAcquisition();
        ConfigureExternalClock();
        SetParameters(m_parameters.top);
        EmitConstantParameters();
        StartDataAcquisition();
        break;

    case DigitizerMessageId::SET_PROCESSING_PARAMETERS:
        for (const auto &t : m_processing_threads)
        {
            t->EmplaceMessage(DataProcessingMessageId::SET_PROCESSING_PARAMETERS,
                              std::move(message.processing_parameters));
        }
        break;

    case DigitizerMessageId::CLEAR_PROCESSING_MEMORY:
        for (const auto &t : m_processing_threads)
            t->EmplaceMessage(DataProcessingMessageId::CLEAR_PROCESSING_MEMORY);
        break;

    case DigitizerMessageId::GET_TOP_PARAMETERS_FILENAME:
        _EmplaceMessage(
            DigitizerMessageId::PARAMETERS_FILENAME, m_watchers.top->GetPath().string());
        break;

    case DigitizerMessageId::GET_CLOCK_SYSTEM_PARAMETERS_FILENAME:
        _EmplaceMessage(
            DigitizerMessageId::PARAMETERS_FILENAME, m_watchers.clock_system->GetPath().string());
        break;

    case DigitizerMessageId::CALL_PYTHON:
        StopDataAcquisition();
        CallPython(message.str);
        StartDataAcquisition();
        break;

    default:
        ThrowDigitizerException("Unsupported action in ACQUISITION '{}'.", message.id);
    }
}

void Digitizer::HandleMessageInState(const DigitizerMessage &message)
{
    try
    {
        switch (m_state)
        {
        case DigitizerState::NOT_INITIALIZED:
            HandleMessageInNotInitialized(message);
            break;

        case DigitizerState::INITIALIZATION:
            HandleMessageInInitialization(message);
            break;

        case DigitizerState::IDLE:
            HandleMessageInIdle(message);
            break;

        case DigitizerState::ACQUISITION:
            HandleMessageInAcquisition(message);
            break;
        }

        /* If the message was processed successfully, we reply back with the
           same message with `result` set to `SCAKE_EOK` and send the 'all
           clear' event. */
        Log::log->trace(FormatLog("Processed message {}.", message.id));
        auto response = message;
        response.result = SCAPE_EOK;
        _EmplaceMessage(std::move(response));
        _EmplaceMessage(DigitizerMessageId::EVENT_CLEAR);
    }
    catch (const DigitizerException &)
    {
        /* On error we reply back with the same message with `result` set to an
           error code then rethrow the exception to trigger the "catch all"
           error handling from the upper layers. */
        auto response = message;
        response.result = SCAPE_EINTERNAL;
        _EmplaceMessage(std::move(response));
        throw;
    }
}

void Digitizer::ConfigureInternalReference()
{
#ifdef MOCK_ADQAPI
    ThrowDigitizerException("ConfigureInternalReference() not implemented.");
#else
    ADQClockSystemParameters clock_system;
    int result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CLOCK_SYSTEM,
                                          &clock_system);
    if (result != sizeof(clock_system))
        ThrowDigitizerException("Failed to initialize clock system parameters, result {}.", result);

    clock_system.clock_generator = ADQ_CLOCK_GENERATOR_INTERNAL_PLL;
    clock_system.reference_source = ADQ_REFERENCE_CLOCK_SOURCE_INTERNAL;

    result = ADQ_SetParameters(m_id.handle, m_id.index, &clock_system);
    if (result != sizeof(clock_system))
        ThrowDigitizerException("Failed to set clock system parameters, result {}.", result);
#endif
}

void Digitizer::ConfigureExternalReference()
{
#ifdef MOCK_ADQAPI
    ThrowDigitizerException("ConfigureExternalReference() not implemented.");
#else
    ADQClockSystemParameters clock_system;
    int result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CLOCK_SYSTEM,
                                          &clock_system);
    if (result != sizeof(clock_system))
        ThrowDigitizerException("Failed to initialize clock system parameters, result {}.", result);

    clock_system.clock_generator = ADQ_CLOCK_GENERATOR_INTERNAL_PLL;
    clock_system.reference_source = ADQ_REFERENCE_CLOCK_SOURCE_PORT_CLK;
    clock_system.reference_frequency = 10e6;
    clock_system.low_jitter_mode_enabled = 1;

    result = ADQ_SetParameters(m_id.handle, m_id.index, &clock_system);
    if (result != sizeof(clock_system))
        ThrowDigitizerException("Failed to set clock system parameters, result {}.", result);
#endif
}

void Digitizer::ConfigureExternalClock()
{
#ifdef MOCK_ADQAPI
    ThrowDigitizerException("ConfigureExternalClock() not implemented.");
#else
    ADQClockSystemParameters clock_system;
    int result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CLOCK_SYSTEM,
                                          &clock_system);
    if (result != sizeof(clock_system))
        ThrowDigitizerException("Failed to initialize clock system parameters, result {}.", result);

    clock_system.clock_generator = ADQ_CLOCK_GENERATOR_EXTERNAL_CLOCK;

    result = ADQ_SetParameters(m_id.handle, m_id.index, &clock_system);
    if (result != sizeof(clock_system))
        ThrowDigitizerException("Failed to set clock system parameters, result {}.", result);
#endif
}

void Digitizer::ConfigureDefaultAcquisition()
{
#ifndef MOCK_ADQAPI
    ADQEventSourcePeriodicParameters periodic;
    int result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_EVENT_SOURCE_PERIODIC, &periodic);
    if (result != sizeof(periodic))
        ThrowDigitizerException("Failed to get periodic parameters, result {}.", result);

    ADQDataAcquisitionParameters acquisition;
    result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_ACQUISITION, &acquisition);
    if (result != sizeof(acquisition))
        ThrowDigitizerException("Failed to get acquisition parameters, result {}.", result);

    ADQDataTransferParameters transfer;
    result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_TRANSFER, &transfer);
    if (result != sizeof(transfer))
        ThrowDigitizerException("Failed to get transfer parameters, result {}.", result);

    ADQDataReadoutParameters readout;
    result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_READOUT, &readout);
    if (result != sizeof(readout))
        ThrowDigitizerException("Failed to get readout parameters, result {}.", result);

    /* The default acquisition parameters is an infinite stream of records from
       each available acquisition channel triggered by the periodic event generator. */
    for (int ch = 0; ch < m_constant.nof_acquisition_channels; ++ch)
    {
        acquisition.channel[ch].nof_records = ADQ_INFINITE_NOF_RECORDS;
        acquisition.channel[ch].record_length = 32 * 1024;
        acquisition.channel[ch].horizontal_offset = 0;
        acquisition.channel[ch].trigger_source = ADQ_EVENT_SOURCE_PERIODIC;
        acquisition.channel[ch].trigger_edge = ADQ_EDGE_RISING;
        acquisition.channel[ch].trigger_blocking_source = ADQ_FUNCTION_INVALID;

        transfer.channel[ch].nof_buffers = 8;
        transfer.channel[ch].metadata_enabled = 1;
        transfer.channel[ch].metadata_buffer_size = sizeof(ADQGen4RecordHeader);
        transfer.channel[ch].record_buffer_size = 32 * m_constant.record_buffer_size_step;
        transfer.channel[ch].dynamic_record_length_enabled = 1;

        readout.channel[ch].nof_record_buffers_max = ADQ_INFINITE_NOF_RECORDS;
        readout.channel[ch].record_buffer_size_max = ADQ_INFINITE_RECORD_LENGTH;
    }

    /* Default trigger frequency of 15 Hz. */
    periodic.frequency = 15.0;

    result = ADQ_SetParameters(m_id.handle, m_id.index, &periodic);
    if (result != sizeof(periodic))
        ThrowDigitizerException("Failed to set periodic parameters, result {}.", result);

    result = ADQ_SetParameters(m_id.handle, m_id.index, &acquisition);
    if (result != sizeof(acquisition))
        ThrowDigitizerException("Failed to set acquisition parameters, result {}.", result);

    result = ADQ_SetParameters(m_id.handle, m_id.index, &transfer);
    if (result != sizeof(transfer))
        ThrowDigitizerException("Failed to set transfer parameters, result {}.", result);

    result = ADQ_SetParameters(m_id.handle, m_id.index, &readout);
    if (result != sizeof(readout))
        ThrowDigitizerException("Failed to set readout parameters, result {}.", result);

    /* Finish up by getting the current parameters to make these settings
       reflect in the configuration file. */
    GetParameters(ADQ_PARAMETER_ID_TOP, m_parameters.top, m_watchers.top);
#endif
}

void Digitizer::CallPython(const std::string &module)
{
    _EmplaceMessage(DigitizerMessageId::EVENT_PYTHON);
    std::string out{};
    int result = m_python->CallMain(module, m_id.handle, m_id.index, out);
    if (result == SCAPE_EOK)
    {
        Log::log->info(FormatLog("Successfully called main() in module '{}'.", module));
        if (out.size() > 0)
            Log::log->info(FormatLog("Captured stdout:\n\n{}", out));

        /* If the call was successful, we synchronize the parameter files
           because something may have changed. */
        GetParameters(ADQ_PARAMETER_ID_TOP, m_parameters.top, m_watchers.top);
        GetParameters(ADQ_PARAMETER_ID_CLOCK_SYSTEM, m_parameters.clock_system,
                      m_watchers.clock_system);
    }
    else
    {
        ThrowDigitizerException("Error when calling main() in module '{}':\n\n{}", module, out);
    }
}

void Digitizer::ScaleRecordLength(double factor)
{
#ifdef MOCK_ADQAPI
    ThrowDigitizerException("ScaleRecordLength({}) not implemented.", factor);
#else
    ADQDataAcquisitionParameters acquisition;
    int result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_ACQUISITION, &acquisition);
    if (result != sizeof(acquisition))
        ThrowDigitizerException("Failed to get acquisition parameters, result {}.", result);

    for (int ch = 0; ch < m_constant.nof_acquisition_channels; ++ch)
    {
        if (acquisition.channel[ch].nof_records == 0)
            continue;

        const auto record_length = static_cast<int64_t>(
            std::round(factor * static_cast<double>(acquisition.channel[ch].record_length)));

        acquisition.channel[ch].record_length = record_length;
    }

    result = ADQ_SetParameters(m_id.handle, m_id.index, &acquisition);
    if (result != sizeof(acquisition))
        ThrowDigitizerException("Failed to set acquisition parameters, result {}.", result);

    /* Finish up by getting the current parameters to make these settings
       reflect in the configuration file. */
    GetParameters(ADQ_PARAMETER_ID_TOP, m_parameters.top, m_watchers.top);
#endif
}

void Digitizer::ForceAcquisition()
{
    /* TODO: Implement */
    ThrowDigitizerException("ForceAcquisition() not implemented.");
}

void Digitizer::SetParameters(const std::shared_ptr<std::string> &str)
{
    _EmplaceMessage(DigitizerMessageId::EVENT_CONFIGURATION);
    int result = ADQ_SetParametersString(m_id.handle, m_id.index, str->c_str(), str->size());
    if (result <= 0)
        ThrowDigitizerException("ADQ_SetParametersString() failed, result {}.", result);
}

void Digitizer::InitializeParameters(ADQParameterId id, const std::unique_ptr<FileWatcher> &watcher)
{
    auto parameters = std::make_shared<std::string>(64 * 1024, '\0');
    int result = ADQ_InitializeParametersString(m_id.handle, m_id.index, id, parameters->data(),
                                                parameters->size(), 1);
    if (result > 0)
    {
        /* The return value includes the null terminator so we have to remove it from the string. */
        parameters->resize(result - 1);
        watcher->EmplaceMessage(FileWatcherMessageId::UPDATE_FILE, parameters);
    }
    else
    {
        ThrowDigitizerException("ADQ_InitializeParametersString failed, result {}.", result);
    }
}

void Digitizer::GetParameters(
    ADQParameterId id, std::shared_ptr<std::string> &str,
    const std::unique_ptr<FileWatcher> &watcher)
{
    /* Write directly to a std::string. We need to use `resize` here instead of
       reserved to make the string understand its own size after we've written
       to the underlying memory via the API call. */
    std::string parameters(64 * 1024, '\0');
    int result = ADQ_GetParametersString(m_id.handle, m_id.index, id, parameters.data(),
                                         parameters.size(), 1);
    if (result > 0)
    {
        /* The return value includes the null terminator so we have to remove it
           from the string. */
        parameters.resize(result - 1);

        /* We use `UPDATE_FILE_IGNORE` to indicate that the act of changing the
           file contents should _not_ generate a` `FILE_UPDATED` message in
           return. The reason is that `GetParameters` gives us the current state
           of the digitizer, so there's no need to indicate that those
           parameters need to be reapplied. */
        str = std::make_shared<std::string>(std::move(parameters));
        watcher->EmplaceMessage(FileWatcherMessageId::UPDATE_FILE_IGNORE, str);
    }
    else
    {
        ThrowDigitizerException("ADQ_GetParametersString failed, result {}.", result);
    }
}

void Digitizer::EmitConstantParameters()
{
    int result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CONSTANT, &m_constant);
    if (result != sizeof(m_constant))
        ThrowDigitizerException("Failed to get constant parameters, result {}.", result);

    _EmplaceMessage(DigitizerMessageId::CONSTANT_PARAMETERS, m_constant);
}

void Digitizer::InitializeFileWatchers()
{
    auto MakeLowercase = [](char c) { return static_cast<char>(std::tolower(c)); };

    auto identifier = fmt::format("{}_{}", m_constant.serial_number, m_constant.firmware.name);
    std::transform(identifier.begin(), identifier.end(), identifier.begin(), MakeLowercase);

    /* Creating new file watchers will destroy the old ones (correctly stopping the threads). */
    m_watchers.top = std::make_unique<FileWatcher>(
        m_configuration_directory / fmt::format("parameters_top_{}.json", identifier));

    m_watchers.clock_system = std::make_unique<FileWatcher>(
        m_configuration_directory / fmt::format("parameters_clock_system_{}.json", identifier));

    m_parameters.top = std::make_shared<std::string>("");
    m_parameters.clock_system = std::make_shared<std::string>("");

    m_watchers.top->Start();
    m_watchers.clock_system->Start();
}

template <typename... Args>
std::string Digitizer::FormatLog(Args &&... args)
{
    std::string result{};
    if (m_constant.product_name[0] && m_constant.serial_number[0])
        result = fmt::format("{} {}: ", m_constant.product_name, m_constant.serial_number);

    return result + fmt::format(std::forward<Args>(args)...);
}

template <typename... Args>
void Digitizer::ThrowDigitizerException(Args &&... args)
{
    throw DigitizerException(std::move(FormatLog(std::forward<Args>(args)...)));
}
