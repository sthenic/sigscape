#include "persistent_configuration.h"
#include "fmt/format.h"

#include <cstdlib>
#include <filesystem>

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
            fprintf(stderr, "Failed to create directory '%s': %s.\n", path.c_str(), e.what());
            return false;
        }
    }

    /* TODO: Test writeable? */
    return true;
}

PersistentConfiguration::PersistentConfiguration()
    : m_directory()
    , m_imgui_configuration_file("imgui.ini")
{
#if defined(__APPLE__)
    /* FIXME: Linux rules apply? */
    #error "NOT IMPLEMENTED"
#elif defined(_WIN32)
    m_directory = GetEnvOrDefault("APPDATA") + SUFFIX;
    ValidateDirectoryCreateIfNeeded(m_directory);
#else
    /* If XDG_CONFIG_HOME is set, we use that, otherwise we default to
       $HOME/.config/adq-rapid. If we fail to validate the directory, this _has_
       to be because of XDG_CONFIG_HOME not pointing to a writable location.
       Again, we fall back to HOME/.config. */
    const auto default_prefix = GetEnvOrDefault("HOME") + "/.config";
    m_directory = GetEnvOrDefault("XDG_CONFIG_HOME", default_prefix) + SUFFIX;

    if (!ValidateDirectoryCreateIfNeeded(m_directory))
    {
        m_directory = default_prefix + SUFFIX;
        ValidateDirectoryCreateIfNeeded(m_directory);
    }
#endif
    printf("Using persistent configuration directory '%s'.\n", m_directory.c_str());
    m_imgui_configuration_file = m_directory + "/imgui.ini";
}

const std::string &PersistentConfiguration::GetDirectory()
{
    return m_directory;
}

const char *PersistentConfiguration::GetImGuiInitializationFile()
{
    return m_imgui_configuration_file.c_str();
}
