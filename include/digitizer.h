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
#define ADQ_MAX_NOF_CHANNELS 8
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
    Digitizer()
        : MessageThread()
        , m_state(DigitizerState::NOT_ENUMERATED)
        , m_watchers{}
        , m_parameters{}
        , m_processing_threads{}
    {
    }

    /* Interface to the digitizer's data processing threads, one per channel. */
    int WaitForProcessedRecord(int channel, std::shared_ptr<ProcessedRecord> &record)
    {
        if ((channel < 0) || (channel > ADQ_MAX_NOF_CHANNELS))
            return ADQR_EINVAL;

        return m_processing_threads[channel]->WaitForBuffer(record, 0);
    }

    /* TODO: We can probably do the same generalization for the MessageThread
             with a persistent read queue and expose that directly. */
    int WaitForParameters(std::shared_ptr<std::string> &str)
    {
        return m_parameters.top->Read(str, 0);
    }

    int WaitForClockSystemParameters(std::shared_ptr<std::string> &str)
    {
        return m_parameters.clock_system->Read(str, 0);
    }

protected:
    /* The digitizer's state. */
    enum DigitizerState m_state;

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

private:
    void ProcessWatcherMessages(const std::unique_ptr<FileWatcher> &watcher,
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
};

#endif
