#include "data_processing.h"
#include "mock_adqapi.h"

#include <thread>
#include <chrono>

#include "CppUTest/TestHarness.h"

TEST_GROUP(DataProcessingGroup)
{
    std::unique_ptr<DataProcessing> processing;
    MockAdqApi mock_adqapi;
    static constexpr int index = 1;
    static constexpr int channel = 0;

    void setup()
    {
        mock_adqapi.AddDigitizer("SPD-SIM01", 1, PID_ADQ32);
        processing = std::make_unique<DataProcessing>(&mock_adqapi, index, channel, "SPD-SIM01 CHA");
    }

    void teardown()
    {
    }
};

TEST(DataProcessingGroup, StartStop)
{
    LONGS_EQUAL(SCAPE_ENOTREADY, processing->Stop());
    LONGS_EQUAL(SCAPE_EOK, processing->Start());
    LONGS_EQUAL(SCAPE_ENOTREADY, processing->Start());
    LONGS_EQUAL(SCAPE_EOK, processing->Stop());
}

TEST(DataProcessingGroup, Records)
{
    constexpr size_t RECORD_LENGTH = 8192;
    constexpr double TRIGGER_FREQUENCY = 20.0;

    std::stringstream ss;
    ss << R"""(TOP
    frequency:
        1e6
    amplitude:
        1.0
    record length:
    )""" << RECORD_LENGTH << R"""(
    trigger frequency:
    )""" << TRIGGER_FREQUENCY << R"""(
    harmonic distortion:
        0
    noise standard deviation:
        0.1
    )""";

    ADQ_SetParametersString(&mock_adqapi, index, ss.str().c_str(), ss.str().size());

    LONGS_EQUAL(SCAPE_EOK, processing->Start());
    LONGS_EQUAL(ADQ_EOK, ADQ_StartDataAcquisition(&mock_adqapi, index));

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    LONGS_EQUAL(SCAPE_EOK, processing->Stop());
    LONGS_EQUAL(ADQ_EOK, ADQ_StopDataAcquisition(&mock_adqapi, index));
}

TEST(DataProcessingGroup, RepeatedStartStop)
{
    constexpr size_t RECORD_LENGTH = 8192;
    constexpr double TRIGGER_FREQUENCY = 100.0;
    constexpr int NOF_RECORDS = 200;
    constexpr int NOF_LOOPS = 2;

    std::stringstream ss;
    ss << R"""(TOP
    frequency:
        1e6
    amplitude:
        1.0
    record length:
    )""" << RECORD_LENGTH << R"""(
    trigger frequency:
    )""" << TRIGGER_FREQUENCY << R"""(
    harmonic distortion:
        0
    noise standard deviation:
        0.1
    )""";

    ADQ_SetParametersString(&mock_adqapi, index, ss.str().c_str(), ss.str().size());

    for (int i = 0; i < NOF_LOOPS; ++i)
    {
        LONGS_EQUAL(SCAPE_EOK, processing->Start());
        LONGS_EQUAL(ADQ_EOK, ADQ_StartDataAcquisition(&mock_adqapi, index));

        int nof_records_received = 0;
        while (nof_records_received != NOF_RECORDS)
        {
            std::shared_ptr<ProcessedRecord> record = NULL;
            int result = processing->WaitForBuffer(record, 1000);
            CHECK(record != NULL);
            CHECK((result == SCAPE_EOK) || (result == SCAPE_ELAST));

            if (result == SCAPE_ELAST)
            {
                LONGS_EQUAL(nof_records_received, record->time_domain->header.record_number);
                nof_records_received++;
            }
        }

        LONGS_EQUAL(SCAPE_EOK, processing->Stop());
        LONGS_EQUAL(ADQ_EOK, ADQ_StopDataAcquisition(&mock_adqapi, index));
    }
}
