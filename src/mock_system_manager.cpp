#include "mock_system_manager.h"

MockSystemManager::MockSystemManager()
{}

MockSystemManager::~MockSystemManager()
{}

void MockSystemManager::MainLoop()
{
    m_thread_exit_code = ADQR_EOK;
    for (;;)
    {
        /* Handle any incoming messages. */
        HandleMessages();

        /* Update simulated sensors. */
        UpdateSensors();

        /* We implement the sleep using the stop event to be able to immediately
           react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::milliseconds(250)) == std::future_status::ready)
            break;
    }
}

void MockSystemManager::HandleMessages()
{
    /* Empty the inwards facing message queue. */
    SystemManagerMessage message;
    while (ADQR_EOK == m_write_queue.Read(message, 0))
    {
        /* FIXME: Implement */
        printf("Processing Sysman message w/ command 0x%04X.\n", message.cmd);
    }
}

void MockSystemManager::UpdateSensors()
{
    /* FIXME: Implement */
}
