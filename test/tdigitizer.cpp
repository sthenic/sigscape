#include "digitizer.h"
#include "embedded_python_thread.h"
#include "mock_control_unit.h"

#include <thread>
#include <chrono>

#include "CppUTest/TestHarness.h"

TEST_GROUP(Digitizer)
{
    std::unique_ptr<Digitizer> digitizer;
    std::shared_ptr<EmbeddedPythonThread> python{std::make_shared<EmbeddedPythonThread>()};
    MockControlUnit mock_control_unit;

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

        digitizer = std::make_unique<Digitizer>(&mock_control_unit, 0, 1, ".", python);
    }

    void teardown()
    {
        /* In case of an error, we have to bring the digitizer to a controlled
           stop to not attempt to access the mocked resources after they get
           destroyed going out of scope. */
        digitizer->Stop();
    }
};

TEST(Digitizer, Initialize)
{
    const int TIMEOUT_MS = 250;
    LONGS_EQUAL(SCAPE_EOK, digitizer->Start());

    struct DigitizerMessage msg;
    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, TIMEOUT_MS));
    LONGS_EQUAL(DigitizerMessageId::STATE, msg.id);
    LONGS_EQUAL(DigitizerState::INITIALIZATION, msg.state);

    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, 1000));
    LONGS_EQUAL(DigitizerMessageId::INITIALIZED, msg.id);
    STRCMP_EQUAL("SPD-SIM01", msg.constant_parameters.serial_number);
    LONGS_EQUAL(1, msg.constant_parameters.nof_channels);
    LONGS_EQUAL(1, msg.constant_parameters.nof_acquisition_channels);
    LONGS_EQUAL(1, msg.constant_parameters.nof_transfer_channels);

    /* System manager objects (boot status). */
    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, TIMEOUT_MS));
    LONGS_EQUAL(DigitizerMessageId::BOOT_STATUS, msg.id);
    CHECK(msg.boot_entries.size() > 0);

    /* System manager objects (sensors). */
    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, TIMEOUT_MS));
    LONGS_EQUAL(DigitizerMessageId::SENSOR_TREE, msg.id);
    CHECK(msg.sensor_tree.size() > 0);

    /* Idle after initialization. */
    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, TIMEOUT_MS));
    LONGS_EQUAL(DigitizerMessageId::STATE, msg.id);
    LONGS_EQUAL(DigitizerState::IDLE, msg.state);

    /* File watchers reporting dirty parameters. */
    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, 2000));
    LONGS_EQUAL(DigitizerMessageId::CHANGED_TOP_PARAMETERS, msg.id);

    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, 2000));
    LONGS_EQUAL(DigitizerMessageId::CHANGED_CLOCK_SYSTEM_PARAMETERS, msg.id);

    /* No more messages */
    LONGS_EQUAL(SCAPE_EAGAIN, digitizer->WaitForMessage(msg, TIMEOUT_MS));

    LONGS_EQUAL(SCAPE_EOK, digitizer->EmplaceMessage(
        DigitizerMessageId::START_ACQUISITION
    ));

    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, TIMEOUT_MS));
    LONGS_EQUAL(DigitizerMessageId::STATE, msg.id);
    LONGS_EQUAL(DigitizerState::ACQUISITION, msg.state);

    /* Expect the same message back w/ `result` SCAPE_EOK followed by the
       'all clear' event message. */
    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, TIMEOUT_MS));
    LONGS_EQUAL(DigitizerMessageId::START_ACQUISITION, msg.id);
    LONGS_EQUAL(SCAPE_EOK, msg.result);

    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, TIMEOUT_MS));
    LONGS_EQUAL(DigitizerMessageId::EVENT_CLEAR, msg.id);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    /* Expect a DRAM fill status message. */
    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, TIMEOUT_MS));
    LONGS_EQUAL(DigitizerMessageId::DRAM_FILL, msg.id);

    /* Stop the acquisition and expect a state transition message. */
    LONGS_EQUAL(SCAPE_EOK, digitizer->EmplaceMessage(
        DigitizerMessageId::STOP_ACQUISITION
    ));

    /* Depending on the timing we may end up with more than one DRAM fill status message. */
    do
    {
        LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, TIMEOUT_MS));
    }
    while (msg.id == DigitizerMessageId::DRAM_FILL);

    LONGS_EQUAL(DigitizerMessageId::STATE, msg.id);
    LONGS_EQUAL(DigitizerState::IDLE, msg.state);

    /* Expect the same message back w/ `result` SCAPE_EOK followed by the
       'all clear' event message. */
    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, TIMEOUT_MS));
    LONGS_EQUAL(DigitizerMessageId::STOP_ACQUISITION, msg.id);
    LONGS_EQUAL(SCAPE_EOK, msg.result);

    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, TIMEOUT_MS));
    LONGS_EQUAL(DigitizerMessageId::EVENT_CLEAR, msg.id);

    LONGS_EQUAL(SCAPE_EOK, digitizer->Stop());
}
