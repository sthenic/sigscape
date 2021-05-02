#include "simulated_data_acquisition.h"

#include <chrono>
#include <cmath>
#include <cstdlib>

SimulatedDataAcquisition::SimulatedDataAcquisition()
    : m_is_running(false)
    , m_thread_exit_code(0)
    , m_mutex()
    , m_read_queue()
    , m_write_queue()
    , m_records()
    , m_record_length(0)
    , m_trigger_rate_hz(0)
    , m_random_generator()
    , m_distribution(0, 0.1)
{
}

SimulatedDataAcquisition::~SimulatedDataAcquisition()
{
    FreeBuffers();
}

int SimulatedDataAcquisition::Initialize(size_t record_length, double trigger_rate_hz)
{
    m_record_length = record_length;
    m_trigger_rate_hz = trigger_rate_hz;
    m_read_queue.Initialize();
    m_write_queue.Initialize();
    return 0;
}

int SimulatedDataAcquisition::Start()
{
    if (m_is_running)
        return -1;

    m_write_queue.Start();
    m_read_queue.Start();
    m_signal_stop = std::promise<void>();
    m_should_stop = m_signal_stop.get_future();
    m_thread = std::thread(&SimulatedDataAcquisition::MainLoop, this);
    m_is_running = true;
    return 0;
}

int SimulatedDataAcquisition::Stop()
{
    if (!m_is_running)
        return -1;

    m_write_queue.Stop();
    m_read_queue.Stop();
    m_signal_stop.set_value();
    m_thread.join();
    FreeBuffers();
    m_is_running = false;
    return m_thread_exit_code;
}

int SimulatedDataAcquisition::WaitForBuffer(void *&buffer, int timeout, void *status)
{
    (void)status;
    struct TimeDomainRecord *record = NULL;
    int result = m_read_queue.Read(record, timeout);
    if (result != 0)
        return result;

    /* For this simulator, the record buffer capacity is the same as the number
       of bytes written. */
    buffer = record;
    return record->capacity;
}

int SimulatedDataAcquisition::ReturnBuffer(void *buffer)
{
    return m_write_queue.Write(reinterpret_cast<struct TimeDomainRecord *>(buffer));
}

void SimulatedDataAcquisition::NoisySine(struct TimeDomainRecord &record, size_t length)
{
    /* Generate a noisy sine wave of the input length. */
    for (size_t i = 0; i < length; ++i)
    {
        record.x[i] = static_cast<double>(i) / static_cast<double>(length);
        record.y[i] = sinf(50 * record.x[i]) + m_distribution(m_random_generator);
    }
}

int SimulatedDataAcquisition::AllocateRecord(struct TimeDomainRecord *&record, size_t nof_samples)
{
    record = static_cast<struct TimeDomainRecord *>(std::malloc(sizeof(struct TimeDomainRecord)));
    if (record == NULL)
    {
        printf("Failed to allocate a new record.\n");
        return -3;
    }

    record->x = static_cast<double *>(std::malloc(nof_samples * sizeof(double)));
    if (record->x == NULL)
    {
        printf("Failed to allocate x axis memory.\n");
        std::free(record);
        return -3;
    }

    record->y = static_cast<double *>(std::malloc(nof_samples * sizeof(double)));
    if (record->y == NULL)
    {
        printf("Failed to allocate y axis memory.\n");
        std::free(record->x);
        std::free(record);
        return -3;
    }

    record->id = TIME_DOMAIN;
    record->capacity = nof_samples * sizeof(double);

    /* Add a reference to the data storage. */
    m_mutex.lock();
    m_records.push_back(record);
    m_mutex.unlock();

    return 0;
}

int SimulatedDataAcquisition::ReuseOrAllocateRecord(struct TimeDomainRecord *&record, size_t nof_samples)
{
    /* We prioritize reusing existing memory over allocating new. */
    if (m_write_queue.Read(record, 0) == 0)
    {
        return 0;
    }
    else
    {
        m_mutex.lock();
        size_t nof_records = m_records.size();
        m_mutex.unlock();
        if (nof_records < NOF_RECORDS_MAX)
            return AllocateRecord(record, nof_samples);
        else
            return m_write_queue.Read(record, -1);
    }
    return 0;
}

int SimulatedDataAcquisition::AllocateNoisySine(struct TimeDomainRecord *&record, size_t nof_samples)
{
    int result = ReuseOrAllocateRecord(record, nof_samples);
    if (result != 0)
        return result;
    NoisySine(*record, nof_samples);
    return 0;
}

void SimulatedDataAcquisition::MainLoop()
{
    m_thread_exit_code = 0;
    int wait_us = static_cast<int>(1000000.0 / m_trigger_rate_hz);
    int record_number = 0;

    for (;;)
    {
        struct TimeDomainRecord *record = NULL;
        int result = AllocateNoisySine(record, m_record_length);
        if (result != 0)
        {
            if (result == -2) /* Convert forced queue stop into an ok. */
                m_thread_exit_code = 0;
            else
                m_thread_exit_code = result;
            return;
        }
        record->header.record_length = m_record_length;
        record->header.record_number = record_number;

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

void SimulatedDataAcquisition::FreeBuffers()
{
    m_mutex.lock();
    for (auto it = m_records.begin(); it != m_records.end(); ++it)
    {
        if (*it != NULL)
        {
            if ((*it)->x != NULL)
                std::free((*it)->x);
            if ((*it)->y != NULL)
                std::free((*it)->y);
            std::free(*it);
        }
    }

    m_records.clear();
    m_mutex.unlock();
}
