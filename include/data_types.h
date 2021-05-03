#ifndef DATA_TYPES_H_WSKIC4
#define DATA_TYPES_H_WSKIC4

#include <cstdint>
#include <cstddef>
#include <complex>
#include <algorithm>

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

    TimeDomainRecord(const TimeDomainRecord &other) = delete;

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

    FrequencyDomainRecord(const FrequencyDomainRecord &other) = delete;

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
    ProcessedRecord(size_t count, bool allocate_time_domain = false)
    {
        if (allocate_time_domain)
            time_domain = new TimeDomainRecord(count);
        else
            time_domain = NULL;

        frequency_domain = new FrequencyDomainRecord(count);
        owns_time_domain = allocate_time_domain;
    }

    ~ProcessedRecord()
    {
        if (owns_time_domain)
            delete time_domain;
        else
            time_domain = NULL;

        delete frequency_domain;
    }

    ProcessedRecord(const ProcessedRecord &other) = delete;

    ProcessedRecord &operator=(const ProcessedRecord &other)
    {
        if (this != &other)
        {
            if (owns_time_domain)
                *time_domain = *other.time_domain;
            else
                time_domain = other.time_domain;

            *frequency_domain = *other.frequency_domain;
        }

        return *this;
    }

    struct TimeDomainRecord *time_domain;
    struct FrequencyDomainRecord *frequency_domain;
    bool owns_time_domain;
};

#endif
