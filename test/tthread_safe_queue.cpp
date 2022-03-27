#include <thread>
#include <chrono>
#include "thread_safe_queue.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(ThreadSafeQueue)
{
    ThreadSafeQueue<int> queue;
    ThreadSafeQueue<int*> heap_queue;

    void setup()
    {
        queue.SetParameters(0, false);
        heap_queue.SetParameters(0, false);
    }

    void teardown()
    {
        queue.Stop();
        heap_queue.Stop();
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

    LONGS_EQUAL(true, queue->IsFull());
    LONGS_EQUAL(ADQR_EAGAIN, queue->Write(100));
    LONGS_EQUAL(ADQR_EOK, queue->Write(101, -1));

    /* Wait for the queue to empty. */
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    LONGS_EQUAL(false, queue->IsFull());
    for (int i = 0; i < 10; ++i)
        LONGS_EQUAL(ADQR_EOK, queue->Write(20 * i));
}

TEST(ThreadSafeQueue, CapacityOverflow)
{
    LONGS_EQUAL(ADQR_EOK, queue.SetParameters(10, false));
    LONGS_EQUAL(ADQR_EOK, queue.Start());

    std::thread overflower(&Overflower, &queue);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* Read first set of values. */
    int value = 0;
    for (int i = 0; i < 10; ++i)
    {
        LONGS_EQUAL(ADQR_EOK, queue.Read(value, 1000));
        LONGS_EQUAL(i * 10, value);
    }

    /* Expect the overflow attempt. */
    LONGS_EQUAL(ADQR_EOK, queue.Read(value, 1000));
    LONGS_EQUAL(101, value);

    /* Read second set of values. */
    for (int i = 0; i < 10; ++i)
    {
        LONGS_EQUAL(ADQR_EOK, queue.Read(value, 1000));
        LONGS_EQUAL(i * 20, value);
    }

    overflower.join();
}

TEST(ThreadSafeQueue, Persistent)
{
    LONGS_EQUAL(ADQR_EOK, queue.SetParameters(10, true));
    LONGS_EQUAL(ADQR_EOK, queue.Start());

    int value = 0;
    LONGS_EQUAL(ADQR_EAGAIN, queue.Read(value, 0));
    LONGS_EQUAL(ADQR_EOK, queue.Write(10));

    for (int i = 0; i < 10; ++i)
    {
        LONGS_EQUAL(ADQR_EOK, queue.Read(value, 0));
        LONGS_EQUAL(10, value);
    }

    LONGS_EQUAL(ADQR_EOK, queue.Write(20));

    /* Expect 10 one more time, marked w/ ADQR_ELAST. */
    LONGS_EQUAL(ADQR_ELAST, queue.Read(value, 0));
    LONGS_EQUAL(10, value);

    /* The value should change on the next read. */
    LONGS_EQUAL(ADQR_EOK, queue.Read(value, 0));
    LONGS_EQUAL(20, value);

    LONGS_EQUAL(ADQR_EOK, queue.Stop());
}

TEST(ThreadSafeQueue, PersistentLeaking)
{
    LONGS_EQUAL(ADQR_EOK, heap_queue.SetParameters(10, true));
    LONGS_EQUAL(ADQR_EOK, heap_queue.Start());

    for (int i = 0; i < 10; ++i)
    {
        LONGS_EQUAL(ADQR_EOK, heap_queue.Write(new int{i}));
    }

    int *value = NULL;
    for (int i = 0; i < 9; ++i)
    {
        LONGS_EQUAL(ADQR_ELAST, heap_queue.Read(value, 0));
        LONGS_EQUAL(i, *value);
        delete value;
    }

    LONGS_EQUAL(ADQR_EOK, heap_queue.Read(value, 0));
    LONGS_EQUAL(9, *value);

    LONGS_EQUAL(ADQR_EOK, heap_queue.Write(new int{100}));
    LONGS_EQUAL(ADQR_ELAST, heap_queue.Read(value, 0));
    LONGS_EQUAL(9, *value);
    delete value;

    LONGS_EQUAL(ADQR_EOK, heap_queue.Read(value, 0));
    LONGS_EQUAL(100, *value);

    heap_queue.Free();
    LONGS_EQUAL(ADQR_EOK, heap_queue.Stop());
}
