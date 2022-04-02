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
    SETUP_STARTING,
    SETUP_OK,
    SETUP_FAILED,
    PARAMETERS_UPDATED,
    START_ACQUISITION,
    STOP_ACQUISITION,
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
        , m_file_watcher()
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

protected:
    /* TODO: Perhaps write a file watcher w/ C++17's std::filesystem:
       https://solarianprogrammer.com/2019/01/13/cpp-17-filesystem-write-file-watcher-monitor/ */

    /* The digitizer's state. */
    enum DigitizerState m_state;

    /* File watcher to watch the digitizer's configuration file. */
    std::unique_ptr<FileWatcher> m_file_watcher;

    /* The digitizer's data processing threads, one per channel. */
    std::vector<std::unique_ptr<DataProcessing>> m_processing_threads;
};

#endif
