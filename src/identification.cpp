#include "identification.h"

void Identification::MainLoop()
{
    void *handle = CreateADQControlUnit();
    if (handle == NULL)
    {
        printf("Failed to create an ADQControlUnit.\n");
        m_thread_exit_code = ADQR_EINTERNAL;
        return;
    }

    /* Filter out the Gen4 digitizers and construct a digitizer object for each one. */
    struct ADQInfoListEntry *adq_list = NULL;
    int nof_devices = 0;
    if (!ADQControlUnit_ListDevices(handle, &adq_list, (unsigned int *)&nof_devices))
    {
        printf("Failed to list devices.\n");
        DeleteADQControlUnit(handle);
        m_thread_exit_code = ADQR_EINTERNAL;
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
    m_read_queue.Write({handle, digitizers});
    m_thread_exit_code = ADQR_EOK;
}
