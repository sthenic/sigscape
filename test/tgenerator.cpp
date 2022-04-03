#include <thread>
#include <chrono>
#include "generator.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(Generator)
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

TEST(Generator, Test0)
{
    LONGS_EQUAL(ADQR_EOK, simulator.Initialize());
    LONGS_EQUAL(ADQR_ENOTREADY, simulator.Stop());
    LONGS_EQUAL(ADQR_EOK, simulator.Start());
    LONGS_EQUAL(ADQR_ENOTREADY, simulator.Start());
    LONGS_EQUAL(ADQR_EOK, simulator.Stop());
}

TEST(Generator, Records)
{
    constexpr size_t RECORD_LENGTH = 1024;
    constexpr double TRIGGER_FREQUENCY = 100.0;
    constexpr int NOF_RECORDS = 200;

    Generator::Parameters parameters;
    parameters.record_length = RECORD_LENGTH;
    parameters.trigger_frequency = TRIGGER_FREQUENCY;

    LONGS_EQUAL(ADQR_EOK, simulator.Initialize(parameters));
    LONGS_EQUAL(ADQR_EOK, simulator.Start());

    std::vector<std::shared_ptr<TimeDomainRecord>> records;
    bool return_records = false;
    int nof_records_received = 0;
    while (nof_records_received != NOF_RECORDS)
    {
        std::shared_ptr<TimeDomainRecord> record = NULL;
        int result = simulator.WaitForBuffer((std::shared_ptr<void> &)record, 1000, NULL);

        if ((result == ADQR_EAGAIN) && !return_records)
        {
            for (auto it = records.begin(); it != records.end(); ++it)
                LONGS_EQUAL(ADQR_EOK, simulator.ReturnBuffer(*it));
            records.clear();
            return_records = true;
            continue;
        }

        LONGS_EQUAL(ADQR_EOK, result);
        CHECK(record != NULL);
        LONGS_EQUAL(TIME_DOMAIN, record->id);
        LONGS_EQUAL(RECORD_LENGTH, record->header.record_length);
        LONGS_EQUAL(RECORD_LENGTH, record->count);
        LONGS_EQUAL(nof_records_received++, record->header.record_number);

        if (!return_records)
            records.push_back(record);
        else
            LONGS_EQUAL(ADQR_EOK, simulator.ReturnBuffer(record));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    LONGS_EQUAL(ADQR_EOK, simulator.Stop());
}

TEST(Generator, Copy)
{
    TimeDomainRecord r0(100);
    TimeDomainRecord r1(50);
    r1 = r0;
}

TEST(Generator, RepeatedStartStop)
{
    constexpr size_t RECORD_LENGTH = 8192;
    constexpr double TRIGGER_FREQUENCY = 1.0;
    constexpr int NOF_RECORDS = 2;
    constexpr int NOF_LOOPS = 5;

    Generator::Parameters parameters;
    parameters.record_length = RECORD_LENGTH;
    parameters.trigger_frequency = TRIGGER_FREQUENCY;

    for (int i = 0; i < NOF_LOOPS; ++i)
    {
        LONGS_EQUAL(ADQR_EOK, simulator.Initialize(parameters));
        LONGS_EQUAL(ADQR_EOK, simulator.Start());

        int nof_records_received = 0;
        while (nof_records_received != NOF_RECORDS)
        {
            std::shared_ptr<TimeDomainRecord> record = NULL;
            LONGS_EQUAL(ADQR_EOK, simulator.WaitForBuffer((std::shared_ptr<void> &)record, 1000, NULL));
            CHECK(record != NULL);

            LONGS_EQUAL(TIME_DOMAIN, record->id);
            LONGS_EQUAL(nof_records_received, record->header.record_number);
            nof_records_received++;

            LONGS_EQUAL(ADQR_EOK, simulator.ReturnBuffer(record));
        }

        LONGS_EQUAL(ADQR_EOK, simulator.Stop());
    }
}
