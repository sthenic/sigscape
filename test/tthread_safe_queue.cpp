#include <thread>
#include <chrono>
#include "thread_safe_queue.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(ThreadSafeQueue)
{
    ThreadSafeQueue<int> queue;
    ThreadSafeQueue<int> capped_queue{10};
    ThreadSafeQueue<int> persistent_queue{0, true};
    ThreadSafeQueue<int*> persistent_queue_heap{0, true};

    void setup()
    {
    }

    void teardown()
    {
        queue.Stop();
        capped_queue.Stop();
        persistent_queue.Stop();
        persistent_queue_heap.Stop();
    }
};

TEST(ThreadSafeQueue, StartStop)
{
    LONGS_EQUAL(SCAPE_ENOTREADY, queue.Stop());
    LONGS_EQUAL(SCAPE_EOK, queue.Start());
    LONGS_EQUAL(SCAPE_ENOTREADY, queue.Start());
    LONGS_EQUAL(SCAPE_EOK, queue.Stop());
}

static void Writer(ThreadSafeQueue<int> *queue)
{
    for (int i = 0; i < 5; ++i)
    {
        LONGS_EQUAL(SCAPE_EOK, queue->Write(10 * i));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

TEST(ThreadSafeQueue, WriteRead)
{
    LONGS_EQUAL(0, queue.Start());
    std::thread writer(&Writer, &queue);

    for (int i = 0; i < 5; ++i)
    {
        int value = 0;
        LONGS_EQUAL(SCAPE_EOK, queue.Read(value, 500));
        LONGS_EQUAL(i * 10, value);
    }

    writer.join();
    LONGS_EQUAL(SCAPE_EOK, queue.Stop());
}

static void Aborter(ThreadSafeQueue<int> *queue)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    LONGS_EQUAL(SCAPE_EOK, queue->Stop());
}

TEST(ThreadSafeQueue, AbruptStop)
{
    LONGS_EQUAL(SCAPE_EOK, queue.Start());
    std::thread aborter(&Aborter, &queue);

    int value = 0;
    LONGS_EQUAL(SCAPE_EINTERRUPTED, queue.Read(value, -1));

    aborter.join();
    LONGS_EQUAL(SCAPE_ENOTREADY, queue.Stop());
}

static void Overflower(ThreadSafeQueue<int> *queue)
{
    for (int i = 0; i < 10; ++i)
        LONGS_EQUAL(SCAPE_EOK, queue->Write(10 * i));

    LONGS_EQUAL(true, queue->IsFull());
    LONGS_EQUAL(SCAPE_EAGAIN, queue->Write(100));
    LONGS_EQUAL(SCAPE_EOK, queue->Write(101, -1));

    /* Wait for the queue to empty. */
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    LONGS_EQUAL(false, queue->IsFull());
    for (int i = 0; i < 10; ++i)
        LONGS_EQUAL(SCAPE_EOK, queue->Write(20 * i));
}

TEST(ThreadSafeQueue, CapacityOverflow)
{
    LONGS_EQUAL(SCAPE_EOK, capped_queue.Start());

    std::thread overflower(&Overflower, &capped_queue);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* Read first set of values. */
    int value = 0;
    for (int i = 0; i < 10; ++i)
    {
        LONGS_EQUAL(SCAPE_EOK, capped_queue.Read(value, 1000));
        LONGS_EQUAL(i * 10, value);
    }

    /* Expect the overflow attempt. */
    LONGS_EQUAL(SCAPE_EOK, capped_queue.Read(value, 1000));
    LONGS_EQUAL(101, value);

    /* Read second set of values. */
    for (int i = 0; i < 10; ++i)
    {
        LONGS_EQUAL(SCAPE_EOK, capped_queue.Read(value, 1000));
        LONGS_EQUAL(i * 20, value);
    }

    overflower.join();
}

TEST(ThreadSafeQueue, Persistent)
{
    LONGS_EQUAL(SCAPE_EOK, persistent_queue.Start());

    int value = 0;
    LONGS_EQUAL(SCAPE_EAGAIN, persistent_queue.Read(value, 0));
    LONGS_EQUAL(SCAPE_EOK, persistent_queue.Write(10));

    for (int i = 0; i < 10; ++i)
    {
        LONGS_EQUAL(SCAPE_EOK, persistent_queue.Read(value, 0));
        LONGS_EQUAL(10, value);
    }

    LONGS_EQUAL(SCAPE_EOK, persistent_queue.Write(20));

    /* Expect 10 one more time, marked w/ SCAPE_ELAST. */
    LONGS_EQUAL(SCAPE_ELAST, persistent_queue.Read(value, 0));
    LONGS_EQUAL(10, value);

    /* The value should change on the next read. */
    LONGS_EQUAL(SCAPE_EOK, persistent_queue.Read(value, 0));
    LONGS_EQUAL(20, value);

    LONGS_EQUAL(SCAPE_EOK, persistent_queue.Stop());
}

TEST(ThreadSafeQueue, PersistentLeaking)
{
    LONGS_EQUAL(SCAPE_EOK, persistent_queue_heap.Start());

    for (int i = 0; i < 10; ++i)
    {
        LONGS_EQUAL(SCAPE_EOK, persistent_queue_heap.Write(new int{i}));
    }

    int *value = NULL;
    for (int i = 0; i < 9; ++i)
    {
        LONGS_EQUAL(SCAPE_ELAST, persistent_queue_heap.Read(value, 0));
        LONGS_EQUAL(i, *value);
        delete value;
    }

    LONGS_EQUAL(SCAPE_EOK, persistent_queue_heap.Read(value, 0));
    LONGS_EQUAL(9, *value);

    LONGS_EQUAL(SCAPE_EOK, persistent_queue_heap.Write(new int{100}));
    LONGS_EQUAL(SCAPE_ELAST, persistent_queue_heap.Read(value, 0));
    LONGS_EQUAL(9, *value);
    delete value;

    LONGS_EQUAL(SCAPE_EOK, persistent_queue_heap.Read(value, 0));
    LONGS_EQUAL(100, *value);

    persistent_queue_heap.Free();
    LONGS_EQUAL(SCAPE_EOK, persistent_queue_heap.Stop());
}

TEST(ThreadSafeQueue, ReadPredicate)
{
    LONGS_EQUAL(SCAPE_EOK, queue.Start());

    LONGS_EQUAL(SCAPE_EOK, queue.EmplaceWrite(101));
    LONGS_EQUAL(SCAPE_EOK, queue.EmplaceWrite(102));

    int value{};
    LONGS_EQUAL(SCAPE_EAGAIN, queue.Read(value, 100, [](const auto e){ return e == 102; }));
    LONGS_EQUAL(SCAPE_EOK, queue.Read(value, 0, [](const auto e){ return e == 101; }));
    LONGS_EQUAL(101, value);

    LONGS_EQUAL(SCAPE_EOK, queue.Read(value, 0));
    LONGS_EQUAL(102, value);

    LONGS_EQUAL(SCAPE_EOK, queue.Stop());
}
