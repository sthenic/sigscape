#pragma once

#include "buffer_thread.h"
#include "error.h"

#ifdef MOCK_ADQAPI
#include "mock/adqapi_definitions.h"
#else
#include "ADQAPI.h"
#endif

#include <random>

/* We don't use the SmartBufferThread here since we'll end up using this to
   emulate data emitted through void pointers by the ADQAPI. */
class Generator : public BufferThread<Generator, ADQGen4Record>
{
public:
    Generator();

    struct Parameters
    {
        Parameters()
            : sine()
            , record_length(1024)
            , trigger_frequency(30)
        {}

        struct SineWave
        {
            SineWave()
                : amplitude(0.8)
                , offset(0.0)
                , frequency(13.12e6)
                , phase(0.0)
                , noise_std_dev(0.1)
                , harmonic_distortion(true)
                , interleaving_distortion(true)
            {};

            double amplitude;
            double offset;
            double frequency;
            double phase;
            double noise_std_dev;
            bool harmonic_distortion;
            bool interleaving_distortion;
        } sine;

        size_t record_length;
        double trigger_frequency;
    };

    int SetParameters(const Parameters &parameters);
    int SetSamplingFrequency(double sampling_frequency);
    void MainLoop() override;

private:
    std::default_random_engine m_random_generator;
    std::normal_distribution<double> m_distribution;
    Parameters m_parameters;
    double m_sampling_frequency;

    void NoisySine(ADQGen4Record &record, size_t count, bool &overrange);
};
