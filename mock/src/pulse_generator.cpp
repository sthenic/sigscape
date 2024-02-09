#include "pulse_generator.h"

void PulseGenerator::Generate()
{
    auto pulse = Pulse();
    if (pulse == NULL)
        return;

    // auto attributes = Attributes(pulse);
    // if (attributes == NULL)
    //     return;

    /* Add to the outgoing queue. */
    EjectBuffer(pulse);

    /* Update bookkeeping variables. */
    /* FIXME: Move these to generator.h? */
    m_record_number++;

    m_timestamp += static_cast<uint64_t>(
        std::round(1.0 / (m_top_parameters.trigger_frequency * pulse->header->time_unit)));
}

int PulseGenerator::SetParameters(GeneratorMessageId id, const nlohmann::json &json)
{
    /* FIXME: Obviously move to generator.h */
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

int PulseGenerator::GetParameters(GeneratorMessageId id, nlohmann::json &json)
{
    /* FIXME: Obviously move to generator.h */
    switch (id)
    {
    case GeneratorMessageId::GET_TOP_PARAMETERS:
        json = PulseGeneratorTopParameters{};
        break;

    case GeneratorMessageId::GET_CLOCK_SYSTEM_PARAMETERS:
        json = PulseGeneratorClockSystemParameters{};
        break;

    default:
        fprintf(stderr, "Unexpected message id %d.\n", static_cast<int>(id));
        return SCAPE_EINVAL;
    }

    return SCAPE_EOK;
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

    if (!(m_record_number % 50))
        record->header->misc |= 0x1u;
    if (!(m_record_number % 30))
        record->header->misc |= 0x2u;

    record->header->timestamp_synchronization_counter = static_cast<uint16_t>(m_record_number / 100);
    return record;
}

std::shared_ptr<ADQGen4Record> PulseGenerator::Attributes()
{
    /* FIXME: Implement */
    return NULL;
}
