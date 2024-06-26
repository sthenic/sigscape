#include "data_types.h"

std::string Value::Format(bool show_sign) const
{
    return Format(value, show_sign);
}

std::string Value::FormatCsv() const
{
    return fmt::format("{:g},{}", value, properties.unit);
}

std::string Value::Format(const char *precision, bool show_sign) const
{
    return Format(value, precision, show_sign);
}

std::string Value::Format(const std::string &precision, bool show_sign) const
{
    return Format(value, precision, show_sign);
}

std::string Value::Format(double other, bool show_sign) const
{
    return Format(other, properties.precision, show_sign);
}

std::string Value::FormatDelta(double other, bool show_sign) const
{
    return FormatDelta(other, properties.precision, show_sign);
}

std::string Value::FormatInverseDelta(double other, bool show_sign) const
{
    return FormatInverseDelta(other, properties.precision, show_sign);
}

std::string Value::Format(double other, const std::string &precision, bool show_sign) const
{
    if (!valid)
        return Format::Invalid(precision, properties.unit);

    return Format::Metric(other, Format::String(precision, properties.unit, show_sign),
                          properties.highest_prefix, properties.lowest_prefix);
}

std::string Value::FormatDelta(double other, const std::string &precision, bool show_sign) const
{
    if (!valid)
        return Format::Invalid(precision, properties.delta_unit);

    return Format::Metric(other, Format::String(precision, properties.delta_unit, show_sign),
                          properties.highest_prefix, properties.lowest_prefix);
}

std::string Value::FormatInverseDelta(double other, const std::string &precision, bool show_sign) const
{
    if (!valid)
        return Format::Invalid(precision, properties.inverse_delta_unit);

    return Format::Metric(other, Format::String(precision, properties.inverse_delta_unit, show_sign),
                          1.0 / properties.lowest_prefix, 1.0 / properties.highest_prefix);
}

BaseRecord::~BaseRecord()
{}

MovingAverage::MovingAverage()
    : m_log{}
    , m_nof_averages(1)
{}

void MovingAverage::SetNumberOfAverages(size_t nof_averages)
{
    if (nof_averages < m_log.size())
        m_log.resize(nof_averages);
    m_nof_averages = nof_averages;
}

void MovingAverage::PrepareNewEntry(size_t size)
{
    if (m_log.size() >= m_nof_averages)
        m_log.pop_back();
    m_log.emplace_front(size);
}

double MovingAverage::InsertAndAverage(size_t i, double y)
{
    /* Assign y[i] to the foremost log entry. */
    m_log.at(0).at(i) = y;

    /* Calculate the average at position i over all the entries in the log. We
       let the first entry define the expected size and as soon as we see an
       entry with a different size, we stop. We need this cutoff because the log
       can consist of entries with varying sizes if the user has changed the
       record length in the middle of an ongoing acquisition. */
    double result = y;
    size_t j = 1;
    size_t size = m_log.at(0).size();
    for (; j < m_log.size(); ++j)
    {
        if (m_log[j].size() != size)
            break;
        result += m_log.at(j).at(i);
    }
    result /= static_cast<double>(j);

    return result;
}

void MovingAverage::Clear()
{
    m_log.clear();
}

MaximumHold::MaximumHold()
    : m_log{}
    , m_enable(false)
{}

double MaximumHold::Compare(size_t i, double y)
{
    if (!m_enable)
        return y;

    if (i >= m_log.size())
        m_log.push_back(y);

    auto &y_log = m_log.at(i);
    if (y > y_log)
        y_log = y;
    return y_log;
}

void MaximumHold::Enable(bool enable)
{
    if (!m_enable && enable)
        m_log.clear();
    m_enable = enable;
}

void MaximumHold::Clear()
{
    m_log.clear();
}
