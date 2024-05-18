#pragma once

#include "message_thread.h"
#include "system_manager.h"

#include <cstdint>
#include <map>
#include <random>

struct SystemManagerMessage
{
    SystemManagerMessage() = default;

    SystemManagerMessage(int result)
        : cmd{}
        , data{}
        , result(result)
    {}

    SystemManagerMessage(SystemManagerCommand cmd, const void *buffer, size_t len)
        : cmd{cmd}
        , data{}
        , result(0)
    {
        if (buffer != NULL)
        {
            auto ldata = static_cast<const uint8_t *>(buffer);
            data.insert(data.end(), ldata, ldata + len);
        }
    }

    SystemManagerMessage(const void *buffer, size_t len, int result = 0)
        : cmd{}
        , data{}
        , result(result)
    {
        if (buffer != NULL)
        {
            auto ldata = static_cast<const uint8_t *>(buffer);
            data.insert(data.end(), ldata, ldata + len);
        }
    }

    SystemManagerCommand cmd{};
    std::vector<uint8_t> data{};
    int result{};
};

class MockSystemManager : public MessageThread<MockSystemManager, SystemManagerMessage>
{
public:
    MockSystemManager();
    ~MockSystemManager() override;

    /* Making copies of an object of this type is not allowed. */
    MockSystemManager(const MockSystemManager &other) = delete;
    MockSystemManager &operator=(const MockSystemManager &other) = delete;

    /* The main loop. */
    void MainLoop() override;

private:
    std::default_random_engine m_random_generator;
    std::vector<uint32_t> m_sensor_map;
    std::vector<uint32_t> m_boot_map;
    std::map<uint32_t, SystemManagerBootInformation> m_boot_information;
    std::map<uint32_t, SystemManagerSensorGroupInformation> m_sensor_group_information;
    std::map<uint32_t, SystemManagerSensorInformation> m_sensor_information;
    std::map<uint32_t, std::normal_distribution<float>> m_sensors;

    int HandleMessage();
};
