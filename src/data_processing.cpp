#include "data_processing.h"

#include "fft.h"
#include "fft_settings.h"

#include <cmath>

DataProcessing::DataProcessing(DataAcquisition &acquisition)
    : m_acquisition(acquisition)
{
}

DataProcessing::~DataProcessing()
{
}

int DataProcessing::Initialize()
{
    /* TODO: Nothing to initialize right now. */
    return ADQR_EOK;
}

int DataProcessing::Start()
{
    int result = m_acquisition.Start();
    if (result != ADQR_EOK)
        return result;
    return BufferThread::Start();
}

int DataProcessing::Stop()
{
    /* Regardless of what happens, we want to send the stop signal to the
       acquisition object too. */
    int processing_result = BufferThread::Stop();
    int acquisition_result = m_acquisition.Stop();
    if (processing_result != ADQR_EOK)
        return processing_result;
    return acquisition_result;
}

int DataProcessing::WaitForBuffer(std::shared_ptr<ProcessedRecord> &buffer, int timeout)
{
    return m_read_queue.Read(buffer, timeout);
}

int DataProcessing::ReturnBuffer(std::shared_ptr<ProcessedRecord> buffer)
{
    /* FIXME: Figure out return code logic. */
    m_acquisition.ReturnBuffer(buffer->time_domain);
    buffer->time_domain = NULL;
    return m_write_queue.Write(buffer);
}

void DataProcessing::MainLoop()
{
    m_thread_exit_code = ADQR_EOK;
    for (;;)
    {
        /* Check if the stop event has been set. */
        if (m_should_stop.wait_for(std::chrono::microseconds(0)) == std::future_status::ready)
            break;

        /* FIXME: This is a hardcoded type for now. */
        std::shared_ptr<TimeDomainRecord> time_domain = NULL;
        int result = m_acquisition.WaitForBuffer((std::shared_ptr<void> &)time_domain, 100, NULL);

        /* Continue on timeout. */
        if (result == ADQR_EAGAIN)
        {
            continue;
        }
        else if (result < 0)
        {
            printf("Failed to get a time domain buffer %d.\n", result);
            m_thread_exit_code = ADQR_EINTERNAL;
            return;
        }

        /* Compute FFT */
        std::shared_ptr<ProcessedRecord> processed_record = NULL;
        int fft_size = PreviousPowerOfTwo(time_domain->count);
        result = ReuseOrAllocateBuffer(processed_record, fft_size);
        if (result != ADQR_EOK)
        {
            if (result == ADQR_EINTERRUPTED) /* Convert forced queue stop into an ok. */
                m_thread_exit_code = ADQR_EOK;
            else
                m_thread_exit_code = result;
            return;
        }
        processed_record->time_domain = time_domain;
        processed_record->frequency_domain->header.record_number = time_domain->header.record_number;

        const char *error = NULL;
        if (!simple_fft::FFT(time_domain->y, processed_record->frequency_domain->yc, fft_size, error))
        {
            printf("Failed to compute FFT: %s.\n", error);
            m_thread_exit_code = ADQR_EINTERNAL;
            return;
        }

        /* Compute real spectrum. */
        for (int i = 0; i < fft_size; ++i)
        {
            processed_record->frequency_domain->x[i] = static_cast<double>(i) / static_cast<double>(fft_size);
            processed_record->frequency_domain->y[i] = 20 * std::log10(std::abs(processed_record->frequency_domain->yc[i]) / fft_size);
        }

        m_read_queue.Write(processed_record);
    }
}

template <typename T>
int DataProcessing::NextPowerOfTwo(T i)
{
    return static_cast<int>(std::pow(2, std::ceil(std::log2(i))));
}

template <typename T>
int DataProcessing::PreviousPowerOfTwo(T i)
{
    return static_cast<int>(std::pow(2, std::floor(std::log2(i))));
}
