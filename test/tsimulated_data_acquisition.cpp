#include <thread>
#include <chrono>
#include "simulated_data_acquisition.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(SimulatedDataAcquisition)
{
    SimulatedDataAcquisition acquisition;

    void setup()
    {
        acquisition.Initialize(1024, 2);
    }

    void teardown()
    {
        acquisition.Stop();
    }
};

TEST(SimulatedDataAcquisition, StartStop)
{
    LONGS_EQUAL(-1, acquisition.Stop());
    LONGS_EQUAL(0, acquisition.Start());
    LONGS_EQUAL(-1, acquisition.Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(750));
    LONGS_EQUAL(0, acquisition.Stop());
}
