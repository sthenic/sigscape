#ifndef THREAD_SAFE_QUEUE_H_V53B5L
#define THREAD_SAFE_QUEUE_H_V53B5L

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
        Initialize();
    }

    int Initialize()
    {
        m_queue = {};
        m_signal_stop = std::promise<void>();
        m_should_stop = m_signal_stop.get_future();
        m_is_started = false;
        return 0;
    }

    int Start()
    {
        if (m_is_started)
            return -3;

        m_is_started = true;
        return 0;
    }

    int Stop()
    {
        if (!m_is_started)
            return -3;

        m_signal_stop.set_value();
        m_is_started = false;
        return 0;
    }

    int Read(T &value, int timeout)
    {
        if (!m_is_started)
            return -3;

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
                    return 0;
                }
                lock.unlock();

                if (m_should_stop.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready)
                    return -2;
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
                return 0;
            }
            return -1;
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
                    return 0;
                }
                lock.unlock();

                ++waited_ms;
                if (m_should_stop.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
                    return -2;
            }

            return -1;
        }
    }

    int Write(T value)
    {
        if (!m_is_started)
            return -3;

        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push(value);
        return 0;
    }

private:
    std::promise<void> m_signal_stop;
    std::future<void> m_should_stop;
    bool m_is_started;
    std::mutex m_mutex;
    std::queue<T> m_queue;
};

#endif
