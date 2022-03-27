#ifndef SIMULATOR_H_69EMSR
#define SIMULATOR_H_69EMSR

#include "buffer_thread.h"
#include "data_acquisition.h"
#include "data_types.h"
#include "error.h"

#include <random>

class Simulator : public BufferThread<Simulator, TimeDomainRecord>
{
public:
    Simulator();
    ~Simulator();

    struct SineWave
    {
        SineWave()
            : amplitude(1.0)
            , offset(0.0)
            , frequency(1e6)
            , phase(0.0)
            , noise_std_dev(0.1)
            , sampling_frequency(500e6)
            , harmonic_distortion(false)
        {};

        double amplitude;
        double offset;
        double frequency;
        double phase;
        double noise_std_dev;
        double sampling_frequency;
        bool harmonic_distortion;
    };

    int Initialize(size_t record_length, double trigger_rate_hz, const struct SineWave &sine = SineWave());
    int WaitForBuffer(std::shared_ptr<TimeDomainRecord> &buffer, int timeout) override;
    int ReturnBuffer(std::shared_ptr<TimeDomainRecord> buffer) override;
    void MainLoop() override;

private:
    size_t m_record_length;
    double m_trigger_rate_hz;
    std::default_random_engine m_random_generator;
    std::normal_distribution<double> m_distribution;
    struct SineWave m_sine;

    void NoisySine(TimeDomainRecord &record, size_t count);
};

/* Thin wrapper around the simulator that implements the interface declared by DataAcquisition. */
class DataAcquisitionSimulator : public DataAcquisition
{
public:
    DataAcquisitionSimulator() {};
    ~DataAcquisitionSimulator() {};

    int Initialize(size_t record_length, double trigger_rate_hz, const struct Simulator::SineWave &sine = Simulator::SineWave())
    {
        return m_simulator.Initialize(record_length, trigger_rate_hz, sine);
    }

    int Start()
    {
        return m_simulator.Start();
    }

    int Stop()
    {
        return m_simulator.Stop();
    }

    int WaitForBuffer(std::shared_ptr<void> &buffer, int timeout, void *status)
    {
        (void)status;
        return m_simulator.WaitForBuffer(reinterpret_cast<std::shared_ptr<TimeDomainRecord> &>(buffer), timeout);
    }

    int ReturnBuffer(std::shared_ptr<void> buffer)
    {
        return m_simulator.ReturnBuffer(reinterpret_cast<std::shared_ptr<TimeDomainRecord> &>(buffer));
    }

private:
    Simulator m_simulator;
};

#endif

