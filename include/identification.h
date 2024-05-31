#pragma once

#include "message_thread.h"
#include "persistent_directories.h"
#include "embedded_python_thread.h"
#include "digitizer.h"

struct IdentificationMessage
{
    void *handle;
    std::vector<std::shared_ptr<Digitizer>> digitizers;
};

class Identification : public MessageThread<Identification, IdentificationMessage>
{
public:
    Identification(const PersistentDirectories &persistent_directories,
                   std::shared_ptr<EmbeddedPythonThread> python);
    void MainLoop() override;

private:
    /* A reference to the object we query for persistent directories. */
    const PersistentDirectories &m_persistent_directories;

    /* A reference to the shared embedded Python session object (passed to digitizers). */
    const std::shared_ptr<EmbeddedPythonThread> m_python;
};
