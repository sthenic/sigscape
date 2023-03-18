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

/* FIXME: Pairing a double and some properties to create a formattable value. */
struct Value
{
    Value() = default;
    Value(double value, const std::string &unit, const std::string &delta_unit,
          const std::string &precision, double highest_prefix = 1e9)
        : value(value)
        , highest_prefix(highest_prefix)
        , precision(precision)
        , unit(unit)
        , delta_unit(delta_unit)
    {}

    Value(double value, const std::string &unit, const std::string &precision,
          double highest_prefix = 1e9)
        : value(value)
        , highest_prefix(highest_prefix)
        , precision(precision)
        , unit(unit)
        , delta_unit(unit)
    {}

    /* Formatter returning a string for UI presentation. */
    std::string Format(bool show_sign = false) const;

    /* Format another value as if it had the properties of this one. This is
       useful when needing to format a derived value, e.g. the result of a
       calculation involving this value. */
    std::string Format(double other, bool show_sign = false) const;
    std::string FormatDelta(double other, bool show_sign = false) const;

    double value;
    double highest_prefix;
    std::string precision;
    std::string unit;
    std::string delta_unit;
};

struct BaseRecord
{
    /* FIXME: Group the unit/delta/precision/prefix? */
    BaseRecord(size_t count, const std::string &x_unit, const std::string &y_unit,
               const std::string &x_delta_unit, const std::string &y_delta_unit,
               const std::string &x_precision, const std::string &y_precision,
               double x_highest_prefix, double y_highest_prefix)
        : x(count)
        , y(count)
        , x_unit(x_unit)
        , y_unit(y_unit)
        , x_delta_unit(x_delta_unit)
        , y_delta_unit(y_delta_unit)
        , x_precision(x_precision)
        , y_precision(y_precision)
        , x_highest_prefix(x_highest_prefix)
        , y_highest_prefix(y_highest_prefix)
        , step(0.0)
    {}

    BaseRecord(size_t count, const std::string &x_unit, const std::string &y_unit,
               const std::string &x_precision, const std::string &y_precision,
               double x_highest_prefix, double y_highest_prefix)
        : BaseRecord(count, x_unit, y_unit, x_unit, y_unit, x_precision, y_precision,
                     x_highest_prefix, y_highest_prefix)
    {}

    virtual ~BaseRecord() = 0;

    /* Object-bound constructor of a value in the x-dimension. */
    Value ValueX(double value) const
    {
        return Value(value, x_unit, x_delta_unit, x_precision, x_highest_prefix);
    }

    /* Object-bound constructor of a value in the y-dimension. */
    Value ValueY(double value) const
    {
        return Value(value, y_unit, y_delta_unit, y_precision, y_highest_prefix);
    }

    /* Convenience functions to construct a homogenously formatted value from one of two dimensions. */
    std::string FormatX(double value, const std::string &precision, bool show_sign = false);
    std::string FormatDeltaX(double value, const std::string &precision, bool show_sign = false);
    std::string FormatY(double value, const std::string &precision, bool show_sign = false);
    std::string FormatDeltaY(double value, const std::string &precision, bool show_sign = false);

    std::vector<double> x;
    std::vector<double> y;
    std::string x_unit;
    std::string y_unit;
    std::string x_delta_unit;
    std::string y_delta_unit;
    std::string x_precision;
    std::string y_precision;
    double x_highest_prefix;
    double y_highest_prefix;
    double step;
};

/* A time domain record. */
struct TimeDomainRecord : public BaseRecord
{
    TimeDomainRecord(const struct ADQGen4Record *raw,
                     const struct ADQAnalogFrontendParametersChannel &afe,
                     double code_normalization, bool convert = true)
        : BaseRecord(raw->header->record_length,
                     convert ? "s" : "S",
                     convert ? "V" : "C",
                     convert ? "8.2" : "6.0",
                     convert ? "8.2" : "6.0",
                     convert ? 1e-3 : 1.0,
                     convert ? 1e-3 : 1.0)
        , header(*raw->header)
        , estimated_trigger_frequency(0.0, "Hz", "8.2", 1e6)
        , estimated_throughput(0.0, "B/s", "8.2", 1e6)
        , sampling_frequency(0.0, "Hz", "8.2", 1e6)
        , sampling_period(0.0, "s", "8.2", 1e-3)
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
    FrequencyDomainRecord(size_t count)
        : BaseRecord(count, "Hz", "dBFS", "Hz", "dB", "7.2", "7.2", 1e6, 1.0)
        , fundamental{}
        , spur{}
        , harmonics{}
        /* FIXME: "gain_phase" */
        , gain_spur{}
        , offset_spur{}
        , snr(0.0, "dB", "7.2", 1.0)
        , sinad(0.0, "dB", "7.2", 1.0)
        , enob(0.0, "bits", "7.2", 1.0)
        , sfdr_dbc(0.0, "dBc", "7.2", 1.0)
        , sfdr_dbfs(0.0, "dBFS", "7.2", 1.0)
        , thd(0.0, "dB", "7.2", 1.0)
        , noise(0.0, "dBFS", "7.2", 1.0)
        , noise_moving_average(0.0, "dBFS", "7.2", 1.0)
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
        : BaseRecord(0, "s", "N/A", "8.2", "8.2", 1e9, 1e9)
        , status(-1)
        , id()
        , group_id()
        , note("No data")
    {}

    SensorRecord(uint32_t id, uint32_t group_id, const std::string &y_unit)
        : BaseRecord(0, "s", y_unit, "8.2", "8.2", 1e9, 1e9)
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
