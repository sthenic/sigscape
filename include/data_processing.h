#ifndef DATA_PROCESSING_H_J8UO0P
#define DATA_PROCESSING_H_J8UO0P

#include "smart_buffer_thread.h"
#include "window.h"
#include "data_types.h"
#include "error.h"

#include <deque>

class DataProcessing : public SmartBufferThread<DataProcessing, ProcessedRecord, 100, true>
{
public:
    DataProcessing(void *handle, int index, int channel, const std::string &label,
                   const struct ADQConstantParameters &constant);
    ~DataProcessing();
    DataProcessing(const DataProcessing &other) = delete;
    DataProcessing &operator=(const DataProcessing &other) = delete;

    void SetAnalogFrontendParameters(const struct ADQAnalogFrontendParametersChannel &afe);
    void SetWindowType(WindowType type);

    void MainLoop() override;

private:
    static const size_t WATERFALL_SIZE = 20;
    static const size_t NOF_SKIRT_BINS_DEFAULT = 5;
    static const size_t PERSISTENCE_SIZE = 30;
    static const size_t NOISE_MOVING_AVERAGE_SIZE = 50;
    void *m_handle;
    int m_index;
    int m_channel;
    std::string m_label;
    struct ADQAnalogFrontendParametersChannel m_afe;
    struct ADQConstantParameters m_constant;
    WindowCache m_window_cache;
    WindowType m_window_type;
    size_t m_nof_skirt_bins;
    std::deque<std::shared_ptr<FrequencyDomainRecord>> m_waterfall;
    std::deque<std::shared_ptr<TimeDomainRecord>> m_persistence;
    std::deque<double> m_noise_moving_average;

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
        Tone(const ProcessedRecord *record, double frequency, size_t nof_skirt_bins);

        double power;
        double frequency;
        size_t idx;
        double idx_fraction;
        size_t idx_low;
        size_t idx_high;
        std::vector<double> values;
    };

    /* Given a frequency `f` and the sampling frequency `fs`, fold `f` until we
       reach the first Nyquist zone. */
    static double FoldFrequency(double f, double fs);

    /* Given a frequency `f` and the sampling frequency `fs`, fold `f` until we
       reach the first Nyquist zone. */
    static size_t FoldIndex(size_t f, size_t fs);

    /* Analyze the fourier transform contained in `fft` of and store the results
       in the processed `record`. */
    void AnalyzeFourierTransform(const std::vector<std::complex<double>> &fft,
                                 ProcessedRecord *record);

    /* Identify the fundamental tone and the worst spur. The spectrum is
       converted into decibels (in place). */
    void ProcessAndIdentify(const std::vector<std::complex<double>> &fft, ProcessedRecord *record,
                            Tone &dc, Tone &fundamental, Tone &spur, double &power);

    /* Given a fundamental tone, place the harmonic overtones into the spectrum. */
    void PlaceHarmonics(const Tone &fundamental, const ProcessedRecord *record,
                        std::vector<Tone> &harmonics);

    /* Given a fundamental tone, place the interleaving  */
    void PlaceInterleavingSpurs(const Tone &fundamental, const ProcessedRecord *record, Tone &gain,
                                Tone &offset);

    /* Resolve overlaps between the harmonics and the other spectral components. */
    void ResolveHarmonicOverlaps(const Tone &dc, const Tone &fundamental,
                                 std::vector<Tone> &harmonics, bool &overlap);

    /* Resolve overlaps between the interleaving spurs and the other spectral components. */
    void ResolveInterleavingSpurOverlaps(const Tone &dc, const Tone &fundamental,
                                         const std::vector<Tone> &harmonics, Tone &gain,
                                         Tone &offset, bool &overlap);

    /* Resolve overlap between two tones. If `tone` and `other` overlap, the
       overlapping bins in `tone` will be set to zero. */
    void ResolveOverlap(Tone &tone, const Tone &other, bool &overlap);

    /* Analyze the time domain data. */
    void AnalyzeTimeDomain(ProcessedRecord *record);
};

#endif
