#ifndef SIMULATOR_H_69EMSR
#define SIMULATOR_H_69EMSR

#include "buffer_thread.h"
#include "data_acquisition.h"
#include "data_types.h"

#include <random>

class Simulator : public BufferThread<Simulator, struct TimeDomainRecord>
{
public:
    Simulator();
    ~Simulator();

    int Initialize(size_t record_length, double trigger_rate_hz);
    int WaitForBuffer(struct TimeDomainRecord *&buffer, int timeout);
    int ReturnBuffer(struct TimeDomainRecord *buffer);
    void MainLoop();

private:
    size_t m_record_length;
    double m_trigger_rate_hz;
    std::default_random_engine m_random_generator;
    std::normal_distribution<double> m_distribution;

    void NoisySine(struct TimeDomainRecord &record, size_t count);
};

/* Thin wrapper around the simulator that implements the interface declared by DataAcquisition. */
class DataAcquisitionSimulator : public DataAcquisition
{
public:
    DataAcquisitionSimulator() {};
    ~DataAcquisitionSimulator() {};

    int Initialize(size_t record_length, double trigger_rate_hz)
    {
        return m_simulator.Initialize(record_length, trigger_rate_hz);
    }

    int Start()
    {
        return m_simulator.Start();
    }

    int Stop()
    {
        return m_simulator.Stop();
    }

    int WaitForBuffer(void *&buffer, int timeout, void *status)
    {
        (void)status;
        return m_simulator.WaitForBuffer(reinterpret_cast<TimeDomainRecord *&>(buffer), timeout);
    }

    int ReturnBuffer(void *buffer)
    {
        return m_simulator.ReturnBuffer(reinterpret_cast<TimeDomainRecord *&>(buffer));
    }

private:
    Simulator m_simulator;
};

#endif

