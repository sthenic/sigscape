#include "marker.h"

Marker::Marker(size_t id, size_t digitizer, size_t channel, size_t sample, double x, double y)
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
