/* A wrapper around managing the persistent configuration. */

#pragma once

#include <filesystem>

class PersistentDirectories
{
public:
    PersistentDirectories();
    ~PersistentDirectories() = default;

    /* Get the directory used to store persistent configuration files. */
    const std::filesystem::path &GetConfigurationDirectory() const;

    /* Get the directory used to store persistent data files. */
    const std::filesystem::path &GetDataDirectory() const;

    /* Get the directory used to store screenshots (subdirectory to the persistent data). */
    const std::filesystem::path &GetScreenshotDirectory() const;

    /* Get the directory used to store log files (subdirectory to the persistent data). */
    const std::filesystem::path &GetLogDirectory() const;

    /* Get the directory used to store Python scripts (subdirectory to the persistent data). */
    const std::filesystem::path &GetPythonDirectory() const;

    /* Get the ImGui initialization file. */
    const char *GetImGuiInitializationFile() const;

private:
    std::filesystem::path m_configuration_directory{};
    std::filesystem::path m_data_directory{};
    std::filesystem::path m_screenshot_directory{};
    std::filesystem::path m_log_directory{};
    std::filesystem::path m_python_directory{};

    /* Static storage for the ImGui configuration file to be able to have 'const char *'
       as the return type of GetImGuiInitializationFile(). */
    std::filesystem::path m_imgui_configuration_file{};

    static constexpr const char *SUFFIX = "sigscape";

    static std::string GetEnvOrDefault(const std::string &key, const std::string &default_value = "");
    static bool ValidateDirectoryCreateIfNeeded(const std::filesystem::path &path);
};
