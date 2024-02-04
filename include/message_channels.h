#pragma once

#include "thread_safe_queue.h"

/* A class wrapping two thread safe queue objects. A public interface for
   message passing is automatically provided to a derived class. The derived
   class is expected to interact with the queues using the protected functions
   prefixed by `_`. These move messages in the opposite direction. */

template<typename T>
class MessageChannels
{
public:
    MessageChannels()
        : m_read_message_queue{}
        , m_write_message_queue{}
    {
        /* Message channels are always running, unless stopped manually by the
           derived class. */
        StartMessageChannels();
    }

    virtual ~MessageChannels()
    {
        StopMessageChannels();
    }

    /* Wait for a message. */
    int WaitForMessage(T &message, int timeout)
    {
        return m_read_message_queue.Read(message, timeout);
    }

    /* Push a message. */
    int PushMessage(const T &message)
    {
        return m_write_message_queue.Write(message);
    }

    /* Push a message and wait for a response as a single action. */
    int PushMessage(const T &in, T &out, int timeout)
    {
        RETURN_CALL(PushMessage(in));
        RETURN_CALL(WaitForMessage(out, timeout));
        return SCAPE_EOK;
    }

    /* Push a message that's constructed in place from the input arguments. */
    template<class... Args>
    int EmplaceMessage(Args &&... args)
    {
        return m_write_message_queue.EmplaceWrite(std::forward<Args>(args)...);
    }

protected:
    int StartMessageChannels()
    {
        RETURN_CALL(m_read_message_queue.Start());
        RETURN_CALL(m_write_message_queue.Start());
        return SCAPE_EOK;
    }

    int StopMessageChannels()
    {
        RETURN_CALL(m_read_message_queue.Stop());
        RETURN_CALL(m_write_message_queue.Stop());
        return SCAPE_EOK;
    }

    int _WaitForMessage(T &message, int timeout)
    {
        return m_write_message_queue.Read(message, timeout);
    }

    int _PushMessage(const T &message)
    {
        return m_read_message_queue.Write(message);
    }

    template<class... Args>
    int _EmplaceMessage(Args &&... args)
    {
        return m_read_message_queue.EmplaceWrite(std::forward<Args>(args)...);
    }

private:
    ThreadSafeQueue<T> m_read_message_queue;
    ThreadSafeQueue<T> m_write_message_queue;
};
