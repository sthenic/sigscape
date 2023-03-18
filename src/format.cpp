#include "format.h"
#include "fmt/format.h"

#include <vector>

std::string Format::Metric(double value, const std::string &format, double highest_prefix)
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

    for (const auto &limit : LIMITS)
    {
        if (limit.first > highest_prefix)
            continue;

        if (std::fabs(value) >= limit.first)
            return fmt::format(format, value / limit.first, limit.second);
    }

    return fmt::format(format, value / LIMITS.back().first, LIMITS.back().second);
}

/* Static member function to parametrize the construction of the fmt::format string. */
std::string Format::String(const std::string &precision, const std::string &unit, bool show_sign)
{
    std::string format = "{:>-" + precision + "f} {}" + unit;
    if (show_sign)
        format[3] = '+';
    return format;
}

int Format::Metric(double value, char *tick_label, int size, void *data)
{
    auto unit = static_cast<const char *>(data);
    auto format = "{:g} {}" + std::string(unit);
    std::snprintf(tick_label, size, "%s", Metric(value, format).c_str());
    return 0;
}
