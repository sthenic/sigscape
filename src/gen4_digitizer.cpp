#include "gen4_digitizer.h"

Gen4Digitizer::Gen4Digitizer(void *handle, int index)
    : Digitizer(handle, index)
{}

void Gen4Digitizer::MainLoop()
{
    /* When the main loop is started, we assume ownership of the digitizer with
       the identifier we were given when this object was constructed. We begin
       by enumerating the digitizer, completing its initial setup procedure. */
    m_read_queue.Write({DigitizerMessageId::NEW_STATE, DigitizerState::NOT_ENUMERATED});
    m_read_queue.Write({DigitizerMessageId::ENUMERATING});

    /* Performing this operation in a thread safe manner requires that
       ADQControlUnit_OpenDeviceInterface() has been called (and returned
       successfully) in a single thread process. */
    int result = ADQControlUnit_SetupDevice(m_id.handle, m_id.index);
    if (result != 1)
    {
        m_read_queue.Write({DigitizerMessageId::SETUP_FAILED});
        m_thread_exit_code = ADQR_EINTERNAL;
        return;
    }
    m_read_queue.Write({DigitizerMessageId::SETUP_OK});

    /* FIXME: Start file watchers, after reading the digitizer's constant parameters. */

    m_thread_exit_code = ADQR_EOK;
    for (;;)
    {
        ProcessMessages();
        ProcessWatcherMessages();

        if (m_should_stop.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            break;
    }
}

void Gen4Digitizer::ProcessMessages()
{
    /* FIXME: Implement */
}
