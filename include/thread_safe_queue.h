/* This is a template class for a thread safe queue. The queue may have a finite
   capacity (infinite by default) and/or a 'persistent' behavior (disabled by
   default). In the persistent mode, the last value remains on the read port
   until there's a new value to present. Perhaps a niche feature, but it can be
   used to represent a state in the reading thread that's controlled by the
   writing thread. */

#ifndef THREAD_SAFE_QUEUE_H_V53B5L
#define THREAD_SAFE_QUEUE_H_V53B5L

#include "error.h"

#include <queue>
#include <mutex>
#include <future>

template <typename T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue(size_t capacity = 0, bool is_persistent = false)
        : m_signal_stop()
        , m_should_stop()
        , m_is_started(false)
        , m_mutex()
        , m_queue()
        , m_capacity(capacity)
        , m_is_persistent(is_persistent)
    {
    }

    int Start()
    {
        if (m_is_started)
            return ADQR_ENOTREADY;

        m_queue = {}; /* TODO: Not ok if we ever want to have stuff prequeued. */
        m_signal_stop = std::promise<void>();
        m_should_stop = m_signal_stop.get_future();
        m_is_started = true;
        return ADQR_EOK;
    }

    int Stop()
    {
        if (!m_is_started)
            return ADQR_ENOTREADY;

        m_signal_stop.set_value();
        m_is_started = false;
        return ADQR_EOK;
    }

    int Read(T &value, int timeout)
    {
        if (!m_is_started)
            return ADQR_ENOTREADY;

        if (timeout < 0)
        {
            /* Waiting indefinitely */
            for (;;)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_queue.size() > 0)
                    return Pop(value);
                lock.unlock();

                if (m_should_stop.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready)
                    return ADQR_EINTERRUPTED;
            }
        }
        else if (timeout == 0)
        {
            /* Immediate */
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_queue.size() > 0)
                return Pop(value);
            else
                return ADQR_EAGAIN;
        }
        else
        {
            int waited_ms = 0;
            while (waited_ms < timeout)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_queue.size() > 0)
                    return Pop(value);
                lock.unlock();

                ++waited_ms;
                if (m_should_stop.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
                    return ADQR_EINTERRUPTED;
            }

            return ADQR_EAGAIN;
        }
    }

    int Write(const T &value, int timeout = 0)
    {
        if (!m_is_started)
            return ADQR_ENOTREADY;

        std::unique_lock<std::mutex> lock(m_mutex);

        /* The timeout is only applicable if the queue has a finite capacity. */
        if ((m_capacity > 0) && (m_queue.size() >= m_capacity))
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
                        m_queue.push(value);
                        return ADQR_EOK;
                    }
                    lock.unlock();

                    if (m_should_stop.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready)
                        return ADQR_EINTERRUPTED;
                }
            }
            else if (timeout == 0)
            {
                /* Immediate */
                return ADQR_EAGAIN;
            }
            else
            {
                int waited_ms = 0;
                while (waited_ms < timeout)
                {
                    lock.lock();
                    if (m_queue.size() < m_capacity)
                    {
                        m_queue.push(value);
                        return ADQR_EOK;
                    }
                    lock.unlock();

                    ++waited_ms;
                    if (m_should_stop.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
                        return ADQR_EINTERRUPTED;
                }

                return ADQR_EAGAIN;
            }
        }

        m_queue.push(value);
        return ADQR_EOK;
    }

private:
    std::promise<void> m_signal_stop;
    std::future<void> m_should_stop;
    bool m_is_started;
    std::mutex m_mutex;
    std::queue<T> m_queue;
    size_t m_capacity;
    bool m_is_persistent;

    int Pop(T &value)
    {
        value = m_queue.front();
        /* We only pop the entry if we're not using the persistent mode and if
           we do (tail condition), only if there's another entry in the queue. */
        if (!m_is_persistent || (m_queue.size() > 1))
            m_queue.pop();
        return ADQR_EOK;
    }
};

#endif
