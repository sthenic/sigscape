#ifndef DATA_PROCESSING_H_J8UO0P
#define DATA_PROCESSING_H_J8UO0P

#include "buffer_thread.h"
#include "data_acquisition.h"
#include "data_types.h"
#include "error.h"

class DataProcessing : public BufferThread<DataProcessing, ProcessedRecord>
{
public:
    DataProcessing(DataAcquisition &acquisition);
    ~DataProcessing();

    int Initialize();
    int Start();
    int Stop();
    int WaitForBuffer(struct ProcessedRecord *&buffer, int timeout);
    int ReturnBuffer(struct ProcessedRecord *buffer);
    void MainLoop();

private:
    DataAcquisition &m_acquisition;

    template <typename T>
    int NextPowerOfTwo(T i);

    template <typename T>
    int PreviousPowerOfTwo(T i);
};

#endif
