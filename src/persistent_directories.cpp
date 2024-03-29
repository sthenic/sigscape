#include "persistent_directories.h"
#include "log.h"

#include <cstdlib>
#include <filesystem>
#include <algorithm>

static std::string GetEnvOrDefault(const std::string &key, const std::string &default_value = "")
{
    auto value = std::getenv(key.c_str());
    if (value != NULL)
        return std::string(value);
    else
        return default_value;
}

static bool ValidateDirectoryCreateIfNeeded(const std::string &path)
{
    if (!std::filesystem::exists(path))
    {
        try
        {
            return std::filesystem::create_directories(path);
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            Log::log->error("Failed to create directory '{}': {}.", path.c_str(), e.what());
            return false;
        }
    }

    /* TODO: Test writeable? */
    return true;
}

PersistentDirectories::PersistentDirectories()
    : m_configuration_directory()
    , m_data_directory()
    , m_screenshot_directory()
    , m_log_directory()
    , m_imgui_configuration_file("imgui.ini")
{
#if defined(__APPLE__)
    /* TODO: Linux rules apply? */
    #error "NOT IMPLEMENTED"
#elif defined(_WIN32)
    /* Make sure to normalize the path to use the path separator preferred by Windows. */
    m_configuration_directory = GetEnvOrDefault("APPDATA") + SUFFIX + "/config";
    std::replace(m_configuration_directory.begin(), m_configuration_directory.end(), '/', '\\');
    ValidateDirectoryCreateIfNeeded(m_configuration_directory);

    m_data_directory = GetEnvOrDefault("APPDATA") + SUFFIX + "/share";
    std::replace(m_data_directory.begin(), m_data_directory.end(), '/', '\\');
    ValidateDirectoryCreateIfNeeded(m_data_directory);

    m_screenshot_directory = m_data_directory + "/screenshots";
    std::replace(m_screenshot_directory.begin(), m_screenshot_directory.end(), '/', '\\');
    ValidateDirectoryCreateIfNeeded(m_screenshot_directory);

    m_log_directory = m_data_directory + "/logs";
    std::replace(m_log_directory.begin(), m_log_directory.end(), '/', '\\');
    ValidateDirectoryCreateIfNeeded(m_log_directory);

    m_python_directory = m_data_directory + "/python";
    std::replace(m_python_directory.begin(), m_python_directory.end(), '/', '\\');
    ValidateDirectoryCreateIfNeeded(m_python_directory);
#else
    /* If XDG_CONFIG_HOME is set, we use that, otherwise we default to
       $HOME/.config/sigscape. If we fail to validate the directory, this _has_
       to be because of XDG_CONFIG_HOME not pointing to a writable location.
       Again, we fall back to $HOME/.config. */
    const auto default_configuration_prefix = GetEnvOrDefault("HOME") + "/.config";
    m_configuration_directory = GetEnvOrDefault("XDG_CONFIG_HOME", default_configuration_prefix) + SUFFIX;

    if (!ValidateDirectoryCreateIfNeeded(m_configuration_directory))
    {
        m_configuration_directory = default_configuration_prefix + SUFFIX;
        ValidateDirectoryCreateIfNeeded(m_configuration_directory);
    }

    /* If XDG_DATA_HOME is set, we use that, otherwise we default to
       $HOME/.local/share/sigscape. If we fail to validate the directory, this
       _has_ to be because of XDG_DATA_HOME not pointing to a writable location.
       Again, we fall back to $HOME/.local/share. */
    const auto default_data_prefix = GetEnvOrDefault("HOME") + "/.local/share";
    m_data_directory = GetEnvOrDefault("XDG_DATA_HOME", default_data_prefix) + SUFFIX;

    if (!ValidateDirectoryCreateIfNeeded(m_data_directory))
    {
      m_data_directory = default_data_prefix + SUFFIX;
      ValidateDirectoryCreateIfNeeded(m_data_directory);
    }

    m_screenshot_directory = m_data_directory + "/screenshots";
    ValidateDirectoryCreateIfNeeded(m_screenshot_directory);

    m_log_directory = m_data_directory + "/logs";
    ValidateDirectoryCreateIfNeeded(m_log_directory);

    m_python_directory = m_data_directory + "/python";
    ValidateDirectoryCreateIfNeeded(m_python_directory);
#endif
    m_imgui_configuration_file = m_configuration_directory + "/imgui.ini";
}

const std::string &PersistentDirectories::GetConfigurationDirectory() const
{
    return m_configuration_directory;
}

const std::string &PersistentDirectories::GetDataDirectory() const
{
    return m_data_directory;
}

const std::string &PersistentDirectories::GetScreenshotDirectory() const
{
    return m_screenshot_directory;
}

const std::string &PersistentDirectories::GetLogDirectory() const
{
    return m_log_directory;
}

const std::string &PersistentDirectories::GetPythonDirectory() const
{
    return m_python_directory;
}

const char *PersistentDirectories::GetImGuiInitializationFile() const
{
    return m_imgui_configuration_file.c_str();
}
