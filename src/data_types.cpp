#include "data_types.h"

std::string Value::Format(bool show_sign) const
{
    return Format(value, show_sign);
}

std::string Value::Format(const char *precision, bool show_sign) const
{
    return Format(value, precision, show_sign);
}

std::string Value::Format(const std::string &precision, bool show_sign) const
{
    return Format(value, precision, show_sign);
}

std::string Value::Format(double value, bool show_sign) const
{
    return Format(value, properties.precision, show_sign);
}

std::string Value::FormatDelta(double value, bool show_sign) const
{
    return FormatDelta(value, properties.precision, show_sign);
}

std::string Value::Format(double value, const std::string &precision, bool show_sign) const
{
    return Format::Metric(value, Format::String(precision, properties.unit, show_sign),
                          properties.highest_prefix, properties.lowest_prefix);
}

std::string Value::FormatDelta(double value, const std::string &precision, bool show_sign) const
{
    return Format::Metric(value, Format::String(precision, properties.delta_unit, show_sign),
                          properties.highest_prefix, properties.lowest_prefix);
}

BaseRecord::~BaseRecord()
{}
