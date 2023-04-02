#include "hotplug_linux.h"

#include <cstdio>
#include <exception>

/* A private exception type that we use for simplified control flow within this class. */
class HotplugLinuxException : public std::runtime_error
{
public:
    HotplugLinuxException(const std::string &str) : std::runtime_error(str) {};
};

HotplugLinux::HotplugLinux()
    : m_handle(NULL)
    , m_nof_devices(0)
{}

HotplugLinux::~HotplugLinux()
{
    Stop();
}

void HotplugLinux::CheckForEvents()
{
    struct udev_enumerate *enumerator = udev_enumerate_new(m_handle);
    try
    {
        /* Filter the PCI subsystem for devices w/ vendor id 0x1b37. */
        if (enumerator == NULL)
            throw HotplugLinuxException("Failed to create libudev enumerator.");

        if (udev_enumerate_add_match_subsystem(enumerator, "pci") < 0)
            throw HotplugLinuxException("Failed to add libudev enumerator filter.");

        if (udev_enumerate_add_match_sysattr(enumerator, "vendor", "0x1b37") < 0)
            throw HotplugLinuxException("Failed to add libudev enumerator vendor filter.");

        udev_enumerate_scan_devices(enumerator);

        struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerator);
        struct udev_list_entry *entry;
        int nof_devices = 0;
        udev_list_entry_foreach(entry, devices)
            nof_devices++;

        if (nof_devices > m_nof_devices)
            m_read_queue.EmplaceWrite(HotplugEvent::CONNECT);
        else if (nof_devices < m_nof_devices)
            m_read_queue.EmplaceWrite(HotplugEvent::DISCONNECT);

        m_nof_devices = nof_devices;
    }
    catch (const HotplugLinuxException& e)
    {
        if (enumerator != NULL)
            udev_enumerate_unref(enumerator);
        fprintf(stderr, "%s\n", e.what());
    }
}

int HotplugLinux::CreateHandle()
{
    if (m_handle != NULL)
    {
        fprintf(stderr, "libudev is already initialized.\n");
        return SCAPE_ENOTREADY;
    }

    m_handle = udev_new();
    if (m_handle == NULL)
    {
        fprintf(stderr, "Failed to create libudev handle.\n");
        return SCAPE_EINTERNAL;
    }

    return SCAPE_EOK;
}

void HotplugLinux::DestroyHandle()
{
    if (m_handle != NULL)
        udev_unref(m_handle);
    m_handle = NULL;
}

void HotplugLinux::MainLoop()
{
    m_thread_exit_code = CreateHandle();
    if (m_thread_exit_code != SCAPE_EOK)
    {
        DestroyHandle();
        return;
    }

    for (;;)
    {
        /* Check for any hotplug events. */
        CheckForEvents();

        /* We implement the sleep using the stop event to be able to immediately
           react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::milliseconds(1000)) == std::future_status::ready)
            break;
    }
}

int HotplugLinux::Stop()
{
    /* We manually stop the thread (calling the base class `Stop()`) before
       destroying the udev objects. Otherwise, the thread may be using them
       while they are being destroyed. */
    int result = Hotplug::Stop();
    DestroyHandle();
    return result;
}
