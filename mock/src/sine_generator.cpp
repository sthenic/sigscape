#include "sine_generator.h"

#include <cmath>
#include <limits>

void SineGenerator::Generate()
{
    auto sine = Sine();
    if (sine == NULL)
        return;

    /* Add to the outgoing queue. */
    EjectBuffer(sine);
}

double SineGenerator::GetTriggerFrequency()
{
    return m_top_parameters.trigger_frequency;
}

double SineGenerator::GetSamplingFrequency()
{
    return m_clock_system_parameters.sampling_frequency;
}

double SineGenerator::GetNoise()
{
    return m_top_parameters.noise;
}

int SineGenerator::GetParameters(GeneratorMessageId id, nlohmann::json &json)
{
    switch (id)
    {
    case GeneratorMessageId::GET_TOP_PARAMETERS:
        json = SineGeneratorTopParameters{};
        return SCAPE_EOK;

    case GeneratorMessageId::GET_CLOCK_SYSTEM_PARAMETERS:
        json = SineGeneratorClockSystemParameters{};
        return SCAPE_EOK;

    default:
        fprintf(stderr, "Unexpected message id %d.\n", static_cast<int>(id));
        return SCAPE_EINVAL;
    }
}

int SineGenerator::SetParameters(GeneratorMessageId id, const nlohmann::json &json)
{
    try
    {
        switch (id)
        {
        case GeneratorMessageId::SET_TOP_PARAMETERS:
            m_top_parameters = json.get<SineGeneratorTopParameters>();
            return SCAPE_EOK;

        case GeneratorMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
            m_clock_system_parameters = json.get<SineGeneratorClockSystemParameters>();
            return SCAPE_EOK;

        default:
            fprintf(stderr, "Unexpected message id %d.\n", static_cast<int>(id));
            return SCAPE_EINVAL;
        }
    }
    catch (const nlohmann::json::exception &e)
    {
        fprintf(stderr, "Failed to parse the parameter set %s.\n", e.what());
        return SCAPE_EINVAL;
    }
}

void SineGenerator::SeedHeader(ADQGen4RecordHeader *header)
{
    Generator::SeedHeader(header);
    header->data_format = ADQ_DATA_FORMAT_INT16;
    header->record_length = m_top_parameters.record_length;
}

std::shared_ptr<ADQGen4Record> SineGenerator::Sine()
{
    std::shared_ptr<ADQGen4Record> record;
    int result = ReuseOrAllocateBuffer(record, m_top_parameters.record_length * sizeof(int16_t));
    if (result != SCAPE_EOK)
    {
        if (result == SCAPE_EINTERRUPTED) /* Convert forced queue stop into an ok. */
            m_thread_exit_code = SCAPE_EOK;
        else
            m_thread_exit_code = result;

        return NULL;
    }

    /* Default header fields. */
    SeedHeader(record->header);

    const auto &p = m_top_parameters;
    const auto &sampling_frequency = m_clock_system_parameters.sampling_frequency;
    auto data = static_cast<int16_t *>(record->data);

    double frequency = p.frequency;
    if (p.randomize)
        frequency = m_uniform_distribution(m_random_generator) * sampling_frequency / 2.0;

    for (size_t i = 0; i < m_top_parameters.record_length; ++i)
    {
        double x = static_cast<double>(i) / static_cast<double>(sampling_frequency);
        double y = (p.amplitude * std::sin(2 * M_PI * frequency * x + p.phase) +
                    m_noise_distribution(m_random_generator) + p.offset);

        /* Add gain and offset mismatch for every other sample. */
        if (p.interleaving_distortion && i % 2)
        {
            y = 1.03 * y + 0.03 * p.amplitude;
        }

        /* Add HD2, HD3, HD4 and HD5. */
        if (p.harmonic_distortion)
        {
            for (int hd = 2; hd <= 5; ++hd)
                y += 0.1 / (1 << hd) * std::sin(2 * M_PI * hd * frequency * x + p.phase);
        }

        if (y > 1.0 || y < -1.0)
            record->header->record_status |= ADQ_RECORD_STATUS_OVERRANGE;

        if (y > 0)
            data[i] = static_cast<int16_t>(std::min(32768.0 * y, 32767.0));
        else
            data[i] = static_cast<int16_t>(std::max(32768.0 * y, -32768.0));
    }

    return record;
}
