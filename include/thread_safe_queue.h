#ifndef THREAD_SAFE_QUEUE_H_V53B5L
#define THREAD_SAFE_QUEUE_H_V53B5L

#include <queue>
#include <mutex>
#include <future>

template <typename T>
class ThreadSafeQueue
{
public:
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
                m_mutex.lock();
                if (m_queue.size() > 0)
                {
                    value = m_queue.front();
                    m_queue.pop();
                    m_mutex.unlock();
                    return 0;
                }
                m_mutex.unlock();

                if (m_should_stop.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
                    return -2;
            }
        }
        else if (timeout == 0)
        {
            /* Immediate */
            m_mutex.lock();
            if (m_queue.size() > 0)
            {
                value = m_queue.front();
                m_queue.pop();
                m_mutex.unlock();
                return 0;
            }
            m_mutex.unlock();
            return -1;
        }
        else
        {
            int waited_ms = 0;
            while (waited_ms < timeout)
            {
                m_mutex.lock();
                if (m_queue.size() > 0)
                {
                    value = m_queue.front();
                    m_queue.pop();
                    m_mutex.unlock();
                    return 0;
                }
                m_mutex.unlock();

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

        m_mutex.lock();
        m_queue.push(value);
        m_mutex.unlock();
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
