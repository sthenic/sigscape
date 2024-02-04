#include "identification.h"
#include "log.h"

#include <utility>

Identification::Identification(const PersistentDirectories &persistent_directories)
    : m_persistent_directories(persistent_directories)
{}

void Identification::MainLoop()
{
    Log::log->trace("Starting identification.");

    /* We double-check the compatibility and abort if the API is incompatible. */
    if (ADQAPI_ValidateVersion(ADQAPI_VERSION_MAJOR, ADQAPI_VERSION_MINOR) == -1)
    {
        Log::log->error("The loaded libadq is not compatible with this version of sigscape.");
        m_thread_exit_code = SCAPE_EINTERNAL;
        return;
    }

    void *handle = CreateADQControlUnit();
    if (handle == NULL)
    {
        Log::log->error("Failed to create an ADQControlUnit.");
        m_thread_exit_code = SCAPE_EINTERNAL;
        return;
    }

    /* Enable the trace logs. */
    const auto &log_directory = m_persistent_directories.GetLogDirectory();
    if (!log_directory.empty()
        && !ADQControlUnit_EnableErrorTrace(handle, 0x00010000, log_directory.c_str()))
    {
        Log::log->error("Failed to redirect trace logging to '{}'.", log_directory.c_str());
    }

    /* Filter out the Gen4 digitizers and construct a digitizer object for each one. */
    ADQInfoListEntry *adq_list = NULL;
    int nof_digitizers = 0;
    if (!ADQControlUnit_ListDevices(handle, &adq_list, (unsigned int *)&nof_digitizers))
    {
        Log::log->error("Failed to list devices.");
        DeleteADQControlUnit(handle);
        m_thread_exit_code = SCAPE_EINTERNAL;
        return;
    }

    std::vector<std::pair<int, int>> indexes{};
    int nof_opened_digitizers = 0;
    int nof_compatible_digitizers = 0;
    for (int i = 0; i < nof_digitizers; ++i)
    {
        switch (adq_list[i].ProductID)
        {
        case PID_ADQ32:
        case PID_ADQ36:
        case PID_ADQ30:
        case PID_ADQ35:
            if (ADQControlUnit_OpenDeviceInterface(handle, i))
                indexes.emplace_back(i, ++nof_opened_digitizers);
            ++nof_compatible_digitizers;
            break;
        default:
            break;
        }
    }

    const int nof_failed_digitizers = nof_compatible_digitizers - nof_opened_digitizers;
    Log::log->info("Found {} compatible digitizers (out of {}).",
                   nof_compatible_digitizers, nof_digitizers);
    Log::log->info("Opened the hardware interface of {} digitizers.", nof_opened_digitizers);

    if (nof_failed_digitizers > 0)
    {
        Log::log->error("Failed to open the hardware interface of {} digitizer{}.",
                        nof_failed_digitizers, nof_failed_digitizers > 1 ? "s" : "");
    }

    /* Get the persistent directory used to store the digitizer's configuration files. */
    const auto &configuration_directory = m_persistent_directories.GetConfigurationDirectory();

    /* Create a digitizer object for each entry. */
    auto digitizers = std::vector<std::shared_ptr<Digitizer>>();
    for (const auto &[init_index, index] : indexes)
    {
        digitizers.push_back(
            std::make_shared<Digitizer>(handle, init_index, index, configuration_directory));
    }

    /* Forward the control unit handle along with digitizer objects. */
    _PushMessage({handle, digitizers});
    m_thread_exit_code = SCAPE_EOK;
}
