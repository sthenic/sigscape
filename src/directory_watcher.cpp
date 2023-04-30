#include "directory_watcher.h"

#include <algorithm>

DirectoryWatcher::DirectoryWatcher(const std::string &path, const std::string &extension_filter)
    : m_path(path)
    , m_extension_filter(extension_filter)
    , m_files{}
{
#if defined(_WIN32)
    /* Normalize the path to feature the Windows-preferred path separator. */
    std::replace(m_path.begin(), m_path.end(), '/', '\\');
#endif
}

const std::string &DirectoryWatcher::GetPath()
{
    return m_path;
}

void DirectoryWatcher::MainLoop()
{
    /* Avoid watching an empty path. */
    if (m_path.empty())
    {
        fprintf(stderr, "The directory watcher cannot watch an empty path.\n");
        m_thread_exit_code = SCAPE_EINTERNAL;
        return;
    }

    m_thread_exit_code = SCAPE_EOK;

    for (;;)
    {
        if (std::filesystem::exists(m_path))
        {
            if (std::filesystem::is_directory(m_path))
            {
                /* Assume every tracked file should be erased and disprove this
                   by traversing the directory. */
                for (auto &[path, state] : m_files)
                    state.should_remove = true;

                for (auto const &entry : std::filesystem::directory_iterator{m_path})
                {
                    if (!entry.is_regular_file())
                        continue;
                    if (!m_extension_filter.empty() && entry.path().extension() != m_extension_filter)
                        continue;

                    auto timestamp = std::filesystem::last_write_time(entry.path());
                    auto match = m_files.find(entry.path());

                    if (match == m_files.end())
                    {
                        /* This is a new file. */
                        m_files.emplace(entry.path(), FileState{false, entry.path(), timestamp});
                        m_read_queue.EmplaceWrite(
                            DirectoryWatcherMessageId::FILE_CREATED, entry.path());
                    }
                    else if (timestamp != match->second.timestamp)
                    {
                        /* The file has been updated. */
                        match->second.timestamp = timestamp;
                        match->second.should_remove = false;
                        m_read_queue.EmplaceWrite(DirectoryWatcherMessageId::FILE_UPDATED,
                                                  entry.path());
                    }
                    else
                    {
                        match->second.should_remove = false;
                    }
                }

                for (auto it = m_files.begin(); it != m_files.end(); )
                {
                    if (it->second.should_remove)
                    {
                        m_read_queue.EmplaceWrite(DirectoryWatcherMessageId::FILE_DELETED,
                                                  it->second.path);
                        it = m_files.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
        }
        else
        {
            /* Directory doesn't exists. If we were tracking files, it was just erased. */
            for (const auto &[path, state] : m_files)
                m_read_queue.EmplaceWrite(DirectoryWatcherMessageId::FILE_DELETED, path);

            m_files.clear();
        }

        /* We implement the sleep using the stop event to be able to immediately
           react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::milliseconds(1000)) == std::future_status::ready)
            break;
    }
}
