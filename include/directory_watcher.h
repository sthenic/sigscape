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
        : id{id}
        , path{}
    {}

    DirectoryWatcherMessage(DirectoryWatcherMessageId id, const std::filesystem::path &path)
        : id{id}
        , path{path}
    {}

    DirectoryWatcherMessageId id;
    std::filesystem::path path;
};

class DirectoryWatcher : public MessageThread<DirectoryWatcher, DirectoryWatcherMessage>
{
public:
    DirectoryWatcher(const std::filesystem::path &path, const std::string &extension_filter = "");
    ~DirectoryWatcher() override;
    const std::filesystem::path &GetPath();

    void MainLoop() override;

private:
    struct FileState
    {
        bool should_remove;
        std::filesystem::path path;
        std::filesystem::file_time_type timestamp;
    };

    std::filesystem::path m_path;
    std::filesystem::path m_extension_filter;
    std::map<std::filesystem::path, FileState> m_files;
};
