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
#include "embedded_python.h"
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
                     const std::string &configuration_directory)
    : m_state(DigitizerState::NOT_INITIALIZED)
    , m_id{handle, init_index, index}
    , m_configuration_directory(configuration_directory)
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

/* Interface to the digitizer's data processing threads, one per channel. */
int Digitizer::WaitForProcessedRecord(int channel, std::shared_ptr<ProcessedRecord> &record)
{
    if ((channel < 0) || (channel >= static_cast<int>(m_processing_threads.size())))
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
        if (m_should_stop.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready)
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

    /* Instantiate one data processing thread for each digitizer channel. */
    for (int ch = 0; ch < m_constant.nof_channels; ++ch)
    {
        const auto label = fmt::format("{} {} CH{}", m_constant.product_name,
                                       m_constant.serial_number, m_constant.channel[ch].label);
        m_processing_threads.emplace_back(
            std::make_unique<DataProcessing>(m_id.handle, m_id.index, ch, std::move(label), m_constant)
        );
    }

    /* Initialize the file watchers now that we know the identifying properties
       of the digitizer. */
    InitializeFileWatchers();

    /* Send the post-initialization message, triggering the GUI initialization. */
    m_read_queue.EmplaceWrite(DigitizerMessageId::INITIALIZED, m_constant);

    /* Initialize the objects associated with the system manager, like sensors
       and boot status. */
    InitializeSystemManagerObjects();
}

void Digitizer::SignalError(const std::string &message)
{
    Log::log->error(message);
    m_read_queue.EmplaceWrite(DigitizerMessageId::ERR);
}

void Digitizer::ProcessMessages()
{
    DigitizerMessage message;
    while (SCAPE_EOK == m_write_queue.Read(message, 0))
        HandleMessageInState(message);
}

void Digitizer::ProcessWatcherMessages()
{
    ProcessWatcherMessages(m_watchers.top, m_parameters.top,
                           DigitizerMessageId::DIRTY_TOP_PARAMETERS, ADQ_PARAMETER_ID_TOP);
    ProcessWatcherMessages(m_watchers.clock_system, m_parameters.clock_system,
                           DigitizerMessageId::DIRTY_CLOCK_SYSTEM_PARAMETERS,
                           ADQ_PARAMETER_ID_CLOCK_SYSTEM);
}

void Digitizer::ProcessWatcherMessages(const std::unique_ptr<FileWatcher> &watcher,
                                       std::shared_ptr<std::string> &str,
                                       DigitizerMessageId dirty_id,
                                       enum ADQParameterId parameter_id)
{
    FileWatcherMessage message;
    while (SCAPE_EOK == watcher->WaitForMessage(message, 0))
    {
        switch (message.id)
        {
        case FileWatcherMessageId::FILE_CREATED:
        case FileWatcherMessageId::FILE_UPDATED:
            str = message.contents;
            m_read_queue.Write(dirty_id);
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

    m_read_queue.EmplaceWrite(DigitizerMessageId::BOOT_STATUS, state, state_information.label,
                              std::move(boot_entries));
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

    m_read_queue.EmplaceWrite(DigitizerMessageId::SENSOR_TREE, std::move(sensor_tree));
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

            if (sensor.status != 0)
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
        m_read_queue.EmplaceWrite(DigitizerMessageId::NO_ACTIVITY);
        m_no_activity_threshold_ms = milliseconds_max;
        m_notified_no_activity = true;
    }
    else if (m_notified_no_activity && milliseconds_max + ACTIVITY_HYSTERESIS_MS < m_no_activity_threshold_ms)
    {
        m_read_queue.EmplaceWrite(DigitizerMessageId::CLEAR);
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

        struct ADQDramStatus dram_status;
        if (sizeof(dram_status) ==
            ADQ_GetStatus(m_id.handle, m_id.index, ADQ_STATUS_ID_DRAM, &dram_status))
        {
            double fill_percent = static_cast<double>(dram_status.fill) /
                                  static_cast<double>(m_constant.dram_size);
            m_read_queue.EmplaceWrite(DigitizerMessageId::DRAM_FILL, fill_percent);
        }

        struct ADQOverflowStatus overflow_status;
        if (sizeof(overflow_status) ==
            ADQ_GetStatus(m_id.handle, m_id.index, ADQ_STATUS_ID_OVERFLOW, &overflow_status))
        {
            if (overflow_status.overflow)
                m_read_queue.EmplaceWrite(DigitizerMessageId::OVF);
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
            ThrowDigitizerException("ADQ_GetParameters failed, result {}.", result);

        /* TODO: Temporary workaround until the `time_unit` precision is fixed. */
        struct ADQClockSystemParameters clock_system;
        result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_CLOCK_SYSTEM, &clock_system);
        if (result != sizeof(clock_system))
            ThrowDigitizerException("ADQ_GetParameters failed, result {}.", result);

        for (size_t i = 0; i < m_processing_threads.size(); ++i)
        {
            m_processing_threads[i]->EmplaceMessage(
                DataProcessingMessageId::SET_AFE_PARAMETERS, afe.channel[i]);
            m_processing_threads[i]->EmplaceMessage(
                DataProcessingMessageId::SET_CLOCK_SYSTEM_PARAMETERS, clock_system);
        }

        for (const auto &t : m_processing_threads)
        {
            if (SCAPE_EOK != t->Start())
                ThrowDigitizerException("Failed to start one of the data processing threads.");
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
    m_read_queue.EmplaceWrite(DigitizerMessageId::STATE, state);
}

void Digitizer::HandleMessageInNotInitialized(const struct DigitizerMessage &message)
{
    ThrowDigitizerException("Unsupported action in NOT INITIALIZED '{}'.", message.id);
}

void Digitizer::HandleMessageInInitialization(const struct DigitizerMessage &message)
{
    ThrowDigitizerException("Unsupported action in INITIALIZATION '{}'.", message.id);
}

void Digitizer::HandleMessageInIdle(const struct DigitizerMessage &message)
{
    /* Any exception propagates to the upper layers to create an error message. */
    switch (message.id)
    {
    case DigitizerMessageId::START_ACQUISITION:
        StartDataAcquisition();
        break;

    case DigitizerMessageId::SET_TOP_PARAMETERS:
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

    case DigitizerMessageId::SCALE_RECORD_LENGTH:
        ScaleRecordLength(message.dvalue);
        break;

    case DigitizerMessageId::FORCE_ACQUISITION:
        ForceAcquisition();
        break;

    case DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
        SetParameters(m_parameters.clock_system, DigitizerMessageId::CLEAN_CLOCK_SYSTEM_PARAMETERS);
        SetParameters(m_parameters.top, DigitizerMessageId::CLEAN_TOP_PARAMETERS);
        EmitConstantParameters();
        break;

    case DigitizerMessageId::GET_TOP_PARAMETERS:
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
        m_read_queue.EmplaceWrite(DigitizerMessageId::PARAMETERS_FILENAME,
                                  m_watchers.top->GetPath());
        break;

    case DigitizerMessageId::GET_CLOCK_SYSTEM_PARAMETERS_FILENAME:
        m_read_queue.EmplaceWrite(DigitizerMessageId::PARAMETERS_FILENAME,
                                  m_watchers.clock_system->GetPath());
        break;

    case DigitizerMessageId::CALL_PYTHON:
        try
        {
            std::string out{};
            EmbeddedPython::CallMain(message.str, m_id.handle, m_id.index, out);
            Log::log->info(FormatLog("Successfully called main() in module '{}'.", message.str));
            if (out.size() > 0)
                Log::log->info(FormatLog("Captured stdout:\n\n{}", out));
        }
        catch (const EmbeddedPythonException &e)
        {
            /* Translate to the local exception type. */
            ThrowDigitizerException("Error when calling main() in module '{}':\n\n{}", message.str,
                                    e.what());
        }
        break;

    default:
        ThrowDigitizerException("Unsupported action in IDLE '{}'.", message.id);
    }
}

void Digitizer::HandleMessageInAcquisition(const struct DigitizerMessage &message)
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

    case DigitizerMessageId::SCALE_RECORD_LENGTH:
        StopDataAcquisition();
        ScaleRecordLength(message.dvalue);
        StartDataAcquisition();
        break;

    case DigitizerMessageId::FORCE_ACQUISITION:
        StopDataAcquisition();
        ForceAcquisition();
        break;

    case DigitizerMessageId::SET_TOP_PARAMETERS:
        StopDataAcquisition();
        SetParameters(m_parameters.top, DigitizerMessageId::CLEAN_TOP_PARAMETERS);
        StartDataAcquisition();
        break;

    case DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
        StopDataAcquisition();
        SetParameters(m_parameters.clock_system, DigitizerMessageId::CLEAN_CLOCK_SYSTEM_PARAMETERS);
        SetParameters(m_parameters.top, DigitizerMessageId::CLEAN_TOP_PARAMETERS);
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
        m_read_queue.EmplaceWrite(DigitizerMessageId::PARAMETERS_FILENAME,
                                  m_watchers.top->GetPath());
        break;

    case DigitizerMessageId::GET_CLOCK_SYSTEM_PARAMETERS_FILENAME:
        m_read_queue.EmplaceWrite(DigitizerMessageId::PARAMETERS_FILENAME,
                                  m_watchers.clock_system->GetPath());
        break;

    default:
        ThrowDigitizerException("Unsupported action in ACQUISITION '{}'.", message.id);
    }
}

void Digitizer::HandleMessageInState(const struct DigitizerMessage &message)
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

    /* If the message was processed successfully, we send the 'all clear'. */
    Log::log->trace(FormatLog("Processed message {}.", message.id));
    m_read_queue.EmplaceWrite(DigitizerMessageId::CLEAR);
}

void Digitizer::ConfigureInternalReference()
{
#ifdef MOCK_ADQAPI
    ThrowDigitizerException("ConfigureInternalReference() not implemented.");
#else
    struct ADQClockSystemParameters clock_system;
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
    struct ADQClockSystemParameters clock_system;
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
    struct ADQClockSystemParameters clock_system;
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
    struct ADQEventSourcePeriodicParameters periodic;
    int result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_EVENT_SOURCE_PERIODIC, &periodic);
    if (result != sizeof(periodic))
        ThrowDigitizerException("Failed to get periodic parameters, result {}.", result);

    struct ADQDataAcquisitionParameters acquisition;
    result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_ACQUISITION, &acquisition);
    if (result != sizeof(acquisition))
        ThrowDigitizerException("Failed to get acquisition parameters, result {}.", result);

    struct ADQDataTransferParameters transfer;
    result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_TRANSFER, &transfer);
    if (result != sizeof(transfer))
        ThrowDigitizerException("Failed to get transfer parameters, result {}.", result);

    struct ADQDataReadoutParameters readout;
    result = ADQ_InitializeParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_READOUT, &readout);
    if (result != sizeof(readout))
        ThrowDigitizerException("Failed to get readout parameters, result {}.", result);

    /* The default acquisition parameters is an infinite stream of records from
       each available channel triggered by the periodic event generator. */
    for (int ch = 0; ch < m_constant.nof_channels; ++ch)
    {
        acquisition.channel[ch].nof_records = ADQ_INFINITE_NOF_RECORDS;
        acquisition.channel[ch].record_length = 32 * 1024;
        acquisition.channel[ch].horizontal_offset = 0;
        acquisition.channel[ch].trigger_source = ADQ_EVENT_SOURCE_PERIODIC;
        acquisition.channel[ch].trigger_edge = ADQ_EDGE_RISING;
        acquisition.channel[ch].trigger_blocking_source = ADQ_FUNCTION_INVALID;

        transfer.channel[ch].metadata_enabled = 1;
        transfer.channel[ch].nof_buffers = ADQ_MAX_NOF_BUFFERS;
        transfer.channel[ch].record_size = acquisition.channel[ch].bytes_per_sample
                                          * acquisition.channel[ch].record_length;
        transfer.channel[ch].record_buffer_size = transfer.channel[ch].record_size;
        transfer.channel[ch].metadata_buffer_size = sizeof(ADQGen4RecordHeader);

        /* TODO: Only if FWATD? */
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
    GetParameters(ADQ_PARAMETER_ID_TOP, m_watchers.top);
#endif
}

void Digitizer::ScaleRecordLength(double factor)
{
#ifdef MOCK_ADQAPI
    ThrowDigitizerException("ScaleRecordLength({}) not implemented.", factor);
#else
    struct ADQDataAcquisitionParameters acquisition;
    int result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_ACQUISITION, &acquisition);
    if (result != sizeof(acquisition))
        ThrowDigitizerException("Failed to get acquisition parameters, result {}.", result);

    struct ADQDataTransferParameters transfer;
    result = ADQ_GetParameters(m_id.handle, m_id.index, ADQ_PARAMETER_ID_DATA_TRANSFER, &transfer);
    if (result != sizeof(transfer))
        ThrowDigitizerException("Failed to get transfer parameters, result {}.", result);

    for (int ch = 0; ch < m_constant.nof_channels; ++ch)
    {
        if (acquisition.channel[ch].nof_records == 0 || transfer.channel[ch].nof_buffers == 0)
            continue;

        const auto record_length = static_cast<int64_t>(
            std::round(factor * static_cast<double>(acquisition.channel[ch].record_length)));

        acquisition.channel[ch].record_length = record_length;
        transfer.channel[ch].record_size = record_length * acquisition.channel[ch].bytes_per_sample;
        transfer.channel[ch].record_buffer_size = transfer.channel[ch].record_size;
        transfer.channel[ch].metadata_buffer_size = sizeof(ADQGen4RecordHeader);
    }

    result = ADQ_SetParameters(m_id.handle, m_id.index, &acquisition);
    if (result != sizeof(acquisition))
        ThrowDigitizerException("Failed to set acquisition parameters, result {}.", result);

    result = ADQ_SetParameters(m_id.handle, m_id.index, &transfer);
    if (result != sizeof(transfer))
        ThrowDigitizerException("Failed to set transfer parameters, result {}.", result);
#endif
}

void Digitizer::ForceAcquisition()
{
    /* TODO: Implement */
    ThrowDigitizerException("ForceAcquisition() not implemented.");
}

void Digitizer::SetParameters(const std::shared_ptr<std::string> &str, DigitizerMessageId clean_id)
{
    m_read_queue.EmplaceWrite(DigitizerMessageId::CONFIGURATION);
    int result = ADQ_SetParametersString(m_id.handle, m_id.index, str->c_str(), str->size());
    if (result > 0)
        m_read_queue.Write(clean_id);
    else
        ThrowDigitizerException("ADQ_SetParametersString() failed, result {}.", result);
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
        watcher->EmplaceMessage(FileWatcherMessageId::UPDATE_FILE, parameters);
    }
    else
    {
        ThrowDigitizerException("ADQ_InitializeParametersString failed, result {}.", result);
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
        watcher->EmplaceMessage(FileWatcherMessageId::UPDATE_FILE, parameters);
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

    m_read_queue.EmplaceWrite(DigitizerMessageId::CONSTANT_PARAMETERS, m_constant);
}

void Digitizer::InitializeFileWatchers()
{
    auto MakeLowercase = [](unsigned char c){ return static_cast<char>(std::tolower(c)); };

    std::string serial_number(m_constant.serial_number);
    std::transform(serial_number.begin(), serial_number.end(), serial_number.begin(),
                   MakeLowercase);

    /* Creating new file watchers will destroy the old ones (correctly stopping the threads). */
    m_watchers.top = std::make_unique<FileWatcher>(
        fmt::format("{}/parameters_top_{}.json", m_configuration_directory, serial_number));

    m_watchers.clock_system = std::make_unique<FileWatcher>(
        fmt::format("{}/parameters_clock_system_{}.json", m_configuration_directory, serial_number));

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
