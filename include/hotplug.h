/* This is an abstract base class to define a common interface for hotplug
   events on different platforms. */

#pragma once

#include "message_thread.h"

enum class HotplugEvent
{
    CONNECT,
    DISCONNECT,
};

class Hotplug : public MessageThread<Hotplug, HotplugEvent>
{};
