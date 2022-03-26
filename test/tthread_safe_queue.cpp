#include <thread>
#include <chrono>
#include "thread_safe_queue.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(ThreadSafeQueue)
{
    ThreadSafeQueue<int> queue;
    ThreadSafeQueue<int> capped_queue{10};
    ThreadSafeQueue<int> persistent_queue{0, true};

    void setup()
    {
    }

    void teardown()
    {
        queue.Stop();
        capped_queue.Stop();
        persistent_queue.Stop();
    }
};

TEST(ThreadSafeQueue, StartStop)
{
    LONGS_EQUAL(ADQR_ENOTREADY, queue.Stop());
    LONGS_EQUAL(ADQR_EOK, queue.Start());
    LONGS_EQUAL(ADQR_ENOTREADY, queue.Start());
    LONGS_EQUAL(ADQR_EOK, queue.Stop());
}

static void Writer(ThreadSafeQueue<int> *queue)
{
    for (int i = 0; i < 5; ++i)
    {
        LONGS_EQUAL(ADQR_EOK, queue->Write(10 * i));
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
        LONGS_EQUAL(ADQR_EOK, queue.Read(value, 500));
        LONGS_EQUAL(i * 10, value);
    }

    writer.join();
    LONGS_EQUAL(ADQR_EOK, queue.Stop());
}

static void Aborter(ThreadSafeQueue<int> *queue)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    LONGS_EQUAL(ADQR_EOK, queue->Stop());
}

TEST(ThreadSafeQueue, AbruptStop)
{
    LONGS_EQUAL(ADQR_EOK, queue.Start());
    std::thread aborter(&Aborter, &queue);

    int value = 0;
    LONGS_EQUAL(ADQR_EINTERRUPTED, queue.Read(value, -1));

    aborter.join();
    LONGS_EQUAL(ADQR_ENOTREADY, queue.Stop());
}

static void Overflower(ThreadSafeQueue<int> *queue)
{
    for (int i = 0; i < 10; ++i)
        LONGS_EQUAL(ADQR_EOK, queue->Write(10 * i));

    LONGS_EQUAL(ADQR_EAGAIN, queue->Write(100));
    LONGS_EQUAL(ADQR_EOK, queue->Write(101, -1));

    /* Wait for the queue to empty. */
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    for (int i = 0; i < 10; ++i)
        LONGS_EQUAL(ADQR_EOK, queue->Write(20 * i));
}

TEST(ThreadSafeQueue, CapacityOverflow)
{
    LONGS_EQUAL(ADQR_EOK, capped_queue.Start());

    std::thread overflower(&Overflower, &capped_queue);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* Read first set of values. */
    int value = 0;
    for (int i = 0; i < 10; ++i)
    {
        LONGS_EQUAL(ADQR_EOK, capped_queue.Read(value, 1000));
        LONGS_EQUAL(i * 10, value);
    }

    /* Expect the overflow attempt. */
    LONGS_EQUAL(ADQR_EOK, capped_queue.Read(value, 1000));
    LONGS_EQUAL(101, value);

    /* Read second set of values. */
    for (int i = 0; i < 10; ++i)
    {
        LONGS_EQUAL(ADQR_EOK, capped_queue.Read(value, 1000));
        LONGS_EQUAL(i * 20, value);
    }

    overflower.join();
}

TEST(ThreadSafeQueue, Persistent)
{
    LONGS_EQUAL(ADQR_EOK, persistent_queue.Start());

    int value = 0;
    LONGS_EQUAL(ADQR_EAGAIN, persistent_queue.Read(value, 0));
    LONGS_EQUAL(ADQR_EOK, persistent_queue.Write(10));

    for (int i = 0; i < 10; ++i)
    {
        LONGS_EQUAL(ADQR_EOK, persistent_queue.Read(value, 0));
        LONGS_EQUAL(10, value);
    }

    LONGS_EQUAL(ADQR_EOK, persistent_queue.Write(20));

    /* Expect 10 one more time. */
    LONGS_EQUAL(ADQR_EOK, persistent_queue.Read(value, 0));
    LONGS_EQUAL(10, value);

    /* The value should change on the next read. */
    LONGS_EQUAL(ADQR_EOK, persistent_queue.Read(value, 0));
    LONGS_EQUAL(20, value);

    LONGS_EQUAL(ADQR_EOK, persistent_queue.Stop());
}
