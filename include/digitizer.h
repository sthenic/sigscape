/*
 * An abstract base class that defines the interface of a digitizer.
 */

#pragma once

#include "message_thread.h"
#include "data_types.h"
#include "data_processing.h"
#include "file_watcher.h"
#include "system_manager.h"

#ifdef MOCK_ADQAPI
#include "mock/adqapi.h"
#else
#include "ADQAPI.h"
#endif

#include <array>
#include <chrono>

enum class DigitizerMessageId
{
    /* Digitizer -> the world */
    DIRTY_TOP_PARAMETERS,
    DIRTY_CLOCK_SYSTEM_PARAMETERS,
    CLEAN_TOP_PARAMETERS,
    CLEAN_CLOCK_SYSTEM_PARAMETERS,
    CONSTANT_PARAMETERS,
    STATE,
    ERR,
    CLEAR,
    CONFIGURATION,
    INITIALIZE_WOULD_OVERWRITE,
    SENSOR_TREE,
    BOOT_STATUS,
    NO_ACTIVITY,
    PARAMETERS_FILENAME,
    DRAM_FILL,
    OVF,
    /* The world -> digitizer */
    SET_INTERNAL_REFERENCE,
    SET_EXTERNAL_REFERENCE,
    SET_EXTERNAL_CLOCK,
    DEFAULT_ACQUISITION,
    FORCE_ACQUISITION,
    START_ACQUISITION,
    STOP_ACQUISITION,
    SET_PARAMETERS, /* FIXME: Probably rename this SET_TOP_PARAMETERS */
    GET_PARAMETERS,
    VALIDATE_PARAMETERS,
    INITIALIZE_PARAMETERS,
    INITIALIZE_PARAMETERS_FORCE,
    SET_CLOCK_SYSTEM_PARAMETERS,
    /* FIXME: Bundle the processing parameters into a single message id. */
    SET_WINDOW_TYPE,
    SET_CONVERT_HORIZONTAL,
    SET_CONVERT_VERTICAL,
    SET_FULLSCALE_ENOB,
    SET_FREQUENCY_DOMAIN_SCALING,
    SET_CONFIGURATION_DIRECTORY,
    GET_TOP_PARAMETERS_FILENAME,
    GET_CLOCK_SYSTEM_PARAMETERS_FILENAME,
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
        , state()
        , str()
        , ivalue()
        , dvalue()
        , bvalue()
        , window_type()
        , frequency_domain_scaling()
        , sensor_tree()
        , boot_entries{}
        , constant_parameters()
    {}

    /* Create a state message. */
    DigitizerMessage(DigitizerMessageId id, DigitizerState state)
        : id(id)
        , state(state)
        , str()
        , ivalue()
        , dvalue()
        , bvalue()
        , window_type()
        , frequency_domain_scaling()
        , sensor_tree()
        , boot_entries{}
        , constant_parameters()
    {}

    /* Create a string message. */
    DigitizerMessage(DigitizerMessageId id, const std::string &str)
        : id(id)
        , state()
        , str(str)
        , ivalue()
        , dvalue()
        , bvalue()
        , window_type()
        , frequency_domain_scaling()
        , sensor_tree()
        , boot_entries{}
        , constant_parameters()
    {}

    /* Create an integer message. */
    DigitizerMessage(DigitizerMessageId id, int ivalue)
        : id(id)
        , state()
        , str()
        , ivalue(ivalue)
        , dvalue()
        , bvalue()
        , window_type()
        , frequency_domain_scaling()
        , sensor_tree()
        , boot_entries{}
        , constant_parameters()
    {}

    /* Create a double message. */
    DigitizerMessage(DigitizerMessageId id, double dvalue)
        : id(id)
        , state()
        , str()
        , ivalue()
        , dvalue(dvalue)
        , bvalue()
        , window_type()
        , frequency_domain_scaling()
        , sensor_tree()
        , boot_entries{}
        , constant_parameters()
    {}

    /* Create a boolean message. */
    DigitizerMessage(DigitizerMessageId id, bool bvalue)
        : id(id)
        , state()
        , str()
        , ivalue()
        , dvalue()
        , bvalue(bvalue)
        , window_type()
        , frequency_domain_scaling()
        , sensor_tree()
        , boot_entries{}
        , constant_parameters()
    {}

    /* Create a window message. */
    DigitizerMessage(DigitizerMessageId id, WindowType window_type)
        : id(id)
        , state()
        , str()
        , ivalue()
        , dvalue()
        , bvalue()
        , window_type(window_type)
        , frequency_domain_scaling()
        , sensor_tree()
        , boot_entries{}
        , constant_parameters()
    {}

    /* Create a frequency domain scaling message. */
    DigitizerMessage(DigitizerMessageId id, FrequencyDomainScaling scaling)
        : id(id)
        , state()
        , str()
        , ivalue()
        , dvalue()
        , bvalue()
        , window_type()
        , frequency_domain_scaling(scaling)
        , sensor_tree()
        , boot_entries{}
        , constant_parameters()
    {}

    /* Create a sensor identification message, taking ownership of the sensor information. */
    DigitizerMessage(DigitizerMessageId id, SensorTree &&sensor_tree)
        : id(id)
        , state()
        , str()
        , ivalue()
        , dvalue()
        , bvalue()
        , window_type()
        , frequency_domain_scaling()
        , sensor_tree(std::move(sensor_tree))
        , boot_entries{}
        , constant_parameters()
    {}

    /* Create a message holding all the boot statuses. */
    DigitizerMessage(DigitizerMessageId id, int state, const char (&state_description)[32],
                     std::vector<BootEntry> &&boot_entries)
        : id(id)
        , state()
        , str(state_description, 0, sizeof(state_description))
        , ivalue(state)
        , dvalue()
        , bvalue()
        , window_type()
        , frequency_domain_scaling()
        , sensor_tree()
        , boot_entries(std::move(boot_entries))
        , constant_parameters()
    {}

    /* Create a message holding the digitizer's constant parameters. */
    DigitizerMessage(DigitizerMessageId id, const struct ADQConstantParameters &constant_parameters)
        : id(id)
        , state()
        , str()
        , ivalue()
        , dvalue()
        , bvalue()
        , window_type()
        , frequency_domain_scaling()
        , sensor_tree()
        , boot_entries{}
        , constant_parameters(constant_parameters)
    {}

    DigitizerMessageId id;
    DigitizerState state;
    std::string str;
    int ivalue;
    double dvalue;
    bool bvalue;
    WindowType window_type;
    FrequencyDomainScaling frequency_domain_scaling;
    SensorTree sensor_tree;
    std::vector<BootEntry> boot_entries;
    struct ADQConstantParameters constant_parameters;
};

class Digitizer : public MessageThread<Digitizer, DigitizerMessage>
{
public:
    Digitizer(void *handle, int index);
    ~Digitizer();

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
       an index. These values must be known at the time of construction. */
    const struct Identifier
    {
        void *handle;
        int index;
    } m_id;

    /* The directory where we store the parameter files. */
    std::string m_configuration_directory;

    /* Storage of the digitizer's constant parameters for easy reference. */
    struct ADQConstantParameters m_constant;

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

    void HandleMessageInNotInitialized(const struct DigitizerMessage &message);
    void HandleMessageInInitialization(const struct DigitizerMessage &message);
    void HandleMessageInIdle(const struct DigitizerMessage &message);
    void HandleMessageInConfiguration(const struct DigitizerMessage &message);
    void HandleMessageInAcquisition(const struct DigitizerMessage &message);
    void HandleMessageInState(const struct DigitizerMessage &message);

    void ConfigureInternalReference();
    void ConfigureExternalReference();
    void ConfigureExternalClock();
    void ConfigureDefaultAcquisition();
    void ForceAcquisition();
    void SetParameters(const std::shared_ptr<std::string> &str, DigitizerMessageId clean_id);
    void InitializeParameters(enum ADQParameterId id, const std::unique_ptr<FileWatcher> &watcher);
    void GetParameters(enum ADQParameterId id, const std::unique_ptr<FileWatcher> &watcher);
    void InitializeFileWatchers();

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
        case DigitizerMessageId::CONSTANT_PARAMETERS:
            name = "CONSTANT_PARAMETERS";
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
        case DigitizerMessageId::SENSOR_TREE:
            name = "SENSOR_TREE";
            break;
        case DigitizerMessageId::BOOT_STATUS:
            name = "BOOT_STATUS";
            break;
        case DigitizerMessageId::NO_ACTIVITY:
            name = "NO_ACTIVITY";
            break;
        case DigitizerMessageId::PARAMETERS_FILENAME:
            name = "PARAMETERS_FILENAME";
            break;
        case DigitizerMessageId::DRAM_FILL:
            name = "DRAM_FILL";
            break;
        case DigitizerMessageId::OVF:
            name = "OVF";
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
        case DigitizerMessageId::FORCE_ACQUISITION:
            name = "FORCE_ACQUISITION";
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
        case DigitizerMessageId::SET_CONVERT_HORIZONTAL:
            name = "SET_CONVERT_HORIZONTAL";
            break;
        case DigitizerMessageId::SET_CONVERT_VERTICAL:
            name = "SET_CONVERT_VERTICAL";
            break;
        case DigitizerMessageId::SET_FULLSCALE_ENOB:
            name = "SET_FULLSCALE_ENOB";
            break;
        case DigitizerMessageId::SET_FREQUENCY_DOMAIN_SCALING:
            name = "SET_FREQUENCY_DOMAIN_SCALING";
            break;
        case DigitizerMessageId::SET_CONFIGURATION_DIRECTORY:
            name = "SET_CONFIGURATION_DIRECTORY";
            break;
        case DigitizerMessageId::GET_TOP_PARAMETERS_FILENAME:
            name = "GET_TOP_PARAMETERS_FILENAME";
            break;
        case DigitizerMessageId::GET_CLOCK_SYSTEM_PARAMETERS_FILENAME:
            name = "GET_CLOCK_SYSTEM_PARAMETERS_FILENAME";
            break;
        }

        return fmt::formatter<string_view>::format(name, ctx);
    }
};
