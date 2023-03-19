/* This design is based on CRTP (https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern). */

#pragma once

#include "thread_safe_queue.h"
#include "error.h"

#include <thread>
#include <future>

template <class C, typename T, size_t CAPACITY = 0, bool PERSISTENT = false>
class BufferThread
{
public:
    BufferThread()
        : m_thread()
        , m_signal_stop()
        , m_should_stop()
        , m_is_running(false)
        , m_thread_exit_code(SCAPE_EINTERRUPTED)
        , m_nof_buffers_max(100)
        , m_mutex()
        , m_read_queue(CAPACITY, PERSISTENT)
        , m_write_queue()
        , m_buffers()
    {};

    virtual ~BufferThread()
    {
        Stop();
    }

    /* Delete copy constructors. */
    BufferThread(const BufferThread &other) = delete;
    BufferThread &operator=(const BufferThread &other) = delete;

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
        FreeBuffers();
        m_is_running = false;
        return m_thread_exit_code;
    }

    /* We provide a default implementation of the outward facing queue interface. */
    virtual int WaitForBuffer(T *&buffer, int timeout)
    {
        return m_read_queue.Read(buffer, timeout);
    }

    virtual int ReturnBuffer(T *buffer)
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

    std::mutex m_mutex;
    ThreadSafeQueue<T*> m_read_queue;
    ThreadSafeQueue<T*> m_write_queue;
    std::vector<T*> m_buffers;

    int AllocateBuffer(T *&buffer, size_t count)
    {
        try
        {
            buffer = new T(count);
        }
        catch (const std::bad_alloc &)
        {
            return SCAPE_EINTERNAL;
        }

        /* Add a reference to the data storage. */
        m_mutex.lock();
        m_buffers.push_back(buffer);
        m_mutex.unlock();
        return SCAPE_EOK;
    }

    int ReuseOrAllocateBuffer(T *&buffer, size_t count)
    {
        /* We prioritize reusing existing memory over allocating new. */
        if (m_write_queue.Read(buffer, 0) == SCAPE_EOK)
        {
            return SCAPE_EOK;
        }
        else
        {
            m_mutex.lock();
            size_t nof_buffers = m_buffers.size();
            m_mutex.unlock();
            if (nof_buffers < m_nof_buffers_max)
                return AllocateBuffer(buffer, count);
            else
                return m_write_queue.Read(buffer, -1);
        }
    }

    void FreeBuffers()
    {
        m_mutex.lock();
        for (auto &b : m_buffers)
            delete b;
        m_buffers.clear();
        m_mutex.unlock();
    }

    virtual void MainLoop() = 0;
};
