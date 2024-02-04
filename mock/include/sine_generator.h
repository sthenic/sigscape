#pragma once

#include "smart_buffer_thread.h"
#include "error.h"
#include "ADQAPI.h"

#include <random>

enum class SineGeneratorMessageId
{
    SET_PARAMETERS,
    SET_SAMPLING_FREQUENCY,
};

struct SineGeneratorParameters
{
    SineGeneratorParameters()
        : record_length{1024}
        , trigger_frequency{30}
        , sampling_frequency{500e6}
        , amplitude{0.8}
        , offset{0.0}
        , frequency{13.12e6}
        , phase{0.0}
        , noise{0.1}
        , harmonic_distortion{true}
        , interleaving_distortion{true}
    {}

    size_t record_length;
    double trigger_frequency;
    double sampling_frequency;
    double amplitude;
    double offset;
    double frequency;
    double phase;
    double noise;
    bool harmonic_distortion;
    bool interleaving_distortion;
};

struct SineGeneratorMessage
{
    SineGeneratorMessage() = default;

    SineGeneratorMessage(const SineGeneratorParameters &parameters)
        : id{SineGeneratorMessageId::SET_PARAMETERS}
        , parameters{parameters}
        , sampling_frequency{}
    {};

    SineGeneratorMessage(double sampling_frequency)
        : id{SineGeneratorMessageId::SET_SAMPLING_FREQUENCY}
        , parameters{}
        , sampling_frequency{sampling_frequency}
    {};

    SineGeneratorMessageId id;
    SineGeneratorParameters parameters;
    double sampling_frequency;
};

/* We don't use the SmartBufferThread here since we'll end up using this to
   emulate data emitted through void pointers by the ADQAPI. */
class SineGenerator
    : public SmartBufferThread<SineGenerator, ADQGen4Record, SineGeneratorMessage, 0, false, true>
{
public:
    SineGenerator();
    ~SineGenerator() = default;

    void MainLoop() override;

private:
    std::default_random_engine m_random_generator;
    std::normal_distribution<double> m_distribution;
    uint32_t m_record_number;
    uint32_t m_timestamp;
    SineGeneratorParameters m_parameters;

    void ProcessMessages();
    void Generate();
    void Sine(int16_t *const data, size_t count, bool &overrange);
};
