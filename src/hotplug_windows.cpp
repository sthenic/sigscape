#include "hotplug_windows.h"
#include "log.h"

#include <cstdio>

HotplugWindows::HotplugWindows()
    : m_nof_devices(0)
{}

std::vector<std::string> HotplugWindows::SplitDeviceList(const std::string &list)
{
    std::vector<std::string> result{};

    /* The list is a sequence of NULL-terminated (NULL-separated) identifiers,
       where the end of the list is terminated by an extra NULL character. Thus,
       if `next` == `last` below, we've reached the end of the list. */
    size_t last = 0;
    for (;;)
    {
        size_t next = list.find('\0', last);
        if (next == std::string::npos || next == last)
            break;

        result.emplace_back(list.substr(last, next - last));
        last = next + 1;
    }

    return result;
}

void HotplugWindows::CheckForEvents()
{
    /* We check for events by polling the Windows kernel for devices
         - present on the system; and
         - matching the setup class GUID for a TSPD digitizer.

       We simply take the difference between the number of devices we got in the
       last call and the number we get from a new call. This obviously misses
       the case when a digitizer gets removed and added between polling events
       but that's so rare that it basically shouldn't happen in practice. */

    const ULONG LIST_FLAGS = CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT;
    ULONG list_size = 0;

    CONFIGRET result = CM_Get_Device_ID_List_SizeA(&list_size, GUID, LIST_FLAGS);
    if (result != CR_SUCCESS)
    {
        Log::log->error("CM_Get_Device_ID_List_SizeA failed, code {}.", result);
        return;
    }

    /* Allocate a string of the prescribed size (includes the NULL terminator). */
    auto list = std::string(list_size, '\0');

    result = CM_Get_Device_ID_ListA(GUID, (PZZSTR)list.data(), (ULONG)list.capacity(), LIST_FLAGS);
    if (result != CR_SUCCESS)
    {
        Log::log->error("CM_Get_Device_ID_ListA failed, code {}.", result);
        return;
    }

    auto devices = std::move(SplitDeviceList(list));
    if (devices.size() > m_nof_devices)
        m_read_queue.EmplaceWrite(HotplugEvent::CONNECT);
    else if (devices.size() < m_nof_devices)
        m_read_queue.EmplaceWrite(HotplugEvent::DISCONNECT);

    m_nof_devices = devices.size();
}

void HotplugWindows::MainLoop()
{
    m_thread_exit_code = SCAPE_EOK;
    Log::log->trace("Starting Windows hotplug event detector.");
    for (;;)
    {
        /* Check for any hotplug events. */
        CheckForEvents();

        /* We implement the sleep using the stop event to be able to immediately
           react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::milliseconds(1000)) == std::future_status::ready)
            break;
    }
    Log::log->trace("Stopping Windows hotplug event detector.");
}
