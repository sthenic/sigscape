#pragma once

#include <cstddef>
#include <deque>
#include <tuple>
#include <array>

#include "data_types.h"


class RecordFrame
{
public:
    enum class Trigger
    {
        DISABLED,
        COUNTER,
        TIMESTAMP_SYNCHRONIZATION,
        PATTERN_GENERATOR0,
        PATTERN_GENERATOR1,
        NOF_ENTRIES,
    };

    static constexpr std::array LABELS = {
        "Disabled",
        "Counter",
        "Timestamp synchronization",
        "Pattern generator 0",
        "Pattern generator 1",
    };

    static constexpr std::array TOOLTIPS = {
        "",
        "A new frame is triggered when the specified capacity is exceeded.",
        "A new frame is triggered when the timestamp synchronization counter increments.",
        "A new frame is triggered when pattern generator 0 outputs logic high.",
        "A new frame is triggered when pattern generator 1 outputs logic high.",
    };

    struct Parameters
    {
        Parameters();
        static Parameters Clamp(const Parameters &parameters);

        Trigger trigger;
        size_t capacity;
        size_t idx;
    };

    RecordFrame();
    ~RecordFrame() = default;

    std::tuple<double, std::string> Fill() const;
    std::shared_ptr<ProcessedRecord> Record();
    const std::deque<std::shared_ptr<ProcessedRecord>> &Records() const;
    void PushRecord(std::shared_ptr<ProcessedRecord> &record);
    void SetParameters(const Parameters &parameters);
    const Parameters &GetParameters() const;
    void Clear();

private:
    Parameters m_parameters;

    std::deque<std::shared_ptr<ProcessedRecord>> m_frame;
    std::shared_ptr<ProcessedRecord> m_record;

    size_t m_last_frame_size;
    uint32_t m_last_timestamp_synchronization_counter;
};
