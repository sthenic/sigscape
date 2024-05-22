#pragma once

#include "generator.h"
#include "nlohmann/json.hpp"

struct SineGeneratorTopParameters
{
    SineGeneratorTopParameters()
        : record_length{18000}
        , trigger_frequency{5.0}
        , amplitude{0.8}
        , offset{0.0}
        , frequency{13.12e6}
        , phase{0.0}
        , noise{0.01}
        , harmonic_distortion{false}
        , interleaving_distortion{false}
        , randomize{false}
    {}

    size_t record_length;
    double trigger_frequency;
    double amplitude;
    double offset;
    double frequency;
    double phase;
    double noise;
    bool harmonic_distortion;
    bool interleaving_distortion;
    bool randomize;
};

/* Define serialization/deserialization for `SineGeneratorTopParameters`. */
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    SineGeneratorTopParameters,
    record_length,
    trigger_frequency,
    amplitude,
    offset,
    frequency,
    phase,
    noise,
    harmonic_distortion,
    interleaving_distortion,
    randomize
);

struct SineGeneratorClockSystemParameters
{
    SineGeneratorClockSystemParameters()
        : sampling_frequency{500e6}
    {}

    double sampling_frequency;
};

/* Define serialization/deserialization for `SineGeneratorClockSystemParameters`. */
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    SineGeneratorClockSystemParameters,
    sampling_frequency
);

class SineGenerator : public Generator
{
public:
    SineGenerator()
        : Generator(1)
        , m_top_parameters{}
        , m_clock_system_parameters{}
        , m_uniform_distribution{0.0, 1.0}
    {}

    ~SineGenerator() override
    {
        Stop();
    }

private:
    SineGeneratorTopParameters m_top_parameters;
    SineGeneratorClockSystemParameters m_clock_system_parameters;
    std::uniform_real_distribution<double> m_uniform_distribution;

    void Generate() override;
    double GetTriggerFrequency() override;
    double GetSamplingFrequency() override;
    double GetNoise() override;
    int GetParameters(GeneratorMessageId id, nlohmann::json &json) override;
    int SetParameters(GeneratorMessageId id, const nlohmann::json &json) override;

    void SeedHeader(ADQGen4RecordHeader *header) override;
    std::shared_ptr<ADQGen4Record> Sine();
};
