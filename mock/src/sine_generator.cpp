#include "sine_generator.h"

#include <cmath>
#include <limits>
#include <chrono>

SineGenerator::SineGenerator()
    : m_random_generator{
          static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())}
    , m_distribution{0, 0.1}
    , m_record_number{0}
    , m_parameters{}
{
}

void SineGenerator::MainLoop()
{
    m_thread_exit_code = SCAPE_EOK;
    m_timestamp = 0;
    m_record_number = 0;

    for (;;)
    {
        const auto start = std::chrono::high_resolution_clock::now();
        ProcessMessages();
        Generate();

        const auto delta = (std::chrono::high_resolution_clock::now() - start).count();
        const int wait_us = static_cast<int>(1000000.0 / m_parameters.trigger_frequency);
        const int remainder_us = std::max(wait_us - static_cast<int>(delta / 1e3), 0);

        /* We implement the sleep using the stop event to be able to immediately
           react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::microseconds(remainder_us)) == std::future_status::ready)
            break;
    }
}

void SineGenerator::ProcessMessages()
{
    SineGeneratorMessage message{};
    while (SCAPE_EOK == m_write_message_queue.Read(message, 0))
    {
        switch (message.id)
        {
        case SineGeneratorMessageId::SET_PARAMETERS:
        {
            auto sampling_frequency = m_parameters.sampling_frequency;
            m_parameters = message.parameters;
            m_parameters.sampling_frequency = sampling_frequency;
            m_distribution = std::normal_distribution<double>(0, m_parameters.noise);
            break;
        }

        case SineGeneratorMessageId::SET_SAMPLING_FREQUENCY:
            m_parameters.sampling_frequency = message.sampling_frequency;
            break;

        default:
            fprintf(stderr, "SineGenerator: Unknown message id %d.\n", static_cast<int>(message.id));
            break;
        }
    }
}

void SineGenerator::Generate()
{
    ADQGen4Record *record = NULL;
    int result = ReuseOrAllocateBuffer(record, m_parameters.record_length * sizeof(int16_t));
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
    record->header->record_length = static_cast<uint32_t>(m_parameters.record_length);
    record->header->record_number = m_record_number;
    record->header->record_start = static_cast<int64_t>(m_distribution(m_random_generator) * 1000);
    record->header->time_unit = 25e-12f; /* TODO: 25ps steps for now */
    record->header->timestamp = m_timestamp;
    record->header->sampling_period = static_cast<uint64_t>(
        1.0 / (record->header->time_unit * m_parameters.sampling_frequency) + 0.5);

    bool overrange;
    Sine(static_cast<int16_t *>(record->data), m_parameters.record_length, overrange);

    if (overrange)
        record->header->record_status |= ADQ_RECORD_STATUS_OVERRANGE;
    if (!(m_record_number % 50))
        record->header->misc |= 0x1u;
    if (!(m_record_number % 30))
        record->header->misc |= 0x2u;

    record->header->timestamp_synchronization_counter = static_cast<uint16_t>(m_record_number / 100);

    /* Add to the outgoing queue. */
    m_read_queue.Write(record);

    /* Update bookkeeping variables. */
    m_record_number++;

    m_timestamp += static_cast<uint64_t>(
        std::round(1.0 / (m_parameters.trigger_frequency * record->header->time_unit)));
}

void SineGenerator::Sine(int16_t *const data, size_t count, bool &overrange)
{
    /* Generate a noisy sine wave of the input length. */
    /* TODO: Only 16-bit datatype for now. */
    overrange = false;
    const SineGeneratorParameters &p = m_parameters;
    for (size_t i = 0; i < count; ++i)
    {
        double x = static_cast<double>(i) / static_cast<double>(p.sampling_frequency);
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
