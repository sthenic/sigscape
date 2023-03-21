#pragma once

#include "message_thread.h"

#include <string>
#include <filesystem>

enum class FileWatcherMessageId
{
    FILE_CREATED,
    FILE_DELETED,
    FILE_UPDATED,
    FILE_DOES_NOT_EXIST,
    UPDATE_FILE
};

struct FileWatcherMessage
{
    FileWatcherMessage() = default;
    FileWatcherMessage(FileWatcherMessageId id)
        : id(id)
        , contents(NULL)
    {}

    FileWatcherMessage(FileWatcherMessageId id, std::string &&contents)
        : id(id)
        , contents(std::make_shared<std::string>(std::move(contents)))
    {}

    FileWatcherMessage(FileWatcherMessageId id, std::shared_ptr<std::string> contents)
        : id(id)
        , contents(contents)
    {}

    FileWatcherMessageId id;
    std::shared_ptr<std::string> contents;
};

class FileWatcher : public MessageThread<FileWatcher, FileWatcherMessage>
{
public:
    FileWatcher(const std::string &path);
    const std::string &GetPath();

    void MainLoop() override;

private:
    std::string m_path;
    std::filesystem::file_time_type m_timestamp;
    bool m_is_watching;

    void ReadContents(std::string &str);
    void WriteContents(const std::string &str);
    void HandleMessages();
};
