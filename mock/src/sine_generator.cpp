#include "sine_generator.h"

#include <cmath>
#include <limits>

void SineGenerator::Generate()
{
    std::shared_ptr<ADQGen4Record> record;
    int result = ReuseOrAllocateBuffer(record, m_top_parameters.record_length * sizeof(int16_t));
    if (result != SCAPE_EOK)
    {
        if (result == SCAPE_EINTERRUPTED) /* Convert forced queue stop into an ok. */
            m_thread_exit_code = SCAPE_EOK;
        else
            m_thread_exit_code = result;
        return;
    }

    /* Fill in a few header fields. */
    *record->header = {};
    record->header->data_format = ADQ_DATA_FORMAT_INT16;
    record->header->record_length = static_cast<uint32_t>(m_top_parameters.record_length);
    record->header->record_number = m_record_number;
    record->header->record_start = static_cast<int64_t>(m_distribution(m_random_generator) * 1000);
    record->header->time_unit = 25e-12f; /* TODO: 25ps steps for now */
    record->header->timestamp = m_timestamp;
    record->header->sampling_period = static_cast<uint64_t>(
        1.0 / (record->header->time_unit * m_clock_system_parameters.sampling_frequency) + 0.5);

    bool overrange;
    Sine(static_cast<int16_t *>(record->data), m_top_parameters.record_length, overrange);

    if (overrange)
        record->header->record_status |= ADQ_RECORD_STATUS_OVERRANGE;
    if (!(m_record_number % 50))
        record->header->misc |= 0x1u;
    if (!(m_record_number % 30))
        record->header->misc |= 0x2u;

    record->header->timestamp_synchronization_counter = static_cast<uint16_t>(m_record_number / 100);

    /* Add to the outgoing queue. */
    EjectBuffer(record);

    /* Update bookkeeping variables. */
    m_record_number++;

    m_timestamp += static_cast<uint64_t>(
        std::round(1.0 / (m_top_parameters.trigger_frequency * record->header->time_unit)));
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
            fprintf(stderr, "Unrecognized parameter set.\n");
            return SCAPE_EINVAL;
        }
    }
    catch (const nlohmann::json::exception &e)
    {
        fprintf(stderr, "Failed to parse the parameter set %s.\n", e.what());
        return SCAPE_EINVAL;
    }
}

int SineGenerator::GetParameters(GeneratorMessageId id, nlohmann::json &json)
{
    switch (id)
    {
    case GeneratorMessageId::GET_TOP_PARAMETERS:
        json = SineGeneratorTopParameters{};
        break;

    case GeneratorMessageId::GET_CLOCK_SYSTEM_PARAMETERS:
        json = SineGeneratorClockSystemParameters{};
        break;

    default:
        fprintf(stderr, "Unexpected message id %d.\n", static_cast<int>(id));
        return SCAPE_EINVAL;
    }

    return SCAPE_EOK;
}

void SineGenerator::Sine(int16_t *const data, size_t count, bool &overrange)
{
    /* Generate a noisy sine wave of the input length. */
    /* TODO: Only 16-bit datatype for now. */
    overrange = false;
    const auto &p = m_top_parameters;
    const auto &sampling_frequency = m_clock_system_parameters.sampling_frequency;
    for (size_t i = 0; i < count; ++i)
    {
        double x = static_cast<double>(i) / static_cast<double>(sampling_frequency);
        double y = (p.amplitude * std::sin(2 * M_PI * p.frequency * x + p.phase) +
                    m_distribution(m_random_generator) + p.offset);

        /* Add gain and offset mismatch for every other sample. */
        if (p.interleaving_distortion && i % 2)
        {
            y = 1.03 * y + 0.03 * p.amplitude;
        }

        /* Add HD2, HD3, HD4 and HD5. */
        if (p.harmonic_distortion)
        {
            for (int hd = 2; hd <= 5; ++hd)
                y += 0.1 / (1 << hd) * std::sin(2 * M_PI * hd * p.frequency * x + p.phase);
        }

        if (y > 1.0 || y < -1.0)
            overrange = true;

        if (y > 0)
            data[i] = static_cast<int16_t>(std::min(32768.0 * y, 32767.0));
        else
            data[i] = static_cast<int16_t>(std::max(32768.0 * y, -32768.0));
    }
}
