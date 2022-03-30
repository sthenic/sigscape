#ifndef DATA_TYPES_H_WSKIC4
#define DATA_TYPES_H_WSKIC4

#include <cstdint>
#include <cstddef>
#include <complex>
#include <algorithm>
#include <limits>

enum RecordId
{
    TIME_DOMAIN = 0,
    FREQUENCY_DOMAIN = 1
};

struct TimeDomainRecordHeader
{
    uint64_t record_number;
    size_t record_length;
};

/* A time domain record. */
struct TimeDomainRecord
{
    TimeDomainRecord(size_t count)
    {
        x = new double[count];
        y = new double[count];
        id = TIME_DOMAIN;
        this->count = count;
        capacity = count;
    }

    ~TimeDomainRecord()
    {
        delete[] x;
        delete[] y;
    }

    TimeDomainRecord(const TimeDomainRecord &other)
    {
        x = new double[other.count];
        y = new double[other.count];
        id = TIME_DOMAIN;
        header = other.header;
        count = other.count;
        capacity = other.count;

        std::copy(other.x, other.x + other.count, x);
        std::copy(other.y, other.y + other.count, y);
    }

    TimeDomainRecord &operator=(const TimeDomainRecord &other)
    {
        if (this != &other)
        {
            if (capacity < other.count)
            {
                delete[] x;
                delete[] y;
                x = new double[other.count];
                y = new double[other.count];
                id = TIME_DOMAIN;
                capacity = other.count;
            }

            std::copy(other.x, other.x + other.count, x);
            std::copy(other.y, other.y + other.count, y);
            count = other.count;
            header = other.header;
        }

        return *this;
    }

    enum RecordId id;
    double *x;
    double *y;
    struct TimeDomainRecordHeader header;
    size_t count;
    size_t capacity;
};

struct FrequencyDomainRecordHeader
{
    uint64_t record_number;
    size_t record_length;
};

struct FrequencyDomainRecord
{
    FrequencyDomainRecord(size_t count)
    {
        x = new double[count];
        y = new double[count];
        yc = new std::complex<double>[count];
        id = FREQUENCY_DOMAIN;
        this->count = count;
        capacity = count;
    }

    ~FrequencyDomainRecord()
    {
        delete[] x;
        delete[] y;
        delete[] yc;
    }

    FrequencyDomainRecord(const FrequencyDomainRecord &other)
    {
        x = new double[other.count];
        y = new double[other.count];
        yc = new std::complex<double>[other.count];
        id = FREQUENCY_DOMAIN;
        header = other.header;
        count = other.count;
        capacity = other.count;

        std::copy(other.x, other.x + other.count, x);
        std::copy(other.y, other.y + other.count, y);
        std::copy(other.yc, other.yc + other.count, yc);
    }

    FrequencyDomainRecord &operator=(const FrequencyDomainRecord &other)
    {
        if (this != &other)
        {
            if (capacity < other.count)
            {
                delete[] x;
                delete[] y;
                delete[] yc;
                x = new double[other.count];
                y = new double[other.count];
                yc = new std::complex<double>[other.count];
                id = FREQUENCY_DOMAIN;
                capacity = other.count;
            }

            std::copy(other.x, other.x + other.count, x);
            std::copy(other.y, other.y + other.count, y);
            std::copy(other.yc, other.yc + other.count, yc);
            count = other.count;
            header = other.header;
        }

        return *this;
    }

    enum RecordId id;
    double *x;
    double *y;
    std::complex<double> *yc;
    struct FrequencyDomainRecordHeader header;
    size_t count;
    size_t capacity;
};

struct ProcessedRecord
{
    ProcessedRecord(size_t count)
    {
        time_domain = std::make_shared<TimeDomainRecord>(count);
        frequency_domain = std::make_shared<FrequencyDomainRecord>(count);
        time_domain_metrics.max_value = std::numeric_limits<double>::lowest();
        time_domain_metrics.min_value = std::numeric_limits<double>::max();
        frequency_domain_metrics.max_value = std::numeric_limits<double>::lowest();
        frequency_domain_metrics.min_value = std::numeric_limits<double>::max();
    }

    ProcessedRecord(const ProcessedRecord &other)
    {
        /* We make a deep copy of the other object and create new shared pointers. */
        if (other.time_domain != NULL)
            time_domain = std::make_shared<TimeDomainRecord>(*other.time_domain);

        if (other.frequency_domain != NULL)
            frequency_domain = std::make_shared<FrequencyDomainRecord>(*other.frequency_domain);

        time_domain_metrics = other.time_domain_metrics;
        frequency_domain_metrics = other.frequency_domain_metrics;
    }

    ProcessedRecord &operator=(const ProcessedRecord &other)
    {
        if (this != &other)
        {
            if (other.time_domain != NULL)
                time_domain = std::make_shared<TimeDomainRecord>(*other.time_domain);
            else
                time_domain = NULL;

            if (other.frequency_domain != NULL)
                frequency_domain = std::make_shared<FrequencyDomainRecord>(*other.frequency_domain);
            else
                frequency_domain = NULL;

            time_domain_metrics = other.time_domain_metrics;
            frequency_domain_metrics = other.frequency_domain_metrics;
        }

        return *this;
    }

    std::shared_ptr<TimeDomainRecord> time_domain;
    std::shared_ptr<FrequencyDomainRecord> frequency_domain;

    struct TimeDomainMetrics
    {
        double max_value;
        double min_value;
    } time_domain_metrics;

    struct FrequencyDomainMetrics
    {
        double max_value;
        double min_value;
    } frequency_domain_metrics;
};

#endif
