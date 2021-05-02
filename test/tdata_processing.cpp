#include <thread>
#include <chrono>
#include "data_processing.h"
#include "simulator.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(DataProcessingGroup)
{
    DataAcquisitionSimulator acquisition;
    DataProcessing *processing;

    void setup()
    {
        processing = new DataProcessing(acquisition);
    }

    void teardown()
    {
        acquisition.Stop();
        processing->Stop();
        delete processing;
    }
};

TEST(DataProcessingGroup, StartStop)
{
    LONGS_EQUAL(0, processing->Initialize());
    LONGS_EQUAL(-1, processing->Stop());
    LONGS_EQUAL(0, processing->Start());
    LONGS_EQUAL(-1, processing->Start());
    LONGS_EQUAL(0, processing->Stop());
}

TEST(DataProcessingGroup, Records)
{
    constexpr size_t RECORD_LENGTH = 8192;
    constexpr double TRIGGER_RATE_HZ = 30.0;
    LONGS_EQUAL(0, acquisition.Initialize(RECORD_LENGTH, TRIGGER_RATE_HZ));

    LONGS_EQUAL(0, processing->Start());
    LONGS_EQUAL(0, acquisition.Start());

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    LONGS_EQUAL(0, processing->Stop());
    LONGS_EQUAL(0, acquisition.Stop());
}
