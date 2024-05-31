#pragma once

#include "error.h"
#include "message_thread.h"
#include <string>
#include <filesystem>

enum class EmbeddedPythonMessageId
{
    IS_PYADQ_COMPATIBLE,
    ADD_TO_PATH,
    HAS_MAIN,
    CALL_MAIN,
};

struct EmbeddedPythonMessage
{
    EmbeddedPythonMessage() = default;
    EmbeddedPythonMessage(EmbeddedPythonMessageId id)
        : id{id}
    {}

    EmbeddedPythonMessage(EmbeddedPythonMessageId id, const std::string &str)
        : id{id}
        , str{str}
    {}

    EmbeddedPythonMessage(EmbeddedPythonMessageId id, const std::string &str, void *handle, int index)
        : id{id}
        , str{str}
        , handle{handle}
        , index{index}
    {}

    EmbeddedPythonMessageId id;
    std::string str{};
    void *handle{NULL};
    int index{0};
    int result{SCAPE_EOK};
};

class EmbeddedPythonThread : public MessageThread<EmbeddedPythonThread, EmbeddedPythonMessage>
{
public:
    EmbeddedPythonThread() = default;
    ~EmbeddedPythonThread() override;

    bool IsInitialized() const;
    void MainLoop() override;

    /* Convenience functions for the calling threads. */
    bool IsPyadqCompatible();
    int AddToPath(const std::string &directory);
    bool HasMain(const std::filesystem::path &path);
    int CallMain(const std::string &module, void *handle, int index, std::string &out);

private:
    void HandleMessages();
    void IsPyadqCompatible(const StampedMessage &message);
    void AddToPath(const StampedMessage &message);
    void HasMain(const StampedMessage &message);
    void CallMain(const StampedMessage &message);
};
