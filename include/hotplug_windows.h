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
    ~HotplugWindows() override = default;

    void MainLoop() override;

private:
    static constexpr char *GUID = "{46c58a4e-05fb-441e-84f5-55aa37e4b412}";
    size_t m_nof_devices;

    static std::vector<std::string> SplitDeviceList(const std::string &list);
    void CheckForEvents();
};
