#include "record_frame.h"
#include "log.h"
#include "fmt/format.h"

#include <algorithm>

RecordFrame::Parameters::Parameters()
    : trigger(Trigger::DISABLED)
    , capacity(1)
    , idx(0)
{}

RecordFrame::Parameters RecordFrame::Parameters::Clamp(const Parameters &parameters)
{
    auto clamped = parameters;
    constexpr size_t CAPACITY_MIN = 1;
    constexpr size_t CAPACITY_MAX = 8192;
    clamped.capacity = std::max(CAPACITY_MIN, std::min(clamped.capacity, CAPACITY_MAX));

    /* We only clamp the index for the counter mode. In the other modes, the
       resulting size of the frame is unknown and might even vary. The frame
       access operation is protected so at worst, no data will be displayed. */
    if (parameters.trigger == Trigger::COUNTER)
        clamped.idx = std::min(clamped.capacity - 1, clamped.idx);

    return clamped;
}

RecordFrame::RecordFrame()
    : m_parameters{}
    , m_frame{}
    , m_record(NULL)
    , m_last_frame_size(0)
    , m_last_timestamp_synchronization_counter(0)
{}

std::tuple<double, std::string> RecordFrame::Fill() const
{
    if (m_parameters.trigger == Trigger::COUNTER)
    {
        return {
            static_cast<double>(m_frame.size()) / static_cast<double>(m_parameters.capacity),
            fmt::format("{}/{}", m_frame.size(), m_parameters.capacity),
        };
    }
    else if (m_last_frame_size > 0)
    {
        return {
            static_cast<double>(m_frame.size()) / static_cast<double>(m_last_frame_size),
            fmt::format("{}/{}", m_frame.size(), m_last_frame_size),
        };
    }
    else
    {
        return {1.0, fmt::format("{}", m_frame.size())};
    }
}

std::shared_ptr<ProcessedRecord> RecordFrame::Record()
{
    return m_record;
}

const std::deque<std::shared_ptr<ProcessedRecord>> &RecordFrame::Records() const
{
    return m_frame;
}

void RecordFrame::PushRecord(std::shared_ptr<ProcessedRecord> &record)
{
    bool clear = false;
    switch (m_parameters.trigger)
    {
        case Trigger::DISABLED:
            m_record = record;
            return;

        case Trigger::COUNTER:
            clear = m_frame.size() >= m_parameters.capacity;
            break;

        case Trigger::TIMESTAMP_SYNCHRONIZATION:
            clear = record->time_domain &&
                    record->time_domain->header.timestamp_synchronization_counter !=
                        m_last_timestamp_synchronization_counter;

            if (clear)
            {
                m_last_timestamp_synchronization_counter =
                    record->time_domain->header.timestamp_synchronization_counter;
            }
            break;

        case Trigger::PATTERN_GENERATOR0:
            clear = record->time_domain && (record->time_domain->header.misc & 0x1u);
            break;

        case Trigger::PATTERN_GENERATOR1:
            clear = record->time_domain && (record->time_domain->header.misc & 0x2u);
            break;

        default:
            m_record = record;
            Log::log->error("Record frame trigger {} not implemented.",
                            static_cast<int>(m_parameters.trigger));
            return;
    }

    if (clear)
    {
        m_last_frame_size = m_frame.size();
        m_frame.clear();
    }

    m_frame.push_back(record);
    if (m_parameters.idx < m_frame.size())
        m_record = m_frame.at(m_parameters.idx);
}

void RecordFrame::SetParameters(const Parameters &parameters)
{
    /* We clamp the values before proceeding. This will automatically propagate to the GUI widgets. */
    auto clamped = Parameters::Clamp(parameters);

    /* We clear the frame on any change in capacity. */
    if (clamped.capacity != m_parameters.capacity)
        Clear();

    m_parameters = clamped;
    if (m_parameters.idx < m_frame.size())
        m_record = m_frame.at(m_parameters.idx);
}

const RecordFrame::Parameters &RecordFrame::GetParameters() const
{
    return m_parameters;
}

void RecordFrame::Clear()
{
    /* We intentionally don't clear the record to avoid the GUI rendering empty
       plots when a new acquisition starts. */
    m_frame.clear();
    m_last_frame_size = 0;
    m_last_timestamp_synchronization_counter = 0;
}
