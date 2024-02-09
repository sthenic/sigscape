#include "pulse_generator.h"

void PulseGenerator::Generate()
{
    auto pulse = Pulse();
    if (pulse == NULL)
        return;

    auto attributes = Attributes(pulse.get());
    if (attributes == NULL)
        return;

    /* Add to the outgoing queue. */
    EjectBuffer(pulse);
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

std::shared_ptr<ADQGen4Record> PulseGenerator::Attributes(const ADQGen4Record */* source */)
{
    /* FIXME: Implement */
    return NULL;
}
