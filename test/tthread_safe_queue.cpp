#include <thread>
#include <chrono>
#include "thread_safe_queue.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(ThreadSafeQueue)
{
    ThreadSafeQueue<int> queue;

    void setup()
    {
        LONGS_EQUAL(0, queue.Initialize());
    }

    void teardown()
    {
        queue.Stop();
    }
};

TEST(ThreadSafeQueue, StartStop)
{
    LONGS_EQUAL(-3, queue.Stop());
    LONGS_EQUAL(0, queue.Start());
    LONGS_EQUAL(-3, queue.Start());
    LONGS_EQUAL(0, queue.Stop());
}

static void Writer(ThreadSafeQueue<int> *queue)
{
    for (int i = 0; i < 5; ++i)
    {
        LONGS_EQUAL(0, queue->Write(10 * i));
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
        LONGS_EQUAL(0, queue.Read(value, 1000));
        LONGS_EQUAL(i * 10, value);
    }

    writer.join();
    LONGS_EQUAL(0, queue.Stop());
}

static void Aborter(ThreadSafeQueue<int> *queue)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    LONGS_EQUAL(0, queue->Stop());
}

TEST(ThreadSafeQueue, AbruptStop)
{
    LONGS_EQUAL(0, queue.Start());
    std::thread aborter(&Aborter, &queue);

    int value = 0;
    LONGS_EQUAL(-2, queue.Read(value, -1));

    aborter.join();
    LONGS_EQUAL(-3, queue.Stop());
}

