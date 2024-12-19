#pragma once

#include "fmt/format.h"
#include "format.h"
#include "imgui_extensions.h"

#include <cstdint>
#include <cstddef>
#include <complex>
#include <limits>
#include <cstring>
#include <vector>
#include <deque>
#include <stdexcept>
#include <string>

#include "ADQAPI.h"

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
                   double lowest_prefix = 1e-12,
                   std::string inverse_delta_unit = "")
            : unit(unit)
            , delta_unit(delta_unit)
            , inverse_delta_unit(inverse_delta_unit)
            , precision(precision)
            , highest_prefix(highest_prefix)
            , lowest_prefix(lowest_prefix)
        {}

        Properties(std::string unit,
                   std::string precision,
                   double highest_prefix = 1e12,
                   double lowest_prefix = 1e-12,
                   std::string inverse_delta_unit = "")
            : Properties(unit, unit, precision, highest_prefix, lowest_prefix, inverse_delta_unit)
        {}

        std::string unit;
        std::string delta_unit;
        std::string inverse_delta_unit;
        std::string precision;
        double highest_prefix;
        double lowest_prefix;
    };

    Value()
        : value{0}
        , properties{}
        , valid{true}
    {}

    Value(double value, const Properties &properties, bool valid = true)
        : value{value}
        , properties{properties}
        , valid{valid}
    {}

    Value &operator=(const double &lhs)
    {
        value = lhs;
        return *this;
    }

    /* Formatter returning a string for UI presentation. */
    std::string Format(bool show_sign = false) const;
    std::string Format(const char *precision, bool show_sign = false) const;
    std::string Format(const std::string &precision, bool show_sign = false) const;

    /* Formatter returning a CSV string as "Value,Unit". */
    std::string FormatCsv() const;

    /* Format another value as if it had the properties of this one. This is
       useful when needing to format a derived value, e.g. the result of a
       calculation involving this value. */
    std::string Format(double other, bool show_sign = false) const;
    std::string FormatDelta(double other, bool show_sign = false) const;
    std::string FormatInverseDelta(double other, bool show_sign = false) const;

    std::string Format(double other, const std::string &precision, bool show_sign = false) const;
    std::string FormatDelta(double other, const std::string &precision, bool show_sign = false) const;
    std::string FormatInverseDelta(double other, const std::string &precision, bool show_sign = false) const;

    double value;
    Properties properties;
    bool valid;
};

class ValueWithStatistics : public Value
{
public:
    ValueWithStatistics(const Value::Properties &properties = {})
        : Value{0, properties}
        , min{std::numeric_limits<double>::max()}
        , max{std::numeric_limits<double>::lowest()}
        , sum{0}
        , nof_values{0}
    {}

    ValueWithStatistics &operator=(const double &lhs)
    {
        value = lhs;

        if (lhs < min)
            min = lhs;

        if (lhs > max)
            max = lhs;

        sum += lhs;
        nof_values += 1;

        return *this;
    }

    void Clear()
    {
        value = 0;
        min = std::numeric_limits<double>::max();
        max = std::numeric_limits<double>::lowest();
        sum = 0;
        nof_values = 0;
    }

    Value Mean() const
    {
        return Value(sum / static_cast<double>(nof_values), properties);
    }

    Value Max() const
    {
        return Value(max, properties);
    }

    Value Min() const
    {
        return Value(min, properties);
    }

private:
    double min;
    double max;
    double sum;
    size_t nof_values;
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
    Value ValueX(double value, bool valid = true) const
    {
        return Value(value, x_properties, valid);
    }

    /* Object-bound constructor of a value in the y-dimension. */
    Value ValueY(double value, bool valid = true) const
    {
        return Value(value, y_properties, valid);
    }

    std::vector<double> x;
    std::vector<double> y;
    Value::Properties x_properties;
    Value::Properties y_properties;
    double step;
};

struct TimeDomainMetrics
{
    TimeDomainMetrics() = default;
    TimeDomainMetrics(const Value::Properties &properties)
        : max{properties}
        , min{properties}
        , mean{properties}
        , sdev{properties}
    {}

    void CopyProperties(const TimeDomainMetrics &other)
    {
        max.properties = other.max.properties;
        min.properties = other.min.properties;
        mean.properties = other.mean.properties;
        sdev.properties = other.sdev.properties;
    }

    void Clear()
    {
        max.Clear();
        min.Clear();
        mean.Clear();
        sdev.Clear();
    }

    ValueWithStatistics max;
    ValueWithStatistics min;
    ValueWithStatistics mean;
    ValueWithStatistics sdev;
};

/* A time domain record. */
struct TimeDomainRecord : public BaseRecord
{
    inline static const std::string PRECISION = "8.2";
    inline static const std::string PRECISION_UNCONVERTED = "8.0";

    TimeDomainRecord(const ADQGen4Record *raw,
                     const ADQAnalogFrontendParametersChannel &afe,
                     double code_normalization,
                     bool convert_horizontal = true, bool convert_vertical = true)
        : BaseRecord(raw->header->record_length,
                     convert_horizontal ? Value::Properties{"s", PRECISION, 1e-3, 1e-12, "Hz"}
                                        : Value::Properties{"S", PRECISION_UNCONVERTED, 1.0, 1.0},
                     convert_vertical ? Value::Properties{"V", PRECISION, 1e-3, 1e-12}
                                      : Value::Properties{"", PRECISION_UNCONVERTED, 1.0, 1.0})
        , header(*raw->header)
        , metrics(y_properties)
        , sampling_frequency(0.0, {"Hz", PRECISION, 1e9})
        , sampling_period(0.0, {"s", PRECISION, 1e-3})
        , range_max(ValueY(0.0))
        , range_min(ValueY(0.0))
        , range_mid(ValueY(0.0))
    {
        /* The time unit is specified in picoseconds at most. Given that we're
           using a 32-bit float, we truncate any information beyond that point. */
        int time_unit_ps = static_cast<int>(raw->header->time_unit * 1e12);
        double time_unit = static_cast<double>(time_unit_ps) * 1e-12;

        sampling_period.value = static_cast<double>(raw->header->sampling_period) * time_unit;
        sampling_frequency.value = std::round(1.0 / sampling_period.value);

        double record_start;
        if (convert_horizontal)
        {
            step = sampling_period.value;
            record_start = static_cast<double>(raw->header->record_start) * time_unit;
        }
        else
        {
            /* We intentionally always start with the first sample at zero to
               keep the horizontal grid in sync w/ the sampling grid. */
            /* TODO: At some point we could add visualization of the trigger point. */
            step = 1.0;
            record_start = 0.0; /* T: Always start at 0? */
        }

        if (convert_vertical)
        {
            range_max.value = (afe.input_range / 2 - afe.dc_offset) / 1e3;
            range_min.value = (-afe.input_range / 2 - afe.dc_offset) / 1e3;
            range_mid.value = (range_max.value + range_min.value) / 2;
        }
        else
        {
            range_max.value = code_normalization / 2 - 1;
            range_min.value = -(code_normalization / 2);
            range_mid.value = 0.0;
        }

        switch (raw->header->data_format)
        {
        case ADQ_DATA_FORMAT_INT16:
            Transform(static_cast<const int16_t *>(raw->data), record_start, step,
                      code_normalization, afe.input_range, afe.dc_offset, x, y, convert_vertical);
            break;

        case ADQ_DATA_FORMAT_INT32:
            Transform(static_cast<const int32_t *>(raw->data), record_start, step,
                      code_normalization, afe.input_range, afe.dc_offset, x, y, convert_vertical);
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
                          std::vector<double> &x, std::vector<double> &y, bool convert_vertial)
    {
        for (size_t i = 0; i < x.size(); ++i)
        {
            x[i] = record_start + static_cast<double>(i) * sampling_period;

            if (convert_vertial)
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

    /* Convert the metrics into a presentable format suitable for a table. */
    std::vector<std::vector<ImGui::TableCell>> FormatMetrics() const
    {
        const double peak_to_peak = metrics.max.value - metrics.min.value; /* FIXME: +1.0 'unit' */
        const double peak_to_peak_mean = metrics.max.Mean().value - metrics.min.Mean().value;

        const auto PeakToPeakHover = [&](double value)
        {
            return fmt::format("{:.2f}%", 100.0 * value / (range_max.value - range_min.value));
        };

        const auto StatisticalMeasureHover = [&](const ValueWithStatistics &value)
        {
            return fmt::format("Max:{}\nMin:{}", value.Max().Format(), value.Min().Format());
        };

        return {
            {"Record number", fmt::format("{: >8d}", header.record_number)},
            {
                "Maximum",
                metrics.max.Format(),
                {metrics.max.Mean().Format(), StatisticalMeasureHover(metrics.max)},
            },
            {
                "Minimum",
                metrics.min.Format(),
                {metrics.min.Mean().Format(), StatisticalMeasureHover(metrics.min)},
            },
            {
                "Peak-to-peak",
                {metrics.max.Format(peak_to_peak), PeakToPeakHover(peak_to_peak)},
                {range_max.Format(peak_to_peak_mean), PeakToPeakHover(peak_to_peak_mean)},
            },
            {
                "Mean",
                metrics.mean.Format(),
                {metrics.mean.Mean().Format(), StatisticalMeasureHover(metrics.mean)},
            },
            {
                "Standard deviation",
                metrics.sdev.Format(),
                {metrics.sdev.Mean().Format(), StatisticalMeasureHover(metrics.sdev)},
            },
            {"Sampling frequency", sampling_frequency.Format()},
            {"Sampling period", sampling_period.Format()},
        };
    }

    /* The record header, as given to us by the ADQAPI. */
    ADQGen4RecordHeader header;

    /* The time domain metrics that we keep statistics for. */
    TimeDomainMetrics metrics;

    /* Somewhat 'static' values that can be readily displayed in the UI. */
    Value sampling_frequency;
    Value sampling_period;
    Value range_max;
    Value range_min;
    Value range_mid;
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
        , harmonics{}
        , gain_phase_spur{}
        , offset_spur{}
        , snr(0.0, {"dB", PRECISION, 1.0})
        , sinad(0.0, {"dB", PRECISION, 1.0})
        , enob(0.0, {"bits", PRECISION, 1.0})
        , sfdr_dbc(0.0, {"dBc", PRECISION, 1.0})
        , sfdr_dbfs(0.0, {"dBFS", PRECISION, 1.0})
        , thd(0.0, {"dB", PRECISION, 1.0})
        , npsd(0.0, {"dBFS/Hz", PRECISION, 1.0})
        , noise_moving_average(0.0, {"dBFS", PRECISION, 1.0})
        , size(0.0, {"pts", "7.0", 1.0})
        , rbw(0.0, {"Hz", PRECISION, 1e6})
        , scale_factor(1.0)
        , energy_factor(1.0)
        , relative_power()
    {}

    /* Delete copy constructors until we need them. */
    FrequencyDomainRecord(const FrequencyDomainRecord &other) = delete;
    FrequencyDomainRecord &operator=(const FrequencyDomainRecord &other) = delete;

    /* Convert the metrics into a presentable format suitable for a table. */
    std::vector<std::vector<ImGui::TableCell>> FormatMetrics() const
    {
        return {
            {
                "SNR",
                snr.Format(),
                "Fund.",
                std::get<0>(fundamental).Format(),
                std::get<1>(fundamental).Format(), /* FIXME: mDBFS wrong */
            },
            {
                "SINAD",
                sinad.Format(),
                "Spur",
                std::get<0>(spur).Format(),
                std::get<1>(spur).Format(),
            },
            {
                "ENOB",
                enob.Format() + "  ", /* Padding for table */
                "HD2",
                std::get<0>(harmonics.at(0)).Format(),
                std::get<1>(harmonics.at(0)).Format(),
            },
            {
                "THD",
                thd.Format(),
                "HD3",
                std::get<0>(harmonics.at(1)).Format(),
                std::get<1>(harmonics.at(1)).Format(),
            },
            {
                "SFDR",
                sfdr_dbfs.Format(),
                "HD4",
                std::get<0>(harmonics.at(2)).Format(),
                std::get<1>(harmonics.at(2)).Format(),
            },
            {
                "NPSD",
                npsd.Format(),
                "HD5",
                std::get<0>(harmonics.at(3)).Format(),
                std::get<1>(harmonics.at(3)).Format(),
            },
            {
                "Size",
                size.Format(),
                "TIx",
                std::get<0>(gain_phase_spur).Format(),
                std::get<1>(gain_phase_spur).Format(),
            },
            {
                "RBW",
                rbw.Format(),
                "TIo",
                std::get<0>(offset_spur).Format(),
                std::get<1>(offset_spur).Format(),
            },
        };
    }

    bool AreAllMetricsValid()
    {
        const auto IsTupleValid = [](const std::tuple<Value, Value> &t) -> bool
        {
            const auto &[x, y] = t;
            return x.valid && y.valid;
        };

        const auto IsVectorValid = [&](const std::vector<std::tuple<Value, Value>> &v) -> bool
        {
            bool result = true;
            for (const auto &x : v)
                result = result && IsTupleValid(x);
            return result;
        };

        return IsTupleValid(fundamental)
               && IsTupleValid(spur)
               && IsTupleValid(gain_phase_spur)
               && IsTupleValid(offset_spur)
               && IsVectorValid(harmonics)
               && snr.valid
               && sinad.valid
               && enob.valid
               && sfdr_dbc.valid
               && sfdr_dbfs.valid
               && thd.valid
               && npsd.valid
               && noise_moving_average.valid
               && size.valid
               && rbw.valid;
    }

    void InvalidateNoiseMetrics()
    {
        snr.valid = false;
        sinad.valid = false;
        enob.valid = false;
        sfdr_dbc.valid = false;
        sfdr_dbfs.valid = false;
        thd.valid = false;
        npsd.valid = false;
        noise_moving_average.valid = false;
    }

    /* Values that can be readily displayed in the UI. */
    std::tuple<Value, Value> fundamental;
    std::tuple<Value, Value> spur;
    std::vector<std::tuple<Value, Value>> harmonics;
    std::tuple<Value, Value> gain_phase_spur;
    std::tuple<Value, Value> offset_spur;
    Value snr;
    Value sinad;
    Value enob;
    Value sfdr_dbc;
    Value sfdr_dbfs;
    Value thd;
    Value npsd;
    Value noise_moving_average;
    Value size;
    Value rbw;
    double scale_factor;
    double energy_factor;
    struct
    {
        double noise;
        double gain_phase_spur;
        double offset_spur;
        std::vector<double> harmonics;
    } relative_power;
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

struct ProcessedRecord
{
    inline static const std::string PRECISION = "8.2";

    ProcessedRecord(const std::string &label, double trigger_frequency, double throughput)
        : time_domain(NULL)
        , frequency_domain(NULL)
        , waterfall(NULL)
        , label(label)
        , trigger_frequency(trigger_frequency, {"Hz", PRECISION, 1e6})
        , throughput(throughput, {"B/s", PRECISION, 1e6})
    {}

    /* Delete copy constructors until we need them. */
    ProcessedRecord(const ProcessedRecord &other) = delete;
    ProcessedRecord &operator=(const ProcessedRecord &other) = delete;

    std::vector<std::vector<ImGui::TableCell>> FormatMetrics() const
    {
        return {
            {"Trigger frequency", trigger_frequency.Format()},
            {"Throughput", throughput.Format()},
        };
    }

    std::shared_ptr<TimeDomainRecord> time_domain;
    std::shared_ptr<FrequencyDomainRecord> frequency_domain;
    std::shared_ptr<Waterfall> waterfall;

    std::string label;
    Value trigger_frequency;
    Value throughput;
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
    uint32_t group_id;
    std::string note;
};

class MovingAverage
{
public:
    MovingAverage();

    /* Delete copy constructors until we need them. */
    MovingAverage(const MovingAverage &other) = delete;
    MovingAverage &operator=(const MovingAverage &other) = delete;

    /* Set the number of averages. This operation clears the rolling log if the
       number of averages differs from the current value. */
    void SetNumberOfAverages(size_t nof_averages);

    /* Prepare a new log entry of the target `size`. This function must be
       called before `InsertAndAverage()`, whose purpose is to fill the entry
       set up by this operation. */
    void PrepareNewEntry(size_t size);

    /* Insert y[i] into the latest entry in the rolling log of FFTs and return
       the averaged result, i.e.

         (y[i] + y[n-1][i] + ... + y[N-1][i]) / N

       The input value y[i] is expected to be the _energy_ in bin `i` or a
       similar quantity for which the averaging operation above is valid.

       `PrepareNewEntry()` _must_ have been called prior to this and `i` cannot
       exceed the size given in that call. */
    double InsertAndAverage(size_t i, double y);

    /* Clear the rolling log. */
    void Clear();

private:
    /* The rolling log of FFTs as a double-ended queue to be able to manipulate
       both ends to insert new entries and evict old ones. */
    std::deque<std::vector<double>> m_log;

    /* The number of averages (maximum size of the log). */
    size_t m_nof_averages;
};

class MaximumHold
{
public:
    MaximumHold();

    /* Delete copy constructors until we need them. */
    MaximumHold(const MaximumHold &other) = delete;
    MaximumHold &operator=(const MaximumHold &other) = delete;

    /* Compare y[i] with what's currently in the memory and return the greater
       of the two values. If y[i] is a new maximum, the memory is updated. */
    double Compare(size_t i, double y);

    /* Enable or disable the maximum hold. Disabling renders the `Compare`
       operation into a no-op. */
    void Enable(bool enable);

    /* Clear the memory. */
    void Clear();

private:
    /* We keep a log of the maximum values in a vector. */
    std::vector<double> m_log;
    bool m_enable;
};
