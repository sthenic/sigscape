#pragma once

#include "message_thread.h"
#include <map>

/* This class extends the `MessageThread` class with dynamic memory management
   for objects of type `T` and channel-like interface to receive and return
   these objects. The assumption is that the derived class has a need to
   continuously create heap-allocated objects of type `T` and emit these to the
   outside world. */

template <typename T, typename M, size_t CAPACITY = 0, bool PERSISTENT = false, bool PRESERVE = false>
class SmartBufferThread
    : public MessageThread<SmartBufferThread<T, M, CAPACITY, PERSISTENT, PRESERVE>, M>
{
public:
    SmartBufferThread()
        : m_mutex{}
        , m_preserved_buffers{}
        , m_read_queue{CAPACITY, PERSISTENT}
    {};

    virtual ~SmartBufferThread()
    {
        Stop();
    }

    /* Delete copy constructors. */
    SmartBufferThread(const SmartBufferThread &other) = delete;
    SmartBufferThread &operator=(const SmartBufferThread &other) = delete;

    /* We provide a default implementation of the outward facing interface. */
    virtual int Start() override
    {
        m_read_queue.Start();
        return MessageThread<SmartBufferThread<T, M, CAPACITY, PERSISTENT, PRESERVE>, M>::Start();
    }

    virtual int Stop() override
    {
        m_read_queue.Stop();
        int result =
            MessageThread<SmartBufferThread<T, M, CAPACITY, PERSISTENT, PRESERVE>, M>::Stop();
        m_preserved_buffers.clear();
        return result;
    }

    virtual int WaitForBuffer(std::shared_ptr<T> &buffer, int timeout)
    {
        return m_read_queue.Read(buffer, timeout);
    }

    virtual int ReturnBuffer(const T *buffer)
    {
        if (PRESERVE)
        {
            /* If the buffer has been kept alive through the preservation
               mechanism, we remove it from the set of tracked buffers. Ideally
               this brings the use count to zero and the memory is freed. */
            std::unique_lock<std::mutex> lock{m_mutex};
            if (m_preserved_buffers.erase(buffer) == 0)
                return SCAPE_EINVAL;
        }

        return SCAPE_EOK;
    }

    virtual int ReturnBuffer(std::shared_ptr<T> buffer)
    {
        /* We maintain a wait/return interface to potentially reuse memory
           manually in the future. For now though, we always allocate/free and
           let the OS handle the rest. As far as this function is concerned that
           means just letting the use count drop to zero after removing it from
           any active tracking. */
        return ReturnBuffer(buffer.get());
    }

    int GetTimeSinceLastActivity(int &milliseconds)
    {
        return m_read_queue.GetTimeSinceLastActivity(milliseconds);
    }

protected:
    std::mutex m_mutex;
    std::map<const T *, std::shared_ptr<T>> m_preserved_buffers;
    ThreadSafeQueue<std::shared_ptr<T>> m_read_queue;

    int ReuseOrAllocateBuffer(std::shared_ptr<T> &buffer, size_t count)
    {
        try
        {
            /* Always allocate for now. */
            buffer = std::make_shared<T>(count);

            if (PRESERVE)
            {
                std::unique_lock<std::mutex> lock{m_mutex};
                m_preserved_buffers.emplace(buffer.get(), buffer);
            }

            return SCAPE_EOK;
        }
        catch (const std::bad_alloc &)
        {
            return SCAPE_EINTERNAL;
        }
    }
};
