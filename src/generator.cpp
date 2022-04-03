#include "generator.h"

#include <cmath>

Generator::Generator()
    : m_random_generator()
    , m_distribution(0, 0.1)
    , m_parameters()
{
}

int Generator::Initialize(const Parameters &parameters)
{
    m_parameters = parameters;
    m_distribution = std::normal_distribution<double>(0, m_parameters.sine.noise_std_dev);
    return ADQR_EOK;
}

int Generator::WaitForBuffer(std::shared_ptr<TimeDomainRecord> &buffer, int timeout)
{
    return m_read_queue.Read(buffer, timeout);
}

int Generator::ReturnBuffer(std::shared_ptr<TimeDomainRecord> buffer)
{
    return m_write_queue.Write(buffer);
}

void Generator::MainLoop()
{
    m_thread_exit_code = ADQR_EOK;
    int wait_us = static_cast<int>(1000000.0 / m_parameters.trigger_frequency);
    int record_number = 0;

    for (;;)
    {
        std::shared_ptr<TimeDomainRecord> record = NULL;
        int result = ReuseOrAllocateBuffer(record, m_parameters.record_length);
        if (result != ADQR_EOK)
        {
            if (result == ADQR_EINTERRUPTED) /* Convert forced queue stop into an ok. */
                m_thread_exit_code = ADQR_EOK;
            else
                m_thread_exit_code = result;
            return;
        }
        record->header.record_length = m_parameters.record_length;
        record->header.record_number = record_number;
        NoisySine(*record, m_parameters.record_length);

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

void Generator::NoisySine(TimeDomainRecord &record, size_t count)
{
    /* Generate a noisy sine wave of the input length. */
    const Parameters::SineWave &sine = m_parameters.sine;
    for (size_t i = 0; i < count; ++i)
    {
        record.x[i] = static_cast<double>(i) / static_cast<double>(sine.sampling_frequency);
        record.y[i] = sine.amplitude
                      * std::sin(2 * M_PI * sine.frequency * record.x[i] + sine.phase)
                      + m_distribution(m_random_generator) + sine.offset;
    }

    /* Add HD2, HD3, HD4 and HD5. */
    if (sine.harmonic_distortion)
    {
        for (size_t i = 0; i < count; ++i)
        {
            for (int hd = 2; hd <= 5; ++hd)
            {
                record.y[i] += 0.1 / (1 << hd)
                               * std::sin(2 * M_PI * hd * sine.frequency * record.x[i]);
            }
        }
    }
}
