#ifndef MOCK_SYSTEM_MANAGER_H_BINPXO
#define MOCK_SYSTEM_MANAGER_H_BINPXO

#include "message_thread.h"
#include <cstdint>

struct SystemManagerMessage
{
    SystemManagerMessage() = default;

    /* Create a message w/o any data. */
    SystemManagerMessage(uint16_t cmd)
        : cmd(cmd)
        , data{}
    {}

    /* Create a message with both a command and data. */
    SystemManagerMessage(uint16_t cmd, std::vector<uint8_t> &data)
        : cmd(cmd)
        , data(data)
    {}

    /* Create a message with both a command and data. */
    SystemManagerMessage(uint16_t cmd, void *buffer, size_t len)
        : cmd(cmd)
        , data{}
    {
        if (buffer != NULL)
        {
            const auto ldata = static_cast<uint8_t *>(buffer);
            data.insert(data.end(), ldata, ldata + len);
        }
    }

    /* Create a message with just data and data. */
    SystemManagerMessage(std::vector<uint8_t> &data)
        : cmd()
        , data(data)
    {}

    uint16_t cmd;
    std::vector<uint8_t> data;
};

class MockSystemManager : public MessageThread<MockSystemManager, SystemManagerMessage>
{
public:
    MockSystemManager();
    ~MockSystemManager();

    /* Making copies of an object of this type is not allowed. */
    MockSystemManager(const MockSystemManager &other) = delete;
    MockSystemManager &operator=(const MockSystemManager &other) = delete;

    /* The main loop. */
    void MainLoop() override;

private:
    void HandleMessages();
    void UpdateSensors();
};

#endif
