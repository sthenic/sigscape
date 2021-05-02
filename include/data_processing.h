#ifndef DATA_PROCESSING_H_J8UO0P
#define DATA_PROCESSING_H_J8UO0P

#include "buffer_thread.h"
#include "data_acquisition.h"
#include "data_types.h"

class DataProcessing : public BufferThread<DataProcessing, ProcessedRecord>
{
public:
    DataProcessing(DataAcquisition &acquisition);
    ~DataProcessing();

    int Initialize();
    int WaitForBuffer(struct ProcessedRecord *&buffer, int timeout);
    int ReturnBuffer(struct ProcessedRecord *buffer);
    void MainLoop();

private:
    DataAcquisition &m_acquisition;

    /* FIXME: Remove */
    static constexpr size_t FFT_SIZE = 8192;
};

#endif
