/*
 * An abstract base class that defines the interface of a digitizer.
 */

#ifndef DIGITIZER_H_TMDVZV
#define DIGITIZER_H_TMDVZV

#include "message_thread.h"
#include "data_types.h"
#include "data_processing.h"
#include "file_watcher.h"

#ifdef SIMULATION_ONLY
#include "mock_adqapi.h"
#else
#include "ADQAPI.h"
#endif

#include <array>

enum class DigitizerMessageId
{
    ENUMERATING,
    SETUP_OK,
    SETUP_FAILED,
    START_ACQUISITION,
    STOP_ACQUISITION,
    SET_PARAMETERS,
    GET_PARAMETERS,
    VALIDATE_PARAMETERS,
    INITIALIZE_PARAMETERS,
    NEW_STATE
};

enum class DigitizerState
{
    NOT_ENUMERATED,
    INITIALIZATION,
    CONFIGURATION,
    ACQUISITION
};

struct DigitizerMessage
{
    /* Default constructor (for receiving messages). */
    DigitizerMessage() = default;

    /* Create an empty message. */
    DigitizerMessage(DigitizerMessageId id)
        : id(id)
    {};

    /* Create a state message. */
    DigitizerMessage(DigitizerMessageId id, DigitizerState state)
        : id(id)
        , state(state)
    {}

    /* Create a string message. */
    DigitizerMessage(DigitizerMessageId id, std::shared_ptr<std::string> str)
        : id(id)
        , str(str)
    {}

    DigitizerMessageId id;
    DigitizerState state;
    std::shared_ptr<std::string> str;
};

class Digitizer : public MessageThread<Digitizer, struct DigitizerMessage>
{
public:
    Digitizer(void *handle, int index);
    ~Digitizer();

    /* Making copies of an object of this type is not allowed. */
    Digitizer(const Digitizer &other) = delete;
    Digitizer &operator=(const Digitizer &other) = delete;

    /* Interface to the digitizer's data processing threads, one per channel. */
    int WaitForProcessedRecord(int channel, std::shared_ptr<ProcessedRecord> &record);

    /* Interface to the digitizer's parameters represented as strings. */
    int WaitForParameters(std::shared_ptr<std::string> &str);
    int WaitForClockSystemParameters(std::shared_ptr<std::string> &str);

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

    typedef ThreadSafeQueue<std::shared_ptr<std::string>> ParameterQueue;
    struct
    {
        std::unique_ptr<ParameterQueue> top;
        std::unique_ptr<ParameterQueue> clock_system;
    } m_parameters;

    void ProcessWatcherMessages()
    {
        ProcessWatcherMessages(m_watchers.top, m_parameters.top);
        ProcessWatcherMessages(m_watchers.clock_system, m_parameters.clock_system);
    }

    /* The digitizer's data processing threads, one per channel. */
    std::vector<std::unique_ptr<DataProcessing>> m_processing_threads;

    void ProcessMessages();
    void ProcessWatcherMessages(const std::unique_ptr<FileWatcher> &watcher,
                                const std::unique_ptr<ParameterQueue> &queue);

    int SetParameters();
    void InitializeFileWatchers(const struct ADQConstantParameters &constant);
};

#endif
