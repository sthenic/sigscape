#pragma once

#include "message_thread.h"
#include "digitizer.h"

struct IdentificationMessage
{
    void *handle;
    uint32_t revision;
    std::vector<std::shared_ptr<Digitizer>> digitizers;
};

class Identification : public MessageThread<Identification, IdentificationMessage>
{
public:
    Identification() = default;
    void MainLoop() override;
};
