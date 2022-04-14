#ifndef DATA_PROCESSING_H_J8UO0P
#define DATA_PROCESSING_H_J8UO0P

#include "smart_buffer_thread.h"
#include "data_types.h"
#include "error.h"

#include <deque>

class DataProcessing : public SmartBufferThread<DataProcessing, ProcessedRecord, 100, true>
{
public:
    DataProcessing(void *control_unit, int index, int channel);
    ~DataProcessing();
    DataProcessing(const DataProcessing &other) = delete;
    DataProcessing &operator=(const DataProcessing &other) = delete;

    void MainLoop() override;

private:
    static const size_t WATERFALL_SIZE = 20;
    void *m_control_unit;
    int m_index;
    int m_channel;

    std::deque<std::shared_ptr<FrequencyDomainRecord>> m_waterfall;

    template <typename T>
    int NextPowerOfTwo(T i);

    template <typename T>
    int PreviousPowerOfTwo(T i);
};

#endif
