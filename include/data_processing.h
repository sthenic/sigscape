#ifndef DATA_PROCESSING_H_J8UO0P
#define DATA_PROCESSING_H_J8UO0P

#include "smart_buffer_thread.h"
#include "data_types.h"
#include "error.h"

#include <deque>

class DataProcessing : public SmartBufferThread<DataProcessing, ProcessedRecord, 100, true>
{
public:
    DataProcessing(void *control_unit, int index, int channel, const std::string &label);
    ~DataProcessing();
    DataProcessing(const DataProcessing &other) = delete;
    DataProcessing &operator=(const DataProcessing &other) = delete;

    void SetAnalogFrontendParameters(const struct ADQAnalogFrontendParametersChannel &afe);

    void MainLoop() override;

private:
    static const size_t WATERFALL_SIZE = 20;
    void *m_control_unit;
    int m_index;
    int m_channel;
    std::string m_label;
    struct ADQAnalogFrontendParametersChannel m_afe;

    std::deque<std::shared_ptr<FrequencyDomainRecord>> m_waterfall;

    template <typename T>
    size_t NextPowerOfTwo(T i);

    template <typename T>
    size_t PreviousPowerOfTwo(T i);

    void AnalyzeFourierTransform(const std::complex<double> *fft, size_t length, ProcessedRecord *record);
    void AnalyzeTimeDomain(ProcessedRecord *record);
};

#endif
