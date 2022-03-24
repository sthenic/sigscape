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
    ThreadSafeQueue()
        : m_signal_stop()
        , m_should_stop()
        , m_is_started(false)
        , m_mutex()
        , m_queue()
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
            /* Waiting indefinitely. */
            for (;;)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_queue.size() > 0)
                {
                    value = m_queue.front();
                    m_queue.pop();
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
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_queue.size() > 0)
            {
                value = m_queue.front();
                m_queue.pop();
                return ADQR_EOK;
            }
            return ADQR_EAGAIN;
        }
        else
        {
            int waited_ms = 0;
            while (waited_ms < timeout)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_queue.size() > 0)
                {
                    value = m_queue.front();
                    m_queue.pop();
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

    int Write(const T &value)
    {
        if (!m_is_started)
            return ADQR_ENOTREADY;

        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push(value);
        return ADQR_EOK;
    }

private:
    std::promise<void> m_signal_stop;
    std::future<void> m_should_stop;
    bool m_is_started;
    std::mutex m_mutex;
    std::queue<T> m_queue;
};

#endif
