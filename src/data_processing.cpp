#include "data_processing.h"

#include "fft.h"
#include "fft_settings.h"

DataProcessing::DataProcessing(DataAcquisition &acquisition)
    : m_acquisition(acquisition)
{
}

DataProcessing::~DataProcessing()
{
}


int DataProcessing::Initialize()
{
    return 0;
}

int DataProcessing::WaitForBuffer(struct ProcessedRecord *&buffer, int timeout)
{
    return m_read_queue.Read(buffer, timeout);
}

int DataProcessing::ReturnBuffer(struct ProcessedRecord *buffer)
{
    /* FIXME: Figure out return code logic. */
    m_acquisition.ReturnBuffer(buffer->time_domain);
    if (!buffer->owns_time_domain)
        buffer->time_domain = NULL;
    return m_write_queue.Write(buffer);
}

void DataProcessing::MainLoop()
{
    m_thread_exit_code = 0;
    for (;;)
    {
        /* Check if the stop event has been set. */
        if (m_should_stop.wait_for(std::chrono::microseconds(0)) == std::future_status::ready)
            break;

        /* FIXME: This is a hardcoded type for now. */
        struct TimeDomainRecord *time_domain = NULL;
        int result = m_acquisition.WaitForBuffer((void *&)time_domain, 100, NULL);

        /* Continue on timeout. */
        if (result == -1)
        {
            continue;
        }
        else if (result < 0)
        {
            printf("Failed to get a time domain buffer %d.\n", result);
            m_thread_exit_code = -3;
            return;
        }

        /* Compute FFT */
        struct ProcessedRecord *processed_record = NULL;
        result = ReuseOrAllocateBuffer(processed_record, FFT_SIZE);
        if (result != 0)
        {
            if (result == -2) /* Convert forced queue stop into an ok. */
                m_thread_exit_code = 0;
            else
                m_thread_exit_code = result;
            return;
        }
        processed_record->time_domain = time_domain;

        const char *error = NULL;
        if (!simple_fft::FFT(time_domain->y, processed_record->frequency_domain->yc, FFT_SIZE, error))
        {
            printf("Failed to compute FFT: %s.\n", error);
            m_thread_exit_code = -3;
            return;
        }

        /* Compute real spectrum. */
        for (int i = 0; i < static_cast<int>(FFT_SIZE); ++i)
        {
            processed_record->frequency_domain->x[i] = static_cast<double>(i) / static_cast<double>(FFT_SIZE);
            processed_record->frequency_domain->y[i] = 20 * std::log10(std::abs(processed_record->frequency_domain->yc[i]) / FFT_SIZE);
        }

        m_read_queue.Write(processed_record);
    }
}
