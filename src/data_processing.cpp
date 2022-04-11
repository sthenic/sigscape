#include "data_processing.h"

#include "fft.h"
#include "fft_settings.h"

#include <cmath>

DataProcessing::DataProcessing(std::shared_ptr<DataAcquisition> acquisition)
    : m_acquisition(acquisition)
{
}

DataProcessing::~DataProcessing()
{
    Stop();
}

int DataProcessing::Initialize()
{
    /* TODO: Nothing to initialize right now. */
    return ADQR_EOK;
}

int DataProcessing::Start()
{
    int result = m_acquisition->Start();
    if (result != ADQR_EOK)
        return result;
    return SmartBufferThread::Start();
}

int DataProcessing::Stop()
{
    /* Regardless of what happens, we want to send the stop signal to the
       acquisition object too. */
    int processing_result = SmartBufferThread::Stop();
    int acquisition_result = m_acquisition->Stop();
    if (processing_result != ADQR_EOK)
        return processing_result;
    return acquisition_result;
}

void DataProcessing::MainLoop()
{
    m_thread_exit_code = ADQR_EOK;
    auto time_point_last_record = std::chrono::high_resolution_clock::now();

    for (;;)
    {
        /* Check if the stop event has been set. */
        if (m_should_stop.wait_for(std::chrono::microseconds(0)) == std::future_status::ready)
            break;

        /* FIXME: This is a hardcoded type for now. */
        std::shared_ptr<TimeDomainRecord> time_domain = NULL;
        int result = m_acquisition->WaitForBuffer((std::shared_ptr<void> &)time_domain, 100, NULL);

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

        auto time_point_this_record = std::chrono::high_resolution_clock::now();
        auto period = time_point_this_record - time_point_last_record;
        time_domain->estimated_trigger_frequency = 1e9 / period.count();
        time_point_last_record = time_point_this_record;

        /* We only allocate memory and calculate the FFT if we know that we're
           going to show it, i.e. if the outbound queue has space available. */
        if (!m_read_queue.IsFull())
        {
            /* Compute FFT */
            int fft_size = PreviousPowerOfTwo(time_domain->count);
            auto processed_record = std::make_shared<ProcessedRecord>(fft_size);

            /* We copy the time domain data in order to return the buffer to the
               acquisition interface ASAP. */
            *processed_record->time_domain = *time_domain;

            const char *error = NULL;
            if (!simple_fft::FFT(time_domain->y, processed_record->frequency_domain->yc, fft_size, error))
            {
                printf("Failed to compute FFT: %s.\n", error);
                m_thread_exit_code = ADQR_EINTERNAL;
                return;
            }

            /* TODO: We could probably optimize this and do (time_domain->count - fftsize)
                     passes of the second loop in the first loop and then just the
                     remaining passes. */

            /* Compute real spectrum. */
            for (int i = 0; i < fft_size; ++i)
            {
                processed_record->frequency_domain->x[i] = static_cast<double>(i) / static_cast<double>(fft_size);
                processed_record->frequency_domain->y[i] = 20 * std::log10(std::abs(processed_record->frequency_domain->yc[i]) / fft_size);

                if (processed_record->frequency_domain->y[i] > processed_record->frequency_domain_metrics.max)
                    processed_record->frequency_domain_metrics.max = processed_record->frequency_domain->y[i];

                if (processed_record->frequency_domain->y[i] < processed_record->frequency_domain_metrics.min)
                    processed_record->frequency_domain_metrics.min = processed_record->frequency_domain->y[i];
            }

            for (size_t i = 0; i < time_domain->count; ++i)
            {
                if (processed_record->time_domain->y[i] > processed_record->time_domain_metrics.max)
                    processed_record->time_domain_metrics.max = processed_record->time_domain->y[i];

                if (processed_record->time_domain->y[i] < processed_record->time_domain_metrics.min)
                    processed_record->time_domain_metrics.min = processed_record->time_domain->y[i];
            }

            /* Push the new FFT at the top of the waterfall and construct a
               row-major array for the plotting. We unfortunately cannot the
               requirement of this linear layout. */
            if (m_waterfall.size() > WATERFALL_SIZE)
                m_waterfall.pop_back();
            m_waterfall.push_front(processed_record->frequency_domain);
            processed_record->waterfall = std::make_shared<Waterfall>(m_waterfall);

            m_read_queue.Write(processed_record);
        }
        else
        {
            static int nof_discarded = 0;
            printf("Skipping (no FFT or allocation) since queue is full (%d).\n", nof_discarded++);
        }

        m_acquisition->ReturnBuffer(time_domain);
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
