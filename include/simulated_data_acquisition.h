/*
 * A pure software implementation of the data acquisition interface. Used for
 * testing and debugging purposes.
 */

#ifndef SIMULATED_DATA_ACQUISITION_H_QLFUTR
#define SIMULATED_DATA_ACQUISITION_H_QLFUTR

#include "data_acquisition.h"
#include "thread_safe_queue.h"
#include "data_types.h"

#include <thread>
#include <future>
#include <vector>
#include <random>

class SimulatedDataAcquisition : public DataAcquisition
{
public:
    SimulatedDataAcquisition();
    ~SimulatedDataAcquisition();

    int Initialize(size_t record_length, double trigger_rate_hz);
    int Start();
    int Stop();
    int WaitForBuffer(void *&buffer, int timeout, void *status);
    int ReturnBuffer(void *buffer);

private:
    std::thread m_thread;
    std::promise<void> m_stop;
    bool m_is_running;
    int m_thread_exit_code;

    std::timed_mutex m_mutex;
    ThreadSafeQueue<struct TimeDomainRecord *> m_read_queue;
    ThreadSafeQueue<struct TimeDomainRecord *> m_write_queue;
    std::vector<struct TimeDomainRecord *> m_records;

    size_t m_record_length;
    double m_trigger_rate_hz;
    std::default_random_engine m_random_generator;
    std::normal_distribution<double> m_distribution;

    void NoisySine(struct TimeDomainRecord &record, size_t nof_samples);
    int AllocateRecord(struct TimeDomainRecord *&record, size_t nof_samples);
    int AllocateOrReuseRecord(struct TimeDomainRecord *&record, size_t nof_samples);
    int AllocateNoisySine(struct TimeDomainRecord *&record, size_t nof_samples);
    void MainLoop(std::future<void> stop);
    void FreeBuffers();

    static constexpr size_t NOF_RECORDS_MAX = 100;
    static constexpr double SAMPLING_RATE_HZ = 1000000.0;
};

#endif
