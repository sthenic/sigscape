#include "CppUTest/TestHarness.h"
#include "mock/generator.h"

#include <thread>
#include <chrono>

TEST_GROUP(Generator)
{
    Generator generator;

    void setup()
    {
    }

    void teardown()
    {
        generator.Stop();
    }
};

TEST(Generator, Test0)
{
    LONGS_EQUAL(SCAPE_EOK, generator.SetParameters(Generator::Parameters()));
    LONGS_EQUAL(SCAPE_EOK, generator.SetSamplingFrequency(500e6));
    LONGS_EQUAL(SCAPE_ENOTREADY, generator.Stop());
    LONGS_EQUAL(SCAPE_EOK, generator.Start());
    LONGS_EQUAL(SCAPE_ENOTREADY, generator.Start());
    LONGS_EQUAL(SCAPE_EOK, generator.Stop());
}

TEST(Generator, Records)
{
    constexpr size_t RECORD_LENGTH = 1024;
    constexpr double TRIGGER_FREQUENCY = 100.0;
    constexpr int NOF_RECORDS = 200;

    Generator::Parameters parameters;
    parameters.record_length = RECORD_LENGTH;
    parameters.trigger_frequency = TRIGGER_FREQUENCY;

    LONGS_EQUAL(SCAPE_EOK, generator.SetParameters(parameters));
    LONGS_EQUAL(SCAPE_EOK, generator.SetSamplingFrequency(500e6));
    LONGS_EQUAL(SCAPE_EOK, generator.Start());

    std::vector<ADQGen4Record *> records;
    bool return_records = false;
    int nof_records_received = 0;
    while (nof_records_received != NOF_RECORDS)
    {
        ADQGen4Record* record = NULL;
        int result = generator.WaitForBuffer(record, 1000);

        if ((result == SCAPE_EAGAIN) && !return_records)
        {
            for (auto it = records.begin(); it != records.end(); ++it)
                LONGS_EQUAL(SCAPE_EOK, generator.ReturnBuffer(*it));
            records.clear();
            return_records = true;
            continue;
        }

        LONGS_EQUAL(SCAPE_EOK, result);
        CHECK(record != NULL);
        LONGS_EQUAL(RECORD_LENGTH, record->header->record_length);
        LONGS_EQUAL(RECORD_LENGTH * sizeof(int16_t), record->size);
        LONGS_EQUAL(nof_records_received++, record->header->record_number);

        if (!return_records)
            records.push_back(record);
        else
            LONGS_EQUAL(SCAPE_EOK, generator.ReturnBuffer(record));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    LONGS_EQUAL(SCAPE_EOK, generator.Stop());
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
        LONGS_EQUAL(SCAPE_EOK, generator.SetParameters(parameters));
        LONGS_EQUAL(SCAPE_EOK, generator.SetSamplingFrequency(500e6));
        LONGS_EQUAL(SCAPE_EOK, generator.Start());

        int nof_records_received = 0;
        while (nof_records_received != NOF_RECORDS)
        {
            ADQGen4Record *record = NULL;
            LONGS_EQUAL(SCAPE_EOK, generator.WaitForBuffer(record, 1000));
            CHECK(record != NULL);

            LONGS_EQUAL(nof_records_received, record->header->record_number);
            nof_records_received++;

            LONGS_EQUAL(SCAPE_EOK, generator.ReturnBuffer(record));
        }

        LONGS_EQUAL(SCAPE_EOK, generator.Stop());
    }
}
