#include "window.h"
#include "error.h"

#include <cmath>

Window::Window(WindowType type, size_t length)
    : type(type)
    , length(length)
{
    data = std::unique_ptr<double[]>( new double[length] );
}

WindowCache::WindowCache()
    : hamming_windows{}
    , blackman_harris_windows{}
    , hanning_windows{}
{}

std::shared_ptr<Window> WindowCache::GetWindow(WindowType type, size_t length)
{
    switch (type)
    {
    case WindowType::NONE:
        return NULL;

    case WindowType::HAMMING:
        return GetWindow(hamming_windows, length, Hamming);

    case WindowType::BLACKMAN_HARRIS:
        return GetWindow(blackman_harris_windows, length, BlackmanHarris);

    case WindowType::HANNING:
        return GetWindow(hanning_windows, length, Hanning);
    }

    return NULL;
}

std::shared_ptr<Window> WindowCache::GetWindow(std::map<size_t, std::shared_ptr<Window>> &windows,
                                               size_t length,
                                               std::function<double(size_t, size_t)> f)
{
    auto search = windows.find(length);
    if (search != windows.end())
        return search->second;

    double mean = 0.0;
    auto window = std::make_shared<Window>(WindowType::HAMMING, length);
    for (size_t i = 0; i < length; ++i)
    {
        double value = f(i, length);
        window->data[i] = value;
        mean += value;
    }
    mean /= static_cast<double>(length);

    for (size_t i = 0; i < length; ++i)
        window->data[i] /= mean;

    windows.insert({length, window});
    return window;
}

double WindowCache::Hamming(size_t i, size_t length)
{
    return (25.0 / 46.0) * (1.0 - std::cos(2.0 * M_PI * i / length));
}

double WindowCache::BlackmanHarris(size_t i, size_t length)
{
    return 0.35875 - 0.48829 * std::cos(2.0 * M_PI * i / length)
                   + 0.14128 * std::cos(4.0 * M_PI * i / length)
                   - 0.01168 * std::cos(6.0 * M_PI * i / length);
}

double WindowCache::Hanning(size_t i, size_t length)
{
    return 0.5 * (1.0 - std::cos(2.0 * M_PI * i / length));
}
