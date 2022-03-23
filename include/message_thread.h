/* This design is based on CRTP (https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern). */

#ifndef MESSAGE_THREAD_H_GXBSCP
#define MESSAGE_THREAD_H_GXBSCP

#include "thread_safe_queue.h"

#include <thread>
#include <future>

template<class C, typename T>
class MessageThread
{
public:
    MessageThread()
        : m_thread()
        , m_signal_stop()
        , m_should_stop()
        , m_is_running(false)
        , m_thread_exit_code(-1)
        , m_read_queue()
        , m_write_queue()
    {};

    virtual ~MessageThread() {};

    virtual int Start()
    {
        if (m_is_running)
            return -1;

        m_write_queue.Start();
        m_read_queue.Start();
        m_signal_stop = std::promise<void>();
        m_should_stop = m_signal_stop.get_future();
        m_thread = std::thread([this]{ static_cast<C*>(this)->MainLoop(); });
        m_is_running = true;
        return 0;
    }

    virtual int Stop()
    {
        if (!m_is_running)
            return -1;

        m_write_queue.Stop();
        m_read_queue.Stop();
        m_signal_stop.set_value();
        m_thread.join();
        m_is_running = false;
        return m_thread_exit_code;
    }

    int WaitForMessage(T &message, int timeout)
    {
        return m_read_queue.Read(message, timeout);
    }

    int PostMessage(const T &message)
    {
        return m_write_queue.Write(message);
    }

protected:
    std::thread m_thread;
    std::promise<void> m_signal_stop;
    std::future<void> m_should_stop;
    bool m_is_running;
    int m_thread_exit_code;

    ThreadSafeQueue<T> m_read_queue;
    ThreadSafeQueue<T> m_write_queue;

    virtual void MainLoop() = 0;
};

#endif