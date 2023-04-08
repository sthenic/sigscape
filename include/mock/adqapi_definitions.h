#pragma once

#include <cinttypes>
#include <memory>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>

#define ADQAPI_VERSION_MAJOR 1
#define ADQAPI_VERSION_MINOR 0

#define ADQ_MAX_NOF_CHANNELS 8
#define ADQ_MAX_NOF_INPUT_RANGES 8

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

#define ADQ_PARAMETERS_MAGIC (0xAA559977AA559977ull)
#define ADQ_RECORD_STATUS_OVERRANGE (1u << 2)

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

enum ADQParameterId
{
    ADQ_PARAMETER_ID_RESERVED = 0,
    ADQ_PARAMETER_ID_DATA_ACQUISITION = 1,
    ADQ_PARAMETER_ID_DATA_TRANSFER = 2,
    ADQ_PARAMETER_ID_DATA_READOUT = 3,
    ADQ_PARAMETER_ID_CONSTANT = 4,
    ADQ_PARAMETER_ID_ANALOG_FRONTEND = 13,
    ADQ_PARAMETER_ID_TOP = 19,
    ADQ_PARAMETER_ID_CLOCK_SYSTEM = 40,
    ADQ_PARAMETER_ID_MAX_VAL = INT32_MAX
};

enum ADQReferenceClockSource
{
    ADQ_REFERENCE_CLOCK_SOURCE_INVALID = 0,
    ADQ_REFERENCE_CLOCK_SOURCE_INTERNAL = 1,
    ADQ_REFERENCE_CLOCK_SOURCE_PORT_CLK = 2,
    ADQ_REFERENCE_CLOCK_SOURCE_PXIE_10M = 3,
    ADQ_REFERENCE_CLOCK_SOURCE_MTCA_TCLKA = 4,
    ADQ_REFERENCE_CLOCK_SOURCE_MTCA_TCLKB = 5,
    ADQ_REFERENCE_CLOCK_SOURCE_PXIE_100M = 6,
    ADQ_REFERENCE_CLOCK_SOURCE_MAX_VAL = INT32_MAX
};

enum ADQClockGenerator
{
    ADQ_CLOCK_GENERATOR_INVALID = 0,
    ADQ_CLOCK_GENERATOR_INTERNAL_PLL = 1,
    ADQ_CLOCK_GENERATOR_EXTERNAL_CLOCK = 2,
    ADQ_CLOCK_GENERATOR_MAX_VAL = INT32_MAX
};

struct ADQClockSystemParameters
{
    enum ADQParameterId id;
    int32_t reserved;
    enum ADQClockGenerator clock_generator;
    enum ADQReferenceClockSource reference_source;
    double sampling_frequency;
    double reference_frequency;
    double delay_adjustment;
    int32_t low_jitter_mode_enabled;
    int32_t delay_adjustment_enabled;
    uint64_t magic;
};

struct ADQConstantParametersChannel
{
    /* Convenience constructor for simulated digitizers. */
    ADQConstantParametersChannel() = default;
    ADQConstantParametersChannel(const std::string &label,
                                 int nof_adc_cores,
                                 const std::vector<double> &input_range,
                                 int code_normalization)
        : label{}
        , nof_adc_cores(static_cast<int32_t>(nof_adc_cores))
        , code_normalization(static_cast<int64_t>(code_normalization))
        , base_sampling_rate(2.5e9)
        , input_range{}
    {
        std::memcpy(this->label, label.c_str(),
                    std::min(sizeof(this->label) - 1, label.size() + 1));

        for (size_t i = 0; i < input_range.size() && i < ADQ_MAX_NOF_INPUT_RANGES; ++i)
            this->input_range[i] = input_range[i];
    };

    char label[8];
    int32_t nof_adc_cores;
    int64_t code_normalization;
    double base_sampling_rate;
    double input_range[ADQ_MAX_NOF_INPUT_RANGES];
};

enum ADQFirmwareType
{
    ADQ_FIRMWARE_TYPE_FWDAQ = 0,
    ADQ_FIRMWARE_TYPE_FWATD = 1
};

struct ADQConstantParametersFirmware
{
    /* Convenience constructor for simulated digitizers. */
    ADQConstantParametersFirmware() = default;
    ADQConstantParametersFirmware(enum ADQFirmwareType type, const std::string &name,
                                  const std::string &revision, const std::string &customization,
                                  const std::string &part_number)
        : type(type)
        , name{}
        , revision{}
        , customization{}
        , part_number{}
    {
        std::memcpy(this->name, name.c_str(), std::min(sizeof(this->name) - 1, name.size() + 1));

        std::memcpy(this->revision, revision.c_str(),
                    std::min(sizeof(this->revision) - 1, revision.size() + 1));

        std::memcpy(this->customization, customization.c_str(),
                    std::min(sizeof(this->customization) - 1, customization.size() + 1));

        std::memcpy(this->part_number, part_number.c_str(),
                    std::min(sizeof(this->part_number) - 1, part_number.size() + 1));
    };

    enum ADQFirmwareType type;
    char name[32];
    char revision[32];
    char customization[16];
    char part_number[16];
};

enum ADQCommunicationInterface
{
    ADQ_COMMUNICATION_INTERFACE_PCIE = 1,
    ADQ_COMMUNICATION_INTERFACE_USB = 2
};

struct ADQConstantParametersCommunicationInterface
{
    /* Convenience constructor for simulated digitizers. */
    ADQConstantParametersCommunicationInterface() = default;
    ADQConstantParametersCommunicationInterface(enum ADQCommunicationInterface type, int link_width,
                                                int link_generation)
        : type(type)
        , link_width(static_cast<int32_t>(link_width))
        , link_generation(static_cast<int32_t>(link_generation))
    {};

    enum ADQCommunicationInterface type;
    int32_t link_width;
    int32_t link_generation;
};


/* A reduced version of the set of constant parameters. */
struct ADQConstantParameters
{
    /* Convenience constructor for simulated digitizers. */
    ADQConstantParameters() = default;
    ADQConstantParameters(const std::string &serial_number, const std::string &product_name,
                          const std::string &product_options,
                          const struct ADQConstantParametersFirmware &firmware,
                          const struct ADQConstantParametersCommunicationInterface &interface,
                          const std::vector<struct ADQConstantParametersChannel> &channel)
        : id(ADQ_PARAMETER_ID_CONSTANT)
        , nof_channels(static_cast<int32_t>(channel.size()))
        , serial_number{}
        , product_name{}
        , product_options{}
        , firmware(firmware)
        , communication_interface(interface)
        , channel{}
        , clock_system{}
        , dram_size(8ull * 1024 * 1024 * 1024)
        , magic(ADQ_PARAMETERS_MAGIC)
    {
        std::memcpy(this->serial_number, serial_number.c_str(),
                    std::min(sizeof(this->serial_number) - 1, serial_number.size() + 1));

        std::memcpy(this->product_name, product_name.c_str(),
                    std::min(sizeof(this->product_name) - 1, product_name.size() + 1));

        std::memcpy(this->product_options, product_options.c_str(),
                    std::min(sizeof(this->product_options) - 1, product_options.size() + 1));

        for (size_t i = 0; i < channel.size() && i < ADQ_MAX_NOF_CHANNELS; ++i)
            this->channel[i] = channel[i];
    };

    enum ADQParameterId id;
    int32_t nof_channels;
    char serial_number[16];
    char product_name[32];
    char product_options[32];
    struct ADQConstantParametersFirmware firmware;
    struct ADQConstantParametersCommunicationInterface communication_interface;
    struct ADQConstantParametersChannel channel[ADQ_MAX_NOF_CHANNELS];
    struct ADQClockSystemParameters clock_system;
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
