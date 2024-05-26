#pragma once

#include "imgui.h"
#include <string>
#include <vector>

/* ImGui extensions to remove conversion clutter when calling certain functions. */
namespace ImGui
{
inline void Text(const std::string &str)
{
    ImGui::Text("%s", str.c_str());
}

struct TableCell
{
    /* We need this constructor for less verbose list initialization syntax (can
       skip one pair of braces). */
    TableCell(const char *contents)
        : contents{contents}
        , hover{}
    {}

    TableCell(const std::string &contents, const std::string &hover = "")
        : contents{contents}
        , hover{hover}
    {}

    std::string contents;
    std::string hover;
};

void RenderTableContents(const std::vector<std::vector<TableCell>> &rows);

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
}

static inline bool operator==(const ImVec2 &lhs, const ImVec2 &rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }
static inline bool operator==(const ImVec4 &lhs, const ImVec4 &rhs) { return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w; }
