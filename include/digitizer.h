/*
 * An abstract base class that defines the interface of a digitizer.
 */

#ifndef DIGITIZER_H_TMDVZV
#define DIGITIZER_H_TMDVZV

#include "message_thread.h"
#include "data_types.h"
#include "data_processing.h"
#include "file_watcher.h"

#ifdef NO_ADQAPI
#include "mock_adqapi.h"
#else
#include "ADQAPI.h"
#endif

#include <array>

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
    {}

    /* Create a state message. */
    DigitizerMessage(DigitizerMessageId id, DigitizerState state)
        : id(id)
        , state(state)
        , str()
        , ivalue()
        , window_type()
    {}

    /* Create a string message. */
    DigitizerMessage(DigitizerMessageId id, std::shared_ptr<std::string> str)
        : id(id)
        , state()
        , str(str)
        , ivalue()
        , window_type()
    {}

    /* Create an integer message. */
    DigitizerMessage(DigitizerMessageId id, int ivalue)
        : id(id)
        , state()
        , str()
        , ivalue(ivalue)
        , window_type()
    {}

    /* Create a window message. */
    DigitizerMessage(DigitizerMessageId id, WindowType window_type)
        : id(id)
        , state()
        , str()
        , ivalue()
        , window_type(window_type)
    {}

    DigitizerMessageId id;
    DigitizerState state;
    std::shared_ptr<std::string> str;
    int ivalue;
    WindowType window_type;
};

struct SensorReading
{
    int id;
    std::string label;
    std::string unit;
    float value;
};

struct SensorGroupReadings
{
    int id;
    std::string label;
    std::map<int, SensorReading> sensors;
};

struct SensorReadings
{
    std::map<int, SensorGroupReadings> groups;
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

    /* Sensor readings. */
    ThreadSafeQueue<std::shared_ptr<SensorReadings>> m_sensor_readings;

    void ProcessMessages();
    void ProcessWatcherMessages();
    void ProcessWatcherMessages(const std::unique_ptr<FileWatcher> &watcher,
                                std::shared_ptr<std::string> &str,
                                DigitizerMessageId dirty_id);

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
};

#endif
