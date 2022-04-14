#ifndef MOCK_ADQAPI_DEFINITIONS_H_RVFILE
#define MOCK_ADQAPI_DEFINITIONS_H_RVFILE

#include <cinttypes>
#include <memory>

#define ADQ_MAX_NOF_CHANNELS 2

#define ADQ_EOK (0)
#define ADQ_EINVAL (-1)
#define ADQ_EAGAIN (-2)
#define ADQ_EOVERFLOW (-3)
#define ADQ_ENOTREADY (-4)
#define ADQ_EINTERRUPTED (-5)
#define ADQ_EIO (-6)
#define ADQ_EEXTERNAL (-7)
#define ADQ_EUNSUPPORTED (-8)
#define ADQ_EINTERNAL (-9)

struct ADQGen4RecordHeader
{
    uint8_t version_major;
    uint8_t version_minor;
    uint16_t timestamp_synchronization_counter;
    uint16_t general_purpose_start;
    uint16_t general_purpose_stop;
    uint64_t timestamp;
    int64_t record_start;
    uint32_t record_length;
    uint8_t user_id;
    uint8_t reserved0;
    uint16_t record_status;
    uint32_t record_number;
    uint8_t channel;
    uint8_t data_format;
    char serial_number[10];
    uint64_t sampling_period;
    float time_unit;
    uint32_t reserved1;
};

struct ADQGen4Record
{
    ADQGen4Record(size_t count)
    {
        data = std::malloc(count); //::operator new(count);
        header = static_cast<ADQGen4RecordHeader*>(std::malloc(sizeof(struct ADQGen4RecordHeader)));
        size = count;
    }

    ~ADQGen4Record()
    {
        // ::operator delete(data);
        std::free(data);
        std::free(header);
    }

    ADQGen4Record(const ADQGen4Record &other) = delete;
    ADQGen4Record &operator=(const ADQGen4Record &other) = delete;

    struct ADQGen4RecordHeader *header;
    void *data;
    uint64_t size;
};

#endif
