#ifndef FORMAT_H_QPXLOO
#define FORMAT_H_QPXLOO

#include <string>

namespace Format
{
/* General formatter for metric values. The `format` is expected to have two
   `{}` format groups, the first one will receive the `value` and the second the
   prefix. Remember to add the unit, e.g. `{: 6.2f} {}V`. */
std::string Metric(double value, const std::string &format, double highest_prefix = 1e9);

/* Quality-of-life formatting for common units in the GUI. */
typedef std::string (*Formatter)(double value, bool show_sign);

std::string TimeDomainX(double value, const std::string &format, bool show_sign);
std::string TimeDomainX(double value, bool show_sign = false);

std::string TimeDomainY(double value, const std::string &format, bool show_sign);
std::string TimeDomainY(double value, bool show_sign = false);

std::string FrequencyDomainX(double value, const std::string &format, bool show_sign);
std::string FrequencyDomainX(double value, bool show_sign = false);

std::string FrequencyDomainY(double value, const std::string &format, bool show_sign);
std::string FrequencyDomainY(double value, bool show_sign = false);

std::string FrequencyDomainDeltaY(double value, const std::string &format, bool show_sign);
std::string FrequencyDomainDeltaY(double value, bool show_sign = false);

/* Only user for ImPlot axis formatting. */
int Metric(double value, char *tick_label, int size, void *data);
};

#endif
