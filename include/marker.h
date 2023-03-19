#ifndef MARKER_H_DJMQPF
#define MARKER_H_DJMQPF

#include "data_types.h"
#include "imgui.h"
#include "implot.h"

#include <set>
#include <cstddef>
#include <string>
#include <map>

enum class MarkerKind
{
    TIME_DOMAIN,
    FREQUENCY_DOMAIN
};

struct Marker
{
    Marker() = default;
    Marker(size_t id, size_t digitizer, size_t channel, size_t sample, const Value &x,
           const Value &y);

    size_t id;
    size_t digitizer;
    size_t channel;
    size_t sample;
    ImVec4 color;
    float thickness;
    Value x;
    Value y;
    std::set<size_t> deltas;
};

/* This struct represents a set of markers, encapsulating an std::map for the
   markers themselves, along with the logic needed to simplify adding and
   recalling them. This type should be seen as an extension of an std::map,
   hence the function naming style. */
struct Markers
{
    Markers(const std::string &label, const std::string &prefix);

    void insert(size_t digitizer, size_t channel, size_t sample, const Value &x, const Value &y,
                bool add_delta_to_last = false);

    void clear();
    bool empty() const;
    Marker &last() { return last_marker->second; }

    /* Expose the iterator interface to the wrapped std::map for convenience. */
    using iterator = std::map<size_t, Marker>::iterator;
    using const_iterator = std::map<size_t, Marker>::const_iterator;

    iterator begin() { return markers.begin(); }
    const_iterator begin() const { return markers.begin(); }

    iterator end() { return markers.end(); }
    const_iterator end() const { return markers.end(); }

    iterator erase(iterator pos) { return markers.erase(pos); }
    iterator erase(const_iterator pos) { return markers.erase(pos); }
    size_t erase (size_t key) { return markers.erase(key); }

    Marker &at(size_t key) { return markers.at(key); }
    const Marker &at(size_t key) const { return markers.at(key); }

    std::string label;
    std::string prefix;
    bool is_adding;
    bool is_dragging;
    size_t next_id;
    std::map<size_t, Marker> markers;
    std::map<size_t, Marker>::iterator last_marker;
};


#endif
