#include <thread>
#include <chrono>
#include "thread_safe_queue.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(ThreadSafeQueue)
{
    ThreadSafeQueue<int> queue;

    void setup()
    {
    }

    void teardown()
    {
        queue.Stop();
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
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

TEST(ThreadSafeQueue, WriteRead)
{
    LONGS_EQUAL(0, queue.Start());
    std::thread writer(&Writer, &queue);

    for (int i = 0; i < 5; ++i)
    {
        int value = 0;
        LONGS_EQUAL(ADQR_EOK, queue.Read(value, 1000));
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

