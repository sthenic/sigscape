#ifndef MOCK_ADQAPI_DEFINITIONS_H_RVFILE
#define MOCK_ADQAPI_DEFINITIONS_H_RVFILE

#include <cinttypes>
#include <memory>
#include <cstdlib>

#define ADQ_MAX_NOF_CHANNELS 8

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

enum ADQProductID_Enum
{
  PID_ADQ32 = 0x0031,
  PID_ADQ36 = 0x0033
};

struct ADQInfoListEntry
{
  enum ADQProductID_Enum ProductID;
};

#define ADQ_DATA_FORMAT_INT16 (0)
#define ADQ_DATA_FORMAT_INT32 (1)

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
    uint8_t misc;
    uint16_t record_status;
    uint32_t record_number;
    uint8_t channel;
    uint8_t data_format;
    char serial_number[10];
    uint64_t sampling_period;
    float time_unit;
    uint32_t firmware_specific;
};

struct ADQGen4Record
{
    ADQGen4Record(size_t count)
    {
        data = std::malloc(count);
        header = static_cast<ADQGen4RecordHeader*>(std::malloc(sizeof(struct ADQGen4RecordHeader)));
        size = count;
    }

    ~ADQGen4Record()
    {
        std::free(data);
        std::free(header);
    }

    ADQGen4Record(const ADQGen4Record &other) = delete;
    ADQGen4Record &operator=(const ADQGen4Record &other) = delete;

    struct ADQGen4RecordHeader *header;
    void *data;
    uint64_t size;
};

/* FIXME: Slim this down to only contain the ones we use? */
enum ADQParameterId
{
  ADQ_PARAMETER_ID_RESERVED = 0,
  ADQ_PARAMETER_ID_DATA_ACQUISITION = 1,
  ADQ_PARAMETER_ID_DATA_TRANSFER = 2,
  ADQ_PARAMETER_ID_DATA_READOUT = 3,
  ADQ_PARAMETER_ID_CONSTANT = 4,
  ADQ_PARAMETER_ID_DIGITAL_GAINANDOFFSET = 5,
  ADQ_PARAMETER_ID_EVENT_SOURCE_LEVEL = 6,
  ADQ_PARAMETER_ID_DBS = 7,
  ADQ_PARAMETER_ID_SAMPLE_SKIP = 8,
  ADQ_PARAMETER_ID_TEST_PATTERN = 9,
  ADQ_PARAMETER_ID_EVENT_SOURCE_PERIODIC = 10,
  ADQ_PARAMETER_ID_EVENT_SOURCE_TRIG = 11,
  ADQ_PARAMETER_ID_EVENT_SOURCE_SYNC = 12,
  ADQ_PARAMETER_ID_ANALOG_FRONTEND = 13,
  ADQ_PARAMETER_ID_PATTERN_GENERATOR0 = 14,
  ADQ_PARAMETER_ID_PATTERN_GENERATOR1 = 15,
  ADQ_PARAMETER_ID_EVENT_SOURCE = 16,
  ADQ_PARAMETER_ID_SIGNAL_PROCESSING = 17,
  ADQ_PARAMETER_ID_FUNCTION = 18,
  ADQ_PARAMETER_ID_TOP = 19,
  ADQ_PARAMETER_ID_PORT_TRIG = 20,
  ADQ_PARAMETER_ID_PORT_SYNC = 21,
  ADQ_PARAMETER_ID_PORT_SYNCO = 22,
  ADQ_PARAMETER_ID_PORT_SYNCI = 23,
  ADQ_PARAMETER_ID_PORT_CLK = 24,
  ADQ_PARAMETER_ID_PORT_CLKI = 25,
  ADQ_PARAMETER_ID_PORT_CLKO = 26,
  ADQ_PARAMETER_ID_PORT_GPIOA = 27,
  ADQ_PARAMETER_ID_PORT_GPIOB = 28,
  ADQ_PARAMETER_ID_PORT_PXIE = 29,
  ADQ_PARAMETER_ID_PORT_MTCA = 30,
  ADQ_PARAMETER_ID_PULSE_GENERATOR0 = 31,
  ADQ_PARAMETER_ID_PULSE_GENERATOR1 = 32,
  ADQ_PARAMETER_ID_PULSE_GENERATOR2 = 33,
  ADQ_PARAMETER_ID_PULSE_GENERATOR3 = 34,
  ADQ_PARAMETER_ID_TIMESTAMP_SYNCHRONIZATION = 35,
  ADQ_PARAMETER_ID_CLOCK_SYSTEM = 40,
#ifdef ADQAPI_INTERNAL
  ADQ_PARAMETER_ID_INTERNAL_DIGITAL_GAINANDOFFSET = 65536,
#endif
  ADQ_PARAMETER_ID_MAX_VAL = INT32_MAX
};

struct ADQConstantParametersChannel
{
    char label[8];
    int32_t nof_adc_cores;
    int64_t code_normalization;
};

enum ADQFirmwareType
{
    ADQ_FIRMWARE_TYPE_FWDAQ = 0,
    ADQ_FIRMWARE_TYPE_FWATD = 1
};

struct ADQConstantParametersFirmware
{
    enum ADQFirmwareType type;
    char name[32];
};

/* A reduced version of the set of constant parameters. */
struct ADQConstantParameters
{
  enum ADQParameterId id;
  int32_t nof_channels;
  char serial_number[16];
  char product_name[32];
  char product_options[32];
  struct ADQConstantParametersFirmware firmware;
  struct ADQConstantParametersChannel channel[ADQ_MAX_NOF_CHANNELS];
  uint64_t dram_size;
  uint64_t magic;
};

struct ADQAnalogFrontendParametersChannel
{
  double input_range;
  double dc_offset;
};

struct ADQAnalogFrontendParameters
{
  enum ADQParameterId id;
  int32_t reserved;
  struct ADQAnalogFrontendParametersChannel channel[ADQ_MAX_NOF_CHANNELS];
  uint64_t magic;
};

enum ADQStatusId
{
  ADQ_STATUS_ID_RESERVED = 0,
  ADQ_STATUS_ID_OVERFLOW = 1,
  ADQ_STATUS_ID_DRAM = 2,
  ADQ_STATUS_ID_ACQUISITION = 3,
  ADQ_STATUS_ID_TEMPERATURE = 4,
  ADQ_STATUS_ID_CLOCK_SYSTEM = 5,
  ADQ_STATUS_ID_MAX_VAL = INT32_MAX
};

struct ADQOverflowStatus
{
  int32_t overflow;
  int32_t reserved;
};

struct ADQDramStatus
{
  uint64_t fill;
  uint64_t fill_max;
};

#define ADQ_PARAMETERS_MAGIC (0xAA559977AA559977ull)
#define ADQ_RECORD_STATUS_OVERRANGE (1u << 2)

#endif
