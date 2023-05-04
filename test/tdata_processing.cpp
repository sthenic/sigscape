#include "data_processing.h"
#include "mock_control_unit.h"

#include <thread>
#include <chrono>

#include "CppUTest/TestHarness.h"

TEST_GROUP(DataProcessing)
{
    std::unique_ptr<DataProcessing> processing;
    MockControlUnit mock_control_unit;
    static constexpr int index = 1;
    static constexpr int channel = 0;

    void setup()
    {
        mock_control_unit.AddDigitizer(PID_ADQ32, {"SPD-SIM01",
                                                   "ADQ32",
                                                   "-SG2G5-BW1G0",
                                                   {
                                                       ADQ_FIRMWARE_TYPE_FWDAQ,
                                                       "1CH-FWDAQ",
                                                       "2023.1.3",
                                                       "STANDARD",
                                                       "400-000-XYZ",
                                                   },
                                                   {
                                                       ADQ_COMMUNICATION_INTERFACE_PCIE,
                                                       3,
                                                       8,
                                                   },
                                                   {
                                                       {"A", 2, {2500.0}, 65536},
                                                   }});

        struct ADQConstantParameters constant;
        mock_control_unit.GetParameters(index, ADQ_PARAMETER_ID_CONSTANT, &constant);
        processing = std::make_unique<DataProcessing>(
            &mock_control_unit, index, channel, "SPD-SIM01 CHA", constant);
    }

    void teardown()
    {
    }
};

TEST(DataProcessing, StartStop)
{
    LONGS_EQUAL(SCAPE_ENOTREADY, processing->Stop());
    LONGS_EQUAL(SCAPE_EOK, processing->Start());
    LONGS_EQUAL(SCAPE_ENOTREADY, processing->Start());
    LONGS_EQUAL(SCAPE_EOK, processing->Stop());
}

TEST(DataProcessing, Records)
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

    ADQ_SetParametersString(&mock_control_unit, index, ss.str().c_str(), ss.str().size());

    LONGS_EQUAL(SCAPE_EOK, processing->Start());
    LONGS_EQUAL(ADQ_EOK, ADQ_StartDataAcquisition(&mock_control_unit, index));

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    LONGS_EQUAL(SCAPE_EOK, processing->Stop());
    LONGS_EQUAL(ADQ_EOK, ADQ_StopDataAcquisition(&mock_control_unit, index));
}

TEST(DataProcessing, RepeatedStartStop)
{
    constexpr size_t RECORD_LENGTH = 8192;
    constexpr double TRIGGER_FREQUENCY = 10.0;
    constexpr int NOF_RECORDS = 30;
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

    ADQ_SetParametersString(&mock_control_unit, index, ss.str().c_str(), ss.str().size());

    for (int i = 0; i < NOF_LOOPS; ++i)
    {
        LONGS_EQUAL(SCAPE_EOK, processing->Start());
        LONGS_EQUAL(ADQ_EOK, ADQ_StartDataAcquisition(&mock_control_unit, index));

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

            /* Cap the refresh rate to something reasonable, e.g. 120 Hz. */
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(1000.0 / 120.0)));
        }

        LONGS_EQUAL(SCAPE_EOK, processing->Stop());
        LONGS_EQUAL(ADQ_EOK, ADQ_StopDataAcquisition(&mock_control_unit, index));
    }
}
