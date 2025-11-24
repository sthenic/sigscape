/* Hotplug events on Windows using cfgmgr32. */
#pragma once

#include "hotplug.h"

#include <Windows.h>
#include <cfgmgr32.h>
#include <vector>
#include <string>

class HotplugWindows : public Hotplug
{
public:
    HotplugWindows();
    ~HotplugWindows() override;

    void MainLoop() override;

private:
    static constexpr char *GUID_PCI = "{46c58a4e-05fb-441e-84f5-55aa37e4b412}";
    static constexpr char *GUID_USB = "{9f4ef953-49da-47a0-a3e9-820fbfad4a24}";
    size_t m_nof_devices;

    static std::vector<std::string> SplitDeviceList(const std::string &list);
    size_t Enumerate(const char *guid);
    void CheckForEvents();
};
