#ifndef DATA_PROCESSING_H_J8UO0P
#define DATA_PROCESSING_H_J8UO0P

#include "buffer_thread.h"
#include "data_acquisition.h"
#include "data_types.h"
#include "error.h"

#include <deque>

class DataProcessing : public BufferThread<DataProcessing, ProcessedRecord, 100, true>
{
public:
    DataProcessing(DataAcquisition &acquisition);
    ~DataProcessing();
    DataProcessing(const DataProcessing &other) = delete;
    DataProcessing &operator=(const DataProcessing &other) = delete;

    int Initialize();
    int Start() override;
    int Stop() override;
    void MainLoop() override;

private:
    static const size_t WATERFALL_SIZE = 20;

    DataAcquisition &m_acquisition;
    std::deque<std::shared_ptr<FrequencyDomainRecord>> m_waterfall;

    template <typename T>
    int NextPowerOfTwo(T i);

    template <typename T>
    int PreviousPowerOfTwo(T i);
};

#endif
