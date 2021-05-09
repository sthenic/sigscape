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
    LONGS_EQUAL(0, acquisition.Initialize(1024, 1.0));
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
    LONGS_EQUAL(0, processing->Initialize());

    LONGS_EQUAL(0, processing->Start());

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    LONGS_EQUAL(0, processing->Stop());
}

TEST(DataProcessingGroup, Copy)
{
    FrequencyDomainRecord r0(100);
    FrequencyDomainRecord r1(25);
    r1 = r0;

    ProcessedRecord r2(100);
    ProcessedRecord r3(25);
    r3 = r2;
}

TEST(DataProcessingGroup, RepeatedStartStop)
{
    constexpr size_t RECORD_LENGTH = 8192;
    constexpr double TRIGGER_RATE_HZ = 100.0;
    constexpr int NOF_RECORDS = 200;
    constexpr int NOF_LOOPS = 2;

    LONGS_EQUAL(0, acquisition.Initialize(RECORD_LENGTH, TRIGGER_RATE_HZ));
    LONGS_EQUAL(0, processing->Initialize());

    for (int i = 0; i < NOF_LOOPS; ++i)
    {
        printf("Interation %d\n", i);
        LONGS_EQUAL(0, processing->Start());

        int nof_records_received = 0;
        while (nof_records_received != NOF_RECORDS)
        {
            struct ProcessedRecord *record = NULL;
            LONGS_EQUAL(0, processing->WaitForBuffer(record, 1000));
            CHECK(record != NULL);

            LONGS_EQUAL(TIME_DOMAIN, record->time_domain->id);
            LONGS_EQUAL(FREQUENCY_DOMAIN, record->frequency_domain->id);
            LONGS_EQUAL(nof_records_received, record->time_domain->header.record_number);
            LONGS_EQUAL(nof_records_received, record->frequency_domain->header.record_number);
            nof_records_received++;

            LONGS_EQUAL(0, processing->ReturnBuffer(record));
        }

        LONGS_EQUAL(0, processing->Stop());
    }
}
