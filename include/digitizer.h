/*
 * An abstract base class that defines the interface of a digitizer.
 */

#ifndef DIGITIZER_H_TMDVZV
#define DIGITIZER_H_TMDVZV

#include "message_thread.h"
#include "data_types.h"
#include "ADQAPI.h"

#include <array>

enum DigitizerMessageId
{
    MESSAGE_ID_SETUP_STARTING,
    MESSAGE_ID_SETUP_OK,
    MESSAGE_ID_SETUP_FAILED,
    MESSAGE_ID_PARAMETER_SET_UPDATED_FROM_FILE,
    MESSAGE_ID_PARAMETER_SET_UPDATED_FROM_GUI, /* FIXME: Maybe we keep it read only in the GUI. */
    MESSAGE_ID_START_ACQUISITION,
    MESSAGE_ID_STOP_ACQUISITION,
    MESSAGE_ID_NEW_STATE
};

enum DigitizerState
{
    STATE_NOT_ENUMERATED,
    STATE_INITIALIZATION,
    STATE_CONFIGURATION,
    STATE_ACQUISITION
};

struct DigitizerMessage
{
    enum DigitizerMessageId id;
    int status;
    void *data;
};

class Digitizer : public MessageThread<Digitizer, struct DigitizerMessage>
{
public:
    Digitizer()
        : MessageThread()
        , m_state(STATE_NOT_ENUMERATED)
        , m_processed_record_queue{}
    {
        for (int i = 0; i < ADQ_MAX_NOF_CHANNELS; ++i)
        {
            m_processed_record_queue.push_back(
                std::make_unique<ThreadSafeQueue<std::shared_ptr<struct ProcessedRecord>>>(100, true));
        }
    }

    /* FIXME: Needed? */
    ~Digitizer()
    {
        Stop();
    }

    /* Delete copy constructors. */
    Digitizer(const Digitizer &) = delete;
    Digitizer &operator=(const Digitizer &) = delete;

    virtual int Start() override
    {
        for (auto &queue : m_processed_record_queue)
        {
            int result = queue->Start();
            if (result != ADQR_EOK)
                return result;
        }

        return MessageThread::Start();
    }

    virtual int Stop() override
    {
        /* FIXME: Error code capture and propagation. */
        for (auto &queue : m_processed_record_queue)
            queue->Stop();

        MessageThread::Stop();
        return ADQR_EOK;
    }

    int WaitForProcessedRecord(int channel, std::shared_ptr<struct ProcessedRecord> &record)
    {
        if ((channel < 0) || (channel > ADQ_MAX_NOF_CHANNELS))
            return ADQR_EINVAL;

        return m_processed_record_queue[channel]->Read(record, 0);
    }

protected:
    /* TODO: Perhaps write a file watcher w/ C++17's std::filesystem:
       https://solarianprogrammer.com/2019/01/13/cpp-17-filesystem-write-file-watcher-monitor/ */

    enum DigitizerState m_state;

    std::vector<
        std::unique_ptr<ThreadSafeQueue<std::shared_ptr<struct ProcessedRecord>>>
    > m_processed_record_queue;
};

#endif
