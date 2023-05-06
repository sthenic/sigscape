#include "generator.h"

#include <cmath>
#include <limits>
#include <chrono>

Generator::Generator()
    : m_random_generator(
          static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()))
    , m_distribution(0, 0.1)
    , m_parameters()
    , m_sampling_frequency(500e6)
{
}

int Generator::SetParameters(const Parameters &parameters)
{
    m_parameters = parameters;
    m_distribution = std::normal_distribution<double>(0, m_parameters.sine.noise_std_dev);
    return SCAPE_EOK;
}

int Generator::SetSamplingFrequency(double sampling_frequency)
{
    m_sampling_frequency = sampling_frequency;
    return SCAPE_EOK;
}

void Generator::MainLoop()
{
    m_thread_exit_code = SCAPE_EOK;
    int wait_us = static_cast<int>(1000000.0 / m_parameters.trigger_frequency);
    int record_number = 0;

    for (;;)
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
        record->header->record_number = record_number;
        record->header->record_start = static_cast<int64_t>(m_distribution(m_random_generator) * 1000);
        record->header->time_unit = 25e-12f; /* TODO: 25ps steps for now */
        record->header->sampling_period = static_cast<uint64_t>(
            1.0 / (record->header->time_unit * m_sampling_frequency) + 0.5);

        bool overrange;
        NoisySine(*record, m_parameters.record_length, overrange);
        if (overrange)
            record->header->record_status |= ADQ_RECORD_STATUS_OVERRANGE;

        /* Add to the outgoing queue. */
        m_read_queue.Write(record);

        /* Update bookkeeping variables. */
        record_number++;

        /* We implement the sleep using the stop event to be able to immediately
           react to the event being set. */
        if (m_should_stop.wait_for(std::chrono::microseconds(wait_us)) == std::future_status::ready)
            break;
    }
}

void Generator::NoisySine(ADQGen4Record &record, size_t count, bool &overrange)
{
    /* Generate a noisy sine wave of the input length. */
    /* TODO: Only 16-bit datatype for now. */
    overrange = false;
    const Parameters::SineWave &sine = m_parameters.sine;
    int16_t *data = static_cast<int16_t *>(record.data);
    for (size_t i = 0; i < count; ++i)
    {
        double x = static_cast<double>(i) / static_cast<double>(m_sampling_frequency);
        double y = (sine.amplitude * std::sin(2 * M_PI * sine.frequency * x + sine.phase) +
                    m_distribution(m_random_generator) + sine.offset);

        /* Add gain and offset mismatch for every other sample. */
        if (sine.interleaving_distortion && i % 2)
        {
            y = 1.03 * y + 0.03 * sine.amplitude;
        }

        /* Add HD2, HD3, HD4 and HD5. */
        if (sine.harmonic_distortion)
        {
            for (int hd = 2; hd <= 5; ++hd)
                y += 0.1 / (1 << hd) * std::sin(2 * M_PI * hd * sine.frequency * x + sine.phase);
        }

        if (y > 1.0 || y < -1.0)
            overrange = true;

        if (y > 0)
            data[i] = static_cast<int16_t>(std::min(32768.0 * y, 32767.0));
        else
            data[i] = static_cast<int16_t>(std::max(32768.0 * y, -32768.0));
    }
}