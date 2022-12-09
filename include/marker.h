#ifndef MARKER_H_DJMQPF
#define MARKER_H_DJMQPF

#include <set>
#include <cstddef>
#include <string>

#include "imgui.h"
#include "implot.h"

enum class MarkerKind
{
    TIME_DOMAIN,
    FREQUENCY_DOMAIN
};

struct Marker
{
    Marker() = default;
    Marker(size_t id, size_t digitizer, size_t channel, size_t sample, double x, double y);

    size_t id;
    size_t digitizer;
    size_t channel;
    size_t sample;
    ImVec4 color;
    float thickness;
    double x;
    double y;
    std::set<size_t> deltas;
};

#endif
