/*
 * An abstract base class that defines the interface of a digitizer.
 */

#ifndef DIGITIZER_H_TMDVZV
#define DIGITIZER_H_TMDVZV

#include "message_thread.h"
#include "data_types.h"

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
    /* FIXME: Delete default constructor? */
    Digitizer()
        : MessageThread()
        , m_state(STATE_NOT_ENUMERATED)
        , m_processed_record_queue(100, true)
    {}

    ~Digitizer()
    {
        Stop();
    }

    /* Delete copy constructors. */
    Digitizer(const Digitizer &) = delete;
    Digitizer &operator=(const Digitizer &) = delete;

    virtual int Start() override
    {
        int result = m_processed_record_queue.Start();
        if (result != ADQR_EOK)
            return result;
        return MessageThread::Start();
    }

    virtual int Stop() override
    {
        /* FIXME: Error code capture and propagation. */
        m_processed_record_queue.Stop();
        m_processed_record_queue.Free();
        MessageThread::Stop();
        return ADQR_EOK;
    }

    int WaitForProcessedRecord(struct ProcessedRecord *&record)
    {
        return m_processed_record_queue.Read(record, 0);
    }

protected:
    /* TODO: Perhaps write a file watcher w/ C++17's std::filesystem:
       https://solarianprogrammer.com/2019/01/13/cpp-17-filesystem-write-file-watcher-monitor/ */

    enum DigitizerState m_state;
    /* FIXME: Need arrays for each channel. */
    ThreadSafeQueue<struct ProcessedRecord *> m_processed_record_queue;
};

#endif
