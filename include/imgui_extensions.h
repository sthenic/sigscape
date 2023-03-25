#pragma once

#include "imgui.h"
#include <string>

/* ImGui extensions to remove conversion clutter when calling certain functions. */
namespace ImGui
{
inline void Text(const std::string &str)
{
    ImGui::Text("%s", str.c_str());
}

struct InputDoubleMetric
{
public:
    InputDoubleMetric(const std::string &label, double value, const std::string &format,
                      double highest_prefix, ImGuiInputTextFlags flags = 0);

    /* We use Changed() to emulate the behavior of calling ImGui::InputDouble(). */
    bool Changed();

    InputDoubleMetric(InputDoubleMetric &other) = delete;
    InputDoubleMetric operator=(InputDoubleMetric &other) = delete;

    double value;

private:
    static const ImGuiInputTextFlags DEFAULT_FLAGS = ImGuiInputTextFlags_CharsScientific |
                                                     ImGuiInputTextFlags_EnterReturnsTrue |
                                                     ImGuiInputTextFlags_CallbackAlways;

    bool first;
    std::string str;
    std::string label;
    std::string format;
    double highest_prefix;
    ImGuiInputTextFlags flags;

    static int Callback(ImGuiInputTextCallbackData *data);
};

// bool InputDoubleMetric(const char *label, std::string &str, ImGuiInputTextFlags flags = 0);

}