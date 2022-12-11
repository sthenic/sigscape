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

std::string Format::TimeDomainX(double value, const std::string &format, bool show_sign)
{
    std::string lformat = "{:>-" + format + "f} {}s";
    if (show_sign)
        lformat[3] = '+';
    return Metric(value, lformat, 1e-3);
}

std::string Format::TimeDomainX(double value, bool show_sign)
{
    return TimeDomainX(value, "7.2", show_sign);
}

std::string Format::TimeDomainY(double value, const std::string &format, bool show_sign)
{
    std::string lformat = "{:>-" + format + "f} {}V";
    if (show_sign)
        lformat[3] = '+';
    return Metric(value, lformat, 1e-3);
}

std::string Format::TimeDomainY(double value, bool show_sign)
{
    return TimeDomainY(value, "8.2", show_sign);
}

std::string Format::FrequencyDomainX(double value, const std::string &format, bool show_sign)
{
    std::string lformat = "{:>-" + format + "f} {}Hz";
    if (show_sign)
        lformat[3] = '+';
    return Metric(value, lformat, 1e6);
}


std::string Format::FrequencyDomainX(double value, bool show_sign)
{
    return FrequencyDomainX(value, "7.2", show_sign);
}

std::string Format::FrequencyDomainY(double value, const std::string &format, bool show_sign)
{
    std::string lformat = "{:>-" + format + "f} dBFS";
    if (show_sign)
        lformat[3] = '+';
    return fmt::format(lformat, value);
}

std::string Format::FrequencyDomainY(double value, bool show_sign)
{
    return FrequencyDomainY(value, "7.2", show_sign);
}

std::string Format::FrequencyDomainDeltaY(double value, const std::string &format, bool show_sign)
{
    std::string lformat = "{:>-" + format + "f} dB";
    if (show_sign)
        lformat[3] = '+';
    return fmt::format(lformat, value);
}

std::string Format::FrequencyDomainDeltaY(double value, bool show_sign)
{
    return FrequencyDomainDeltaY(value, "7.2", show_sign);
}

int Format::Metric(double value, char *tick_label, int size, void *data)
{
    auto unit = static_cast<const char *>(data);
    auto format = "{:g} {}" + std::string(unit);
    std::snprintf(tick_label, size, "%s", Metric(value, format).c_str());
    return 0;
}
