#include "file_watcher.h"
#include "log.h"

#include <fstream>

FileWatcher::FileWatcher(const std::filesystem::path &path)
    : m_path{path}
    , m_timestamp{}
    , m_is_watching{false}
    , m_ignore_next_update{false}
{
}

FileWatcher::~FileWatcher()
{
    Stop();
}

const std::filesystem::path &FileWatcher::GetPath()
{
    return m_path;
}

void FileWatcher::MainLoop()
{
    /* Avoid watching an empty path. */
    if (m_path.empty())
    {
        Log::log->error("The file watcher cannot watch an empty path.");
        m_thread_exit_code = SCAPE_EINTERNAL;
        return;
    }

    Log::log->trace("Starting file watcher for '{}'.", m_path.string());
    m_thread_exit_code = SCAPE_EOK;

    /* Before we enter the main loop, we check if the file exists. If it
       doesn't, we emit the `FILE_DOES_NOT_EXIST` message. Message should only
       be emitted once, which is why it sits outside the loop. */
    if (!std::filesystem::exists(m_path))
        _EmplaceMessage(FileWatcherMessageId::FILE_DOES_NOT_EXIST);

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
                std::string contents{};
                ReadContents(contents);
                _EmplaceMessage(FileWatcherMessageId::FILE_CREATED, std::move(contents));
            }
            else if (timestamp != m_timestamp)
            {
                /* The file has been changed, we only read the contents and emit
                   a message unless instructed to ignore the next update
                   (usually because this application is responsible for the
                   change so we don't have to synchronize again). */
                m_timestamp = timestamp;
                if (m_ignore_next_update)
                {
                    m_ignore_next_update = false;
                }
                else
                {
                    std::string contents{};
                    ReadContents(contents);
                    _EmplaceMessage(FileWatcherMessageId::FILE_UPDATED, std::move(contents));
                }
            }
        }
        else if (m_is_watching)
        {
            /* File was erased, emit a message. */
            m_is_watching = false;
            m_timestamp = std::filesystem::file_time_type();
            _EmplaceMessage(FileWatcherMessageId::FILE_DELETED);
        }

        /* Handle any incoming messages. */
        HandleMessages();

        /* We implement the sleep using the stop event to be able to immediately
           react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            break;
    }

    Log::log->trace("Stopping file watcher for '{}'.", m_path.string());
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
    while (SCAPE_EOK == _WaitForMessage(message, 250))
    {
        switch (message.id)
        {
        case FileWatcherMessageId::UPDATE_FILE_IGNORE:
            m_ignore_next_update = true;
            /* FALLTHROUGH */
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
