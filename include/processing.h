#ifndef PROCESSING_H_DCFVWG
#define PROCESSING_H_DCFVWG

#include "data_acquisition.h"
#include "thread_safe_queue.h"
#include "data_types.h"

#include <thread>
#include <future>
#include <mutex>
#include <vector>

class Processing
{
public:
    Processing(DataAcquisition &acquisition);
    ~Processing();

    struct ProcessedRecord
    {
        struct TimeDomainRecord *time_domain;
        struct FrequencyDomainRecord *frequency_domain;
    };

    int Initialize();
    int Start();
    int Stop();
    int WaitForBuffer(struct ProcessedRecord *&buffer, int timeout);
    int ReturnBuffer(struct ProcessedRecord *buffer);

private:
    std::thread m_thread;
    std::promise<void> m_stop;
    bool m_is_running;
    int m_thread_exit_code;
    DataAcquisition &m_acquisition;

    std::timed_mutex m_mutex;
    ThreadSafeQueue<struct ProcessedRecord *> m_read_queue;
    ThreadSafeQueue<struct FrequencyDomainRecord *> m_write_queue;
    std::vector<struct FrequencyDomainRecord *> m_records;

    int AllocateRecord(struct FrequencyDomainRecord *&record, size_t nof_samples);
    /* Swap order -> ReuseOrAllocate */
    int AllocateOrReuseRecord(struct FrequencyDomainRecord *&record, size_t nof_samples);
    void MainLoop(std::future<void> stop);
    void FreeBuffers();

    static constexpr size_t NOF_RECORDS_MAX = 100;
    static constexpr size_t FFT_SIZE = 8192; /* FIXME: Make a function to look up the closest size. */
};

#endif
