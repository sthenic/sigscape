#include "window.h"
#include "error.h"

#include <cmath>

Window::Window(size_t length)
    : data(length)
    , length(length)
    , amplitude_factor(1.0)
    , energy_factor(1.0)
{}

WindowCache::WindowCache()
    : hamming_windows{}
    , blackman_harris_windows{}
    , hanning_windows{}
    , flat_top_windows{}
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

    case WindowType::FLAT_TOP:
        return GetWindow(flat_top_windows, length, FlatTop);

    default:
        break;
    }

    return NULL;
}

std::shared_ptr<Window> WindowCache::GetWindow(std::map<size_t, std::shared_ptr<Window>> &windows,
                                               size_t length,
                                               std::function<double(size_t, size_t)> f)
{
    /* First we try to find a window of matching length in the cache. This will
       almost always return a hit (which is obviously the point of the cache). */
    auto search = windows.find(length);
    if (search != windows.end())
        return search->second;

    /* Create a new window with the target length. */
    auto window = std::make_shared<Window>(length);
    /* Calculated as the mean. */
    window->amplitude_factor = 0.0;
    /* Calculated as the RMS. */
    window->energy_factor = 0.0;

    for (size_t i = 0; i < length; ++i)
    {
        double value = f(i, length);
        window->data[i] = value;
        window->amplitude_factor += value;
        window->energy_factor += value * value;
    }

    /* Calculate the factor intended for scaling by multiplication by this value squared. */
    window->amplitude_factor = std::pow(static_cast<double>(length) / window->amplitude_factor, 2.0);
    window->energy_factor = static_cast<double>(length) / window->energy_factor;

    /* This conversion factor exists to handle conversion from an
       amplitude-accurate windowed FFT to an energy-accurate windowed FFT. Since
       the scaling is carried out by multiplying by the amplitude factor above,
       this operation that out and instead scales by the energy factor. */
    window->amplitude_to_energy = window->energy_factor / window->amplitude_factor;

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

double WindowCache::FlatTop(size_t i, size_t length)
{
    return 0.21557895 - 0.416631580 * std::cos(2.0 * M_PI * i / length)
                      + 0.277263158 * std::cos(4.0 * M_PI * i / length)
                      - 0.083578947 * std::cos(6.0 * M_PI * i / length)
                      + 0.006947368 * std::cos(8.0 * M_PI * i / length);
}
