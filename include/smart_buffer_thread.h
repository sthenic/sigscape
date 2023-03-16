/* This design is based on CRTP (https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern). */

#ifndef SMART_BUFFER_THREAD_H_69EMSR
#define SMART_BUFFER_THREAD_H_69EMSR

#include "thread_safe_queue.h"
#include "error.h"

#include <thread>
#include <future>

template <class C, typename T, size_t CAPACITY = 0, bool PERSISTENT = false>
class SmartBufferThread
{
public:
    SmartBufferThread()
        : m_thread()
        , m_signal_stop()
        , m_should_stop()
        , m_is_running(false)
        , m_thread_exit_code(SCAPE_EINTERRUPTED)
        , m_nof_buffers_max(100)
        , m_nof_buffers(0)
        , m_read_queue(CAPACITY, PERSISTENT)
        , m_write_queue()
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

        m_write_queue.Start();
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

        m_write_queue.Stop();
        m_read_queue.Stop();
        m_signal_stop.set_value();
        m_thread.join();
        m_is_running = false;
        return m_thread_exit_code;
    }

    /* We provide a default implementation of the outward facing queue interface. */
    virtual int WaitForBuffer(std::shared_ptr<T> &buffer, int timeout)
    {
        return m_read_queue.Read(buffer, timeout);
    }

    virtual int ReturnBuffer(std::shared_ptr<T> buffer)
    {
        return m_write_queue.Write(buffer);
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
    size_t m_nof_buffers_max;
    size_t m_nof_buffers;

    ThreadSafeQueue<std::shared_ptr<T>> m_read_queue;
    ThreadSafeQueue<std::shared_ptr<T>> m_write_queue;

    int AllocateBuffer(std::shared_ptr<T> &buffer, size_t count)
    {
        try
        {
            buffer = std::make_shared<T>(count);
        }
        catch (const std::bad_alloc &)
        {
            return SCAPE_EINTERNAL;
        }

        ++m_nof_buffers;
        return SCAPE_EOK;
    }

    int ReuseOrAllocateBuffer(std::shared_ptr<T> &buffer, size_t count)
    {
        /* We prioritize reusing existing memory over allocating new. */
        if (m_write_queue.Read(buffer, 0) == SCAPE_EOK)
        {
            return SCAPE_EOK;
        }
        else
        {
            if (m_nof_buffers < m_nof_buffers_max)
                return AllocateBuffer(buffer, count);
            else
                return m_write_queue.Read(buffer, -1);
        }
    }

    virtual void MainLoop() = 0;
};

#endif
