#include <thread>
#include <chrono>
#include "data_processing.h"
#include "generator.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(DataProcessingGroup)
{
    std::shared_ptr<DataAcquisitionSimulator> acquisition;
    std::unique_ptr<DataProcessing> processing;

    void setup()
    {
        acquisition = std::make_shared<DataAcquisitionSimulator>();
        processing = std::make_unique<DataProcessing>(acquisition);
    }

    void teardown()
    {
        acquisition->Stop();
        processing->Stop();
    }
};

TEST(DataProcessingGroup, StartStop)
{
    LONGS_EQUAL(ADQR_EOK, acquisition->Initialize());
    LONGS_EQUAL(ADQR_EOK, processing->Initialize());
    LONGS_EQUAL(ADQR_ENOTREADY, processing->Stop());
    LONGS_EQUAL(ADQR_EOK, processing->Start());
    LONGS_EQUAL(ADQR_ENOTREADY, processing->Start());
    LONGS_EQUAL(ADQR_EOK, processing->Stop());
}

TEST(DataProcessingGroup, Records)
{
    constexpr size_t RECORD_LENGTH = 8192;
    constexpr double TRIGGER_FREQUENCY = 30.0;

    Generator::Parameters parameters;
    parameters.record_length = RECORD_LENGTH;
    parameters.trigger_frequency = TRIGGER_FREQUENCY;

    LONGS_EQUAL(ADQR_EOK, acquisition->Initialize(parameters));
    LONGS_EQUAL(ADQR_EOK, processing->Initialize());

    LONGS_EQUAL(ADQR_EOK, processing->Start());

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    LONGS_EQUAL(ADQR_EOK, processing->Stop());
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
    constexpr double TRIGGER_FREQUENCY = 100.0;
    constexpr int NOF_RECORDS = 200;
    constexpr int NOF_LOOPS = 2;

    Generator::Parameters parameters;
    parameters.record_length = RECORD_LENGTH;
    parameters.trigger_frequency = TRIGGER_FREQUENCY;

    LONGS_EQUAL(ADQR_EOK, acquisition->Initialize(parameters));
    LONGS_EQUAL(ADQR_EOK, processing->Initialize());

    for (int i = 0; i < NOF_LOOPS; ++i)
    {
        LONGS_EQUAL(ADQR_EOK, processing->Start());

        int nof_records_received = 0;
        while (nof_records_received != NOF_RECORDS)
        {
            std::shared_ptr<ProcessedRecord> record = NULL;
            int result = processing->WaitForBuffer(record, 1000);
            CHECK(record != NULL);
            CHECK((result == ADQR_EOK) || (result == ADQR_ELAST));

            if (result == ADQR_ELAST)
            {
                LONGS_EQUAL(nof_records_received, record->time_domain->header.record_number);
                nof_records_received++;
            }
        }

        LONGS_EQUAL(ADQR_EOK, processing->Stop());
    }
}
