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
            size_t fft_length = PreviousPowerOfTwo(time_domain->header->record_length);
            auto yc = std::unique_ptr<std::complex<double>[]>( new std::complex<double>[fft_length] );

            processed_record->time_domain = std::make_shared<TimeDomainRecord>(time_domain, m_afe);
            processed_record->frequency_domain = std::make_shared<FrequencyDomainRecord>(fft_length / 2 + 1);
            processed_record->time_domain->estimated_trigger_frequency = estimated_trigger_frequency;
            processed_record->label = m_label;

            processed_record->frequency_domain->bin_range =
                processed_record->time_domain->sampling_frequency / static_cast<double>(fft_length);

            /* Compute FFT */
            const char *error = NULL;
            if (!simple_fft::FFT(processed_record->time_domain->y, yc, fft_length, error))
            {
                /* FIXME: Perhaps just continue instead? */
                printf("Failed to compute FFT: %s.\n", error);
                m_thread_exit_code = ADQR_EINTERNAL;
                return;
            }

            /* Analyze the fourier transform data, scaling the data and extracting key metrics. */
            AnalyzeFourierTransform(yc.get(), fft_length, processed_record.get());

            /* Analyze the time domain data. */
            AnalyzeTimeDomain(processed_record.get());

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
size_t DataProcessing::NextPowerOfTwo(T i)
{
    return static_cast<size_t>(std::pow(2, std::ceil(std::log2(i))));
}

template <typename T>
size_t DataProcessing::PreviousPowerOfTwo(T i)
{
    return static_cast<size_t>(std::pow(2, std::floor(std::log2(i))));
}

double DataProcessing::FoldFrequency(double f, double fs)
{
    double result = f;
    while (result > (fs / 2))
    {
        if (result > fs)
            result = result - fs;
        else if (result > (fs / 2))
            result = fs - result;
    }
    return result;
}

void DataProcessing::AnalyzeFourierTransform(const std::complex<double> *fft, size_t length,
                                             ProcessedRecord *record)
{
    auto &metrics = record->frequency_domain_metrics;
    auto &x = record->frequency_domain->x;
    auto &y = record->frequency_domain->y;
    const auto &bin_range = record->frequency_domain->bin_range;

    /* The loop upper bound is expected to be N/2 + 1. */
    for (size_t i = 0; i < record->frequency_domain->count; ++i)
    {
        x[i] = static_cast<double>(i) * bin_range;
        y[i] = 20.0 * std::log10(2.0 * std::abs(fft[i]) / static_cast<double>(length));

        if (y[i] > metrics.max)
        {
            metrics.max = y[i];
            metrics.sfdr_limiter = std::move(metrics.fundamental);
            metrics.fundamental = std::make_pair(x[i], y[i]);
        }

        /* FIXME: Add skirt bins */

        /* The SFDR limiter will be the second highest bin. We exempt the
           current fundamental index and rely on the fundamental search above to
           assign its old value to the SFDR limiter if a new maximum is found. */
        if ((x[i] != metrics.fundamental.first) && (y[i] > metrics.sfdr_limiter.second))
            metrics.sfdr_limiter = std::make_pair(x[i], y[i]);

        if (y[i] < metrics.min)
            metrics.min = y[i];
    }

    /* Identify harmonics. */
    /* FIXME: Reset SFDR limiter if this is one of the harmonics? */
    /* FIXME: Add skirt bins */
    for (int hd = 2; hd <= 5; ++hd)
    {
        double f = FoldFrequency(metrics.fundamental.first * hd,
                                 record->time_domain->sampling_frequency);
        int i = static_cast<int>(f / bin_range + 0.5);
        metrics.harmonics.push_back(std::make_pair(x[i], y[i]));
    }
}

void DataProcessing::AnalyzeTimeDomain(ProcessedRecord *record)
{
    for (size_t i = 0; i < record->time_domain->count; ++i)
    {
        if (record->time_domain->y[i] > record->time_domain_metrics.max)
            record->time_domain_metrics.max = record->time_domain->y[i];

        if (record->time_domain->y[i] < record->time_domain_metrics.min)
            record->time_domain_metrics.min = record->time_domain->y[i];
    }
}
