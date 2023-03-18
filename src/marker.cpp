#include "marker.h"

Marker::Marker(size_t id, size_t digitizer, size_t channel, size_t sample, const Value &x,
               const Value &y)
    : id(id)
    , digitizer(digitizer)
    , channel(channel)
    , sample(sample)
    , color(ImPlot::GetColormapColor(static_cast<int>(id), ImPlotColormap_Pastel))
    , thickness(1.0f)
    , x(x)
    , y(y)
    , deltas{}
{
}

Markers::Markers(const std::string &label, const std::string &prefix)
    : label(label)
    , prefix(prefix)
    , is_adding(false)
    , is_dragging(false)
    , next_id(0)
    , markers{}
    , last_marker(markers.end())
{
}

void Markers::clear()
{
    markers.clear();
    last_marker = markers.end();
    next_id = 0;
}

bool Markers::empty() const
{
    return markers.empty();
}

void Markers::insert(size_t digitizer, size_t channel, size_t sample, const Value &x,
                     const Value &y, bool add_delta_to_last)
{
    if (add_delta_to_last)
        last_marker->second.deltas.insert(next_id);

    auto [it, success] =
        markers.insert({next_id, Marker(next_id, digitizer, channel, sample, x, y)});

    ++next_id;
    last_marker = it;
}
