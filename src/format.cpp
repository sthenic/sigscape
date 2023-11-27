#include "format.h"
#include "fmt/format.h"

#include <vector>
#include <cctype>

std::string Format::Metric(double value, const std::string &format, double highest_prefix,
                           double lowest_prefix)
{
    static const std::vector<std::pair<double, const char *>> LIMITS = {
        {1e12, "T"},
        {1e9, "G"},
        {1e6, "M"},
        {1e3, "k"},
        {1, ""},
        {1e-3, "m"},
        {1e-6, "u"},
        {1e-9, "n"},
        {1e-12, "p"}
    };

    if (value == 0)
        return fmt::format(format, 0.0, "");

    /* Loop through the limits (descending order) checking if the input value is
       larger than the limit. If it is, we pick the corresponding prefix. If
       we've exhausted the search, we pick the last entry (smallest prefix). */

    for (size_t i = 0; i < LIMITS.size(); ++i)
    {
        if (LIMITS[i].first > highest_prefix)
            continue;

        bool next_is_not_allowed = i < LIMITS.size() - 1 && LIMITS[i + 1].first < lowest_prefix;
        bool is_larger_than_limit = std::fabs(value) >= LIMITS[i].first;

        if (next_is_not_allowed || is_larger_than_limit)
            return fmt::format(format, value / LIMITS[i].first, LIMITS[i].second);
    }

    return fmt::format(format, value / LIMITS.back().first, LIMITS.back().second);
}

std::string Format::String(const std::string &precision, const std::string &unit, bool show_sign)
{
    std::string format = "{:>-" + precision + "f} {}" + unit;
    if (show_sign)
        format[3] = '+';
    return format;
}

std::string Format::Invalid(const std::string &precision, const std::string &unit)
{
    /* Only pad fixed-width format specifiers. */
    if (precision.size() > 0 && std::isdigit(precision[0]))
    {
        size_t width = static_cast<size_t>(precision[0]) - 0x30u + unit.size() + 1;
        return fmt::format("{:<{w}}", "Invalid", fmt::arg("w", width));
    }
    else
    {
        return "Invalid";
    }
}

int Format::Metric(double value, char *tick_label, int size, void *data)
{
    auto unit = static_cast<const char *>(data);
    auto format = "{:g} {}" + std::string(unit);
    std::snprintf(tick_label, size, "%s", Metric(value, format).c_str());
    return 0;
}
