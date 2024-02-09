#pragma once

#include "generator.h"
#include "nlohmann/json.hpp"

struct PulseGeneratorTopParameters
{
    PulseGeneratorTopParameters()
        : record_length{1024}
        , trigger_frequency{30}
        , amplitude{0.8}
        , baseline{0.0}
        , level{0.4}
        , noise{0.1}
        , width{32}
        , period{256}
        , offset{0}
        , gauss{true}
    {}

    size_t record_length;
    double trigger_frequency;
    double amplitude;
    double baseline;
    double level;
    double noise;
    size_t width;
    size_t period;
    size_t offset;
    bool gauss;
};

/* Define serialization/deserialization for `PulseGeneratorTopParameters`. */
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    PulseGeneratorTopParameters,
    record_length,
    trigger_frequency,
    amplitude,
    baseline,
    level,
    noise,
    width,
    period,
    offset,
    gauss
);

struct PulseGeneratorClockSystemParameters
{
    PulseGeneratorClockSystemParameters()
        : sampling_frequency{500e6}
    {}

    double sampling_frequency;
};

/* Define serialization/deserialization for `PulseGeneratorClockSystemParameters`. */
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    PulseGeneratorClockSystemParameters,
    sampling_frequency
)

class PulseGenerator
    : public Generator<PulseGeneratorTopParameters, PulseGeneratorClockSystemParameters>
{
public:
    PulseGenerator(size_t nof_channels = 1)
        : Generator<PulseGeneratorTopParameters, PulseGeneratorClockSystemParameters>(nof_channels)
    {}

    ~PulseGenerator() = default;

protected:
    void Generate() override;
    int SetParameters(GeneratorMessageId id, const nlohmann::json &json) override;
    int GetParameters(GeneratorMessageId id, nlohmann::json &json) override;

private:
    std::shared_ptr<ADQGen4Record> Pulse();
    std::shared_ptr<ADQGen4Record> Attributes();
};
