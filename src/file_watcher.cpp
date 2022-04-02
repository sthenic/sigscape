#include "file_watcher.h"

#include <fstream>

FileWatcher::FileWatcher(const std::string &path)
    : m_path(path)
    , m_timestamp()
    , m_is_watching(false)
{
}

void FileWatcher::MainLoop()
{
    m_thread_exit_code = ADQR_EOK;
    for (;;)
    {
        std::filesystem::directory_entry file(m_path);
        if (file.exists())
        {
            auto timestamp = std::filesystem::last_write_time(file);
            if (!m_is_watching)
            {
                /* The file has been created, read the contents in full and emit a message. */
                m_is_watching = true;
                m_timestamp = timestamp;
                std::string contents;
                ReadContents(contents);
                m_read_queue.Write({FileWatcherMessageId::FILE_CREATED,
                                    std::make_shared<std::string>(std::move(contents))});
            }
            else if (timestamp != m_timestamp)
            {
                /* The file has been changed, read the contents in full and emit a message. */
                m_timestamp = timestamp;
                std::string contents;
                ReadContents(contents);
                m_read_queue.Write({FileWatcherMessageId::FILE_UPDATED,
                                    std::make_shared<std::string>(std::move(contents))});
            }
        }
        else if (m_is_watching)
        {
            /* File was erased, emit a message. */
            m_is_watching = false;
            m_timestamp = std::filesystem::file_time_type();
            m_read_queue.Write({FileWatcherMessageId::FILE_DELETED,
                                std::make_shared<std::string>("")});
        }

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
