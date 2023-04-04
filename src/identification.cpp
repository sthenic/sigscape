#include "identification.h"

void Identification::SetLogDirectory(const std::string &log_directory)
{
    m_log_directory = log_directory;
}

void Identification::MainLoop()
{
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
        fprintf(stderr, "Failed to create an ADQControlUnit.\n");
        m_thread_exit_code = SCAPE_EINTERNAL;
        return;
    }

    /* Enable the trace logs. */
    if (!m_log_directory.empty()
        && !ADQControlUnit_EnableErrorTrace(handle, 0x00010000, m_log_directory.c_str()))
    {
        fprintf(stderr, "Failed to redirect trace logging to '%s'.\n", m_log_directory.c_str());
    }

    /* Filter out the Gen4 digitizers and construct a digitizer object for each one. */
    struct ADQInfoListEntry *adq_list = NULL;
    int nof_devices = 0;
    if (!ADQControlUnit_ListDevices(handle, &adq_list, (unsigned int *)&nof_devices))
    {
        fprintf(stderr, "Failed to list devices.\n");
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

    /* Create a digitizer object for each entry. */
    auto digitizers = std::vector<std::shared_ptr<Digitizer>>();
    for (int i = 0; i < nof_gen4_digitizers; ++i)
        digitizers.push_back(std::make_shared<Digitizer>(handle, i + 1));

    /* Forward the control unit handle along with digitizer objects. */
    /* FIXME: Propagate errors? */
    m_read_queue.Write({handle, revision, true, digitizers});
    m_thread_exit_code = SCAPE_EOK;
}
