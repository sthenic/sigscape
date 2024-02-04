/* This design is based on CRTP (https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern). */

#pragma once

#include "thread_safe_queue.h"
#include "message_channels.h"

#include <thread>
#include <future>

template<class C, typename T>
class MessageThread : public MessageChannels<T>
{
public:
    MessageThread()
        : m_thread()
        , m_signal_stop()
        , m_should_stop()
        , m_is_running(false)
        , m_thread_exit_code(SCAPE_EINTERRUPTED)
    {};

    virtual ~MessageThread()
    {
        Stop();
    };

    /* Delete copy constructors. */
    MessageThread(const MessageThread &other) = delete;
    MessageThread &operator=(const MessageThread &other) = delete;

    /* Start the thread. */
    virtual int Start()
    {
        if (m_is_running)
            return SCAPE_ENOTREADY;

        m_signal_stop = std::promise<void>();
        m_should_stop = m_signal_stop.get_future();
        m_thread = std::thread([this]{ static_cast<C*>(this)->MainLoop(); });
        m_is_running = true;
        return SCAPE_EOK;
    }

    /* Stop the thread. */
    virtual int Stop()
    {
        if (!m_is_running)
            return SCAPE_ENOTREADY;

        /* We have to stop the message channels to be sure that we can join the
           thread. However, once the thread has finished, we want to restart the
           channels since we'd like to be able to queue up messages to the
           thread while it's not running. */
        this->StopMessageChannels();
        m_signal_stop.set_value();

        m_thread.join();
        m_is_running = false;

        this->StartMessageChannels();
        return m_thread_exit_code;
    }

protected:
    std::thread m_thread;
    std::promise<void> m_signal_stop;
    std::future<void> m_should_stop;
    bool m_is_running;
    int m_thread_exit_code;

    /* The thread's main loop. This function must be implemented by the derived class. */
    virtual void MainLoop() = 0;
};
