#ifndef DATA_TYPES_H_WSKIC4
#define DATA_TYPES_H_WSKIC4

#include <cstdint>
#include <cstddef>
#include <complex>
#include <limits>
#include <cstring>

#ifdef NO_ADQAPI
#include "mock_adqapi_definitions.h"
#else
#include "ADQAPI.h"
#endif

/* A time domain record. */
struct TimeDomainRecord
{
    TimeDomainRecord(const struct ADQGen4Record *raw,
                     const struct ADQAnalogFrontendParametersChannel &afe)
        : x(raw->header->record_length)
        , y(raw->header->record_length)
        , header(*raw->header)
        , estimated_trigger_frequency(0)
        , estimated_throughput(0)
        , step(0)
        , sampling_frequency(0)
        , record_start(0.0)
    {
        /* FIXME: Assuming two bytes per sample. */

        /* The time unit is specified in picoseconds at most. Given that we're
           using a 32-bit float, we truncate any information beyond that point. */

        int time_unit_ps = static_cast<int>(raw->header->time_unit * 1e12);
        double time_unit = static_cast<double>(time_unit_ps) * 1e-12;
        const int16_t *data = static_cast<const int16_t *>(raw->data);
        step = static_cast<double>(raw->header->sampling_period) * time_unit;
        record_start = static_cast<double>(raw->header->record_start) * time_unit;

        /* This will be an integer number of Hz */
        sampling_frequency = std::round(1.0 / step);

        for (size_t i = 0; i < raw->header->record_length; ++i)
        {
            /* FIXME: Read vertical resolution from header->data_format. */
            x[i] = record_start + i * step;
            y[i] = static_cast<double>(data[i]) / 65536.0 * afe.input_range - afe.dc_offset;
            y[i] /= 1e3;
        }
    }

    /* Delete copy constructors until we need them. */
    TimeDomainRecord(const TimeDomainRecord &other) = delete;
    TimeDomainRecord &operator=(const TimeDomainRecord &other) = delete;

    std::vector<double> x;
    std::vector<double> y;
    struct ADQGen4RecordHeader header;
    double estimated_trigger_frequency;
    double estimated_throughput;
    double step; /* Sampling period */
    double sampling_frequency;
    double record_start;
};

struct FrequencyDomainRecord
{
    FrequencyDomainRecord(size_t count)
        : x(count)
        , y(count)
        , step(0)
    {
    }

    /* Delete copy constructors until we need them. */
    FrequencyDomainRecord(const FrequencyDomainRecord &other) = delete;
    FrequencyDomainRecord &operator=(const FrequencyDomainRecord &other) = delete;

    std::vector<double> x;
    std::vector<double> y;
    double step; /* Bin range */
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
    } time_domain_metrics;

    struct FrequencyDomainMetrics
    {
        std::pair<double, double> fundamental;
        std::pair<double, double> spur;
        std::vector<std::pair<double, double>> harmonics;
        double snr;
        double sinad;
        double enob;
        double sfdr_dbc;
        double sfdr_dbfs;
        double thd;
        double noise;
        bool overlap;
    } frequency_domain_metrics;
};

#endif
