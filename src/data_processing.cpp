#include "data_processing.h"

#include "fft.h"
#include "fft_settings.h"

#ifdef NO_ADQAPI
#include "mock_adqapi.h"
#else
#include "ADQAPI.h"
#endif

#include <cmath>
#include <cinttypes>

DataProcessing::DataProcessing(void *control_unit, int index, int channel, const std::string &label)
    : m_control_unit(control_unit)
    , m_index(index)
    , m_channel(channel)
    , m_label(label)
    , m_afe{1000.0, 0.0}
{
}

DataProcessing::~DataProcessing()
{
    Stop();
}

void DataProcessing::SetAnalogFrontendParameters(const struct ADQAnalogFrontendParametersChannel &afe)
{
    m_afe = afe;
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

        struct ADQGen4Record *time_domain = NULL;
        int channel = m_channel;
        int64_t bytes_received = ADQ_WaitForRecordBuffer(m_control_unit, m_index, &channel,
                                                         (void **)&time_domain, 100, NULL);

        /* Continue on timeout. */
        if ((bytes_received == ADQ_EAGAIN) || (bytes_received == ADQ_ENOTREADY) || (bytes_received == ADQR_EINTERRUPTED))
        {
            continue;
        }
        else if (bytes_received < 0)
        {
            printf("Failed to get a time domain buffer %" PRId64 ".\n", bytes_received);
            m_thread_exit_code = ADQR_EINTERNAL;
            return;
        }

        auto time_point_this_record = std::chrono::high_resolution_clock::now();
        auto period = time_point_this_record - time_point_last_record;
        double estimated_trigger_frequency = 1e9 / period.count();
        time_point_last_record = time_point_this_record;

        /* We only allocate memory and calculate the FFT if we know that we're
           going to show it, i.e. if the outbound queue has space available. */
        if (!m_read_queue.IsFull())
        {
            /* We copy the time domain data in order to return the buffer to the
               acquisition interface ASAP. */
            auto processed_record = std::make_shared<ProcessedRecord>();
            int fft_size = PreviousPowerOfTwo(time_domain->header->record_length);
            processed_record->time_domain = std::make_shared<TimeDomainRecord>(time_domain, m_afe);
            processed_record->frequency_domain = std::make_shared<FrequencyDomainRecord>(fft_size);
            processed_record->time_domain->estimated_trigger_frequency = estimated_trigger_frequency;
            processed_record->label = m_label;

            processed_record->frequency_domain->bin_range =
                processed_record->time_domain->sampling_frequency / static_cast<double>(fft_size);

            /* Compute FFT */
            const char *error = NULL;
            if (!simple_fft::FFT(processed_record->time_domain->y,
                                 processed_record->frequency_domain->yc, fft_size, error))
            {
                /* FIXME: Perhaps just continue instead? */
                printf("Failed to compute FFT: %s.\n", error);
                m_thread_exit_code = ADQR_EINTERNAL;
                return;
            }

            /* TODO: We could probably optimize this and do (time_domain->count - fftsize)
                     passes of the second loop in the first loop and then just the
                     remaining passes. */

            /* Compute real spectrum */
            for (int i = 0; i < fft_size / 2 + 1; ++i)
            {
                processed_record->frequency_domain->x[i] =
                    static_cast<double>(i) * processed_record->frequency_domain->bin_range;

                /* We scale all bins by 2, except for bins 0 and N/2, i.e. DC and fs/2. */
                const double compensation = ((i == 0) || (i == fft_size / 2)) ? 1.0 : 2.0;
                processed_record->frequency_domain->y[i] =
                    20 * std::log10(compensation * std::abs(processed_record->frequency_domain->yc[i]) / fft_size);

                if (processed_record->frequency_domain->y[i] > processed_record->frequency_domain_metrics.max)
                {
                    processed_record->frequency_domain_metrics.max = processed_record->frequency_domain->y[i];
                    processed_record->frequency_domain_metrics.fundamental = std::make_pair(
                        processed_record->frequency_domain->x[i], processed_record->frequency_domain->y[i]);
                }

                if (processed_record->frequency_domain->y[i] < processed_record->frequency_domain_metrics.min)
                    processed_record->frequency_domain_metrics.min = processed_record->frequency_domain->y[i];

            }

            for (size_t i = 0; i < processed_record->time_domain->count; ++i)
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

        ADQ_ReturnRecordBuffer(m_control_unit, m_index, channel, time_domain);
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
