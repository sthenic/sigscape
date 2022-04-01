#include <thread>
#include <chrono>
#include "simulated_digitizer.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(SimulatedDigitizer)
{
    SimulatedDigitizer digitizer;

    void setup()
    {
    }

    void teardown()
    {
        digitizer.Stop();
    }
};

TEST(SimulatedDigitizer, Initialize)
{
    LONGS_EQUAL(ADQR_EOK, digitizer.Initialize());
    LONGS_EQUAL(ADQR_EOK, digitizer.Start());

    struct DigitizerMessage msg;
    LONGS_EQUAL(ADQR_EOK, digitizer.WaitForMessage(msg, 100));
    LONGS_EQUAL(MESSAGE_ID_NEW_STATE, msg.id);
    LONGS_EQUAL(STATE_NOT_ENUMERATED, msg.status);

    LONGS_EQUAL(ADQR_EOK, digitizer.WaitForMessage(msg, 100));
    LONGS_EQUAL(MESSAGE_ID_SETUP_STARTING, msg.id);

    LONGS_EQUAL(ADQR_EOK, digitizer.WaitForMessage(msg, 1000));
    LONGS_EQUAL(MESSAGE_ID_SETUP_OK, msg.id);

    LONGS_EQUAL(ADQR_EOK, digitizer.PushMessage({
        MESSAGE_ID_START_ACQUISITION, 0, NULL
    }));

    LONGS_EQUAL(ADQR_EOK, digitizer.WaitForMessage(msg, 100));
    LONGS_EQUAL(MESSAGE_ID_NEW_STATE, msg.id);
    LONGS_EQUAL(STATE_ACQUISITION, msg.status);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    LONGS_EQUAL(ADQR_EOK, digitizer.PushMessage({
        MESSAGE_ID_STOP_ACQUISITION, 0, NULL
    }));

    LONGS_EQUAL(ADQR_EOK, digitizer.WaitForMessage(msg, 100));
    LONGS_EQUAL(MESSAGE_ID_NEW_STATE, msg.id);
    LONGS_EQUAL(STATE_CONFIGURATION, msg.status);

    printf("Stopping.\n");

    LONGS_EQUAL(ADQR_EOK, digitizer.Stop());
}
