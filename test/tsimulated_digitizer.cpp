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
    LONGS_EQUAL(DigitizerMessageId::NEW_STATE, msg.id);
    LONGS_EQUAL(DigitizerState::NOT_ENUMERATED, msg.state);

    LONGS_EQUAL(ADQR_EOK, digitizer.WaitForMessage(msg, 100));
    LONGS_EQUAL(DigitizerMessageId::SETUP_STARTING, msg.id);

    LONGS_EQUAL(ADQR_EOK, digitizer.WaitForMessage(msg, 1000));
    LONGS_EQUAL(DigitizerMessageId::SETUP_OK, msg.id);

    LONGS_EQUAL(ADQR_EOK, digitizer.PushMessage(
        DigitizerMessage(DigitizerMessageId::START_ACQUISITION)
    ));

    LONGS_EQUAL(ADQR_EOK, digitizer.WaitForMessage(msg, 100));
    LONGS_EQUAL(DigitizerMessageId::NEW_STATE, msg.id);
    LONGS_EQUAL(DigitizerState::ACQUISITION, msg.state);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    LONGS_EQUAL(ADQR_EOK, digitizer.PushMessage(
        DigitizerMessage(DigitizerMessageId::STOP_ACQUISITION)
    ));

    LONGS_EQUAL(ADQR_EOK, digitizer.WaitForMessage(msg, 500));
    LONGS_EQUAL(DigitizerMessageId::NEW_STATE, msg.id);
    LONGS_EQUAL(DigitizerState::CONFIGURATION, msg.state);

    printf("Stopping.\n");

    LONGS_EQUAL(ADQR_EOK, digitizer.Stop());
}
