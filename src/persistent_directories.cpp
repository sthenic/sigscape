#include "persistent_directories.h"
#include "log.h"

#include <cstdlib>
#include <filesystem>

PersistentDirectories::PersistentDirectories()
    : m_imgui_configuration_file{"imgui.ini"}
{
#if defined(__APPLE__)
    /* TODO: Linux rules apply? */
    #error "NOT IMPLEMENTED"
#elif defined(_WIN32)

    /* When we construct the path, make sure to interpret the value of `APPDATA`
       using the OS's native format. */
    const std::filesystem::path appdata{
        GetEnvOrDefault("APPDATA"), std::filesystem::path::native_format};

    m_configuration_directory = appdata / SUFFIX / "config";
    ValidateDirectoryCreateIfNeeded(m_configuration_directory);

    m_data_directory = appdata / SUFFIX / "share";
    ValidateDirectoryCreateIfNeeded(m_data_directory);

#else
    auto ResolvePrefix = [](const std::string &key, const std::filesystem::path &default_prefix)
    {
        /* The resolver works by preferring the path that results from combining
           the value of the target environment variable `key` with the static
           prefix. If the environment variable is not set _or_ set but fails the
           validation (e.g. not pointing to a writable location), we fall back
           to the default prefix. */

        const std::filesystem::path prefix{GetEnvOrDefault(key, default_prefix)};

        auto resolved = prefix / SUFFIX;
        if (!ValidateDirectoryCreateIfNeeded(resolved))
        {
            resolved = default_prefix / SUFFIX;
            ValidateDirectoryCreateIfNeeded(resolved);
        }

        return resolved;
    };

    const std::filesystem::path home{GetEnvOrDefault("HOME")};
    m_configuration_directory = ResolvePrefix("XDG_CONFIG_HOME", home / ".config");
    m_data_directory = ResolvePrefix("XDG_DATA_HOME", home / ".local" / "share");

#endif

    m_screenshot_directory = m_data_directory / "screenshots";
    ValidateDirectoryCreateIfNeeded(m_screenshot_directory);

    m_log_directory = m_data_directory / "logs";
    ValidateDirectoryCreateIfNeeded(m_log_directory);

    m_python_directory = m_data_directory / "python";
    ValidateDirectoryCreateIfNeeded(m_python_directory);

    m_imgui_configuration_file = (m_configuration_directory / "imgui.ini").string();
}

const std::filesystem::path &PersistentDirectories::GetConfigurationDirectory() const
{
    return m_configuration_directory;
}

const std::filesystem::path &PersistentDirectories::GetDataDirectory() const
{
    return m_data_directory;
}

const std::filesystem::path &PersistentDirectories::GetScreenshotDirectory() const
{
    return m_screenshot_directory;
}

const std::filesystem::path &PersistentDirectories::GetLogDirectory() const
{
    return m_log_directory;
}

const std::filesystem::path &PersistentDirectories::GetPythonDirectory() const
{
    return m_python_directory;
}

const char *PersistentDirectories::GetImGuiInitializationFile() const
{
    return m_imgui_configuration_file.c_str();
}

std::string PersistentDirectories::GetEnvOrDefault(
    const std::string &key, const std::string &default_value)
{
    auto value = std::getenv(key.c_str());
    if (value != NULL)
        return std::string(value);
    else
        return default_value;
}

bool PersistentDirectories::ValidateDirectoryCreateIfNeeded(const std::filesystem::path &path)
{
    if (!std::filesystem::exists(path))
    {
        try
        {
            return std::filesystem::create_directories(path);
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            Log::log->error("Failed to create directory '{}': {}.", path.string(), e.what());
            return false;
        }
    }

    /* TODO: Test writeable? */
    return true;
}
