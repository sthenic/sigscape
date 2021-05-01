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
    enum RecordId id;
    double *x;
    double *y;
    std::complex<double> *yc;
    struct FrequencyDomainRecordHeader header;
    size_t capacity;
};

#endif
