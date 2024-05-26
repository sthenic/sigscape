#include "imgui_extensions.h"

#include "imgui.h"
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
