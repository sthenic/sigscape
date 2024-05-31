#pragma once

#include "thread_safe_queue.h"
#include <atomic>

/* A class wrapping two thread safe queue objects. A public interface for
   message passing is automatically provided to a derived class. The derived
   class is expected to interact with the queues using the protected functions
   prefixed by `_`. These move messages in the opposite direction.

   The template class works with `StampedMessage` objects in the internal
   queues. These objects attach a (for practical purposes) unique `id` to a
   message that's passed with that intent through the public interface. This
   intent is signaled by the caller asking to receive the `id`, usually
   expecting a response with a matching `id`. Otherwise, `id` is set to zero,
   which symbolizes an "untraced" message that simply passes in one direction.

   The protected interface provides functions to read stamped messages
   (unconditionally), or _untraced_ messages (discarding the `id`). The idea is
   that the derived class will primarily use one of these functions to receive
   messages, opting to use the stamped interface if a call/response (or
   async/await) mechanism is needed. */

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

    /* Push a message (traced) and receive a unique id. */
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

    /* Push a message (untraced). */
    int PushMessage(const T &message)
    {
        return m_write_message_queue.EmplaceWrite(message);
    }

    /* Push a message (traced) then wait for a response as a single action. In
       the event of a timeout when waiting for the response, the `id` is
       returned to the user. */
    int PushMessageWaitForResponse(const T &in, T &out, uint32_t &id, int timeout)
    {
        RETURN_CALL(PushMessage(in, id));
        RETURN_CALL(WaitForMessage(out, timeout, id));
        return SCAPE_EOK;
    }

    /* Push a message (traced) then wait for a response as a single action. This
       call blocks until complete. */
    int PushMessageWaitForResponse(const T &in, T &out)
    {
        uint32_t id{};
        RETURN_CALL(PushMessageWaitForResponse(in, out, id, -1));
        return SCAPE_EOK;
    }

    /* Push a message (traced) then wait for and _discard_ the response as a single action. */
    int PushMessageWaitForResponse(const T &in)
    {
        T out{};
        RETURN_CALL(PushMessageWaitForResponse(in, out));
        return SCAPE_EOK;
    }

    /* Push a message (untraced) that's constructed in place from the input arguments. */
    template<class... Args>
    int EmplaceMessage(Args &&... args)
    {
        /* TODO: Move here? */
        return m_write_message_queue.EmplaceWrite(T(std::forward<Args>(args)...));
    }

    /* Push a message (traced) that's constructed in place from the input arguments. */
    template<class... Args>
    int EmplaceMessage(uint32_t id, Args &&... args)
    {
        /* TODO: Move here? */
        return m_write_message_queue.EmplaceWrite(id, T(std::forward<Args>(args)...));
    }

protected:
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

        StampedMessage(uint32_t id)
            : id{id}
        {}

        template<class... Args>
        StampedMessage(uint32_t id, Args &&... args)
            : contents(std::forward<Args>(args)...)
            , id{id}
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
        StampedMessage tmp;
        auto Predicate = [&](const StampedMessage &m) { return m.id == 0; };
        int result = m_write_message_queue.Read(tmp, timeout, Predicate);
        if (result == SCAPE_EOK)
            message = std::move(tmp.contents);
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

    template<class... Args>
    int _EmplaceMessage(uint32_t id, Args &&... args)
    {
        return m_read_message_queue.EmplaceWrite(id, T(std::forward<Args>(args)...));
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
