#pragma once

#include <string>

namespace Format
{
/* General formatter for metric values. The `format` is expected to have two
   `{}` format groups, the first one will receive the `value` and the second the
   prefix. Remember to add the unit, e.g. `{: 6.2f} {}V`. */
std::string Metric(double value, const std::string &format, double highest_prefix = 1e12,
                   double lowest_prefix = 1e-12);

/* Function to parametrize the construction of the fmt::format string. */
std::string String(const std::string &precision, const std::string &unit, bool show_sign);

/* Only used for ImPlot axis formatting. */
int Metric(double value, char *tick_label, int size, void *data);
};
