/* This is a template class for a thread safe queue. The queue may have a finite
   capacity (infinite by default) and/or a 'persistent' behavior (disabled by
   default). In the persistent mode, the last value remains on the read port
   until there's a new value to present. Perhaps a niche feature, but it can be
   used to represent a state in the reading thread that's controlled by the
   writing thread. The queue also has an activity detection mechanism (measuring
   on the write port) that can be queried in a thread-safe manner.

   Additionally, a predicate can be supplied to `Read` for remove-if-style
   functionality. This can be used to implement traced queue entries on a higher
   level. */

#pragma once

#include "error.h"

#include <queue>
#include <mutex>
#include <future>
#include <chrono>
#include <functional>
#include <atomic>

template <typename T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue(size_t capacity = 0, bool is_persistent = false)
        : m_signal_stop()
        , m_should_stop()
        , m_is_started(false)
        , m_is_persistent(is_persistent)
        , m_mutex()
        , m_queue()
        , m_capacity(capacity)
        , m_last_write_timestamp()
    {
    }

    virtual ~ThreadSafeQueue()
    {
        Stop();
    }

    ThreadSafeQueue(const ThreadSafeQueue &other) = delete;
    ThreadSafeQueue &operator=(const ThreadSafeQueue &other) = delete;

    int Start()
    {
        if (m_is_started)
            return SCAPE_ENOTREADY;

        m_queue = {}; /* TODO: Not ok if we ever want to have stuff prequeued. */
        m_signal_stop = std::promise<void>();
        m_should_stop = m_signal_stop.get_future();
        m_last_write_timestamp = std::chrono::high_resolution_clock::now();
        m_is_started = true;
        return SCAPE_EOK;
    }

    int Stop()
    {
        if (!m_is_started)
            return SCAPE_ENOTREADY;

        m_signal_stop.set_value();
        m_is_started = false;
        return SCAPE_EOK;
    }

    /* Only compiles when <T> is a pointer type. */
    void Free()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_queue.empty())
        {
            delete m_queue.front();
            m_queue.pop();
        }
    }

    int Read(
        T &value, int timeout,
        std::function<bool(const T &)> predicate = [](const T &) { return true; })
    {
        if (!m_is_started)
            return SCAPE_ENOTREADY;

        if (timeout < 0)
        {
            /* Waiting indefinitely */
            for (;;)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_queue.size() > 0 && predicate(m_queue.front()))
                    return Pop(value);
                lock.unlock();

                if (m_should_stop.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready)
                    return SCAPE_EINTERRUPTED;
            }
        }
        else if (timeout == 0)
        {
            /* Immediate */
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_queue.size() > 0 && predicate(m_queue.front()))
                return Pop(value);
            else
                return SCAPE_EAGAIN;
        }
        else
        {
            int waited_ms = 0;
            while (waited_ms < timeout)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_queue.size() > 0 && predicate(m_queue.front()))
                    return Pop(value);
                lock.unlock();

                ++waited_ms;
                if (m_should_stop.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
                    return SCAPE_EINTERRUPTED;
            }

            return SCAPE_EAGAIN;
        }
    }

    int Write(const T &value, int timeout = 0)
    {
        if (!m_is_started)
            return SCAPE_ENOTREADY;

        std::unique_lock<std::mutex> lock(m_mutex);

        /* The timeout is only applicable if the queue has a finite capacity. */
        if (m_capacity > 0 && m_queue.size() >= m_capacity)
        {
            lock.unlock();

            if (timeout < 0)
            {
                /* Waiting indefinitely */
                for (;;)
                {
                    lock.lock();
                    if (m_queue.size() < m_capacity)
                    {
                        m_last_write_timestamp = std::chrono::high_resolution_clock::now();
                        m_queue.emplace(value);
                        return SCAPE_EOK;
                    }
                    lock.unlock();

                    if (m_should_stop.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready)
                        return SCAPE_EINTERRUPTED;
                }
            }
            else if (timeout == 0)
            {
                /* Immediate */
                return SCAPE_EAGAIN;
            }
            else
            {
                int waited_ms = 0;
                while (waited_ms < timeout)
                {
                    lock.lock();
                    if (m_queue.size() < m_capacity)
                    {
                        m_last_write_timestamp = std::chrono::high_resolution_clock::now();
                        m_queue.emplace(value);
                        return SCAPE_EOK;
                    }
                    lock.unlock();

                    ++waited_ms;
                    if (m_should_stop.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
                        return SCAPE_EINTERRUPTED;
                }

                return SCAPE_EAGAIN;
            }
        }

        m_last_write_timestamp = std::chrono::high_resolution_clock::now();
        m_queue.emplace(value);
        return SCAPE_EOK;
    }

    /* An `emplace_back`-style call, forwarding the arguments to a matching
       constructor for a new object of type `T`. */
    template<class... Args>
    int EmplaceWrite(Args &&... args)
    {
        return Write(T(std::forward<Args>(args)...), 0);
    }

    bool IsFull()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_capacity > 0 && m_queue.size() >= m_capacity;
    }

    bool IsEmpty()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    int GetTimeSinceLastActivity(int &milliseconds)
    {
        if (!m_is_started)
            return SCAPE_ENOTREADY;

        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::high_resolution_clock::now();
        auto delta_ms = static_cast<double>((now - m_last_write_timestamp).count()) / 1e6;
        milliseconds = static_cast<int>(delta_ms);
        return SCAPE_EOK;
    }

private:
    std::promise<void> m_signal_stop;
    std::future<void> m_should_stop;
    std::atomic_bool m_is_started;
    std::atomic_bool m_is_persistent;
    std::mutex m_mutex;
    std::queue<T> m_queue;
    size_t m_capacity;
    std::chrono::high_resolution_clock::time_point m_last_write_timestamp;

    int Pop(T &value)
    {
        value = m_queue.front();

        /* We only pop the entry if we're not using the persistent mode and if
           we do (tail condition), only if there's another entry in the queue.
           If we popped the entry in the persistent mode we return SCAPE_ELAST to
           signal that this happened. If the queue contains memory allocated on
           the heap, the reader may want to return the memory. */

        if (!m_is_persistent || (m_queue.size() > 1))
        {
            m_queue.pop();
            if (m_is_persistent)
                return SCAPE_ELAST;
        }

        return SCAPE_EOK;
    }
};
