#ifndef DATA_TYPES_H_WSKIC4
#define DATA_TYPES_H_WSKIC4

#include "fmt/format.h"

#include <cstdint>
#include <cstddef>
#include <complex>
#include <limits>
#include <cstring>
#include <vector>
#include <deque>
#include <stdexcept>
#include <string>

#ifdef MOCK_ADQAPI
#include "mock/adqapi_definitions.h"
#else
#include "ADQAPI.h"
#endif

struct BaseRecord
{
    BaseRecord(size_t count, const std::string &x_unit, const std::string &y_unit,
               const std::string &x_delta_unit, const std::string &y_delta_unit)
        : x(count)
        , y(count)
        , x_unit(x_unit)
        , y_unit(y_unit)
        , x_delta_unit(x_delta_unit)
        , y_delta_unit(y_delta_unit)
        , step(0)
    {}

    BaseRecord(size_t count, const std::string &x_unit, const std::string &y_unit)
        : BaseRecord(count, x_unit, y_unit, x_unit, y_unit)
    {}

    virtual ~BaseRecord() = 0;

    std::vector<double> x;
    std::vector<double> y;
    std::string x_unit;
    std::string y_unit;
    std::string x_delta_unit;
    std::string y_delta_unit;
    double step;
};

/* A time domain record. */
struct TimeDomainRecord : public BaseRecord
{
    TimeDomainRecord(const struct ADQGen4Record *raw,
                     const struct ADQAnalogFrontendParametersChannel &afe,
                     double code_normalization, bool convert = true)
        : BaseRecord(raw->header->record_length,
                     convert ? "s" : "Samples",
                     convert ? "V" : "Codes")
        , header(*raw->header)
        , estimated_trigger_frequency(0)
        , estimated_throughput(0)
        , sampling_frequency(0)
        , range_max(0)
        , range_min(0)
        , range_mid(0)
    {
        /* The time unit is specified in picoseconds at most. Given that we're
           using a 32-bit float, we truncate any information beyond that point. */
        int time_unit_ps = static_cast<int>(raw->header->time_unit * 1e12);
        double time_unit = static_cast<double>(time_unit_ps) * 1e-12;
        step = static_cast<double>(raw->header->sampling_period) * time_unit;
        double record_start = static_cast<double>(raw->header->record_start) * time_unit;

        /* This will be an integer number of Hz */
        sampling_frequency = std::round(1.0 / step);

        switch (raw->header->data_format)
        {
        case ADQ_DATA_FORMAT_INT16:
            Transform(static_cast<const int16_t *>(raw->data), record_start, step,
                      code_normalization, afe.input_range, afe.dc_offset, x, y, convert);
            break;

        case ADQ_DATA_FORMAT_INT32:
            Transform(static_cast<const int32_t *>(raw->data), record_start, step,
                      code_normalization, afe.input_range, afe.dc_offset, x, y, convert);
            break;

        default:
            throw std::invalid_argument(
                fmt::format("Unknown data format '{}' when transforming time domain record.",
                            raw->header->data_format));
        }

        range_max = (afe.input_range / 2 - afe.dc_offset) / 1e3;
        range_min = (-afe.input_range / 2 - afe.dc_offset) / 1e3;
        range_mid = (range_max + range_min) / 2;
    }

    template <typename T>
    static void Transform(const T *data, double record_start, double sampling_period,
                          double code_normalization, double input_range, double dc_offset,
                          std::vector<double> &x, std::vector<double> &y, bool convert)
    {
        for (size_t i = 0; i < x.size(); ++i)
        {
            x[i] = record_start + i * sampling_period;

            if (convert)
            {
                y[i] = static_cast<double>(data[i]) / code_normalization * input_range - dc_offset;
                y[i] /= 1e3; /* The value is in millivolts before we scale it. */
            }
            else
            {
                y[i] = static_cast<double>(data[i]);
            }
        }
    }

    /* Delete copy constructors until we need them. */
    TimeDomainRecord(const TimeDomainRecord &other) = delete;
    TimeDomainRecord &operator=(const TimeDomainRecord &other) = delete;

    struct ADQGen4RecordHeader header;
    double estimated_trigger_frequency;
    double estimated_throughput;
    double sampling_frequency;
    double range_max;
    double range_min;
    double range_mid;
};

struct FrequencyDomainRecord : public BaseRecord
{
    FrequencyDomainRecord(size_t count)
        : BaseRecord(count, "Hz", "dBFS", "Hz", "dB")
    {}

    /* Delete copy constructors until we need them. */
    FrequencyDomainRecord(const FrequencyDomainRecord &other) = delete;
    FrequencyDomainRecord &operator=(const FrequencyDomainRecord &other) = delete;
};

struct Waterfall
{
    Waterfall(const std::deque<std::shared_ptr<FrequencyDomainRecord>> &waterfall)
        : data{}
        , rows(0)
        , columns(0)
    {
        /* Create a waterfall from a dequeue of frequency domain records. These
           must be of the same size. Otherwise, we exit without making the copy. */

        bool exit_without_copy = false;
        if (waterfall.size() == 0)
        {
            exit_without_copy = true;
        }
        else
        {
            /* +1 for the Nyquist bin */
            for (const auto &record : waterfall)
            {
                if (record->x.size() != waterfall.front()->x.size())
                {
                    exit_without_copy = true;
                    break;
                }
            }
        }

        if (exit_without_copy)
            return;

        /* Perform the actual allocation and linear copy of the objects in the deque. */
        columns = waterfall.front()->x.size();
        rows = waterfall.size();
        data.resize(rows * columns);

        size_t idx = 0;
        for (const auto &record : waterfall)
        {
            std::memcpy(data.data() + idx, record->y.data(), columns * sizeof(*data.data()));
            idx += columns;
        }
    }

    /* Delete copy constructors until we need them. */
    Waterfall(const Waterfall &other) = delete;
    Waterfall &operator=(const Waterfall &other) = delete;

    std::vector<double> data;
    size_t rows;
    size_t columns;
};

struct Persistence
{
    Persistence(const std::deque<std::shared_ptr<TimeDomainRecord>> &persistence)
        : data(persistence)
    {}

    /* Delete copy constructors until we need them. */
    Persistence(const Persistence &other) = delete;
    Persistence &operator=(const Persistence &other) = delete;

    std::deque<std::shared_ptr<TimeDomainRecord>> data;
};

struct ProcessedRecord
{
    ProcessedRecord()
        : time_domain(NULL)
        , frequency_domain(NULL)
        , waterfall(NULL)
        , persistence(NULL)
        , label("")
        , time_domain_metrics{}
        , frequency_domain_metrics{}
    {
        time_domain_metrics.max = std::numeric_limits<double>::lowest();
        time_domain_metrics.min = (std::numeric_limits<double>::max)();
    }

    /* Delete copy constructors until we need them. */
    ProcessedRecord(const ProcessedRecord &other) = delete;
    ProcessedRecord &operator=(const ProcessedRecord &other) = delete;

    std::shared_ptr<TimeDomainRecord> time_domain;
    std::shared_ptr<FrequencyDomainRecord> frequency_domain;
    std::shared_ptr<Waterfall> waterfall;
    std::shared_ptr<Persistence> persistence;
    std::string label;

    struct TimeDomainMetrics
    {
        double max;
        double min;
        double mean;
        double rms;
    } time_domain_metrics;

    struct FrequencyDomainMetrics
    {
        std::pair<double, double> fundamental;
        std::pair<double, double> spur;
        std::vector<std::pair<double, double>> harmonics;
        std::pair<double, double> gain_spur;
        std::pair<double, double> offset_spur;
        double snr;
        double sinad;
        double enob;
        double sfdr_dbc;
        double sfdr_dbfs;
        double thd;
        double noise;
        double noise_moving_average;
        bool overlap;
    } frequency_domain_metrics;
};

struct SensorRecord : public BaseRecord
{
    SensorRecord()
        : BaseRecord(0, "s", "N/A")
        , status(-1)
        , id()
        , group_id()
        , note("No data")
    {}

    SensorRecord(uint32_t id, uint32_t group_id, const std::string &y_unit)
        : BaseRecord(0, "s", y_unit)
        , status(-1)
        , id(id)
        , group_id(group_id)
        , note()
    {}

    int status;
    uint32_t id;
    uint32_t group_id; /* FIXME: Prob not needed */
    std::string note;
};

#endif
