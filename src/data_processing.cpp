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
#include <set>

DataProcessing::DataProcessing(void *handle, int index, int channel, const std::string &label,
                               const struct ADQConstantParameters &constant)
    : m_handle(handle)
    , m_index(index)
    , m_channel(channel)
    , m_label(label)
    , m_afe{1000.0, 0.0}
    , m_constant{constant}
    , m_window_cache()
    , m_window_type(WindowType::FLAT_TOP)
    , m_nof_skirt_bins(NOF_SKIRT_BINS_DEFAULT)
    , m_waterfall{}
    , m_persistence{}
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

void DataProcessing::SetWindowType(WindowType type)
{
    m_window_type = type;
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
        int64_t bytes_received = ADQ_WaitForRecordBuffer(m_handle, m_index, &channel,
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

        /* FIXME: Low-pass filter these values. Average over the last N record should do. */
        auto time_point_this_record = std::chrono::high_resolution_clock::now();
        auto period = time_point_this_record - time_point_last_record;
        const double estimated_trigger_frequency = 1e9 / period.count();
        const double estimated_throughput = bytes_received * estimated_trigger_frequency;
        time_point_last_record = time_point_this_record;

        /* We only allocate memory and calculate the FFT if we know that we're
           going to show it, i.e. if the outbound queue has space available. */
        if (!m_read_queue.IsFull())
        {
            /* We copy the time domain data in order to return the buffer to the
               acquisition interface ASAP. */
            auto processed_record = std::make_shared<ProcessedRecord>();
            processed_record->label = m_label;

            /* Local alias */
            const double code_normalization = m_constant.channel[channel].code_normalization;
            /* FIXME: Divide by the number of accumulations if FWATD. */

            /* FIXME: Windowing in the time domain struct?  */
            /* FIXME: This can throw if data format is unsupported. */
            processed_record->time_domain =
                std::make_shared<TimeDomainRecord>(time_domain, m_afe, code_normalization);
            processed_record->time_domain->estimated_trigger_frequency = estimated_trigger_frequency;
            processed_record->time_domain->estimated_throughput = estimated_throughput;

            if (m_persistence.size() >= CLOUD_SIZE)
                m_persistence.pop_back();
            m_persistence.push_front(processed_record->time_domain);
            processed_record->persistence = std::make_shared<Persistence>(m_persistence);

            const size_t FFT_LENGTH = PreviousPowerOfTwo(time_domain->header->record_length);
            processed_record->frequency_domain =
                std::make_shared<FrequencyDomainRecord>(FFT_LENGTH / 2 + 1);
            processed_record->frequency_domain->step =
                processed_record->time_domain->sampling_frequency / static_cast<double>(FFT_LENGTH);

            /* Windowing and scaling to [-1, 1] for the correct FFT values. We
               use the raw data again since the processed time domain record
               will have been scaled to Volts with the input range and DC offset
               taken into account. */

            auto window = m_window_cache.GetWindow(m_window_type, FFT_LENGTH);
            auto y = std::vector<double>(FFT_LENGTH);
            switch (time_domain->header->data_format)
            {
            case ADQ_DATA_FORMAT_INT16:
                TransformToUnitRange(static_cast<const int16_t *>(time_domain->data),
                                     code_normalization, window.get(), y);
                break;

            case ADQ_DATA_FORMAT_INT32:
                TransformToUnitRange(static_cast<const int16_t *>(time_domain->data),
                                     code_normalization, window.get(), y);
                break;

            default:
                printf("Unknown data format '%d', aborting.\n", time_domain->header->data_format);
                m_thread_exit_code = ADQR_EINTERNAL;
                return;
            }

            /* Calculate the FFT */
            const char *error = NULL;
            auto yc = std::vector<std::complex<double>>(FFT_LENGTH);
            if (!simple_fft::FFT(y, yc, FFT_LENGTH, error))
            {
                printf("Failed to compute FFT: %s.\n", error);
                ADQ_ReturnRecordBuffer(m_handle, m_index, channel, time_domain);
                continue;
            }

            /* Analyze the fourier transform data, scaling the data and extracting key metrics. */
            AnalyzeFourierTransform(yc, processed_record.get());

            /* Analyze the time domain data. */
            AnalyzeTimeDomain(processed_record.get());

            /* Push the new FFT at the top of the waterfall and construct a
               row-major array for the plotting. We unfortunately cannot the
               requirement of this linear layout. */
            if (m_waterfall.size() >= WATERFALL_SIZE)
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

        ADQ_ReturnRecordBuffer(m_handle, m_index, channel, time_domain);
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

template <typename T>
void DataProcessing::TransformToUnitRange(const T *data, double code_normalization,
                                          const Window *window, std::vector<double> &y)
{
    for (size_t i = 0; i < y.size(); ++i)
    {
        /* Scale to [-1, 1], which means code_normalization / 2. */
        y[i] = static_cast<double>(data[i]) / (code_normalization / 2.0);
        if (window != NULL)
            y[i] *= window->data[i];
    }
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

size_t DataProcessing::FoldIndex(size_t f, size_t fs)
{
    size_t result = f;
    while (result > (fs / 2))
    {
        if (result > fs)
            result = result - fs;
        else if (result > (fs / 2))
            result = fs - result;
    }
    return result;
}

void DataProcessing::AnalyzeFourierTransform(const std::vector<std::complex<double>> &fft,
                                             ProcessedRecord *record)
{
    Tone fundamental{};
    Tone spur{};
    Tone dc{};
    std::vector<Tone> harmonics{};
    double total_power = 0.0;
    auto &metrics = record->frequency_domain_metrics;

    ProcessAndIdentify(fft, record, dc, fundamental, spur, total_power);
    PlaceHarmonics(fundamental, record, harmonics);
    ResolveOverlaps(dc, fundamental, harmonics, metrics.overlap);

    /* Remove the power of the fundamental tone from the total noise power. */
    double harmonic_distortion_power = 0.0;
    auto &y = record->frequency_domain->y;
    for (auto &harmonic : harmonics)
    {
        harmonic.power = 0.0;
        for (const auto &v : harmonic.values)
            harmonic.power += v;
        harmonic_distortion_power += harmonic.power;
        metrics.harmonics.push_back(std::make_pair(harmonic.frequency, y[harmonic.idx]));
    }
    double noise_power = total_power - fundamental.power - dc.power - harmonic_distortion_power;

    /* FIXME: Linear interpolation? */
    metrics.fundamental = std::make_pair(fundamental.frequency, y[fundamental.idx]);
    metrics.spur = std::make_pair(spur.frequency, y[spur.idx]);
    metrics.snr = 10.0 * std::log10(fundamental.power / noise_power);
    metrics.thd = 10.0 * std::log10(fundamental.power / harmonic_distortion_power);
    metrics.sinad = 10.0 * std::log10(fundamental.power / (noise_power + harmonic_distortion_power));
    metrics.enob = (metrics.sinad - 1.76) / 6.02;
    metrics.sfdr_dbfs = -y[spur.idx];
    metrics.sfdr_dbc = y[fundamental.idx] - y[spur.idx];
    metrics.noise = 10.0 * std::log10(noise_power / static_cast<double>(fft.size()));

    /* FIXME: Adjust worst spur if it turns out that it's one of the harmonics
              that ended up within the blind spot? */
}

void DataProcessing::ProcessAndIdentify(const std::vector<std::complex<double>> &fft,
                                        ProcessedRecord *record, Tone &dc, Tone &fundamental,
                                        Tone &spur, double &power)
{
    /* The loop upper bound is expected to be N/2 + 1 where N is the length of
       the transform, i.e. fft.size(). During our pass through the spectrum, our
       goal is to identify the fundamental, the worst spur and also to
       accumulate the total power since we're already traversing all the data
       points.

       We'll use a shift register to implement a cursor that we drag across the
       spectrum. If the total power contained in the window is a new maximum, we
       note the new location and mark the old location as the worst spur unless
       there's an overlap.

       The goal is to only loop over the spectrum once and to not allocate
       additional memory. We avoid making a copy of the spectrum just for the
       analysis process. We do this to manage the complexity to stay performant
       for long FFTs. */

    auto &x = record->frequency_domain->x;
    auto &y = record->frequency_domain->y;
    const auto &bin_range = record->frequency_domain->step;
    std::deque<double> cursor;

    dc = {};
    fundamental = {};
    spur = {};
    power = 0.0;

    /* FIXME: Make the fundamental frequency configurable (default to dynamic). */
    for (size_t i = 0; i < record->frequency_domain->x.size(); ++i)
    {
        x[i] = static_cast<double>(i) * bin_range;
        y[i] = std::pow(2.0 * std::abs(fft[i]) / static_cast<double>(fft.size()), 2.0);

        if (i <= m_nof_skirt_bins)
        {
            dc.power += y[i];
            dc.idx_high = i;
            dc.values.push_back(y[i]);
            power += y[i];
            y[i] = 10 * std::log10(y[i]);
            continue;
        }

        if (cursor.size() >= (2 * m_nof_skirt_bins + 1))
            cursor.pop_front();
        cursor.push_back(y[i]);

        double numerator = 0.0;
        double denominator = 0.0;
        for (size_t j = 0; j < cursor.size(); ++j)
        {
            numerator += static_cast<double>(j) * cursor[j];
            denominator += cursor[j];
        }

        const size_t idx_low = i - cursor.size() + 1;
        const double center_of_mass = static_cast<double>(idx_low) + (numerator / denominator);
        const size_t center_idx = static_cast<size_t>(center_of_mass + 0.5);
        const double center_fraction = center_of_mass - static_cast<double>(center_idx);
        const double center_frequency = bin_range * center_of_mass;

        if (denominator > fundamental.power)
        {
            if ((center_idx - fundamental.idx) > (2 * m_nof_skirt_bins))
                spur = fundamental;

            fundamental.power = denominator;
            fundamental.frequency = center_frequency;
            fundamental.idx = center_idx;
            fundamental.idx_fraction = center_fraction;
            fundamental.idx_low = idx_low;
            fundamental.idx_high = i;
            fundamental.values.clear();
            for (const auto &c : cursor)
                fundamental.values.push_back(c);
        }

        if (denominator > spur.power && (center_idx - fundamental.idx) > (2 * m_nof_skirt_bins))
        {
            spur.power = denominator;
            spur.frequency = center_frequency;
            spur.idx = center_idx;
            spur.idx_fraction = center_fraction;
            spur.idx_low = idx_low;
            spur.idx_high = i;
            spur.values.clear();
            for (const auto &c : cursor)
                spur.values.push_back(c);
        }

        /* Convert to decibels. But not before adding to bin value as a
           contribution to the total noise power. We'll adjust this value later
           on. */
        power += y[i];
        y[i] = 10.0 * std::log10(y[i]);
    }
}

void DataProcessing::PlaceHarmonics(const Tone &fundamental, const ProcessedRecord *record,
                                    std::vector<Tone> &harmonics)
{
    const auto &y = record->frequency_domain->y;
    const auto &bin_range = record->frequency_domain->step;

    /* Analyze the harmonics where we only look at HD2 to HD5. We use the same
       approach as for the tone analysis above, with an interpolation. Though we
       have to reverse the conversion to decibel for the surrounding bins. Still
       it's more efficient to do it this way than to loop over the entire
       spectrum again just to convert to decibels. */

    for (int hd = 2; hd <= 5; ++hd)
    {
        const double f = FoldFrequency(fundamental.frequency * hd,
                                       record->time_domain->sampling_frequency);
        const int idx = static_cast<int>(f / bin_range + 0.5);
        Tone harmonic{};

        harmonic.idx_low = static_cast<size_t>((std::max)(idx - static_cast<int>(m_nof_skirt_bins), 0));
        harmonic.idx_high = (std::min)(idx + m_nof_skirt_bins, record->frequency_domain->x.size() - 1);

        double numerator = 0.0;
        double denominator = 0.0;
        for (size_t i = harmonic.idx_low; i <= harmonic.idx_high; ++i)
        {
            const double power = std::pow(10, y[i] / 10);
            numerator += static_cast<double>(i - harmonic.idx_low) * power;
            denominator += power;
            harmonic.values.push_back(power);
        }

        const double center_of_mass = static_cast<double>(harmonic.idx_low) + (numerator / denominator);
        harmonic.idx = static_cast<size_t>(center_of_mass + 0.5);
        harmonic.idx_fraction = center_of_mass - static_cast<double>(harmonic.idx);
        harmonic.frequency = bin_range * center_of_mass;
        /* We'll determine the total power once we resolve the overlaps. */
        harmonic.power = 0.0;
        harmonics.push_back(harmonic);
    }
}

void DataProcessing::ResolveOverlaps(const Tone &dc, const Tone &fundamental,
                                    std::vector<Tone> &harmonics, bool &overlap)
{
    /* Now we have to figure out if these tones overlap in any way. If they do,
       we mark the metrics as 'not trustworthy' but still perform the
       calculations as best we can. The goal is to only count the energy
       contribution from a bin _once_, favoring signal energy over distortion
       energy when there's an overlap. */

    overlap = false;
    for (size_t i = 0; i < harmonics.size(); ++i)
    {
        /* For each harmonic, check for overlap with
            1. the fundamental tone; and
            2. other harmonics. */

        /* TODO: Check against worst spur too? */
        ResolveOverlap(harmonics[i], fundamental, overlap);
        ResolveOverlap(harmonics[i], dc, overlap);

        /* When we check for overlap with the other harmonics, we swap the order
           around to prioritize keeping the power for lower index harmonics. */
        for (size_t j = i + 1; j < harmonics.size(); ++j)
            ResolveOverlap(harmonics[j], harmonics[i], overlap);
    }
}

void DataProcessing::ResolveOverlap(Tone &tone, const Tone &other, bool &overlap)
{
    /* Set the overlapping bins to zero for the tone if there's an overlap. */
    if ((tone.idx_low >= other.idx_low) && (tone.idx_low <= other.idx_high))
    {
        for (size_t j = 0; j <= (other.idx_high - tone.idx_low); ++j)
            tone.values[j] = 0;
        overlap = true;
    }
    else if ((tone.idx_high <= other.idx_high) && (tone.idx_high >= other.idx_low))
    {
        for (size_t j = (other.idx_low - tone.idx_low); j < tone.values.size(); ++j)
            tone.values[j] = 0;
        overlap = true;
    }
}

void DataProcessing::AnalyzeTimeDomain(ProcessedRecord *record)
{
    for (const auto &y : record->time_domain->y)
    {
        if (y > record->time_domain_metrics.max)
            record->time_domain_metrics.max = y;

        if (y < record->time_domain_metrics.min)
            record->time_domain_metrics.min = y;

        record->time_domain_metrics.mean += y;
    }

    record->time_domain_metrics.mean /= static_cast<double>(record->time_domain->y.size());
}
