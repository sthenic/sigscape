#ifndef DATA_TYPES_H_WSKIC4
#define DATA_TYPES_H_WSKIC4

#include <cstdint>
#include <cstddef>
#include <complex>
#include <limits>
#include <cstring>

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
        x = std::shared_ptr<double[]>( new double[count] );
        y = std::shared_ptr<double[]>( new double[count] );
        id = TIME_DOMAIN;
        this->count = count;
        capacity = count;
        estimated_trigger_frequency = 0;
    }

    /* We don't share ownership of the data intentionally. All copies
       are deep copies. */
    TimeDomainRecord(const TimeDomainRecord &other)
    {
        id = TIME_DOMAIN;
        header = other.header;
        count = other.count;
        capacity = other.count;
        estimated_trigger_frequency = other.estimated_trigger_frequency;

        x = std::shared_ptr<double[]>( new double[other.count] );
        y = std::shared_ptr<double[]>( new double[other.count] );
        std::memcpy(x.get(), other.x.get(), other.count * sizeof(*x.get()));
        std::memcpy(y.get(), other.y.get(), other.count * sizeof(*y.get()));
    }

    TimeDomainRecord &operator=(const TimeDomainRecord &other)
    {
        if (this != &other)
        {
            if (capacity < other.count)
            {
                x = std::shared_ptr<double[]>( new double[other.count] );
                y = std::shared_ptr<double[]>( new double[other.count] );
                id = TIME_DOMAIN;
                capacity = other.count;
            }

            std::memcpy(x.get(), other.x.get(), other.count * sizeof(*x.get()));
            std::memcpy(y.get(), other.y.get(), other.count * sizeof(*y.get()));
            count = other.count;
            header = other.header;
            estimated_trigger_frequency = other.estimated_trigger_frequency;
        }

        return *this;
    }

    enum RecordId id;
    std::shared_ptr<double[]> x;
    std::shared_ptr<double[]> y;
    struct TimeDomainRecordHeader header;
    size_t count;
    size_t capacity;
    double estimated_trigger_frequency;
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
        x = std::shared_ptr<double[]>( new double[count] );
        y = std::shared_ptr<double[]>( new double[count] );
        yc = std::shared_ptr<std::complex<double>[]>( new std::complex<double>[count] );
        id = FREQUENCY_DOMAIN;
        this->count = count;
        capacity = count;
    }

    FrequencyDomainRecord(const FrequencyDomainRecord &other)
    {
        x = std::shared_ptr<double[]>( new double[other.count] );
        y = std::shared_ptr<double[]>( new double[other.count] );
        yc = std::shared_ptr<std::complex<double>[]>( new std::complex<double>[other.count] );
        id = FREQUENCY_DOMAIN;
        header = other.header;
        count = other.count;
        capacity = other.count;

        std::memcpy(x.get(), other.x.get(), other.count * sizeof(*x.get()));
        std::memcpy(y.get(), other.y.get(), other.count * sizeof(*y.get()));
        std::memcpy(yc.get(), other.yc.get(), other.count * sizeof(*yc.get()));
    }

    FrequencyDomainRecord &operator=(const FrequencyDomainRecord &other)
    {
        if (this != &other)
        {
            if (capacity < other.count)
            {
                x = std::shared_ptr<double[]>( new double[other.count] );
                y = std::shared_ptr<double[]>( new double[other.count] );
                yc = std::shared_ptr<std::complex<double>[]>( new std::complex<double>[other.count] );
                id = FREQUENCY_DOMAIN;
                capacity = other.count;
            }

            std::memcpy(x.get(), other.x.get(), other.count * sizeof(*x.get()));
            std::memcpy(y.get(), other.y.get(), other.count * sizeof(*y.get()));
            std::memcpy(yc.get(), other.yc.get(), other.count * sizeof(*yc.get()));
            count = other.count;
            header = other.header;
        }

        return *this;
    }

    enum RecordId id;
    std::shared_ptr<double[]> x;
    std::shared_ptr<double[]> y;
    std::shared_ptr<std::complex<double>[]> yc;
    struct FrequencyDomainRecordHeader header;
    size_t count;
    size_t capacity;
};

struct Waterfall
{
    Waterfall(size_t count)
    {
        data = std::shared_ptr<double[]>( new double[count] );
        this->count = count;
        capacity = count;
        rows = 0;
        columns = 0;
    }

    Waterfall(const std::deque<std::shared_ptr<FrequencyDomainRecord>> &waterfall)
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
            columns = waterfall.front()->count / 2;
            for (const auto &record : waterfall)
            {
                if ((record->count / 2) != columns)
                {
                    exit_without_copy = true;
                    break;
                }
            }
        }

        if (exit_without_copy)
        {
            data = NULL;
            count = 0;
            capacity = 0;
            rows = 0;
            columns = 0;
            return;
        }

        /* Perform the actual allocation and linear copy of the objects in the deque. */
        rows = waterfall.size();
        count = rows * columns;
        data = std::shared_ptr<double[]>( new double[rows * columns] );
        capacity = count;

        size_t idx = 0;
        for (const auto &record : waterfall)
        {
            std::memcpy(data.get() + idx, record->y.get(), columns * sizeof(*data.get()));
            idx += columns;
        }
    }

    Waterfall(const Waterfall &other)
    {
        data = std::shared_ptr<double[]>( new double[other.count] );
        count = other.count;
        capacity = other.count;
        rows = other.rows;
        columns = other.columns;

        std::memcpy(data.get(), other.data.get(), other.count * sizeof(*data.get()));
    }

    Waterfall &operator=(const Waterfall &other)
    {
        if (this != &other)
        {
            if (capacity < other.count)
            {
                data = std::shared_ptr<double[]>( new double[other.count] );
                capacity = other.count;
            }

            std::memcpy(data.get(), other.data.get(), other.count * sizeof(*data.get()));
            count = other.count;
            rows = other.rows;
            columns = other.columns;
        }
    }

    std::shared_ptr<double[]> data;
    size_t count;
    size_t capacity;
    size_t rows;
    size_t columns;
};

struct ProcessedRecord
{
    ProcessedRecord(size_t count)
    {
        time_domain = std::make_shared<TimeDomainRecord>(count);
        frequency_domain = std::make_shared<FrequencyDomainRecord>(count); /* FIXME: perhaps skip this? */
        waterfall = NULL;
        time_domain_metrics.max = std::numeric_limits<double>::lowest();
        time_domain_metrics.min = std::numeric_limits<double>::max();
        frequency_domain_metrics.max = std::numeric_limits<double>::lowest();
        frequency_domain_metrics.min = std::numeric_limits<double>::max();
    }

    ProcessedRecord(const ProcessedRecord &other)
    {
        /* We make a deep copy of the other object and create new shared pointers. */
        if (other.time_domain != NULL)
            time_domain = std::make_shared<TimeDomainRecord>(*other.time_domain);

        if (other.frequency_domain != NULL)
            frequency_domain = std::make_shared<FrequencyDomainRecord>(*other.frequency_domain);

        if (other.waterfall != NULL)
            waterfall = std::make_shared<Waterfall>(*other.waterfall);

        time_domain_metrics = other.time_domain_metrics;
        frequency_domain_metrics = other.frequency_domain_metrics;
    }

    ProcessedRecord &operator=(const ProcessedRecord &other)
    {
        if (this != &other)
        {
            if (other.time_domain != NULL)
                time_domain = std::make_shared<TimeDomainRecord>(*other.time_domain);
            else
                time_domain = NULL;

            if (other.frequency_domain != NULL)
                frequency_domain = std::make_shared<FrequencyDomainRecord>(*other.frequency_domain);
            else
                frequency_domain = NULL;

            if (other.waterfall != NULL)
                waterfall = std::make_shared<Waterfall>(*other.waterfall);
            else
                waterfall = NULL;

            time_domain_metrics = other.time_domain_metrics;
            frequency_domain_metrics = other.frequency_domain_metrics;
        }

        return *this;
    }

    std::shared_ptr<TimeDomainRecord> time_domain;
    std::shared_ptr<FrequencyDomainRecord> frequency_domain;
    std::shared_ptr<Waterfall> waterfall;

    struct TimeDomainMetrics
    {
        double max;
        double min;
    } time_domain_metrics;

    struct FrequencyDomainMetrics
    {
        double max;
        double min;
    } frequency_domain_metrics;
};

#endif
