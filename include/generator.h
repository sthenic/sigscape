#ifndef SIMULATOR_H_69EMSR
#define SIMULATOR_H_69EMSR

#include "buffer_thread.h"
#include "data_acquisition.h"
#include "data_types.h"
#include "error.h"

#include <random>

class Generator : public BufferThread<Generator, TimeDomainRecord>
{
public:
    Generator();

    struct Parameters
    {
        Parameters()
            : sine()
            , record_length(1024)
            , trigger_frequency(1)
        {}

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
        } sine;

        size_t record_length;
        double trigger_frequency;
    };

    int Initialize(const Parameters &parameters);
    int WaitForBuffer(std::shared_ptr<TimeDomainRecord> &buffer, int timeout) override;
    int ReturnBuffer(std::shared_ptr<TimeDomainRecord> buffer) override;
    void MainLoop() override;

private:
    std::default_random_engine m_random_generator;
    std::normal_distribution<double> m_distribution;
    Parameters m_parameters;

    void NoisySine(TimeDomainRecord &record, size_t count);
};

/* Thin wrapper around the simulator that implements the interface declared by DataAcquisition. */
class DataAcquisitionSimulator : public DataAcquisition
{
public:
    int Initialize(const Generator::Parameters &parameters = Generator::Parameters())
    {
        return m_generator.Initialize(parameters);
    }

    int Start()
    {
        return m_generator.Start();
    }

    int Stop()
    {
        return m_generator.Stop();
    }

    int WaitForBuffer(std::shared_ptr<void> &buffer, int timeout, void *status)
    {
        (void)status;
        /* FIXME: This void business sucks. I don't think this cast is ok, strictly speaking. */
        return m_generator.WaitForBuffer(reinterpret_cast<std::shared_ptr<TimeDomainRecord> &>(buffer), timeout);
    }

    int ReturnBuffer(std::shared_ptr<void> buffer)
    {
        return m_generator.ReturnBuffer(std::static_pointer_cast<TimeDomainRecord>(buffer));
    }

private:
    Generator m_generator;
};

#endif

