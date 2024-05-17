#pragma once

#include "thread_safe_queue.h"
#include <atomic>

/* A class wrapping two thread safe queue objects. A public interface for
   message passing is automatically provided to a derived class. The derived
   class is expected to interact with the queues using the protected functions
   prefixed by `_`. These move messages in the opposite direction. */

/* TODO: Should we attempt to move messages in `PushMessage`? */

template<typename T>
class MessageChannels
{
public:
    MessageChannels()
        : m_next_id{1}
        , m_read_message_queue{}
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

    /* Wait for a message, requiring a matching id. This function only returns
       messages that were pushed _with_ an id. */
    int WaitForMessage(T &message, int timeout, uint32_t id)
    {
        auto Predicate = [&](const StampedMessage &m) { return m.id > 0 && m.id == id; };
        return WaitForMessage(message, timeout, Predicate);
    }

    /* Wait for a message. This function only returns messages that were pushed
       _without_ an id.*/
    int WaitForMessage(T &message, int timeout)
    {
        auto Predicate = [&](const StampedMessage &m) { return m.id == 0; };
        return WaitForMessage(message, timeout, Predicate);
    }

    /* Push a message and receive a unique id. */
    int PushMessage(const T &message, uint32_t &id)
    {
        /* We get our 'unique' id by an atomic post-increment operation. If the
           id turns out to be zero, we kindly as for another one since we
           reserve that value to mean "no id". */
        id = m_next_id++;
        if (id == 0)
            id = m_next_id++;
        return m_write_message_queue.EmplaceWrite(id, message);
    }

    /* Push a message. */
    int PushMessage(const T &message)
    {
        return m_write_message_queue.EmplaceWrite(message);
    }

    /* Push a message then wait for a response as a single action. */
    int PushMessage(const T &in, T &out, int timeout)
    {
        /* TODO: Make this into id-matched calls. */
        RETURN_CALL(PushMessage(in));
        RETURN_CALL(WaitForMessage(out, timeout));
        return SCAPE_EOK;
    }

    /* Push a message then wait for and _discard_ the response as a single action. */
    int PushMessage(const T &in, int timeout)
    {
        /* TODO: Make this into id-matched calls. */
        T response;
        RETURN_CALL(PushMessage(in, response, timeout));
        return SCAPE_EOK;
    }

    /* Push a message that's constructed in place from the input arguments. */
    template<class... Args>
    int EmplaceMessage(Args &&... args)
    {
        return m_write_message_queue.EmplaceWrite(T(std::forward<Args>(args)...));
    }

protected:
    /* The inheriting class gets `StampedMessage` objects, which includes an `id`. */
    struct StampedMessage
    {
        StampedMessage() = default;
        StampedMessage(uint32_t id, const T &message)
            : contents{message}
            , id{id}
        {}

        StampedMessage(const T &message)
            : contents{message}
        {}

        T contents{};
        uint32_t id{0};
    };

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

    int _WaitForMessage(StampedMessage &message, int timeout)
    {
        return m_write_message_queue.Read(message, timeout);
    }

    int _WaitForMessage(T &message, int timeout)
    {
        /* TODO: For backwards compatibility until we can refactor consumers to
                 always respect the id? */
        StampedMessage tmp;
        int result = m_write_message_queue.Read(tmp, timeout);
        message = tmp.contents;
        return result;
    }

    int _PushMessage(const T &message)
    {
        return m_read_message_queue.EmplaceWrite(message);
    }

    int _PushMessage(const StampedMessage &message)
    {
        return m_read_message_queue.EmplaceWrite(message);
    }

    template<class... Args>
    int _EmplaceMessage(Args &&... args)
    {
        return m_read_message_queue.EmplaceWrite(T(std::forward<Args>(args)...));
    }

private:

    std::atomic_uint32_t m_next_id;
    ThreadSafeQueue<StampedMessage> m_read_message_queue;
    ThreadSafeQueue<StampedMessage> m_write_message_queue;

    int WaitForMessage(
        T &message, int timeout, std::function<bool(const StampedMessage &)> predicate)
    {
        StampedMessage stamped_message;
        int result = m_read_message_queue.Read(stamped_message, timeout, predicate);
        if (result == SCAPE_EOK)
            message = std::move(stamped_message.contents);
        return result;
    }
};
