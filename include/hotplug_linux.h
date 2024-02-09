/* Hotplug events on Linux using libudev. */
#pragma once

#include "hotplug.h"
#include <libudev.h>

class HotplugLinux : public Hotplug
{
public:
    HotplugLinux();
    ~HotplugLinux() override;

    void MainLoop() override;
    int Stop() override;

private:
    struct udev *m_handle;
    int m_nof_devices;

    void CheckForEvents();
    int CreateHandle();
    void DestroyHandle();
};
