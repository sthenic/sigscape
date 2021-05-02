#include "processing.h"

#include "fft.h"
#include "fft_settings.h"

Processing::Processing(DataAcquisition &acquisition)
    : m_is_running(false)
    , m_thread_exit_code(0)
    , m_acquisition(acquisition)
{}

Processing::~Processing()
{
    FreeBuffers();
}

int Processing::Initialize()
{
    return 0;
}

int Processing::Start()
{
    if (m_is_running)
        return -1;

    m_write_queue.Start();
    m_read_queue.Start();
    m_signal_stop = std::promise<void>();
    m_should_stop = m_signal_stop.get_future();
    m_thread = std::thread(&Processing::MainLoop, this);
    m_is_running = true;
    return 0;
}

int Processing::Stop()
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

int Processing::WaitForBuffer(struct ProcessedRecord *&buffer, int timeout)
{
    return m_read_queue.Read(buffer, timeout);
}

int Processing::ReturnBuffer(struct ProcessedRecord *buffer)
{
    /* We split the processed record and forward the time domain part to the
       acquisition process. */
    /* FIXME: Error handling logic. */
    int result = m_acquisition.ReturnBuffer(buffer->time_domain);
    if (result != 0)
    {
        printf("Failed to return the time domain record buffer.\n");
    }

    buffer->time_domain = NULL;
    return m_write_queue.Write(buffer);
}

int Processing::AllocateRecord(struct ProcessedRecord *&record, size_t nof_samples)
{
    record = static_cast<struct ProcessedRecord *>(std::malloc(sizeof(struct ProcessedRecord)));
    if (record == NULL)
    {
        printf("Failed to allocate a new record.\n");
        return -3;
    }

    record->time_domain = NULL;
    record->frequency_domain = static_cast<struct FrequencyDomainRecord *>(std::malloc(sizeof(struct FrequencyDomainRecord)));
    if (record->frequency_domain == NULL)
    {
        printf("Failed to allocate frequency domain memory.\n");
        std::free(record);
        return -3;
    }

    record->frequency_domain->x = static_cast<double *>(std::malloc(nof_samples * sizeof(double)));
    if (record->frequency_domain->x == NULL)
    {
        printf("Failed to allocate x axis memory.\n");
        std::free(record);
        return -3;
    }

    record->frequency_domain->y = static_cast<double *>(std::malloc(nof_samples * sizeof(double)));
    if (record->frequency_domain->y == NULL)
    {
        printf("Failed to allocate y axis memory.\n");
        std::free(record->frequency_domain->x);
        std::free(record);
        return -3;
    }

    record->frequency_domain->yc = static_cast<std::complex<double> *>(std::malloc(nof_samples * sizeof(std::complex<double>)));
    if (record->frequency_domain->yc == NULL)
    {
        printf("Failed to allocate y axis memory.\n");
        std::free(record->frequency_domain->x);
        std::free(record->frequency_domain->y);
        std::free(record);
        return -3;
    }

    record->frequency_domain->id = FREQUENCY_DOMAIN;
    record->frequency_domain->capacity = nof_samples * sizeof(double);

    /* Add a reference to the data storage. */
    m_mutex.lock();
    m_records.push_back(record);
    m_mutex.unlock();

    return 0;
}

int Processing::ReuseOrAllocateRecord(struct ProcessedRecord *&record, size_t nof_samples)
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

void Processing::MainLoop()
{
    m_thread_exit_code = 0;
    for (;;)
    {
        /* Check if the stop event has been set. */
        if (m_should_stop.wait_for(std::chrono::microseconds(0)) == std::future_status::ready)
            break;

        /* FIXME: This is a hardcoded type for now. */
        struct TimeDomainRecord *time_domain = NULL;
        int result = m_acquisition.WaitForBuffer((void *&)time_domain, 100, NULL);

        /* Continue on timeout. */
        if (result == -1)
            continue;

        /* Compute FFT */
        struct ProcessedRecord *processed_record = NULL;
        result = ReuseOrAllocateRecord(processed_record, FFT_SIZE);
        if (result != 0)
        {
            if (result == -2) /* Convert forced queue stop into an ok. */
                m_thread_exit_code = 0;
            else
                m_thread_exit_code = result;
            return;
        }
        processed_record->time_domain = time_domain;

        const char *error = NULL;
        if (!simple_fft::FFT(time_domain->y, processed_record->frequency_domain->yc, FFT_SIZE, error))
        {
            printf("Failed to compute FFT: %s.\n", error);
            m_thread_exit_code = -3;
            return;
        }

        /* Compute real spectrum. */
        for (int i = 0; i < static_cast<int>(FFT_SIZE); ++i)
        {
            processed_record->frequency_domain->x[i] = static_cast<double>(i) / static_cast<double>(FFT_SIZE);
            processed_record->frequency_domain->y[i] = 20 * std::log10(std::abs(processed_record->frequency_domain->yc[i]) / FFT_SIZE);
        }

        m_read_queue.Write(processed_record);
    }
}

void Processing::FreeBuffers()
{
    m_mutex.lock();
    for (auto it = m_records.begin(); it != m_records.end(); ++it)
    {
        if (*it != NULL)
        {
            if ((*it)->frequency_domain->x != NULL)
                std::free((*it)->frequency_domain->x);
            if ((*it)->frequency_domain->y != NULL)
                std::free((*it)->frequency_domain->y);
            if ((*it)->frequency_domain->yc != NULL)
                std::free((*it)->frequency_domain->yc);
            std::free((*it)->frequency_domain);
            std::free(*it);
        }
    }

    m_records.clear();
    m_mutex.unlock();
}
