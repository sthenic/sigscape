/* A wrapper around managing the persistent configuration. */

#pragma once

#include <string>

class PersistentDirectories
{
public:
    PersistentDirectories();
    ~PersistentDirectories() = default;

    /* Get the directory used to store persistent configuration files. */
    const std::string &GetConfigurationDirectory() const;

    /* Get the directory used to store persistent data files. */
    const std::string &GetDataDirectory() const;

    /* Get the directory used to store screenshots (subdirectory to the persistent data). */
    const std::string &GetScreenshotDirectory() const;

    /* Get the directory used to store log files (subdirectory to the persistent data). */
    const std::string &GetLogDirectory() const;

    /* Get the directory used to store Python scripts (subdirectory to the persistent data). */
    const std::string &GetPythonDirectory() const;

    /* Get the ImGui initialization file. */
    const char *GetImGuiInitializationFile() const;

private:
    std::string m_configuration_directory;
    std::string m_data_directory;
    std::string m_screenshot_directory;
    std::string m_log_directory;
    std::string m_python_directory;

    /* Static storage for the ImGui configuration file to be able to have 'const char *'
       as the return type of GetImGuiInitializationFile(). */
    std::string m_imgui_configuration_file;

    inline static const std::string SUFFIX = "/sigscape";
};
