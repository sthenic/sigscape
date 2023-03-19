#ifndef DATA_TYPES_H_WSKIC4
#define DATA_TYPES_H_WSKIC4

#include "fmt/format.h"
#include "format.h"

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

/* The `Value` type groups together a `double` with a few properties to greatly
   simplify its formatting for UI presentation. This is handled through the
   various `Format` functions. */
struct Value
{
    struct Properties
    {
        Properties() = default;
        Properties(std::string unit,
                   std::string delta_unit,
                   std::string precision,
                   double highest_prefix = 1e12,
                   double lowest_prefix = 1e-12)
            : unit(unit)
            , delta_unit(delta_unit)
            , precision(precision)
            , highest_prefix(highest_prefix)
            , lowest_prefix(lowest_prefix)
        {}

        Properties(std::string unit,
                   std::string precision,
                   double highest_prefix = 1e12,
                   double lowest_prefix = 1e-12)
            : Properties(unit, unit, precision, highest_prefix, lowest_prefix)
        {}

        std::string unit;
        std::string delta_unit;
        std::string precision;
        double highest_prefix;
        double lowest_prefix;
    };

    Value() = default;
    Value(double value, const Properties &properties)
        : value(value)
        , properties(properties)
    {}

    /* Formatter returning a string for UI presentation. */
    std::string Format(bool show_sign = false) const;
    std::string Format(const char *precision, bool show_sign = false) const;
    std::string Format(const std::string &precision, bool show_sign = false) const;

    /* Format another value as if it had the properties of this one. This is
       useful when needing to format a derived value, e.g. the result of a
       calculation involving this value. */
    std::string Format(double value, bool show_sign = false) const;
    std::string FormatDelta(double value, bool show_sign = false) const;

    std::string Format(double value, const std::string &precision, bool show_sign = false) const;
    std::string FormatDelta(double value, const std::string &precision, bool show_sign = false) const;

    double value;
    Properties properties;
};

struct BaseRecord
{
    BaseRecord(size_t count, const Value::Properties &x_properties,
               const Value::Properties &y_properties)
        : x(count)
        , y(count)
        , x_properties(x_properties)
        , y_properties(y_properties)
        , step(0.0)
    {}

    virtual ~BaseRecord() = 0;

    /* Object-bound constructor of a value in the x-dimension. */
    Value ValueX(double value) const
    {
        return Value(value, x_properties);
    }

    /* Object-bound constructor of a value in the y-dimension. */
    Value ValueY(double value) const
    {
        return Value(value, y_properties);
    }

    std::vector<double> x;
    std::vector<double> y;
    Value::Properties x_properties;
    Value::Properties y_properties;
    double step;
};

/* A time domain record. */
struct TimeDomainRecord : public BaseRecord
{
    inline static const std::string PRECISION = "8.2";
    inline static const std::string PRECISION_UNCONVERTED = "8.0";

    TimeDomainRecord(const struct ADQGen4Record *raw,
                     const struct ADQAnalogFrontendParametersChannel &afe,
                     double code_normalization, bool convert = true)
        : BaseRecord(raw->header->record_length,
                     convert ? Value::Properties{"s", PRECISION, 1e-3, 1e-12}
                             : Value::Properties{"S", PRECISION_UNCONVERTED, 1.0, 1.0},
                     convert ? Value::Properties{"V", PRECISION, 1e-3, 1e-12}
                             : Value::Properties{"C", PRECISION_UNCONVERTED, 1.0, 1.0})
        , header(*raw->header)
        , estimated_trigger_frequency(0.0, {"Hz", PRECISION, 1e6})
        , estimated_throughput(0.0, {"B/s", PRECISION, 1e6})
        , sampling_frequency(0.0, {"Hz", PRECISION, 1e6})
        , sampling_period(0.0, {"s", PRECISION, 1e-3})
        , range_max(ValueY(0.0))
        , range_min(ValueY(0.0))
        , range_mid(ValueY(0.0))
        , max(ValueY(std::numeric_limits<double>::lowest()))
        , min(ValueY(std::numeric_limits<double>::max()))
        , mean(ValueY(0.0))
        , rms(ValueY(0.0))
    {
        /* The time unit is specified in picoseconds at most. Given that we're
           using a 32-bit float, we truncate any information beyond that point. */
        int time_unit_ps = static_cast<int>(raw->header->time_unit * 1e12);
        double time_unit = static_cast<double>(time_unit_ps) * 1e-12;

        sampling_period.value = static_cast<double>(raw->header->sampling_period) * time_unit;
        sampling_frequency.value = std::round(1.0 / sampling_period.value);

        double record_start;
        if (convert)
        {
            step = sampling_period.value;
            record_start = static_cast<double>(raw->header->record_start) * time_unit;
            range_max.value = (afe.input_range / 2 - afe.dc_offset) / 1e3;
            range_min.value = (-afe.input_range / 2 - afe.dc_offset) / 1e3;
            range_mid.value = (range_max.value + range_min.value) / 2;
        }
        else
        {
            step = 1.0;
            record_start = 0.0; /* FIXME: Always start at 0? */
            range_max.value = code_normalization / 2 - 1;
            range_min.value = -(code_normalization / 2);
            range_mid.value = 0.0;
        }

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

    }

    template <typename T>
    static void Transform(const T *data, double record_start, double sampling_period,
                          double code_normalization, double input_range, double dc_offset,
                          std::vector<double> &x, std::vector<double> &y, bool convert)
    {
        for (size_t i = 0; i < x.size(); ++i)
        {
            x[i] = record_start + static_cast<double>(i) * sampling_period;

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

    /* The record header, as given to us by the ADQAPI. */
    struct ADQGen4RecordHeader header;

    /* Values that can be readily displayed in the UI. */
    Value estimated_trigger_frequency;
    Value estimated_throughput;
    Value sampling_frequency;
    Value sampling_period;
    Value range_max;
    Value range_min;
    Value range_mid;
    Value max;
    Value min;
    Value mean;
    Value rms;
};


struct FrequencyDomainRecord : public BaseRecord
{
    inline static const std::string PRECISION = "7.2";

    FrequencyDomainRecord(size_t count)
        : BaseRecord(count,
                     Value::Properties{"Hz", PRECISION, 1e6, 1.0},
                     Value::Properties{"dBFS", "dB", PRECISION, 1.0, 1.0})
        , fundamental{}
        , spur{}
        , harmonics{} /* FIXME: "gain_phase" */
        , gain_spur{}
        , offset_spur{}
        , snr(0.0, {"dB", PRECISION, 1.0})
        , sinad(0.0, {"dB", PRECISION, 1.0})
        , enob(0.0, {"bits", PRECISION, 1.0})
        , sfdr_dbc(0.0, {"dBc", PRECISION, 1.0})
        , sfdr_dbfs(0.0, {"dBFS", PRECISION, 1.0})
        , thd(0.0, {"dB", PRECISION, 1.0})
        , noise(0.0, {"dBFS", PRECISION, 1.0})
        , noise_moving_average(0.0, {"dBFS", PRECISION, 1.0})
        , size(0.0, {"pts", "7.0", 1.0})
        , bin(0.0, {"Hz", PRECISION, 1e6})
    {}

    /* Delete copy constructors until we need them. */
    FrequencyDomainRecord(const FrequencyDomainRecord &other) = delete;
    FrequencyDomainRecord &operator=(const FrequencyDomainRecord &other) = delete;

    /* Values that can be readily displayed in the UI. */
    std::tuple<Value, Value> fundamental;
    std::tuple<Value, Value> spur;
    std::vector<std::tuple<Value, Value>> harmonics;
    std::tuple<Value, Value> gain_spur;
    std::tuple<Value, Value> offset_spur;
    Value snr;
    Value sinad;
    Value enob;
    Value sfdr_dbc;
    Value sfdr_dbfs;
    Value thd;
    Value noise;
    Value noise_moving_average;
    Value size;
    Value bin;
    bool overlap;
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
    {}

    /* Delete copy constructors until we need them. */
    ProcessedRecord(const ProcessedRecord &other) = delete;
    ProcessedRecord &operator=(const ProcessedRecord &other) = delete;

    std::shared_ptr<TimeDomainRecord> time_domain;
    std::shared_ptr<FrequencyDomainRecord> frequency_domain;
    std::shared_ptr<Waterfall> waterfall;
    std::shared_ptr<Persistence> persistence;
    std::string label;
};

struct SensorRecord : public BaseRecord
{
    SensorRecord()
        : BaseRecord(0, {"s", "8.2"}, {"N/A", "8.2"})
        , status(-1)
        , id()
        , group_id()
        , note("No data")
    {}

    SensorRecord(uint32_t id, uint32_t group_id, const std::string &y_unit)
        : BaseRecord(0, {"s", "8.2", 1.0}, {y_unit, "8.2"})
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
