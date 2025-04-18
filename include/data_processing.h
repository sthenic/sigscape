#pragma once

#include "smart_buffer_thread.h"
#include "window.h"
#include "data_types.h"
#include "error.h"

#include <deque>
#include <cmath>

enum class FrequencyDomainScaling
{
    AMPLITUDE,
    ENERGY,
    NOF_ENTRIES
};

#define FREQUENCY_DOMAIN_SCALING_LABELS {"Amplitude", "Energy"}

enum class DataProcessingMessageId
{
    SET_AFE_PARAMETERS,
    SET_PROCESSING_PARAMETERS,
    CLEAR_PROCESSING_MEMORY,
};

struct DataProcessingParameters
{
    DataProcessingParameters();

    WindowType window_type;
    FrequencyDomainScaling fft_scaling;
    int nof_skirt_bins;
    int nof_fft_averages;
    double fundamental_frequency;
    bool convert_horizontal;
    bool convert_vertical;
    bool fullscale_enob;
    bool fft_maximum_hold;
};

struct DataProcessingMessage
{
    DataProcessingMessage() = default;

    DataProcessingMessage(DataProcessingMessageId id)
        : id(id)
    {}

    DataProcessingMessage(DataProcessingMessageId id, ADQAnalogFrontendParametersChannel &afe)
        : id(id)
        , afe(afe)
    {}

    DataProcessingMessage(DataProcessingMessageId id, ADQClockSystemParameters &clock_system)
        : id(id)
        , clock_system(clock_system)
    {}

    DataProcessingMessage(DataProcessingMessageId id, const DataProcessingParameters &processing)
        : id(id)
        , processing(processing)
    {}

    DataProcessingMessageId id;
    ADQAnalogFrontendParametersChannel afe;
    ADQClockSystemParameters clock_system;
    DataProcessingParameters processing;
};

class DataProcessing
    : public SmartBufferThread<ProcessedRecord, DataProcessingMessage>
{
public:
    DataProcessing(void *handle, int index, int channel, const std::string &label,
                   const ADQConstantParameters &constant);
    ~DataProcessing();
    DataProcessing(const DataProcessing &other) = delete;
    DataProcessing &operator=(const DataProcessing &other) = delete;

    void MainLoop() override;

private:
    static const size_t WATERFALL_SIZE = 20;
    static const size_t NOISE_MOVING_AVERAGE_SIZE = 50;
    void *m_handle;
    int m_index;
    int m_channel;
    std::string m_label;
    ADQAnalogFrontendParametersChannel m_afe;
    ADQConstantParameters m_constant;
    WindowCache m_window_cache;
    DataProcessingParameters m_parameters;
    TimeDomainMetrics m_time_domain_metrics;
    std::deque<std::shared_ptr<FrequencyDomainRecord>> m_waterfall;
    std::deque<double> m_noise_moving_average;
    MovingAverage m_fft_moving_average;
    MaximumHold m_fft_maximum_hold;

    template <typename T>
    static size_t NextPowerOfTwo(T i);

    template <typename T>
    static size_t PreviousPowerOfTwo(T i);

    template <typename T>
    static void TransformToUnitRange(const T *data, double code_normalization,
                                     const Window *window, std::vector<double> &y);

    struct Tone
    {
        Tone() = default;
        Tone(const FrequencyDomainRecord &record, double frequency, size_t nof_skirt_bins);
        std::string Stringify() const;

        /* Get the power in decibels. */
        double PowerInDecibels() const { return 10.0 * std::log10(power); }
        double UpdatePower()
        {
            power = 0.0;
            for (const auto &v : values)
                power += v;
            return power;
        }

        size_t Bins() const
        {
            size_t result = 0;
            for (const auto &v : values)
                result += (v != 0.0) ? 1 : 0;
            return result;
        }

        double power;
        double frequency;
        size_t idx;
        double idx_fraction;
        size_t idx_low;
        size_t idx_high;
        bool overlap;
        std::vector<double> values;
    };

    /* Given a frequency `f` and the sampling frequency `fs`, fold `f` until we
       reach the first Nyquist zone. */
    static double FoldFrequency(double f, double fs);

    /* Given a frequency `f` and the sampling frequency `fs`, fold `f` until we
       reach the first Nyquist zone. */
    static size_t FoldIndex(size_t f, size_t fs);

    /* Process the raw data from the digitizer, filling the `time_domain` and
       `frequency_domain` members of the `processed_record`. The parameter
       `raw_time_domain` _must not_ be NULL. */
    int ProcessRecord(const ADQGen4Record *raw_time_domain, ProcessedRecord &processed_record);

    /* Analyze the fourier transform contained in `fft` of and store the results
       in the processed `record`. */
    void AnalyzeFrequencyDomain(
        const std::vector<std::complex<double>> &fft, ProcessedRecord &record);

    /* Identify the fundamental tone and the worst spur. The spectrum is
       converted into decibels (in place). */
    void ProcessAndIdentify(const std::vector<std::complex<double>> &fft, ProcessedRecord &record,
                            Tone &dc, Tone &fundamental, Tone &spur, double &power);

    /* Given a fundamental tone, place the harmonic overtones into the spectrum. */
    void PlaceHarmonics(const Tone &fundamental, const ProcessedRecord &record,
                        std::vector<Tone> &harmonics);

    /* Given a fundamental tone, place the interleaving  */
    void PlaceInterleavingSpurs(const Tone &fundamental, const ProcessedRecord &record, Tone &gain,
                                Tone &offset);

    /* Resolve overlaps between the harmonics and the other spectral components. */
    void ResolveHarmonicOverlaps(const Tone &dc, const Tone &fundamental,
                                 std::vector<Tone> &harmonics);

    /* Resolve overlaps between the interleaving spurs and the other spectral components. */
    void ResolveInterleavingSpurOverlaps(const Tone &dc, const Tone &fundamental,
                                         const std::vector<Tone> &harmonics, Tone &gain,
                                         Tone &offset);

    /* Resolve overlap between two tones. If `tone` and `other` overlap, the
       overlapping bins in `tone` will be set to zero. */
    void ResolveOverlap(Tone &tone, const Tone &other);

    /* Analyze the time domain data. */
    void AnalyzeTimeDomain(TimeDomainRecord &record);

    /* Postprocess the time domain and frequency domain data. */
    void Postprocess(ProcessedRecord &record);

    /* Process messages posted to the thread. */
    void ProcessMessages();

    /* Format the log message using `fmt::format`, attaching the thread-specific header. */
    template <typename... Args>
    std::string FormatLog(Args &&... args);
};
