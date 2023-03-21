#include "file_watcher.h"

#include <fstream>
#include <algorithm>

FileWatcher::FileWatcher(const std::string &path)
    : m_path(path)
    , m_timestamp()
    , m_is_watching(false)
{
#if defined(_WIN32)
    /* Normalize the path to feature the Windows-preferred path separator. */
    std::replace(m_path.begin(), m_path.end(), '/', '\\');
#endif
}

const std::string &FileWatcher::GetPath()
{
    return m_path;
}

void FileWatcher::MainLoop()
{
    m_thread_exit_code = SCAPE_EOK;

    /* Before we enter the main loop, we check if the file exists. If it
       doesn't, we emit the `FILE_DOES_NOT_EXIST` message. Message should only
       be emitted once, which is why it sits outside the loop. */
    if (!std::filesystem::exists(m_path))
        m_read_queue.EmplaceWrite(FileWatcherMessageId::FILE_DOES_NOT_EXIST);

    for (;;)
    {
        if (std::filesystem::exists(m_path))
        {
            auto timestamp = std::filesystem::last_write_time(m_path);
            if (!m_is_watching)
            {
                /* The file has been created, read the contents in full and emit a message. */
                m_is_watching = true;
                m_timestamp = timestamp;
                std::string contents;
                ReadContents(contents);
                m_read_queue.EmplaceWrite(FileWatcherMessageId::FILE_CREATED, std::move(contents));
            }
            else if (timestamp != m_timestamp)
            {
                /* The file has been changed, read the contents in full and emit a message. */
                m_timestamp = timestamp;
                std::string contents;
                ReadContents(contents);
                m_read_queue.EmplaceWrite(FileWatcherMessageId::FILE_UPDATED, std::move(contents));
            }
        }
        else if (m_is_watching)
        {
            /* File was erased, emit a message. */
            m_is_watching = false;
            m_timestamp = std::filesystem::file_time_type();
            m_read_queue.EmplaceWrite(FileWatcherMessageId::FILE_DELETED);
        }

        /* Handle any incoming messages. */
        HandleMessages();

        /* We implement the sleep using the stop event to be able to immediately
           react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::milliseconds(250)) == std::future_status::ready)
            break;
    }
}

void FileWatcher::ReadContents(std::string &str)
{
    /* Create an input filestream, determine the file's size and read all its contents. */
    std::ifstream ifs(m_path, std::ios::in);
    const auto size = std::filesystem::file_size(m_path);
    str.reserve(size);
    str.resize(size);
    ifs.read(&str[0], size);
}

void FileWatcher::WriteContents(const std::string &str)
{
    /* Open an output filestream, truncating the current contents and write the input string. */
    std::ofstream ofs(m_path, std::ios::out | std::ios::trunc);
    ofs << str;
    ofs.close();
}

void FileWatcher::HandleMessages()
{
    /* Empty the inwards facing message queue. */
    FileWatcherMessage message;
    while (SCAPE_EOK == m_write_queue.Read(message, 0))
    {
        switch (message.id)
        {
        case FileWatcherMessageId::UPDATE_FILE:
            WriteContents(*message.contents);
            break;

        case FileWatcherMessageId::FILE_CREATED:
        case FileWatcherMessageId::FILE_DELETED:
        case FileWatcherMessageId::FILE_UPDATED:
        case FileWatcherMessageId::FILE_DOES_NOT_EXIST:
        default:
            break;
        }
    }
}
