#include "imgui_extensions.h"

#include "imgui.h"
#include "imgui_internal.h" /* For ItemAdd() for custom items. */
#include "fmt/format.h"
#include "format.h"

#include <cmath>

void ImGui::RenderTableContents(const std::vector<std::vector<TableCell>> &rows)
{
    for (const auto &row : rows)
    {
        ImGui::TableNextRow();
        for (const auto &column : row)
        {
            ImGui::TableNextColumn();
            ImGui::Text(column.contents);
            if (!column.hover.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("%s", column.hover.c_str());
        }
    }
}

ImGui::InputDoubleMetric::InputDoubleMetric(const std::string &label, double value,
                                            const std::string &format, double highest_prefix,
                                            ImGuiInputTextFlags flags)
    : value(value)
    , first(true)
    , str(Format::Metric(value, format, highest_prefix))
    , label(label)
    , format(format)
    , highest_prefix(highest_prefix)
    , flags(flags | DEFAULT_FLAGS)
{}

bool ImGui::InputDoubleMetric::Changed()
{
    bool result = ImGui::InputText(
        label.c_str(), str.data(), str.capacity() + 1, flags, Callback, this
    );

    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        try
        {
            value = std::stod(str.data());
        }
        catch (const std::invalid_argument &)
        {
            return false;
        }
        catch (const std::out_of_range &)
        {
            return false;
        }

        if (std::isnan(value) || std::isinf(value))
            return false;

        str = Format::Metric(value, format, highest_prefix);
        str.reserve(64);

        first = true;
        result = true;
    }

    return result;
}

int ImGui::InputDoubleMetric::Callback(ImGuiInputTextCallbackData *data)
{
    auto obj = static_cast<InputDoubleMetric *>(data->UserData);
    if (obj->first)
    {
        obj->first = false;
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, fmt::format("{:e}", obj->value).c_str());
        data->SelectAll();
    }
    return 0;
}

ImVec4 ImGui::GetTextColor(const ImVec4 &color)
{
    /* See https://stackoverflow.com/a/3943023 */
    if (color.x * 0.299 + color.y * 0.587 + color.z * 0.114 > 0.73)
        return ImVec4{0.0f, 0.0f, 0.0f, 1.0f};
    else
        return ImVec4{1.0f, 1.0f, 1.0f, 1.0f};
}

static void FilledRect(const std::string &label, const ImRect &rectangle, const ImVec4 &color)
{
    /* Add the rectangle. */
    ImGui::GetWindowDrawList()->AddRectFilled(
        rectangle.Min, rectangle.Max, ImGui::GetColorU32(color));

    /* Only add the text (centered) if it's going to fit in the rectangle. */
    auto text_size = ImGui::CalcTextSize(label.c_str());
    if (label.size() > 0 &&
        text_size.x + 2 * ImGui::GetStyle().FramePadding.x < rectangle.GetWidth() &&
        text_size.y < rectangle.GetHeight())
    {
        ImVec2 text_position{
            rectangle.Min.x + (rectangle.GetWidth() - text_size.x) / 2.0f,
            rectangle.Min.y + (rectangle.GetHeight() - text_size.y) / 2.0f};

        ImGui::GetWindowDrawList()->AddText(
            text_position, ImGui::GetColorU32(ImGui::GetTextColor(color)), label.c_str());
    }
}

void ImGui::BarStack(
    const std::string &label, const ImVec2 &size, const std::vector<ImGui::Bar> &bars)
{
    ImVec2 effective_size = ImGui::CalcItemSize(size, 0.0f, 0.0f);
    auto cursor = ImGui::GetCursorScreenPos();
    ImRect bounding_box{cursor, cursor + effective_size};

    /* We call `ItemSize` and `ItemAdd` from imgui_internal.h to make this
       custom rectangle behave as a proper item. */
    ImGui::ItemSize(bounding_box);
    if (!ImGui::ItemAdd(bounding_box, ImGui::GetID(label.c_str())))
        return;

    /* Calculate the sum of all the values so we can calculate size of each
       individual rectangle. */
    auto total = 0.0;
    for (const auto &bar : bars)
        total += bar.value;

    ImRect rectangle{cursor, cursor + ImVec2{0.0f, effective_size.y}};
    for (const auto &bar : bars)
    {
        /* Move the right corner, draw the rectangle then move the left corner. */
        rectangle.Max.x += static_cast<float>(effective_size.x * bar.value / total);
        FilledRect(bar.label, rectangle, bar.color);
        rectangle.Min.x = rectangle.Max.x;
    }
}
