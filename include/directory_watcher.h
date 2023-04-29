#pragma once

#include "message_thread.h"

#include <string>
#include <filesystem>
#include <vector>
#include <map>

enum class DirectoryWatcherMessageId
{
    FILE_CREATED,
    FILE_DELETED,
    FILE_UPDATED
};

struct DirectoryWatcherMessage
{
    DirectoryWatcherMessage() = default;
    DirectoryWatcherMessage(DirectoryWatcherMessageId id)
        : id(id)
        , filename()
    {}

    DirectoryWatcherMessage(DirectoryWatcherMessageId id, const std::string &filename)
        : id(id)
        , filename(filename)
    {}

    DirectoryWatcherMessageId id;
    std::string filename;
};

class DirectoryWatcher : public MessageThread<DirectoryWatcher, DirectoryWatcherMessage>
{
public:
    DirectoryWatcher(const std::string &path);
    const std::string &GetPath();

    void MainLoop() override;

private:
    struct FileState
    {
        bool should_remove;
        std::string path;
        std::filesystem::file_time_type timestamp;
    };

    std::string m_path;
    std::map<std::string, FileState> m_files;
};
