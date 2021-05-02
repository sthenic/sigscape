#ifndef DATA_TYPES_H_WSKIC4
#define DATA_TYPES_H_WSKIC4

#include <cstdint>
#include <cstddef>
#include <complex>

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
    }

    ~TimeDomainRecord()
    {
        delete[] x;
        delete[] y;
    }

    enum RecordId id;
    double *x;
    double *y;
    struct TimeDomainRecordHeader header;
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
    }

    ~FrequencyDomainRecord()
    {
        delete[] x;
        delete[] y;
        delete[] yc;
    }

    enum RecordId id;
    double *x;
    double *y;
    std::complex<double> *yc;
    struct FrequencyDomainRecordHeader header;
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

    struct TimeDomainRecord *time_domain;
    struct FrequencyDomainRecord *frequency_domain;
    bool owns_time_domain;
};

#endif
