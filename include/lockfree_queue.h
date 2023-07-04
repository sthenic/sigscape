#pragma once

/* FIXME: Give credit */

#include "error.h"

#include <vector>
#include <atomic>
#include <future>
#include <chrono>
#include <limits>
#include <new> /* For std::hardware_destructive_interference_size */

template <typename T>
class LockfreeQueue
{
public:
    /* Set the default capacity to 2^16 entries. */
    LockfreeQueue(size_t capacity = 1u << 16, bool is_persistent = false)
        : m_signal_stop()
        , m_should_stop()
        , m_is_started(false)
        , m_data()
        , m_capacity(capacity)
        , m_is_persistent(is_persistent)
        , m_head(0)
        , m_tail(0)
        , m_last_write_timestamp()
    {
        /* FIXME: Aligned malloc really that much faster? */
        m_data = std::unique_ptr<Entry[]>( new Entry[m_capacity] );
    }

    virtual ~LockfreeQueue()
    {
        Stop();
    }

    LockfreeQueue(const LockfreeQueue &other) = delete;
    LockfreeQueue &operator=(const LockfreeQueue &other) = delete;

    int Start()
    {
        if (m_is_started)
            return SCAPE_ENOTREADY;

        // m_data = std::vector<T>(m_capacity, 0); /* TODO: Not ok if we ever want to have stuff prequeued. */

        /* FIXME: A condition variable instead? */
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
        /* FIXME: Implement */
        // std::unique_lock<std::mutex> lock(m_mutex);
        // while (!m_queue.empty())
        // {
        //     delete m_queue.front();
        //     m_queue.pop();
        // }
    }

    int Read(T &value, int timeout)
    {
        (void)timeout;
        const auto tail = m_tail.fetch_add(1);
        auto &entry = m_data[tail % m_capacity];
        while (read_ticket(tail) != entry.ticket.load(std::memory_order_acquire))
            ;
        value = std::move(reinterpret_cast<T &&>(entry.data)); /* FIXME: Leaking? */
        entry.ticket.store(read_ticket(tail) + 1, std::memory_order_release);
        return SCAPE_EOK;
    }

    int Read2(T &value, int timeout)
    {
        (void)timeout;
        auto tail = m_tail.load(std::memory_order_acquire);

        for (;;)
        {
            auto &entry = m_data[tail % m_capacity];
            // const auto ticket = entry.ticket.load(std::memory_order_acquire);
            // printf("Want to read tail %zu ticket is %zu \n", tail, ticket);
            if (read_ticket(tail) == entry.ticket.load(std::memory_order_acquire))
            {
                if (m_tail.compare_exchange_strong(tail, tail + 1, std::memory_order_relaxed))
                {
                    value = std::move(reinterpret_cast<T &&>(entry.data));
                    // printf("Stamping ticket %zu\n", read_ticket(tail) + 1);
                    entry.ticket.store(read_ticket(tail) + 1, std::memory_order_release);
                    return SCAPE_EOK;
                }
            }
            else
            {
                /* FIXME: Probably sleep or yield? */
                // if (m_should_stop.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
                //     return SCAPE_EINTERRUPTED;
                tail = m_tail.load(std::memory_order_acquire);
            }
        }

        return SCAPE_EOK;
    }

    int Write(const T &value, int timeout)
    {
        (void)timeout;
        auto const head = m_head.fetch_add(1);
        auto &entry = m_data[head % m_capacity];
        while (write_ticket(head) != entry.ticket.load(std::memory_order_acquire))
            ;
        entry.construct(value);
        entry.ticket.store(write_ticket(head) + 1, std::memory_order_release);
        return SCAPE_EOK;
    }

    int Write2(const T &value, int timeout = 0)
    {
        (void)timeout;
        auto head = m_head.load(std::memory_order_acquire);

        for (;;)
        {
            auto &entry = m_data[head % m_capacity];
            // const auto &ticket = entry.ticket.load(std::memory_order_acquire);
            // printf("Want to write head %zu ticket is %zu expecting %zu\n", head, ticket, write_ticket(head));
            if (write_ticket(head) == entry.ticket.load(std::memory_order_acquire))
            {
                if (m_head.compare_exchange_strong(head, head + 1, std::memory_order_relaxed))
                {
                    // printf("Stamping ticket %zu\n", write_ticket(head) + 1);
                    entry.construct(value);
                    entry.ticket.store(write_ticket(head) + 1, std::memory_order_release);
                    return SCAPE_EOK;
                }
            }
            else
            {
                /* FIXME: Probably sleep or yield? */
                // if (m_should_stop.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
                //     return SCAPE_EINTERRUPTED;
                head = m_head.load(std::memory_order_acquire);
            }
        }

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
        /* FIXME: */
        return false;
    }

    bool IsEmpty()
    {
        /* FIXME: */
        return false;
    }

    int GetTimeSinceLastActivity(int &milliseconds)
    {
        /* FIXME: */
        return SCAPE_EUNSUPPORTED;
    }

private:
    /* The minimum offset between two objects to prevent false sharing. */
    static constexpr size_t hardware_destructive_interference_size =
#if defined(__cpp_lib_hardware_interference_size)
        std::hardware_destructive_interference_size;
#else
        64;
#endif
    /* Determine the write ticket given a head index. */
    constexpr size_t write_ticket(size_t head) const { return 2 * (head / m_capacity); }
    /* Determine the read ticket given a tail index. */
    constexpr size_t read_ticket(size_t tail) const { return 2 * (tail / m_capacity) + 1; }

    std::promise<void> m_signal_stop;
    std::future<void> m_should_stop;
    bool m_is_started;

    /* TODO: Hardware align the `Entry` type in the vector. Is that possible or
             do we have to use raw pointers? */
    struct Entry
    {
        Entry()
            : ticket(0)
            , data()
        {}

        /* FIXME: Think of something better? Need placement new for alignment. */
        void construct(const T &value)
        {
            new (&data) T(value);
        };

        alignas(hardware_destructive_interference_size) std::atomic<size_t> ticket;
        typename std::aligned_storage<sizeof(T), alignof(T)>::type data;
    };

    /* The queue container. */
    // std::vector<Entry> m_data;
    std::unique_ptr<Entry[]> m_data;
    /* The capacity of the queue. */
    size_t m_capacity;
    /* Indicates whether the last remaining item stays at the output or not. */
    bool m_is_persistent;
    /* We write new queue entries at the head of the queue. */
    alignas(hardware_destructive_interference_size) std::atomic<size_t> m_head;
    /* We read queue entries from the tail os the queue. */
    alignas(hardware_destructive_interference_size) std::atomic<size_t> m_tail;
    /* A reference point in time when the last entry was added. */
    /* FIXME: Make atomic? */
    std::chrono::high_resolution_clock::time_point m_last_write_timestamp;

    int Pop(T &value)
    {
        /* FIXME: Implement */
        return SCAPE_EUNSUPPORTED;
    }
};
