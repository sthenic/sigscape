#include "pulse_generator.h"

void PulseGenerator::Generate()
{
    auto pulse = Pulse();
    if (pulse == NULL)
        return;

    auto attributes = Attributes(pulse.get());
    if (attributes == NULL)
        return;

    /* Add to the outgoing queues. */
    EjectBuffer(pulse, 0);
    EjectBuffer(attributes, 1);
}

double PulseGenerator::GetTriggerFrequency()
{
    return m_top_parameters.trigger_frequency;
}

double PulseGenerator::GetSamplingFrequency()
{
    return m_clock_system_parameters.sampling_frequency;
}

double PulseGenerator::GetNoise()
{
    return m_top_parameters.noise;
}

int PulseGenerator::GetParameters(GeneratorMessageId id, nlohmann::json &json)
{
    switch (id)
    {
    case GeneratorMessageId::GET_TOP_PARAMETERS:
        json = PulseGeneratorTopParameters{};
        return SCAPE_EOK;

    case GeneratorMessageId::GET_CLOCK_SYSTEM_PARAMETERS:
        json = PulseGeneratorClockSystemParameters{};
        return SCAPE_EOK;

    default:
        fprintf(stderr, "Unexpected message id %d.\n", static_cast<int>(id));
        return SCAPE_EINVAL;
    }
}

int PulseGenerator::SetParameters(GeneratorMessageId id, const nlohmann::json &json)
{
    try
    {
        switch (id)
        {
        case GeneratorMessageId::SET_TOP_PARAMETERS:
            m_top_parameters = json.get<PulseGeneratorTopParameters>();
            return SCAPE_EOK;

        case GeneratorMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
            m_clock_system_parameters = json.get<PulseGeneratorClockSystemParameters>();
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

void PulseGenerator::SeedHeader(ADQGen4RecordHeader *header)
{
    Generator::SeedHeader(header);
    header->data_format = ADQ_DATA_FORMAT_INT16;
    header->record_length = m_top_parameters.record_length;
}

std::shared_ptr<ADQGen4Record> PulseGenerator::Pulse()
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

    /* Generate one period of a pulse that we'll repeatedly insert into the buffer. */
    std::vector<double> pulse(m_top_parameters.period);
    if (m_top_parameters.gauss)
    {
        const auto sigma = static_cast<double>(m_top_parameters.width) / 2.0;
        for (size_t i = 0; i < pulse.size(); ++i)
        {
            pulse[i] = m_top_parameters.amplitude *
                           std::exp(
                               -0.5 * std::pow(static_cast<double>(i) - 3.0 * sigma, 2.0) /
                               std::pow(sigma, 2.0)) +
                       m_top_parameters.baseline;
        }
    }
    else
    {
        for (size_t i = 0; i < pulse.size(); ++i)
        {
            if (i < m_top_parameters.width)
                pulse[i] = m_top_parameters.baseline + m_top_parameters.amplitude;
            else
                pulse[i] = m_top_parameters.baseline;
        }
    }

    auto data = static_cast<int16_t *>(record->data);
    for (size_t i = 0; i < m_top_parameters.record_length; ++i)
    {
        double y;
        if (i < m_top_parameters.offset)
            y = m_top_parameters.baseline;
        else
            y = pulse[(i - m_top_parameters.offset) % pulse.size()];

        y += m_distribution(m_random_generator);

        if (y > 1.0 || y < -1.0)
            record->header->record_status |= ADQ_RECORD_STATUS_OVERRANGE;

        if (y > 0)
            data[i] = static_cast<int16_t>(std::min(32768.0 * y, 32767.0));
        else
            data[i] = static_cast<int16_t>(std::max(32768.0 * y, -32768.0));
    }

    return record;
}

std::shared_ptr<ADQGen4Record> PulseGenerator::Attributes(const ADQGen4Record *source)
{
    std::vector<ADQPulseAttributes> attributes = {
        {65433, 100, 3544, 64, ADQ_PULSE_ATTRIBUTES_STATUS_VALID, {}},
        {71722, 200, 7283, 63, ADQ_PULSE_ATTRIBUTES_STATUS_VALID, {}},
        {59091, 300, 2211, 82, ADQ_PULSE_ATTRIBUTES_STATUS_VALID, {}},
    };

    std::shared_ptr<ADQGen4Record> record;
    int result = ReuseOrAllocateBuffer(record, attributes.size() * sizeof(ADQPulseAttributes));
    if (result != SCAPE_EOK)
    {
        if (result == SCAPE_EINTERRUPTED) /* Convert forced queue stop into an ok. */
            m_thread_exit_code = SCAPE_EOK;
        else
            m_thread_exit_code = result;

        return NULL;
    }

    *record->header = *source->header;
    record->header->data_format = ADQ_DATA_FORMAT_PULSE_ATTRIBUTES;
    record->header->record_length = 3;
    std::memcpy(record->data, attributes.data(), attributes.size() * sizeof(ADQPulseAttributes));

    return record;
}
