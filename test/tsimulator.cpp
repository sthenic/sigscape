#include <thread>
#include <chrono>
#include "simulator.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(SimulatorGroup)
{
    DataAcquisitionSimulator simulator;

    void setup()
    {
    }

    void teardown()
    {
        simulator.Stop();
    }
};

TEST(SimulatorGroup, Test0)
{
    constexpr size_t RECORD_LENGTH = 1024;
    constexpr double TRIGGER_RATE_HZ = 4.0;
    LONGS_EQUAL(0, simulator.Initialize(RECORD_LENGTH, TRIGGER_RATE_HZ));
    LONGS_EQUAL(-1, simulator.Stop());
    LONGS_EQUAL(0, simulator.Start());
    LONGS_EQUAL(-1, simulator.Start());
    LONGS_EQUAL(0, simulator.Stop());
}

TEST(SimulatorGroup, Records)
{
    constexpr size_t RECORD_LENGTH = 1024;
    constexpr double TRIGGER_RATE_HZ = 100.0;
    constexpr int NOF_RECORDS = 200;
    LONGS_EQUAL(0, simulator.Initialize(RECORD_LENGTH, TRIGGER_RATE_HZ));
    LONGS_EQUAL(0, simulator.Start());

    std::vector<struct TimeDomainRecord *> records;
    bool return_records = false;
    int nof_records_received = 0;
    while (nof_records_received != NOF_RECORDS)
    {
        struct TimeDomainRecord *record = NULL;
        int result = simulator.WaitForBuffer((void *&)record, 1000, NULL);

        if ((result == -1) && !return_records)
        {
            for (auto it = records.begin(); it != records.end(); ++it)
                LONGS_EQUAL(0, simulator.ReturnBuffer(*it));
            records.clear();
            return_records = true;
            continue;
        }

        LONGS_EQUAL(0, result);
        CHECK(record != NULL);
        LONGS_EQUAL(TIME_DOMAIN, record->id);
        LONGS_EQUAL(RECORD_LENGTH, record->header.record_length);
        LONGS_EQUAL(RECORD_LENGTH, record->count);
        LONGS_EQUAL(nof_records_received++, record->header.record_number);

        if (!return_records)
            records.push_back(record);
        else
            LONGS_EQUAL(0, simulator.ReturnBuffer(record));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    LONGS_EQUAL(0, simulator.Stop());
}

TEST(SimulatorGroup, Copy)
{
    TimeDomainRecord r0(100);
    TimeDomainRecord r1(50);
    r1 = r0;
}
