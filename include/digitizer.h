/*
 * An abstract base class that defines the interface of a digitizer.
 */

#pragma once

#include "message_thread.h"
#include "data_types.h"
#include "data_processing.h"
#include "file_watcher.h"
#include "system_manager.h"

#include "ADQAPI.h"

#include <array>
#include <chrono>

enum class DigitizerMessageId
{
    /* Digitizer -> the world */
    CHANGED_TOP_PARAMETERS,
    CHANGED_CLOCK_SYSTEM_PARAMETERS,
    INITIALIZED,
    CONSTANT_PARAMETERS,
    STATE,
    EVENT_ERROR,
    EVENT_CLEAR,
    EVENT_OVERFLOW,
    EVENT_CONFIGURATION,
    EVENT_NO_ACTIVITY,
    INITIALIZE_WOULD_OVERWRITE,
    SENSOR_TREE,
    BOOT_STATUS,
    PARAMETERS_FILENAME,
    DRAM_FILL,
    /* The world -> digitizer (command) */
    /* Digitizer -> the world (execution result) */
    SET_INTERNAL_REFERENCE,
    SET_EXTERNAL_REFERENCE,
    SET_EXTERNAL_CLOCK,
    DEFAULT_ACQUISITION,
    SCALE_RECORD_LENGTH_DOUBLE,
    SCALE_RECORD_LENGTH_HALF,
    FORCE_ACQUISITION,
    START_ACQUISITION,
    STOP_ACQUISITION,
    SET_TOP_PARAMETERS,
    GET_TOP_PARAMETERS,
    VALIDATE_PARAMETERS,
    INITIALIZE_PARAMETERS,
    INITIALIZE_PARAMETERS_FORCE,
    SET_CLOCK_SYSTEM_PARAMETERS,
    SET_PROCESSING_PARAMETERS,
    GET_TOP_PARAMETERS_FILENAME,
    GET_CLOCK_SYSTEM_PARAMETERS_FILENAME,
    CLEAR_PROCESSING_MEMORY,
    CALL_PYTHON,
};

enum class DigitizerState
{
    NOT_INITIALIZED,
    INITIALIZATION,
    IDLE,
    ACQUISITION
};

struct Sensor
{
    Sensor() = default;
    Sensor(uint32_t id, uint32_t group_id, const char (&label)[32])
        : id(id)
        , group_id(group_id)
        , label(label, 0, sizeof(label))
    {}

    uint32_t id;
    uint32_t group_id;
    std::string label;
};

struct SensorGroup
{
    SensorGroup() = default;
    SensorGroup(uint32_t id, const char (&label)[32])
        : id(id)
        , label(label, 0, sizeof(label))
    {}

    uint32_t id;
    std::string label;
    std::vector<Sensor> sensors;
};

typedef std::vector<SensorGroup> SensorTree;

struct BootEntry
{
    BootEntry(uint32_t id, const char (&label)[32], int32_t status)
        : id(id)
        , status(status)
        , label(label, 0, sizeof(label))
        , note()
    {}

    uint32_t id;
    int32_t status;
    std::string label;
    std::string note;
};

struct DigitizerMessage
{
    /* Default constructor (for receiving messages). */
    DigitizerMessage() = default;

    /* Create an empty message. */
    DigitizerMessage(DigitizerMessageId id)
        : id(id)
    {}

    /* Create a state message. */
    DigitizerMessage(DigitizerMessageId id, DigitizerState state)
        : id(id)
        , state(state)
    {}

    /* Create a string message. */
    DigitizerMessage(DigitizerMessageId id, const std::string &str)
        : id(id)
        , str(str)
    {}

    /* Create an integer message. */
    DigitizerMessage(DigitizerMessageId id, int ivalue)
        : id(id)
        , ivalue(ivalue)
    {}

    /* Create a double message. */
    DigitizerMessage(DigitizerMessageId id, double dvalue)
        : id(id)
        , dvalue(dvalue)
    {}

    /* Create a message holding data processing parameters. */
    DigitizerMessage(DigitizerMessageId id, const DataProcessingParameters &parameters)
        : id(id)
        , processing_parameters(parameters)
    {}

    /* Create a sensor identification message, taking ownership of the sensor information. */
    DigitizerMessage(DigitizerMessageId id, SensorTree &&sensor_tree)
        : id(id)
        , sensor_tree(std::move(sensor_tree))
    {}

    /* Create a message holding all the boot statuses. */
    DigitizerMessage(DigitizerMessageId id, int state, const char (&state_description)[32],
                     std::vector<BootEntry> &&boot_entries)
        : id(id)
        , str(state_description, 0, sizeof(state_description))
        , ivalue(state)
        , boot_entries(std::move(boot_entries))
    {}

    /* Create a message holding the digitizer's constant parameters. */
    DigitizerMessage(DigitizerMessageId id, const ADQConstantParameters &constant_parameters)
        : id(id)
        , constant_parameters(constant_parameters)
    {}

    DigitizerMessageId id;
    DigitizerState state;
    std::string str;
    int ivalue;
    int result;
    double dvalue;
    DataProcessingParameters processing_parameters;
    SensorTree sensor_tree;
    std::vector<BootEntry> boot_entries;
    ADQConstantParameters constant_parameters;
};

class Digitizer : public MessageThread<Digitizer, DigitizerMessage>
{
public:
    Digitizer(void *handle, int init_index, int index, const std::string &configuration_directory);
    ~Digitizer() override;

    /* Making copies of an object of this type is not allowed. */
    Digitizer(const Digitizer &other) = delete;
    Digitizer &operator=(const Digitizer &other) = delete;

    /* Interface to the digitizer's data processing threads, one per channel. */
    int WaitForProcessedRecord(int channel, std::shared_ptr<ProcessedRecord> &record);

    /* Interface to the digitizer's sensor data. */
    int WaitForSensorRecords(std::shared_ptr<std::vector<SensorRecord>> &records);

    /* The main loop. */
    void MainLoop() override;

private:
    /* The digitizer's state. */
    enum DigitizerState m_state;

    /* The digitizer identification information. This consists of a handle and
       two indexes: the `init_index` is _only_ used in the call to
       `SetupDevice()` while the one simply named `index` is used in all the
       `ADQ_*` calls. These values must be known at the time of construction. */
    const struct Identifier
    {
        void *handle;
        int init_index;
        int index;
    } m_id;

    /* The directory where we store the parameter files. */
    std::string m_configuration_directory;

    /* Storage of the digitizer's constant parameters for easy reference. */
    ADQConstantParameters m_constant;

    /* File watchers and queues to watch and propagate contents from the
       digitizer's configuration files. Anonymous structs are intentional. */
    struct
    {
        std::unique_ptr<FileWatcher> top;
        std::unique_ptr<FileWatcher> clock_system;
    } m_watchers;

    struct
    {
        std::shared_ptr<std::string> top;
        std::shared_ptr<std::string> clock_system;
    } m_parameters;

    /* The digitizer's data processing threads, one per channel. */
    std::vector<std::unique_ptr<DataProcessing>> m_processing_threads;
    int m_no_activity_threshold_ms;
    bool m_notified_no_activity;

    /* Sensor records. */
    std::vector<SensorRecord> m_sensor_records;
    ThreadSafeQueue<std::shared_ptr<std::vector<SensorRecord>>> m_sensor_record_queue;
    std::chrono::high_resolution_clock::time_point m_sensor_last_record_timestamp;
    std::chrono::high_resolution_clock::time_point m_last_status_timestamp;

    void MainInitialization();

    void SignalError(const std::string &message);
    void ProcessMessages();
    void ProcessWatcherMessages();
    void ProcessWatcherMessages(const std::unique_ptr<FileWatcher> &watcher,
                                std::shared_ptr<std::string> &str, DigitizerMessageId dirty_id,
                                enum ADQParameterId parameter_id);

    void InitializeSystemManagerBootStatus();
    void InitializeSystemManagerSensors();
    void InitializeSystemManagerObjects();
    void UpdateSystemManagerObjects();
    void CheckActivity();
    void CheckStatus();

    void StartDataAcquisition();
    void StopDataAcquisition();

    void SetState(DigitizerState state);

    void HandleMessageInNotInitialized(const DigitizerMessage &message);
    void HandleMessageInInitialization(const DigitizerMessage &message);
    void HandleMessageInIdle(const DigitizerMessage &message);
    void HandleMessageInAcquisition(const DigitizerMessage &message);
    void HandleMessageInState(const DigitizerMessage &message);

    void ConfigureInternalReference();
    void ConfigureExternalReference();
    void ConfigureExternalClock();
    void ConfigureDefaultAcquisition();
    void CallPython(const std::string &module);
    void ScaleRecordLength(double factor);
    void ForceAcquisition();
    void SetParameters(const std::shared_ptr<std::string> &str);
    void InitializeParameters(enum ADQParameterId id, const std::unique_ptr<FileWatcher> &watcher);
    void GetParameters(enum ADQParameterId id, const std::unique_ptr<FileWatcher> &watcher);
    void EmitConstantParameters();
    void InitializeFileWatchers();

    /* Prepend an object-specific header to the message and return the `fmt::format` string. */
    template <typename... Args>
    std::string FormatLog(Args &&... args);

    /* Throws a `DigitizerException` with a object-specific header prepended to the message. */
    template <typename... Args>
    void ThrowDigitizerException(Args &&... args);

    static constexpr double SENSOR_SAMPLING_PERIOD_MS = 1000.0;
    static constexpr double STATUS_SAMPLING_PERIOD_MS = 1000.0;
    static constexpr int DEFAULT_ACTIVITY_THRESHOLD_MS = 1000;
    static constexpr int ACTIVITY_HYSTERESIS_MS = 500;
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
        case DigitizerMessageId::CHANGED_TOP_PARAMETERS:
            name = "CHANGED_TOP_PARAMETERS";
            break;
        case DigitizerMessageId::CHANGED_CLOCK_SYSTEM_PARAMETERS:
            name = "CHANGED_CLOCK_SYSTEM_PARAMETERS";
            break;
        case DigitizerMessageId::INITIALIZED:
            name = "INITIALIZED";
            break;
        case DigitizerMessageId::CONSTANT_PARAMETERS:
            name = "CONSTANT_PARAMETERS";
            break;
        case DigitizerMessageId::STATE:
            name = "STATE";
            break;
        case DigitizerMessageId::EVENT_ERROR:
            name = "EVENT_ERROR";
            break;
        case DigitizerMessageId::EVENT_CLEAR:
            name = "EVENT_CLEAR";
            break;
        case DigitizerMessageId::EVENT_OVERFLOW:
            name = "EVENT_OVERFLOW";
            break;
        case DigitizerMessageId::EVENT_CONFIGURATION:
            name = "EVENT_CONFIGURATION";
            break;
        case DigitizerMessageId::EVENT_NO_ACTIVITY:
            name = "EVENT_NO_ACTIVITY";
            break;
        case DigitizerMessageId::INITIALIZE_WOULD_OVERWRITE:
            name = "INITIALIZE_WOULD_OVERWRITE";
            break;
        case DigitizerMessageId::SENSOR_TREE:
            name = "SENSOR_TREE";
            break;
        case DigitizerMessageId::BOOT_STATUS:
            name = "BOOT_STATUS";
            break;
        case DigitizerMessageId::PARAMETERS_FILENAME:
            name = "PARAMETERS_FILENAME";
            break;
        case DigitizerMessageId::DRAM_FILL:
            name = "DRAM_FILL";
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
        case DigitizerMessageId::SCALE_RECORD_LENGTH_DOUBLE:
            name = "SCALE_RECORD_LENGTH_DOUBLE";
            break;
        case DigitizerMessageId::SCALE_RECORD_LENGTH_HALF:
            name = "SCALE_RECORD_LENGTH_HALF";
            break;
        case DigitizerMessageId::FORCE_ACQUISITION:
            name = "FORCE_ACQUISITION";
            break;
        case DigitizerMessageId::START_ACQUISITION:
            name = "START_ACQUISITION";
            break;
        case DigitizerMessageId::STOP_ACQUISITION:
            name = "STOP_ACQUISITION";
            break;
        case DigitizerMessageId::SET_TOP_PARAMETERS:
            name = "SET_TOP_PARAMETERS";
            break;
        case DigitizerMessageId::GET_TOP_PARAMETERS:
            name = "GET_TOP_PARAMETERS";
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
        case DigitizerMessageId::SET_PROCESSING_PARAMETERS:
            name = "SET_PROCESSING_PARAMETERS";
            break;
        case DigitizerMessageId::GET_TOP_PARAMETERS_FILENAME:
            name = "GET_TOP_PARAMETERS_FILENAME";
            break;
        case DigitizerMessageId::GET_CLOCK_SYSTEM_PARAMETERS_FILENAME:
            name = "GET_CLOCK_SYSTEM_PARAMETERS_FILENAME";
            break;
        case DigitizerMessageId::CLEAR_PROCESSING_MEMORY:
            name = "CLEAR_PROCESSING_MEMORY";
            break;
        case DigitizerMessageId::CALL_PYTHON:
            name = "CALL_PYTHON";
            break;
        }

        return fmt::formatter<string_view>::format(name, ctx);
    }
};
