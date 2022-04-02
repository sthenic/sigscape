#ifndef FILE_WATCHER_H_XPOJEM
#define FILE_WATCHER_H_XPOJEM

#include "message_thread.h"

#include <string>
#include <filesystem>

enum class FileWatcherMessageId
{
    FILE_CREATED,
    FILE_DELETED,
    FILE_UPDATED,
    UPDATE_FILE
};

struct FileWatcherMessage
{
    FileWatcherMessageId id;
    std::shared_ptr<std::string> contents;
};

class FileWatcher : public MessageThread<FileWatcher, struct FileWatcherMessage>
{
public:
    FileWatcher(const std::string &path);

    void MainLoop() override;

private:
    std::string m_path;
    std::filesystem::file_time_type m_timestamp;
    bool m_is_watching;

    void ReadContents(std::string &str);
    void WriteContents(const std::string &str);
    void HandleMessages();
};

#endif
