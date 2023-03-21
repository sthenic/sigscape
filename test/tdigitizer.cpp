#include <thread>
#include <chrono>
#include "digitizer.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(Digitizer)
{
    std::unique_ptr<Digitizer> digitizer;
    MockAdqApi mock_adqapi;

    void setup()
    {
        mock_adqapi.AddDigitizer("SPD-SIM01", 2, PID_ADQ32);
        digitizer = std::make_unique<Digitizer>(&mock_adqapi, 1);
    }

    void teardown()
    {
    }
};

TEST(Digitizer, Initialize)
{
    LONGS_EQUAL(SCAPE_EOK, digitizer->Start());

    struct DigitizerMessage msg;
    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, 100));
    LONGS_EQUAL(DigitizerMessageId::NEW_STATE, msg.id);
    LONGS_EQUAL(DigitizerState::NOT_INITIALIZED, msg.state);

    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, 100));
    LONGS_EQUAL(DigitizerMessageId::ENUMERATING, msg.id);

    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, 1000));
    LONGS_EQUAL(DigitizerMessageId::SETUP_OK, msg.id);
    STRCMP_EQUAL("SPD-SIM01", msg.str->c_str());

    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, 1000));
    LONGS_EQUAL(DigitizerMessageId::NEW_STATE, msg.id);
    LONGS_EQUAL(DigitizerState::IDLE, msg.state);

    LONGS_EQUAL(SCAPE_EOK, digitizer->PushMessage(
        DigitizerMessage(DigitizerMessageId::START_ACQUISITION)
    ));

    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, 100));
    LONGS_EQUAL(DigitizerMessageId::NEW_STATE, msg.id);
    LONGS_EQUAL(DigitizerState::ACQUISITION, msg.state);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    LONGS_EQUAL(SCAPE_EOK, digitizer->PushMessage(
        DigitizerMessage(DigitizerMessageId::STOP_ACQUISITION)
    ));

    LONGS_EQUAL(SCAPE_EOK, digitizer->WaitForMessage(msg, 500));
    LONGS_EQUAL(DigitizerMessageId::NEW_STATE, msg.id);
    LONGS_EQUAL(DigitizerState::IDLE, msg.state);

    LONGS_EQUAL(SCAPE_EOK, digitizer->Stop());
}
