#include <thread>
#include <chrono>
#include "simulated_data_acquisition.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(SimulatedDataAcquisition)
{
    SimulatedDataAcquisition acquisition;

    void setup()
    {
    }

    void teardown()
    {
        acquisition.Stop();
    }
};

TEST(SimulatedDataAcquisition, StartStop)
{
    constexpr int RECORD_LENGTH = 1024;
    constexpr double TRIGGER_RATE_HZ = 4.0;
    LONGS_EQUAL(0, acquisition.Initialize(RECORD_LENGTH, TRIGGER_RATE_HZ));
    LONGS_EQUAL(-1, acquisition.Stop());
    LONGS_EQUAL(0, acquisition.Start());
    LONGS_EQUAL(-1, acquisition.Start());
    LONGS_EQUAL(0, acquisition.Stop());
}

TEST(SimulatedDataAcquisition, Records)
{
    constexpr int RECORD_LENGTH = 1024;
    constexpr double TRIGGER_RATE_HZ = 100.0;
    constexpr int NOF_RECORDS = 200;
    LONGS_EQUAL(0, acquisition.Initialize(RECORD_LENGTH, TRIGGER_RATE_HZ));
    LONGS_EQUAL(0, acquisition.Start());

    std::vector<struct TimeDomainRecord *> records;
    bool return_records = false;
    int nof_records_received = 0;
    while (nof_records_received != NOF_RECORDS)
    {
        struct TimeDomainRecord *record = NULL;
        int result = acquisition.WaitForBuffer((void *&)record, 1000, NULL);

        if ((result == -1) && !return_records)
        {
            for (auto it = records.begin(); it != records.end(); ++it)
                LONGS_EQUAL(0, acquisition.ReturnBuffer(*it));
            records.clear();
            return_records = true;
            continue;
        }

        LONGS_EQUAL(RECORD_LENGTH * sizeof(double), result);
        CHECK(record != NULL);
        LONGS_EQUAL(TIME_DOMAIN, record->id);
        LONGS_EQUAL(RECORD_LENGTH, record->header.record_length);
        LONGS_EQUAL(RECORD_LENGTH * sizeof(double), record->capacity);
        LONGS_EQUAL(nof_records_received++, record->header.record_number);

        if (!return_records)
            records.push_back(record);
        else
            LONGS_EQUAL(0, acquisition.ReturnBuffer(record));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    LONGS_EQUAL(0, acquisition.Stop());
}
