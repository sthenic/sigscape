#pragma once

#include "message_thread.h"
#include "persistent_directories.h"
#include "digitizer.h"

struct IdentificationMessage
{
    void *handle;
    uint32_t revision;
    bool compatible;
    std::vector<std::shared_ptr<Digitizer>> digitizers;
};

class Identification : public MessageThread<Identification, IdentificationMessage>
{
public:
    Identification(const PersistentDirectories &persistent_directories);

    void SetLogDirectory(const std::string &log_directory);
    void MainLoop() override;

private:
    /* A reference to the object we query for persistent directories. */
    const PersistentDirectories &m_persistent_directories;
};
