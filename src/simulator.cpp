#include "simulator.h"

Simulator::Simulator()
    : m_record_length(0)
    , m_trigger_rate_hz(0)
    , m_random_generator()
    , m_distribution(0, 0.1)
    , m_sine()
{
}

Simulator::~Simulator()
{
}

int Simulator::Initialize(size_t record_length, double trigger_rate_hz, const struct SineWave &sine)
{
    m_record_length = record_length;
    m_trigger_rate_hz = trigger_rate_hz;
    m_sine = sine;
    m_distribution = std::normal_distribution<double>(0, m_sine.noise_std_dev);
    return ADQR_EOK;
}

int Simulator::WaitForBuffer(struct TimeDomainRecord *&buffer, int timeout)
{
    return m_read_queue.Read(buffer, timeout);
}

int Simulator::ReturnBuffer(struct TimeDomainRecord *buffer)
{
    return m_write_queue.Write(buffer);
}

void Simulator::MainLoop()
{
    m_thread_exit_code = ADQR_EOK;
    int wait_us = static_cast<int>(1000000.0 / m_trigger_rate_hz);
    int record_number = 0;

    for (;;)
    {
        struct TimeDomainRecord *record = NULL;
        int result = ReuseOrAllocateBuffer(record, m_record_length);
        if (result != ADQR_EOK)
        {
            if (result == ADQR_EINTERRUPTED) /* Convert forced queue stop into an ok. */
                m_thread_exit_code = ADQR_EOK;
            else
                m_thread_exit_code = result;
            return;
        }
        record->header.record_length = m_record_length;
        record->header.record_number = record_number;
        NoisySine(*record, m_record_length);

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

void Simulator::NoisySine(struct TimeDomainRecord &record, size_t count)
{
    /* Generate a noisy sine wave of the input length. */
    for (size_t i = 0; i < count; ++i)
    {
        record.x[i] = static_cast<double>(i) / static_cast<double>(m_sine.sampling_frequency);
        record.y[i] = m_sine.amplitude
                      * std::sin(2 * M_PI * m_sine.frequency * record.x[i] + m_sine.phase)
                      + m_distribution(m_random_generator) + m_sine.offset;
    }

    /* Add HD2, HD3, HD4 and HD5. */
    if (m_sine.harmonic_distortion)
    {
        for (size_t i = 0; i < count; ++i)
        {
            for (int hd = 2; hd <= 5; ++hd)
            {
                record.y[i] += 0.1 / (1 << hd)
                               * std::sin(2 * M_PI * hd * m_sine.frequency * record.x[i]);
            }
        }
    }
}
