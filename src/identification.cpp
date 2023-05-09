#include "identification.h"
#include "log.h"

Identification::Identification(const PersistentDirectories &persistent_directories)
    : m_persistent_directories(persistent_directories)
{}

void Identification::MainLoop()
{
    Log::log->trace("Starting identification.");
    uint32_t revision = ADQAPI_GetRevision();

    /* We only abort if the API is incompatible. */
    if (ADQAPI_ValidateVersion(ADQAPI_VERSION_MAJOR, ADQAPI_VERSION_MINOR) == -1)
    {
        m_read_queue.Write({NULL, revision, false, {}});
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
    struct ADQInfoListEntry *adq_list = NULL;
    int nof_devices = 0;
    if (!ADQControlUnit_ListDevices(handle, &adq_list, (unsigned int *)&nof_devices))
    {
        Log::log->error("Failed to list devices.");
        DeleteADQControlUnit(handle);
        m_thread_exit_code = SCAPE_EINTERNAL;
        return;
    }

    int nof_gen4_digitizers = 0;
    for (int i = 0; i < nof_devices; ++i)
    {
        switch (adq_list[i].ProductID)
        {
        case PID_ADQ32:
        case PID_ADQ36:
            if (ADQControlUnit_OpenDeviceInterface(handle, i))
                nof_gen4_digitizers++;
            break;
        default:
            break;
        }
    }

    Log::log->info("Found {} compatible digitizers (out of {}).", nof_gen4_digitizers, nof_devices);

    /* Get the persistent directory used to store the digitizer's configuration files. */
    const auto &configuration_directory = m_persistent_directories.GetConfigurationDirectory();

    /* Create a digitizer object for each entry. */
    auto digitizers = std::vector<std::shared_ptr<Digitizer>>();
    for (int i = 0; i < nof_gen4_digitizers; ++i)
        digitizers.push_back(std::make_shared<Digitizer>(handle, i + 1, configuration_directory));

    /* Forward the control unit handle along with digitizer objects. */
    /* TODO: Propagate errors? */
    m_read_queue.Write({handle, revision, true, digitizers});
    m_thread_exit_code = SCAPE_EOK;
}
