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
        , noise{0.1}
        , harmonic_distortion{false}
        , interleaving_distortion{false}
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
    interleaving_distortion
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

class SineGenerator
    : public Generator<SineGeneratorTopParameters, SineGeneratorClockSystemParameters>
{
public:
    SineGenerator(size_t nof_channels = 1)
        : Generator<SineGeneratorTopParameters, SineGeneratorClockSystemParameters>(nof_channels)
    {}

    ~SineGenerator() override = default;

private:
    void Generate() override;
    void Sine(int16_t *const data, size_t count, bool &overrange);
};
