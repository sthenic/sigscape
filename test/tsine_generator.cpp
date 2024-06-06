#include "CppUTest/TestHarness.h"
#include "sine_generator.h"

#include <thread>
#include <chrono>

TEST_GROUP(SineGenerator)
{
    SineGenerator generator;

    void setup()
    {
        generator.Start();
    }

    void teardown()
    {
        generator.Stop();
    }
};

TEST(SineGenerator, TestSetParameters)
{
    const GeneratorMessage top{
        GeneratorMessageId::SET_TOP_PARAMETERS, SineGeneratorTopParameters{}};

    LONGS_EQUAL(SCAPE_EOK, generator.PushMessageWaitForResponse(top));

    const GeneratorMessage clock_system{
        GeneratorMessageId::SET_CLOCK_SYSTEM_PARAMETERS, SineGeneratorClockSystemParameters{}};

    LONGS_EQUAL(SCAPE_EOK, generator.PushMessageWaitForResponse(clock_system));
}

TEST(SineGenerator, Records)
{
    constexpr size_t RECORD_LENGTH = 1024;
    constexpr double TRIGGER_FREQUENCY = 30.0;
    constexpr int NOF_RECORDS = 30;

    SineGeneratorTopParameters top{};
    top.record_length = RECORD_LENGTH;
    top.trigger_frequency = TRIGGER_FREQUENCY;

    SineGeneratorClockSystemParameters clock_system{};
    clock_system.sampling_frequency = 500e6;

    LONGS_EQUAL(SCAPE_EOK, generator.PushMessageWaitForResponse({GeneratorMessageId::SET_TOP_PARAMETERS, top}));
    LONGS_EQUAL(SCAPE_EOK, generator.PushMessageWaitForResponse({GeneratorMessageId::SET_CLOCK_SYSTEM_PARAMETERS, clock_system}));
    LONGS_EQUAL(SCAPE_EOK, generator.PushMessageWaitForResponse({GeneratorMessageId::ENABLE}));

    std::vector<std::shared_ptr<ADQGen4Record>> records;
    bool return_records = false;
    int nof_records_received = 0;
    while (nof_records_received != NOF_RECORDS)
    {
        std::shared_ptr<ADQGen4Record> record;
        int result = generator.WaitForBuffer(record, 1000);
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

    LONGS_EQUAL(SCAPE_EOK, generator.PushMessageWaitForResponse({GeneratorMessageId::DISABLE}));
}

TEST(SineGenerator, RepeatedStartStop)
{
    constexpr size_t RECORD_LENGTH = 8192;
    constexpr double TRIGGER_FREQUENCY = 20.0;
    constexpr int NOF_RECORDS = 5;
    constexpr int NOF_LOOPS = 3;

    SineGeneratorTopParameters top{};
    top.record_length = RECORD_LENGTH;
    top.trigger_frequency = TRIGGER_FREQUENCY;

    SineGeneratorClockSystemParameters clock_system{};
    clock_system.sampling_frequency = 500e6;

    LONGS_EQUAL(SCAPE_EOK, generator.PushMessageWaitForResponse({GeneratorMessageId::SET_TOP_PARAMETERS, top}));
    LONGS_EQUAL(SCAPE_EOK, generator.PushMessageWaitForResponse({GeneratorMessageId::SET_CLOCK_SYSTEM_PARAMETERS, clock_system}));

    for (int i = 0; i < NOF_LOOPS; ++i)
    {
        LONGS_EQUAL(SCAPE_EOK, generator.PushMessageWaitForResponse({GeneratorMessageId::ENABLE}));

        int nof_records_received = 0;
        while (nof_records_received != NOF_RECORDS)
        {
            std::shared_ptr<ADQGen4Record> record;
            LONGS_EQUAL(SCAPE_EOK, generator.WaitForBuffer(record, 1000));
            CHECK(record != NULL);

            LONGS_EQUAL(nof_records_received, record->header->record_number);
            nof_records_received++;

            LONGS_EQUAL(SCAPE_EOK, generator.ReturnBuffer(record));
        }

        LONGS_EQUAL(SCAPE_EOK, generator.PushMessageWaitForResponse({GeneratorMessageId::DISABLE}));
    }
}
