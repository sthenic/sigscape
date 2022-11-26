#ifndef WINDOW_H_DT9K5B
#define WINDOW_H_DT9K5B

#include <map>
#include <memory>
#include <functional>

enum class WindowType
{
    NONE,
    HAMMING,
    BLACKMAN_HARRIS,
    HANNING
};

struct Window
{
    Window() = delete;
    Window(WindowType type, size_t length);

    WindowType type;
    std::vector<double> data;
    size_t length;
};

class WindowCache
{
public:
    WindowCache();
    std::shared_ptr<Window> GetWindow(WindowType type, size_t length);

private:
    std::shared_ptr<Window> GetWindow(std::map<size_t, std::shared_ptr<Window>> &windows,
                                      size_t length, std::function<double(size_t, size_t)> f);

    static double Hamming(size_t i, size_t length);
    static double BlackmanHarris(size_t i, size_t length);
    static double Hanning(size_t i, size_t length);

    std::map<size_t, std::shared_ptr<Window>> hamming_windows;
    std::map<size_t, std::shared_ptr<Window>> blackman_harris_windows;
    std::map<size_t, std::shared_ptr<Window>> hanning_windows;
};

#endif
