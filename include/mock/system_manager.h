#ifndef MOCK_SYSTEM_MANAGER_H_BINPXO
#define MOCK_SYSTEM_MANAGER_H_BINPXO

#include "message_thread.h"
#include "mock/../system_manager.h"

#include <cstdint>
#include <map>
#include <random>

struct SystemManagerMessage
{
    SystemManagerMessage() = default;

    SystemManagerMessage(SystemManagerCommand cmd)
        : cmd(cmd)
        , data{}
        , result(0)
    {}

    SystemManagerMessage(int result)
        : cmd()
        , data{}
        , result(result)
    {}

    /* FIXME: Maybe remove? */
    SystemManagerMessage(SystemManagerCommand cmd, std::vector<uint8_t> &data)
        : cmd(cmd)
        , data(data)
        , result(0)
    {}

    SystemManagerMessage(SystemManagerCommand cmd, const void *buffer, size_t len)
        : cmd(cmd)
        , data{}
        , result(0)
    {
        if (buffer != NULL)
        {
            auto ldata = static_cast<const uint8_t *>(buffer);
            data.insert(data.end(), ldata, ldata + len);
        }
    }

    /* FIXME: Maybe remove? */
    SystemManagerMessage(std::vector<uint8_t> &data, int result = 0)
        : cmd()
        , data(data)
        , result(result)
    {}

    SystemManagerMessage(const void *buffer, size_t len, int result = 0)
        : cmd()
        , data{}
        , result(result)
    {
        if (buffer != NULL)
        {
            auto ldata = static_cast<const uint8_t *>(buffer);
            data.insert(data.end(), ldata, ldata + len);
        }
    }

    SystemManagerCommand cmd;
    std::vector<uint8_t> data;
    int result;
};

class MockSystemManager : public MessageThread<MockSystemManager, SystemManagerMessage>
{
public:
    MockSystemManager();
    ~MockSystemManager() = default;

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

#endif
