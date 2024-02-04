/* This design is based on CRTP (https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern). */

#pragma once

#include "thread_safe_queue.h"
#include "message_channels.h"

#include <thread>
#include <future>
#include <map>

template <class C, typename T, typename M,  size_t CAPACITY = 0, bool PERSISTENT = false, bool PRESERVE = false>
class SmartBufferThread : public MessageChannels<M>
{
public:
    SmartBufferThread()
        : m_thread()
        , m_signal_stop()
        , m_should_stop()
        , m_is_running(false)
        , m_thread_exit_code(SCAPE_EINTERRUPTED)
        , m_mutex{}
        , m_preserved_buffers{}
        , m_read_queue(CAPACITY, PERSISTENT)
    {};

    virtual ~SmartBufferThread()
    {
        Stop();
    }

    /* Delete copy constructors. */
    SmartBufferThread(const SmartBufferThread &other) = delete;
    SmartBufferThread &operator=(const SmartBufferThread &other) = delete;

    virtual int Start()
    {
        if (m_is_running)
            return SCAPE_ENOTREADY;

        m_read_queue.Start();
        m_signal_stop = std::promise<void>();
        m_should_stop = m_signal_stop.get_future();
        m_thread = std::thread([this]{ static_cast<C*>(this)->MainLoop(); });
        m_is_running = true;
        return SCAPE_EOK;
    }

    virtual int Stop()
    {
        if (!m_is_running)
            return SCAPE_ENOTREADY;

        m_read_queue.Stop();
        m_signal_stop.set_value();
        m_thread.join();
        m_preserved_buffers.clear();
        m_is_running = false;
        return m_thread_exit_code;
    }

    /* We provide a default implementation of the outward facing queue interface. */
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
    std::thread m_thread;
    std::promise<void> m_signal_stop;
    std::future<void> m_should_stop;
    bool m_is_running;
    int m_thread_exit_code;
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

    virtual void MainLoop() = 0;
};
