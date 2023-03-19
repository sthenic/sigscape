/* A wrapper around managing the persistent configuration. */

#pragma once

#include <string>

class PersistentConfiguration
{
public:
    PersistentConfiguration();
    ~PersistentConfiguration() = default;

    /* Get the directory used to store persistent configuration files. */
    const std::string &GetDirectory();

    /* Get the ImGui initialization file. */
    const char *GetImGuiInitializationFile();

private:
    std::string m_directory;

    /* Static storage for the ImGui configuration file to be able to have 'const char *'
       as the return type of GetImGuiInitializationFile(). */
    std::string m_imgui_configuration_file;

    inline static const std::string SUFFIX = "/sigscape";
};
