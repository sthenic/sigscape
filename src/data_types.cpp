#include "data_types.h"

std::string Value::Format(bool show_sign) const
{
    return Format(value, show_sign);
}

std::string Value::Format(double other, bool show_sign) const
{
    return Format::Metric(other, Format::String(precision, unit, show_sign), highest_prefix);
}

std::string Value::FormatDelta(double other, bool show_sign) const
{
    return Format::Metric(other, Format::String(precision, delta_unit, show_sign), highest_prefix);
}

BaseRecord::~BaseRecord()
{}

std::string BaseRecord::FormatX(double value, const std::string &precision, bool show_sign)
{
    return Format::Metric(value, Format::String(precision, x_unit, show_sign), x_highest_prefix);
}

std::string BaseRecord::FormatDeltaX(double value, const std::string &precision, bool show_sign)
{
    return Format::Metric(value, Format::String(precision, x_delta_unit, show_sign), x_highest_prefix);
}

std::string BaseRecord::FormatY(double value, const std::string &precision, bool show_sign)
{
    return Format::Metric(value, Format::String(precision, y_unit, show_sign), y_highest_prefix);
}

std::string BaseRecord::FormatDeltaY(double value, const std::string &precision, bool show_sign)
{
    return Format::Metric(value, Format::String(precision, y_delta_unit, show_sign), y_highest_prefix);
}
