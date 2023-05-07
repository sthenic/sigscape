#include "data_processing.h"

#include "fft.h"
#include "fft_settings.h"
#include "log.h"

#include "ADQAPI.h"

#include <cinttypes>
#include <set>

DataProcessingParameters::DataProcessingParameters()
    : window_type(WindowType::FLAT_TOP)
    , fft_scaling(FrequencyDomainScaling::AMPLITUDE)
    , nof_skirt_bins(5)
    , nof_fft_averages(1)
    , fundamental_frequency(-1.0)
    , convert_horizontal(true)
    , convert_vertical(true)
    , fullscale_enob(true)
{}

DataProcessing::Tone::Tone(const FrequencyDomainRecord &record, double f, size_t nof_skirt_bins)
    : power{}
    , frequency{}
    , idx{}
    , idx_fraction{}
    , idx_low{}
    , idx_high{}
    , values{}
{
    /* We use the same approach as for the tone analysis identifying the
       fundamental tone (with interpolation). We assume that the processed
       record's frequency domain contents has values converted to decibels full
       scale, so we have to reverse that conversion for the surrounding bins.
       Still, it's more efficient to do it this way than to loop over the entire
       spectrum again just to convert to decibels. */

    const auto &y = record.y;
    const auto &bin_range = record.step;
    const auto &scale_factor = record.scale_factor;
    const auto &energy_factor = record.energy_factor;
    const int lidx = static_cast<int>(f / bin_range + 0.5); /* FIXME: std::round? */

    idx_low = static_cast<size_t>(std::max(lidx - static_cast<int>(nof_skirt_bins), 0));
    idx_high = std::min(lidx + nof_skirt_bins, record.x.size() - 1);

    double numerator = 0.0;
    double denominator = 0.0;
    for (size_t i = idx_low; i <= idx_high; ++i)
    {
        const double bin_power = std::pow(10, y[i] / 10) / scale_factor * energy_factor;
        numerator += static_cast<double>(i - idx_low) * bin_power;
        denominator += bin_power;
        values.push_back(bin_power);
    }

    const double center_of_mass = static_cast<double>(idx_low) + (numerator / denominator);
    idx = static_cast<size_t>(center_of_mass + 0.5);
    idx_fraction = center_of_mass - static_cast<double>(idx);
    frequency = bin_range * center_of_mass;

    /* This power estimation does not take overlaps into account. This must
       be handled separately. */
    power = denominator;
}

DataProcessing::DataProcessing(void *handle, int index, int channel, const std::string &label,
                               const struct ADQConstantParameters &constant)
    : m_handle(handle)
    , m_index(index)
    , m_channel(channel)
    , m_label(label)
    , m_afe{1000.0, 0.0}
    , m_constant{constant}
    , m_clock_system{constant.clock_system}
    , m_window_cache()
    , m_parameters{}
    , m_waterfall{}
    , m_persistence{}
    , m_noise_moving_average{}
    , m_fft_moving_average()
{
}

DataProcessing::~DataProcessing()
{
    Stop();
}

void DataProcessing::MainLoop()
{
    m_thread_exit_code = SCAPE_EOK;
    auto time_point_last_record = std::chrono::high_resolution_clock::now();

    Log::log->trace(FormatLog("Starting data processing."));

    for (;;)
    {
        /* Check if the stop event has been set. */
        if (m_should_stop.wait_for(std::chrono::microseconds(0)) == std::future_status::ready)
            break;

        /* Process any messages posted to the thread. */
        ProcessMessages();

        struct ADQGen4Record *time_domain = NULL;
        int channel = m_channel;
        int64_t bytes_received = ADQ_WaitForRecordBuffer(m_handle, m_index, &channel,
                                                         (void **)&time_domain, 100, NULL);

        /* Continue on timeout. */
        if ((bytes_received == ADQ_EAGAIN) || (bytes_received == ADQ_ENOTREADY) || (bytes_received == ADQ_EINTERRUPTED))
        {
            continue;
        }
        else if (bytes_received < 0)
        {
            Log::log->error(FormatLog("Failed to get a time domain buffer {}.", bytes_received));
            m_thread_exit_code = SCAPE_EINTERNAL;
            return;
        }

        /* TODO: Low-pass filter these values. Average over the last N record should do. */
        const auto time_point_this_record = std::chrono::high_resolution_clock::now();
        const auto period = time_point_this_record - time_point_last_record;
        const double estimated_trigger_frequency = 1e9 / period.count();
        const double estimated_throughput = bytes_received * estimated_trigger_frequency;
        time_point_last_record = time_point_this_record;

        /* We only allocate memory and process the record if we know that we're
           going to show it, i.e. if the outbound queue has space available. */
        if (!m_read_queue.IsFull())
        {
            auto processed_record = std::make_shared<ProcessedRecord>(
                m_label, estimated_trigger_frequency, estimated_throughput);

            if (SCAPE_EOK == ProcessRecord(time_domain, *processed_record))
                m_read_queue.Write(processed_record);
        }
        else
        {
            static int nof_discarded = 0;
            Log::log->info(FormatLog("Skipping (no FFT or allocation) since queue is full ({}).",
                                     nof_discarded++));
        }

        ADQ_ReturnRecordBuffer(m_handle, m_index, channel, time_domain);
    }

    Log::log->trace(FormatLog("Stopping data processing."));
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

int DataProcessing::ProcessRecord(const ADQGen4Record *raw_time_domain,
                                  ProcessedRecord &processed_record)
{
    /* TODO: Split this function into time domain/frequency domain? */

    /* Determine the code normalization value to use to convert the data from
       ADC codes to Volts. Multiply by the number of accumulations if we're
       running FWATD (normalizing with a higher value). */

    const auto channel = raw_time_domain->header->channel;
    auto code_normalization = static_cast<double>(m_constant.channel[channel].code_normalization);

    if (m_constant.firmware.type == ADQ_FIRMWARE_TYPE_FWATD)
    {
        if (raw_time_domain->header->firmware_specific > 0)
        {
            code_normalization *= raw_time_domain->header->firmware_specific;
        }
        else
        {
            Log::log->warn(
                FormatLog("Expected a nonzero number of accumulations, skipping normalization."));
        }
    }

    try
    {
        /* Processing the raw time domain data can throw if the data format is unsupported. */
        processed_record.time_domain = std::make_shared<TimeDomainRecord>(
            raw_time_domain, m_afe, m_clock_system, code_normalization,
            m_parameters.convert_horizontal, m_parameters.convert_vertical
        );
    }
    catch (const std::invalid_argument &e)
    {
        Log::log->error(FormatLog(e.what()));
        return SCAPE_EINTERNAL;
    }

    /* Add to the persistence memory. */
    if (m_persistence.size() >= PERSISTENCE_SIZE)
        m_persistence.pop_back();
    m_persistence.push_front(processed_record.time_domain);
    processed_record.persistence = std::make_shared<Persistence>(m_persistence);

    /* Calculate the FFT and assign parameters we know at this stage. */
    const size_t FFT_LENGTH = PreviousPowerOfTwo(raw_time_domain->header->record_length);
    processed_record.frequency_domain =
        std::make_shared<FrequencyDomainRecord>(FFT_LENGTH / 2 + 1);

    processed_record.frequency_domain->step =
        processed_record.time_domain->sampling_frequency.value / static_cast<double>(FFT_LENGTH);
    processed_record.frequency_domain->rbw.value = processed_record.frequency_domain->step;
    processed_record.frequency_domain->size.value = static_cast<double>(FFT_LENGTH);

    /* Window the time domain data and scale it to the unit range [-1, 1] for
       the correct FFT values. We use the 'raw' data again since the processed
       time domain record will have been scaled to Volts with the input range
       and DC offset taken into account. */

    /* TODO: Make 'no window' into a proper uniform window? */
    const auto window = m_window_cache.GetWindow(m_parameters.window_type, FFT_LENGTH);
    auto &scale_factor = processed_record.frequency_domain->scale_factor;
    auto &energy_factor = processed_record.frequency_domain->energy_factor;
    energy_factor = (window != NULL) ? window->energy_factor : 1.0;
    switch (m_parameters.fft_scaling)
    {
    case FrequencyDomainScaling::AMPLITUDE:
        scale_factor = (window != NULL) ? window->amplitude_factor : 1.0;
        break;
    case FrequencyDomainScaling::ENERGY:
        scale_factor = (window != NULL) ? window->energy_factor : 1.0;
        break;
    }

    auto y = std::vector<double>(FFT_LENGTH);
    switch (raw_time_domain->header->data_format)
    {
    case ADQ_DATA_FORMAT_INT16:
        TransformToUnitRange(static_cast<const int16_t *>(raw_time_domain->data),
                             code_normalization, window.get(), y);
        break;

    case ADQ_DATA_FORMAT_INT32:
        TransformToUnitRange(static_cast<const int32_t *>(raw_time_domain->data),
                             code_normalization, window.get(), y);
        break;

    default:
        Log::log->error(
            FormatLog("Unknown data format '{}'.", raw_time_domain->header->data_format));
        return SCAPE_EINTERNAL;
    }

    /* Calculate the FFT */
    const char *error = NULL;
    auto yc = std::vector<std::complex<double>>(FFT_LENGTH);
    if (!simple_fft::FFT(y, yc, FFT_LENGTH, error))
    {
        Log::log->error(FormatLog("Failed to compute FFT, reason '{}'.", error));
        return SCAPE_EINTERNAL;
    }

    /* Analyze the fourier transform data, scaling the data and extracting key metrics. */
    AnalyzeFourierTransform(yc, processed_record);

    /* Analyze the time domain data. */
    AnalyzeTimeDomain(*processed_record.time_domain);

    /* Push the new FFT at the top of the waterfall and construct a row-major
       array for the plotting. We unfortunately cannot do anything about the
       requirement of this linear layout. */
    if (m_waterfall.size() >= WATERFALL_SIZE)
        m_waterfall.pop_back();
    m_waterfall.push_front(processed_record.frequency_domain);
    processed_record.waterfall = std::make_shared<Waterfall>(m_waterfall);

    return SCAPE_EOK;
}

void DataProcessing::AnalyzeFourierTransform(const std::vector<std::complex<double>> &fft,
                                             ProcessedRecord &record)
{
    Tone fundamental{};
    Tone spur{};
    Tone dc{};
    double total_power = 0.0;
    ProcessAndIdentify(fft, record, dc, fundamental, spur, total_power);

    std::vector<Tone> harmonics{};
    PlaceHarmonics(fundamental, record, harmonics);

    auto &frequency_domain = record.frequency_domain;
    frequency_domain->overlap = false;
    ResolveHarmonicOverlaps(dc, fundamental, harmonics, frequency_domain->overlap);

    /* Reevaluate the power now that overlaps have been resolved. */
    double harmonic_distortion_power = 0.0;
    for (auto &harmonic : harmonics)
    {
        harmonic_distortion_power += harmonic.UpdatePower();
        frequency_domain->harmonics.emplace_back(
            frequency_domain->ValueX(harmonic.frequency),
            frequency_domain->ValueY(harmonic.PowerInDecibels())
        );
    }

    /* TODO: Manual opt-out from interleaving analysis? */
    Tone gain_phase_spur{};
    Tone offset_spur{};
    PlaceInterleavingSpurs(fundamental, record, gain_phase_spur, offset_spur);
    ResolveInterleavingSpurOverlaps(dc, fundamental, harmonics, gain_phase_spur, offset_spur,
                                    frequency_domain->overlap);

    const double interleaving_spur_power = gain_phase_spur.UpdatePower() +
                                           offset_spur.UpdatePower();

    frequency_domain->gain_phase_spur = {
        frequency_domain->ValueX(gain_phase_spur.frequency),
        frequency_domain->ValueY(gain_phase_spur.PowerInDecibels()),
    };

    frequency_domain->offset_spur = {
        frequency_domain->ValueX(offset_spur.frequency),
        frequency_domain->ValueY(offset_spur.PowerInDecibels()),
    };

    /* Calculate the noise power by removing the power of the fundamental tone
       and other spectral components from the total power. */
    const double noise_power = total_power - fundamental.power - dc.power -
                               harmonic_distortion_power - interleaving_spur_power;

    /* TODO: Linear interpolation? */
    frequency_domain->fundamental = {
        frequency_domain->ValueX(fundamental.frequency),
        frequency_domain->ValueY(fundamental.PowerInDecibels()),
    };

    frequency_domain->spur = {
        frequency_domain->ValueX(spur.frequency),
        frequency_domain->ValueY(spur.PowerInDecibels()),
    };

    frequency_domain->snr.value = 10.0 * std::log10(fundamental.power / noise_power);
    frequency_domain->thd.value = 10.0 * std::log10(fundamental.power / harmonic_distortion_power);

    const double noise_and_distortion_power = noise_power + harmonic_distortion_power +
                                              interleaving_spur_power;
    frequency_domain->sinad.value = 10.0 * std::log10(fundamental.power / noise_and_distortion_power);

    double sinad_for_enob = frequency_domain->sinad.value;
    if (m_parameters.fullscale_enob)
        sinad_for_enob = 10.0 * std::log10(1.0 / noise_and_distortion_power);

    frequency_domain->enob.value = (sinad_for_enob - 1.76) / 6.02;

    frequency_domain->sfdr_dbfs.value = -spur.PowerInDecibels();
    frequency_domain->sfdr_dbc.value = fundamental.PowerInDecibels() - spur.PowerInDecibels();

    const double noise_average = 10.0 * std::log10(noise_power / static_cast<double>(frequency_domain->x.size()));
    frequency_domain->npsd.value = noise_average - 10.0 * std::log10(frequency_domain->step);

    /* To compute the moving average, we want to use a value scaled as the plot
       will be presented. This will be used as a lower bound when plotting. */
    const double noise_average_scaled = noise_average +
                                        10.0 * std::log10(frequency_domain->scale_factor /
                                                          frequency_domain->energy_factor);

    frequency_domain->noise_moving_average.value = 0;
    if (m_noise_moving_average.size() >= NOISE_MOVING_AVERAGE_SIZE)
        m_noise_moving_average.pop_back();
    m_noise_moving_average.push_front(noise_average_scaled);

    const double normalization = static_cast<double>(m_noise_moving_average.size());
    for (const auto &noise : m_noise_moving_average)
        frequency_domain->noise_moving_average.value += noise / normalization;

    /* TODO: Adjust worst spur if it turns out that it's one of the harmonics
             that ended up within the blind spot? */
}

void DataProcessing::ProcessAndIdentify(const std::vector<std::complex<double>> &fft,
                                        ProcessedRecord &record, Tone &dc, Tone &fundamental,
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

    auto &x = record.frequency_domain->x;
    auto &y = record.frequency_domain->y;
    const auto &bin_range = record.frequency_domain->step;
    const auto &scale_factor = record.frequency_domain->scale_factor;
    const auto &energy_factor = record.frequency_domain->energy_factor;
    const size_t nof_skirt_bins = static_cast<size_t>(m_parameters.nof_skirt_bins);
    const double nyquist_frequency = static_cast<double>(x.size() - 1) * bin_range;
    const bool fixed_fundamental = m_parameters.fundamental_frequency > 0 &&
                                   m_parameters.fundamental_frequency <= nyquist_frequency;
    std::deque<double> cursor;

    dc = {};
    fundamental = {};
    spur = {};
    power = 0.0;

    auto FromComplex = [&](std::complex<double> value) -> double {
        return std::pow(2.0 * std::abs(value) / static_cast<double>(fft.size()), 2.0);
    };

    /* Prepare the FFT moving average object to receive a new entry. */
    m_fft_moving_average.PrepareNewEntry(fft.size());

    /* If we're performing the analysis with a fixed fundamental frequency, we
       start off by constructing that `Tone` object. */
    if (fixed_fundamental)
    {
        const double idx = m_parameters.fundamental_frequency / bin_range;
        const size_t idx_center = static_cast<size_t>(m_parameters.fundamental_frequency / bin_range + 0.5);
        const size_t idx_low = idx_center < nof_skirt_bins ? 0 : idx_center - nof_skirt_bins;
        const size_t idx_high = std::min(idx_center + nof_skirt_bins, x.size() - 1);

        for (size_t i = idx_low; i <= idx_high; ++i)
        {
            /* We need to base this analysis on potentially averaged data. We'll
               revisit these bins in the loop below and thus recalculate these
               values. This obviously slightly suboptimal, but I think the code
               is clearer and the drawbacks are not that significant. */
            double value = m_fft_moving_average.InsertAndAverage(i, FromComplex(fft[i]));
            fundamental.values.push_back(value * energy_factor);
        }

        fundamental.UpdatePower();
        fundamental.frequency = m_parameters.fundamental_frequency;
        fundamental.idx = idx_center;
        fundamental.idx_fraction = idx - static_cast<double>(idx_center);
        fundamental.idx_low = idx_low;
        fundamental.idx_high = idx_high;
    }

    for (size_t i = 0; i < x.size(); ++i)
    {
        x[i] = static_cast<double>(i) * bin_range;

        /* Calculate the unscaled value. */
        y[i] = m_fft_moving_average.InsertAndAverage(i, FromComplex(fft[i]));

        /* We will always need the energy-accurate bin value for the calculations below. */
        const double y_power = y[i] * energy_factor;

        /* Scale the value stored for plotting with the `scale_factor`, which
           can result in either an amplitude-accurate spectrum or an
           energy-accurate spectrum. Convert to decibels. */
        y[i] = 10.0 * std::log10(y[i] * scale_factor);

        /* Add the bin's contribution to the total power. */
        power += y_power;

        /* DC tone analysis. */
        if (i <= nof_skirt_bins)
        {
            dc.power += y_power;
            dc.idx_high = i;
            dc.values.push_back(y_power);
            continue;
        }

        if (cursor.size() >= (2 * nof_skirt_bins + 1))
            cursor.pop_front();
        cursor.push_back(y_power);

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

        if (!fixed_fundamental && denominator > fundamental.power)
        {
            if ((center_idx - fundamental.idx) > (2 * nof_skirt_bins))
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

        if (denominator > spur.power && (center_idx - fundamental.idx) > (2 * nof_skirt_bins))
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
    }
}

void DataProcessing::PlaceHarmonics(const Tone &fundamental, const ProcessedRecord &record,
                                    std::vector<Tone> &harmonics)
{
    /* Analyze the harmonics. We only look at HD2 to HD5. */
    for (int hd = 2; hd <= 5; ++hd)
    {
        const double f = FoldFrequency(fundamental.frequency * hd,
                                       record.time_domain->sampling_frequency.value);
        harmonics.emplace_back(*record.frequency_domain, f, m_parameters.nof_skirt_bins);
    }
}

void DataProcessing::PlaceInterleavingSpurs(const Tone &fundamental, const ProcessedRecord &record,
                                            Tone &gain, Tone &offset)
{
    const double f_gain = FoldFrequency(
        fundamental.frequency + record.time_domain->sampling_frequency.value / 2,
        record.time_domain->sampling_frequency.value
    );

    /* Estimate the interleaving gain spur. */
    gain = std::move(Tone(*record.frequency_domain, f_gain, m_parameters.nof_skirt_bins));

    /* Estimate the interleaving offset spur. */
    offset = std::move(Tone(*record.frequency_domain,
                            record.time_domain->sampling_frequency.value / 2,
                            m_parameters.nof_skirt_bins));
}

void DataProcessing::ResolveHarmonicOverlaps(const Tone &dc, const Tone &fundamental,
                                             std::vector<Tone> &harmonics, bool &overlap)
{
    /* Now we have to figure out if these tones overlap in any way. If they do,
       we mark the metrics as 'not trustworthy' but still perform the
       calculations as best we can. The goal is to only count the energy
       contribution from a bin _once_, favoring signal energy over distortion
       energy when there's an overlap. */

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

void DataProcessing::ResolveInterleavingSpurOverlaps(const Tone &dc, const Tone &fundamental,
                                                     const std::vector<Tone> &harmonics, Tone &gain,
                                                     Tone &offset, bool &overlap)
{
    ResolveOverlap(gain, fundamental, overlap);
    ResolveOverlap(offset, fundamental, overlap);

    ResolveOverlap(gain, dc, overlap);
    ResolveOverlap(offset, dc, overlap);

    for (const auto &harmonic : harmonics)
    {
        ResolveOverlap(gain, harmonic, overlap);
        ResolveOverlap(offset, harmonic, overlap);
    }
}

void DataProcessing::ResolveOverlap(Tone &tone, const Tone &other, bool &overlap)
{
    /* Set the overlapping bins to zero for the tone if there's an overlap. */
    if ((tone.idx_low >= other.idx_low) && (tone.idx_low <= other.idx_high))
    {
        for (size_t j = 0; j <= (other.idx_high - tone.idx_low) && j < tone.values.size(); ++j)
            tone.values.at(j) = 0;
        overlap = true;
    }
    else if ((tone.idx_high <= other.idx_high) && (tone.idx_high >= other.idx_low))
    {
        for (size_t j = (other.idx_low - tone.idx_low); j < tone.values.size(); ++j)
            tone.values.at(j) = 0;
        overlap = true;
    }
}

void DataProcessing::AnalyzeTimeDomain(TimeDomainRecord &record)
{
    for (const auto &y : record.y)
    {
        if (y > record.max.value)
            record.max.value = y;

        if (y < record.min.value)
            record.min.value = y;

        record.mean.value += y;
    }

    record.mean.value /= static_cast<double>(record.y.size());

    for (const auto &y : record.y)
    {
        const double diff = (y - record.mean.value);
        record.sdev.value += diff * diff;
    }

    record.sdev.value /= static_cast<double>(record.y.size());
    record.sdev.value = std::sqrt(record.sdev.value);
}

void DataProcessing::ProcessMessages()
{
    DataProcessingMessage message;
    while (SCAPE_EOK == m_write_message_queue.Read(message, 0))
    {
        switch (message.id)
        {
        case DataProcessingMessageId::SET_AFE_PARAMETERS:
            m_afe = std::move(message.afe);
            break;

        case DataProcessingMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
            m_clock_system = std::move(message.clock_system);
            break;

        case DataProcessingMessageId::SET_PROCESSING_PARAMETERS:
            m_parameters = std::move(message.processing);
            m_fft_moving_average.SetNumberOfAverages(m_parameters.nof_fft_averages);
            m_noise_moving_average.clear();
            break;

        case DataProcessingMessageId::CLEAR_PROCESSING_MEMORY:
            m_fft_moving_average.Clear();
            m_noise_moving_average.clear();
            break;

        default:
            break;
        }
    }
}

template <typename... Args>
std::string DataProcessing::FormatLog(Args &&... args)
{
    return m_label + ": " + fmt::format(std::forward<Args>(args)...);
}
