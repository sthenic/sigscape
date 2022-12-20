/*
 * An abstract base class that defines the interface of a digitizer.
 */

#ifndef DIGITIZER_H_TMDVZV
#define DIGITIZER_H_TMDVZV

#include "message_thread.h"
#include "data_types.h"
#include "data_processing.h"
#include "file_watcher.h"
#include "system_manager.h"

#ifdef NO_ADQAPI
#include "mock_adqapi.h"
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
    IDENTIFIER,
    NOF_CHANNELS,
    STATE,
    ERR,
    CLEAR,
    CONFIGURATION,
    INITIALIZE_WOULD_OVERWRITE,
    SENSOR_TREE,
    BOOT_STATUS,
    /* The world -> digitizer */
    SET_INTERNAL_REFERENCE,
    SET_EXTERNAL_REFERENCE,
    SET_EXTERNAL_CLOCK,
    DEFAULT_ACQUISITION,
    START_ACQUISITION,
    STOP_ACQUISITION,
    SET_PARAMETERS, /* FIXME: Probably rename this SET_TOP_PARAMETERS */
    GET_PARAMETERS,
    VALIDATE_PARAMETERS,
    INITIALIZE_PARAMETERS,
    INITIALIZE_PARAMETERS_FORCE,
    SET_CLOCK_SYSTEM_PARAMETERS,
    SET_WINDOW_TYPE,
};

enum class DigitizerState
{
    NOT_ENUMERATED,
    ENUMERATION,
    IDLE,
    ACQUISITION
};

struct Sensor
{
    Sensor() = default;
    Sensor(uint32_t id, uint32_t group_id, const char *label, const char *unit)
        : id(id)
        , group_id(group_id)
        , label(label)
        , unit(unit)
    {}

    uint32_t id;
    uint32_t group_id;
    std::string label;
    std::string unit;
};

struct SensorGroup
{
    SensorGroup() = default;
    SensorGroup(uint32_t id, const char *label)
        : id(id)
        , label(label)
    {}

    uint32_t id;
    std::string label;
    std::vector<Sensor> sensors;
};

typedef std::vector<SensorGroup> SensorTree;

struct BootEntry
{
    BootEntry(uint32_t id, const char *label, int32_t status)
        : id(id)
        , status(status)
        , label(label)
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
        , window_type()
        , sensor_tree()
        , boot_entries{}
    {}

    /* Create a state message. */
    DigitizerMessage(DigitizerMessageId id, DigitizerState state)
        : id(id)
        , state(state)
        , str()
        , ivalue()
        , window_type()
        , sensor_tree()
        , boot_entries{}
    {}

    /* Create a string message. */
    DigitizerMessage(DigitizerMessageId id, const std::string &str)
        : id(id)
        , state()
        , str(str)
        , ivalue()
        , window_type()
        , sensor_tree()
        , boot_entries{}
    {}

    /* Create an integer message. */
    DigitizerMessage(DigitizerMessageId id, int ivalue)
        : id(id)
        , state()
        , str()
        , ivalue(ivalue)
        , window_type()
        , sensor_tree()
        , boot_entries{}
    {}

    /* Create a window message. */
    DigitizerMessage(DigitizerMessageId id, WindowType window_type)
        : id(id)
        , state()
        , str()
        , ivalue()
        , window_type(window_type)
        , sensor_tree()
        , boot_entries{}
    {}

    /* Create a sensor identification message, taking ownership of the sensor information. */
    DigitizerMessage(DigitizerMessageId id, SensorTree &&sensor_tree)
        : id(id)
        , state()
        , str()
        , ivalue()
        , window_type()
        , sensor_tree(std::move(sensor_tree))
        , boot_entries{}
    {}

    /* Create a message holding all the boot statuses. */
    DigitizerMessage(DigitizerMessageId id, int state, const char *state_description,
                     std::vector<BootEntry> &&boot_entries)
        : id(id)
        , state()
        , str(state_description)
        , ivalue(state)
        , window_type()
        , sensor_tree()
        , boot_entries(std::move(boot_entries))
    {}

    DigitizerMessageId id;
    DigitizerState state;
    std::string str;
    int ivalue;
    WindowType window_type;
    SensorTree sensor_tree;
    std::vector<BootEntry> boot_entries;
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

    /* Sensor records. */
    std::vector<SensorRecord> m_sensor_records;
    ThreadSafeQueue<std::shared_ptr<std::vector<SensorRecord>>> m_sensor_record_queue;
    std::chrono::high_resolution_clock::time_point m_sensor_last_record_timestamp;

    void SignalError(const std::string &message);
    void ProcessMessages();
    void ProcessWatcherMessages();
    void ProcessWatcherMessages(const std::unique_ptr<FileWatcher> &watcher,
                                std::shared_ptr<std::string> &str,
                                DigitizerMessageId dirty_id);

    void InitializeSystemManagerBootStatus();
    void InitializeSystemManagerSensors();
    void InitializeSystemManagerObjects();
    void UpdateSystemManagerObjects();

    void StartDataAcquisition();
    void StopDataAcquisition();

    void SetState(DigitizerState state);

    void HandleMessageInNotEnumerated(const struct DigitizerMessage &message);
    void HandleMessageInEnumeration(const struct DigitizerMessage &message);
    void HandleMessageInIdle(const struct DigitizerMessage &message);
    void HandleMessageInConfiguration(const struct DigitizerMessage &message);
    void HandleMessageInAcquisition(const struct DigitizerMessage &message);
    void HandleMessageInState(const struct DigitizerMessage &message);

    void ConfigureInternalReference();
    void ConfigureExternalReference();
    void ConfigureExternalClock();
    void ConfigureDefaultAcquisition();
    void SetParameters(const std::shared_ptr<std::string> &str, DigitizerMessageId clean_id);
    void InitializeParameters(enum ADQParameterId id, const std::unique_ptr<FileWatcher> &watcher);
    void GetParameters(enum ADQParameterId id, const std::unique_ptr<FileWatcher> &watcher);
    void InitializeFileWatchers(const struct ADQConstantParameters &constant);

    static constexpr double SENSOR_SAMPLING_PERIOD_MS = 1000.0;
};

#endif
