#include "ui.h"
#include "implot_internal.h" /* To be able to get item visibility state. */

#include "fmt/format.h"
#include "format.h"
#include <cinttypes>
#include <cmath>

const ImVec4 Ui::COLOR_GREEN = {0.0f, 1.0f, 0.5f, 0.6f};
const ImVec4 Ui::COLOR_RED = {1.0f, 0.0f, 0.2f, 0.6f};
const ImVec4 Ui::COLOR_YELLOW = {1.0f, 1.0f, 0.3f, 0.8f};
const ImVec4 Ui::COLOR_ORANGE = {0.86f, 0.38f, 0.1f, 0.8f};
const ImVec4 Ui::COLOR_PURPLE = {0.6f, 0.3f, 1.0f, 0.8f};

const ImVec4 Ui::COLOR_WOW_RED = {0.77f, 0.12f, 0.23f, 0.8f};
const ImVec4 Ui::COLOR_WOW_DARK_MAGENTA = {0.64f, 0.19f, 0.79f, 0.8f};
const ImVec4 Ui::COLOR_WOW_ORANGE = {1.00f, 0.49f, 0.04f, 0.8f};
const ImVec4 Ui::COLOR_WOW_CHROMOPHOBIA_GREEN = {0.20f, 0.58f, 0.50f, 0.8f};
const ImVec4 Ui::COLOR_WOW_GREEN = {0.67f, 0.83f, 0.45f, 0.8f};
const ImVec4 Ui::COLOR_WOW_LIGHT_BLUE = {0.25f, 0.78f, 0.92f, 0.8f};
const ImVec4 Ui::COLOR_WOW_SPRING_GREEN = {0.00f, 1.00f, 0.60f, 0.8f};
const ImVec4 Ui::COLOR_WOW_PINK = {0.96f, 0.55f, 0.73f, 0.8f};
const ImVec4 Ui::COLOR_WOW_WHITE = {1.00f, 1.00f, 1.00f, 0.8f};
const ImVec4 Ui::COLOR_WOW_YELLOW = {1.00f, 0.96f, 0.41f, 0.8f};
const ImVec4 Ui::COLOR_WOW_BLUE = {0.00f, 0.44f, 0.87f, 0.8f};
const ImVec4 Ui::COLOR_WOW_PURPLE = {0.53f, 0.53f, 0.93f, 0.8f};
const ImVec4 Ui::COLOR_WOW_TAN  = {0.78f, 0.61f, 0.43f, 0.8f};

/* ImGui extensions to remove conversion clutter when calling certain functions. */
namespace ImGui
{
static inline void Text(const std::string &str)
{
    ImGui::Text("%s", str.c_str());
}
}

Ui::ChannelUiState::ChannelUiState(int &nof_channels_total)
    : color{}
    , is_selected(false)
    , is_sample_markers_enabled(true)
    , is_persistence_enabled(false)
    , is_time_domain_visible(true)
    , is_frequency_domain_visible(true)
    , record(NULL)
{
    if (nof_channels_total == 0)
        is_selected = true;

    color = ImPlot::GetColormapColor(nof_channels_total++);
}

Ui::DigitizerUiState::DigitizerUiState()
    : identifier("")
    , state("")
    , event("")
    , state_color(COLOR_GREEN)
    , event_color(COLOR_GREEN)
    , set_top_color(ImGui::GetStyleColorVec4(ImGuiCol_Button))
    , set_clock_system_color(ImGui::GetStyleColorVec4(ImGuiCol_Button))
    , popup_initialize_would_overwrite(false)
    , is_selected(false)
    , channels{}
{
}

Ui::Ui()
    : m_identification()
    , m_adq_control_unit()
    , m_show_imgui_demo_window(false)
    , m_show_implot_demo_window(false)
    , m_is_time_domain_collapsed(false)
    , m_is_frequency_domain_collapsed(false)
    , m_nof_channels_total(0)
    , m_digitizers()
    , m_frequency_domain_markers{}
    , m_is_dragging_frequency_domain_marker(false)
    , m_is_adding_frequency_domain_marker(false)
    , m_next_frequency_domain_marker_id(0)
    , m_time_domain_markers{}
    , m_is_dragging_time_domain_marker(false)
    , m_is_adding_time_domain_marker(false)
    , m_next_time_domain_marker_id(0)
{
    m_last_frequency_domain_marker = m_frequency_domain_markers.end();
    m_last_time_domain_marker = m_time_domain_markers.end();
}

Ui::~Ui()
{
}

void Ui::Initialize(GLFWwindow *window, const char *glsl_version)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    ImGui::StyleColorsDark();

    /* Start device identification thread. */
    m_identification.Start();
}

void Ui::Terminate()
{
    for (const auto &d : m_digitizers)
        d.interface->Stop();

    if (m_adq_control_unit != NULL)
        DeleteADQControlUnit(m_adq_control_unit);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void Ui::Render(float width, float height)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    HandleMessages();
    UpdateRecords();
    RenderMenuBar();
    RenderLeft(width, height);
    RenderCenter(width, height);
    RenderRight(width, height);
    RenderPopups();

    if (m_show_imgui_demo_window)
        ImGui::ShowDemoWindow();

    if (m_show_implot_demo_window)
        ImPlot::ShowDemoWindow();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Ui::ClearChannelSelection()
{
    for (auto &d : m_digitizers)
    {
        for (auto &chui : d.ui.channels)
            chui.is_selected = false;
    }
}

void Ui::PushMessage(const DigitizerMessage &message, bool selected)
{
    for (auto &digitizer : m_digitizers)
    {
        if (selected && !digitizer.ui.is_selected)
            continue;
        digitizer.interface->PushMessage(message);
    }
}

void Ui::UpdateRecords()
{
    /* FIXME: Skip unselected digitizers? We need a different queue then that
              pops entries at the end once saturated, regardless if they've been
              read or not. */

    /* Attempt to update the set of processed records for each digitizer. */
    for (auto &digitizer : m_digitizers)
    {
        for (size_t ch = 0; ch < digitizer.ui.channels.size(); ++ch)
            digitizer.interface->WaitForProcessedRecord(ch, digitizer.ui.channels[ch].record);
    }
}

void Ui::HandleMessage(const IdentificationMessage &message)
{
    /* If we get a message, the identification went well. */
    for (const auto &d : m_digitizers)
        d.interface->Stop();

    m_digitizers.clear();
    for (auto interface : message.digitizers)
        m_digitizers.emplace_back(interface);

    m_adq_control_unit = message.handle;

    for (const auto &d : m_digitizers)
        d.interface->Start();

    /* FIXME: Remove since debug convenience */
    m_digitizers.front().ui.is_selected = true;
}

void Ui::HandleMessage(DigitizerUi &digitizer, const DigitizerMessage &message)
{
    switch (message.id)
    {
    case DigitizerMessageId::IDENTIFIER:
        digitizer.ui.identifier = *message.str;
        break;

    case DigitizerMessageId::NOF_CHANNELS:
        digitizer.ui.channels.clear();
        for (int ch = 0; ch < message.ivalue; ++ch)
            digitizer.ui.channels.emplace_back(m_nof_channels_total);
        break;

    case DigitizerMessageId::CONFIGURATION:
        digitizer.ui.event = "CONFIGURATION";
        digitizer.ui.event_color = COLOR_WOW_TAN;
        break;

    case DigitizerMessageId::CLEAR:
        digitizer.ui.event = "";
        break;

    case DigitizerMessageId::ERR:
        digitizer.ui.event = "ERROR";
        digitizer.ui.event_color = COLOR_RED;
        break;

    case DigitizerMessageId::STATE:
        switch (message.state)
        {
        case DigitizerState::NOT_ENUMERATED:
            digitizer.ui.state = "NOT ENUMERATED";
            digitizer.ui.state_color = COLOR_RED;
            break;
        case DigitizerState::ENUMERATION:
            digitizer.ui.state = "ENUMERATION";
            digitizer.ui.state_color = COLOR_PURPLE;
            break;
        case DigitizerState::IDLE:
            digitizer.ui.state = "IDLE";
            digitizer.ui.state_color = COLOR_GREEN;
            break;
        case DigitizerState::ACQUISITION:
            digitizer.ui.state = "ACQUISITION";
            digitizer.ui.state_color = COLOR_ORANGE;
            break;
        }
        break;

    case DigitizerMessageId::DIRTY_TOP_PARAMETERS:
        digitizer.ui.set_top_color = COLOR_ORANGE;
        /* FIXME: Remove since debug convenience */
        digitizer.interface->PushMessage(DigitizerMessageId::SET_PARAMETERS);
        break;

    case DigitizerMessageId::DIRTY_CLOCK_SYSTEM_PARAMETERS:
        digitizer.ui.set_clock_system_color = COLOR_ORANGE;
        break;

    case DigitizerMessageId::CLEAN_TOP_PARAMETERS:
        digitizer.ui.set_top_color = COLOR_GREEN;
        break;

    case DigitizerMessageId::CLEAN_CLOCK_SYSTEM_PARAMETERS:
        digitizer.ui.set_clock_system_color = COLOR_GREEN;
        break;

    case DigitizerMessageId::INITIALIZE_WOULD_OVERWRITE:
        digitizer.ui.popup_initialize_would_overwrite = true;
        ImGui::OpenPopup("InitializeWouldOverwrite");
        break;

    default:
        /* These are not expected as a message from a digitizer thread. */
        break;
    }
}

void Ui::HandleMessages()
{
    IdentificationMessage identification_message;
    if (ADQR_EOK == m_identification.WaitForMessage(identification_message, 0))
        HandleMessage(identification_message);

    for (auto &digitizer : m_digitizers)
    {
        DigitizerMessage digitizer_message;
        if (ADQR_EOK == digitizer.interface->WaitForMessage(digitizer_message, 0))
            HandleMessage(digitizer, digitizer_message);
    }
}

void Ui::RenderMenuBar()
{
    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("Demo"))
    {
        ImGui::MenuItem("ImGui", NULL, &m_show_imgui_demo_window);
        ImGui::MenuItem("ImPlot", NULL, &m_show_implot_demo_window);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Style"))
    {
        static int style_idx = 0;
        if (ImGui::Combo("##combostyle", &style_idx, "Dark\0Light\0"))
        {
            switch (style_idx)
            {
            case 0:
                ImGui::StyleColorsDark();
                break;
            case 1:
                ImGui::StyleColorsLight();
                break;
            }
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void Ui::RenderRight(float width, float height)
{
    const float FRAME_HEIGHT = ImGui::GetFrameHeight();
    const float PLOT_WINDOW_HEIGHT = (height - 1 * FRAME_HEIGHT) / 2;
    const ImVec2 POSITION_UPPER(
        width * (FIRST_COLUMN_RELATIVE_WIDTH + SECOND_COLUMN_RELATIVE_WIDTH),
        FRAME_HEIGHT
    );
    const ImVec2 POSITION_LOWER(
        width * (FIRST_COLUMN_RELATIVE_WIDTH + SECOND_COLUMN_RELATIVE_WIDTH),
        FRAME_HEIGHT + PLOT_WINDOW_HEIGHT
    );
    const ImVec2 SIZE(width * SECOND_COLUMN_RELATIVE_WIDTH, PLOT_WINDOW_HEIGHT);

    /* In the right column, we show various metrics. */
    RenderTimeDomainMetrics(POSITION_UPPER, SIZE);
    RenderFrequencyDomainMetrics(POSITION_LOWER, SIZE);
}

void Ui::RenderCenter(float width, float height)
{
    /* We show two plots in the center, taking up equal vertical space. */
    const float FRAME_HEIGHT = ImGui::GetFrameHeight();
    const float PLOT_WINDOW_HEIGHT = (height - 1 * FRAME_HEIGHT) / 2;

    ImVec2 TIME_DOMAIN_POSITION{width * FIRST_COLUMN_RELATIVE_WIDTH, FRAME_HEIGHT};
    ImVec2 TIME_DOMAIN_SIZE{width * SECOND_COLUMN_RELATIVE_WIDTH, PLOT_WINDOW_HEIGHT};
    ImVec2 FREQUENCY_DOMAIN_POSITION{width * FIRST_COLUMN_RELATIVE_WIDTH, FRAME_HEIGHT + PLOT_WINDOW_HEIGHT};
    ImVec2 FREQUENCY_DOMAIN_SIZE{width * SECOND_COLUMN_RELATIVE_WIDTH, PLOT_WINDOW_HEIGHT};

    if (m_is_time_domain_collapsed)
    {
        FREQUENCY_DOMAIN_POSITION = ImVec2{width * FIRST_COLUMN_RELATIVE_WIDTH, 2 * FRAME_HEIGHT};
        FREQUENCY_DOMAIN_SIZE = ImVec2{width * SECOND_COLUMN_RELATIVE_WIDTH, height - 2 * FRAME_HEIGHT};
    }

    if (m_is_frequency_domain_collapsed)
        TIME_DOMAIN_SIZE = ImVec2{width * SECOND_COLUMN_RELATIVE_WIDTH, height - 2 * FRAME_HEIGHT};

    if (m_is_frequency_domain_collapsed && !m_is_time_domain_collapsed)
        FREQUENCY_DOMAIN_POSITION = ImVec2{width * FIRST_COLUMN_RELATIVE_WIDTH, height - FRAME_HEIGHT};

    /* The lower plot window, showing time domain data. */
    RenderTimeDomain(TIME_DOMAIN_POSITION, TIME_DOMAIN_SIZE);

    /* The lower plot window, showing frequency domain data. */
    RenderFrequencyDomain(FREQUENCY_DOMAIN_POSITION, FREQUENCY_DOMAIN_SIZE);
}

void Ui::RenderLeft(float width, float height)
{
    const float FRAME_HEIGHT = ImGui::GetFrameHeight();
    const ImVec2 DIGITIZER_SELECTION_POS(0.0f, FRAME_HEIGHT);
    const ImVec2 DIGITIZER_SELECTION_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 200.0f);
    RenderDigitizerSelection(DIGITIZER_SELECTION_POS, DIGITIZER_SELECTION_SIZE);

    const ImVec2 COMMAND_PALETTE_POS(0.0f, DIGITIZER_SELECTION_POS.y + DIGITIZER_SELECTION_SIZE.y);
    const ImVec2 COMMAND_PALETTE_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 300.0f);
    RenderCommandPalette(COMMAND_PALETTE_POS, COMMAND_PALETTE_SIZE);

    /* Measured from the bottom */
    const ImVec2 METRICS_POS(0.0f, height - 3 * FRAME_HEIGHT);
    const ImVec2 METRICS_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 3 * FRAME_HEIGHT);
    RenderApplicationMetrics(METRICS_POS, METRICS_SIZE);

    const ImVec2 PROCESSING_OPTIONS_POS(0.0f, METRICS_POS.y - 200.0f);
    const ImVec2 PROCESSING_OPTIONS_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 200.0f);
    RenderProcessingOptions(PROCESSING_OPTIONS_POS, PROCESSING_OPTIONS_SIZE);

    /* The marker window is the one we allow to stretch stretchable. */
    const ImVec2 MARKERS_POS(0.0f, COMMAND_PALETTE_POS.y + COMMAND_PALETTE_SIZE.y);
    const ImVec2 MARKERS_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH,
                              height - (FRAME_HEIGHT + DIGITIZER_SELECTION_SIZE.y +
                                        COMMAND_PALETTE_SIZE.y + PROCESSING_OPTIONS_SIZE.y +
                                        METRICS_SIZE.y));
    RenderMarkers(MARKERS_POS, MARKERS_SIZE);

}

void Ui::RenderPopups()
{
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (m_digitizers[i].ui.popup_initialize_would_overwrite)
            RenderPopupInitializeWouldOverwrite(i);
    }
}

void Ui::RenderPopupInitializeWouldOverwrite(size_t idx)
{
    ImGui::OpenPopup("##initializewouldoverwrite");
    if (ImGui::BeginPopupModal("##initializewouldoverwrite", NULL,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Initializing parameters for %s would\n"
                    "overwrite (reset) the existing parameters.\n\n"
                    "Proceed?\n\n",
                    m_digitizers[idx].ui.identifier.c_str());
        ImGui::Separator();

        if (ImGui::Button("Yes"))
        {
            m_digitizers[idx].interface->PushMessage(DigitizerMessageId::INITIALIZE_PARAMETERS_FORCE);
            m_digitizers[idx].ui.popup_initialize_would_overwrite = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("No"))
        {
            m_digitizers[idx].ui.popup_initialize_would_overwrite = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void Ui::RenderDigitizerSelection(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Digitizers", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (ImGui::Button("Select All"))
    {
        for (auto &digitizer : m_digitizers)
            digitizer.ui.is_selected = true;
    }
    ImGui::SameLine();

    if (ImGui::Button("Deselect All"))
    {
        for (auto &digitizer : m_digitizers)
            digitizer.ui.is_selected = false;
    }
    ImGui::Separator();

    if (m_digitizers.size() == 0)
    {
        ImGui::Text("No digitizers found.");
    }
    else
    {
        ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings;
        if (ImGui::BeginTable("Digitizers", 3, flags))
        {
            const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
            ImGui::TableSetupColumn("Identifier",
                                    ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide,
                                    TEXT_BASE_WIDTH * 14.0f);
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed,
                                    TEXT_BASE_WIDTH * 14.0f);
            ImGui::TableSetupColumn("Event");
            ImGui::TableHeadersRow();

            for (auto &digitizer : m_digitizers)
            {
                ImGui::TableNextColumn();
                if (ImGui::Selectable(digitizer.ui.identifier.c_str(), digitizer.ui.is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns))
                {
                    /* Clear all if CTRL is not held during the press. */
                    if (!ImGui::GetIO().KeyCtrl)
                    {
                        for (auto &d : m_digitizers)
                            d.ui.is_selected = false;
                    }

                    /* Toggle the selection state. */
                    digitizer.ui.is_selected ^= true;
                }

                ImGui::TableNextColumn();
                if (digitizer.ui.state.size() > 0)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, digitizer.ui.state_color);
                    ImGui::SmallButton(digitizer.ui.state.c_str());
                    ImGui::PopStyleColor();
                }

                ImGui::TableNextColumn();
                if (digitizer.ui.event.size() > 0)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, digitizer.ui.event_color);
                    ImGui::SmallButton(digitizer.ui.event.c_str());
                    ImGui::PopStyleColor();
                }
                ImGui::TableNextRow();
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void Ui::RenderCommandPalette(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Command Palette", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    /* FIXME: Maybe this is overkill. */
    std::stringstream ss;
    bool any_selected = false;
    for (const auto &digitizer : m_digitizers)
    {
        if (digitizer.ui.is_selected)
        {
            if (!any_selected)
                ss << "Commands will be applied to ";
            else
                ss << ", ";

            ss << digitizer.ui.identifier;
            any_selected = true;
        }
    }

    if (!any_selected)
        ss << "No digitizer selected.";

    ImGui::Text(ss.str());

    const ImVec2 COMMAND_PALETTE_BUTTON_SIZE{85, 50};
    if (!any_selected)
        ImGui::BeginDisabled();

    /* First row */
    if (ImGui::Button("Start", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::START_ACQUISITION);

    ImGui::SameLine();
    if (ImGui::Button("Stop", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::STOP_ACQUISITION);

    ImGui::SameLine();
    RenderSetTopParametersButton(COMMAND_PALETTE_BUTTON_SIZE);

    ImGui::SameLine();
    if (ImGui::Button("Get", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::GET_PARAMETERS);

    /* Second row */
    ImGui::BeginDisabled();
    ImGui::Button("Set\nSelection", COMMAND_PALETTE_BUTTON_SIZE);
    ImGui::EndDisabled();

    ImGui::SameLine();
    RenderSetClockSystemParametersButton(COMMAND_PALETTE_BUTTON_SIZE);

    ImGui::SameLine();
    if (ImGui::Button("Initialize", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::INITIALIZE_PARAMETERS);

    ImGui::SameLine();
    if (ImGui::Button("Validate", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::VALIDATE_PARAMETERS);

    if (ImGui::Button("64k\n5Hz", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::DEFAULT_ACQUISITION);

    ImGui::SameLine();
    if (ImGui::Button("Internal\nReference", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::SET_INTERNAL_REFERENCE);

    ImGui::SameLine();
    if (ImGui::Button("External\nReference", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::SET_EXTERNAL_REFERENCE);

    ImGui::SameLine();
    if (ImGui::Button("External\nClock", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::SET_EXTERNAL_CLOCK);

    if (!any_selected)
        ImGui::EndDisabled();

    ImGui::End();
}

void Ui::RenderSetTopParametersButton(const ImVec2 &size)
{
    ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Button);
    for (auto &digitizer : m_digitizers)
    {
        if (!digitizer.ui.is_selected)
            continue;

        /* FIXME: Figure out color when multiple units are selected. Perhaps we
                  need to store the state... */
        color = digitizer.ui.set_top_color;
        break;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    if (ImGui::Button("Set", size))
        PushMessage(DigitizerMessageId::SET_PARAMETERS);
    ImGui::PopStyleColor(2);
}

void Ui::RenderSetClockSystemParametersButton(const ImVec2 &size)
{
    ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Button);
    for (auto &digitizer : m_digitizers)
    {
        if (!digitizer.ui.is_selected)
            continue;

        /* FIXME: Figure out color when multiple units are selected. Perhaps we
                  need to store the state... */
        color = digitizer.ui.set_clock_system_color;
        break;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    if (ImGui::Button("Set Clock\nSystem", size))
        PushMessage(DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS);
    ImGui::PopStyleColor(2);
}

/* FIXME: Remove if no longer needed. */
void CenteredTextInCell(const std::string &str)
{
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         (ImGui::GetColumnWidth() - ImGui::CalcTextSize(str.c_str()).x) / 2);
    ImGui::Text(str);
}

void Ui::MarkerTree(std::map<size_t, Marker> &markers, const std::string &label,
                    const std::string &prefix, Formatter FormatX, Formatter FormatY)
{
    if (markers.empty())
        return;

    ImGui::Spacing();
    ImGui::Separator();
    if (!ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        return;
    ImGui::Spacing();

    /* We don't need a vector to store multiple removal indexes since the user
       will only have one context men u up at a time and this loop runs a
       magnitude faster than the fastest clicking in GUI can achieve.  */
    int id_to_remove = -1;
    bool add_separator = false;

    for (auto &[id, marker] : markers)
    {
        const auto &ui = m_digitizers[marker.digitizer].ui.channels[marker.channel];

        if (add_separator)
            ImGui::Separator();
        add_separator = true;

        ImGui::ColorEdit4(fmt::format("##color{}{}", label, id).c_str(), (float *)&marker.color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        ImGui::SameLine();

        int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                    ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        bool node_open = ImGui::TreeNodeEx(fmt::format("##node{}", marker.id, label).c_str(), flags);

        /* TODO: Maybe change the color instead. */
        if (ImGui::IsItemHovered())
            marker.thickness = 3.0f;
        else
            marker.thickness = 1.0f; /* FIXME: Restore 'unhovered' thickness instead. */

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Remove"))
                id_to_remove = static_cast<int>(id);

            if (ImGui::MenuItem("Clear deltas", NULL, false, !marker.deltas.empty()))
                marker.deltas.clear();

            ImGui::EndPopup();
        }

        const std::string payload_identifier = fmt::format("MARKER_REFERENCE{}", label);
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload(payload_identifier.c_str(), &id, sizeof(id));
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            /* FIXME: Not a oneliner */
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(payload_identifier.c_str()))
            {
                size_t payload_id = *static_cast<const size_t *>(payload->Data);
                marker.deltas.insert(payload_id);
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        const float LINE_START = ImGui::GetCursorPosX();
        ImGui::Text(fmt::format("{}{} {}, {}", prefix, id, FormatX(marker.x, false),
                                FormatY(marker.y, false)));

        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 30.0f -
                        ImGui::CalcTextSize(ui.record->label.c_str()).x);
        ImGui::Text(ui.record->label);
        ImGui::SameLine();
        ImGui::ColorEdit4(fmt::format("##channel{}{}", label, id).c_str(), (float *)&ui.color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                              ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker);

        if (node_open)
        {
            if (!marker.deltas.empty())
            {
                const auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings |
                                   ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX;
                if (ImGui::BeginTable(fmt::format("##table{}{}", label, id).c_str(), 2, flags))
                {
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

                    for (auto id : marker.deltas)
                    {
                        const auto &delta_marker = markers.at(id);
                        const double delta_x = delta_marker.x - marker.x;
                        const double delta_y = delta_marker.y - marker.y;

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::SetCursorPosX(LINE_START - ImGui::CalcTextSize("d").x);
                        ImGui::Text(fmt::format("d{}{}", prefix, delta_marker.id));
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text(fmt::format(" {}, {}", FormatX(delta_x, true),
                                                FormatY(delta_y, true)));
                    }

                    ImGui::EndTable();
                }
            }

            ImGui::TreePop();
        }
    }

    if (id_to_remove >= 0)
    {
        /* Make sure to remove any delta references to the marker we're about to
           remove. The std::set::erase has no effect if the key does not exist.*/
        for (auto &[_, marker] : markers)
            marker.deltas.erase(id_to_remove);
        markers.erase(id_to_remove);
    }

    ImGui::TreePop();
}

void Ui::RenderMarkers(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Markers", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (ImGui::SmallButton("Remove all"))
    {
        m_time_domain_markers.clear();
        m_next_time_domain_marker_id = 0;
        m_frequency_domain_markers.clear();
        m_next_frequency_domain_marker_id = 0;
    }

    MarkerTree(m_time_domain_markers, "Time Domain", "T", Format::TimeDomainX, Format::TimeDomainY);
    MarkerTree(m_frequency_domain_markers, "Frequency Domain", "F", Format::FrequencyDomainX,
               Format::FrequencyDomainY);
    ImGui::End();
}

void Ui::RenderProcessingOptions(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Processing Options", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    static int window_idx = 1;
    static int window_idx_prev = 1;
    const char *window_labels[] = {"No window", "Blackman-Harris", "Flat top", "Hamming", "Hanning"};
    ImGui::Combo("Window##window", &window_idx, window_labels, IM_ARRAYSIZE(window_labels));

    if (window_idx != window_idx_prev)
    {
        switch (window_idx)
        {
        case 0:
            PushMessage({DigitizerMessageId::SET_WINDOW_TYPE, WindowType::NONE}, false);
            break;

        case 1:
            PushMessage({DigitizerMessageId::SET_WINDOW_TYPE, WindowType::BLACKMAN_HARRIS}, false);
            break;

        case 2:
            PushMessage({DigitizerMessageId::SET_WINDOW_TYPE, WindowType::FLAT_TOP}, false);
            break;

        case 3:
            PushMessage({DigitizerMessageId::SET_WINDOW_TYPE, WindowType::HAMMING}, false);
            break;

        case 4:
            PushMessage({DigitizerMessageId::SET_WINDOW_TYPE, WindowType::HANNING}, false);
            break;
        }
        window_idx_prev = window_idx;
    }

    ImGui::End();
}

/* FIXME: Remove */
void Ui::Reduce(double xsize, double sampling_period, int &count, int &stride)
{
    /* Determine how many samples fit in the current view. If this number
       exceeds MAX_NOF_SAMPLES, repeatedly increase the stride by a factor of
       two until we're in range. */
    static const int MAX_NOF_SAMPLES = 32768;
    const int MAX_NOF_SAMPLES_IN_WINDOW = static_cast<int>(xsize / sampling_period + 0.5);

    stride = 1;
    if ((count < MAX_NOF_SAMPLES) || (MAX_NOF_SAMPLES_IN_WINDOW < MAX_NOF_SAMPLES))
        return;

    /* Reduce the number of visible samples. */
    do
    {
        stride += 1;
    } while ((count / stride) > MAX_NOF_SAMPLES);

    count /= stride;
}

void Ui::SnapX(double x, const BaseRecord *record, double &snap_x, double &snap_y)
{
    if (x < record->x.front())
    {
        snap_x = record->x.front();
        snap_y = record->y.front();
    }
    else if (x > record->x.back())
    {
        snap_x = record->x.back();
        snap_y = record->y.back();
    }
    else
    {
        /* Get the distance to the first sample, which we now know is a positive
           value in the range spanned by the x-vector. */
        double distance = x - record->x.front();
        size_t index = static_cast<size_t>(std::round(distance / record->step));
        snap_x = record->x[index];
        snap_y = record->y[index];
    }
}

void Ui::GetClosestSampleIndex(double x, double y, const BaseRecord *record, const ImPlotRect &view,
                               size_t &index)
{
    /* Find the closest sample to the coordinates (x,y) by minimizing the
       Euclidian distance. We have to normalize the data fpr this method to give
       the desired results---namely, we have to normalize using the _plot_
       limits since that is the perceived reference frame of the user. */

    const double kx = (view.X.Max - view.X.Min) / 2;
    const double mx = -(view.X.Min + kx);
    const double ky = (view.Y.Max - view.Y.Min) / 2;
    const double my = -(view.Y.Min + ky);

    const double x_normalized = (x + mx) / kx;
    const double x_step_normalized = record->step / kx;
    const double y_normalized = (y + my) / ky;

    const double x0_normalized = (record->x.front() + mx) / kx;
    const double center = std::round((x_normalized - x0_normalized) / x_step_normalized);

    /* Create a symmetric span around the rounded x-coordinate and then clip the
       limits to the range where there's data. */
    const double span = 16.0;
    const double low_limit = 0.0;
    const double high_limit = static_cast<double>(record->x.size() - 1);
    double span_low = center - span;
    double span_high = center + span;

    if (span_low < low_limit)
        span_low = low_limit;
    else if (span_low > (high_limit - span))
        span_low = (std::max)(high_limit - span, low_limit);

    if (span_high < (low_limit + span))
        span_high = (std::min)(low_limit + span, high_limit);
    else if (span_high > high_limit)
        span_high = high_limit;

    const size_t low = static_cast<size_t>(span_low);
    const size_t high = static_cast<size_t>(span_high);
    double distance_min = (std::numeric_limits<double>::max)();

    for (size_t i = low; i <= high; ++i)
    {
        const double xi = (record->x[i] + mx) / kx;
        const double yi = (record->y[i] + my) / ky;

        const double x2 = std::pow(x_normalized - xi, 2);
        const double y2 = std::pow(y_normalized - yi, 2);
        const double distance = x2 + y2;

        if (distance < distance_min)
        {
            index = i;
            distance_min = distance;
        }
    }
}

std::vector<Ui::ChannelUiState *> Ui::GetUiWithSelectedLast()
{
    std::vector<ChannelUiState *> result;
    ChannelUiState *selected = NULL;

    for (auto &digitizer : m_digitizers)
    {
        if (!digitizer.ui.is_selected)
            continue;

        for (auto &ui : digitizer.ui.channels)
        {
            if (ui.record == NULL)
                continue;

            if (ui.is_selected)
                selected = &ui;
            else
                result.push_back(&ui);
        }
    }

    if (selected != NULL)
        result.push_back(selected);

    return result;
}

void Ui::PlotTimeDomainSelected()
{
    /* We need a (globally) unique id for each marker. */
    int marker_id = 0;

    /* FIXME: Want range based for */
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_digitizers[i].ui.is_selected)
            continue;

        /* FIXME: Want range based for */
        for (size_t ch = 0; ch < m_digitizers[i].ui.channels.size(); ++ch)
        {
            auto &ui = m_digitizers[i].ui.channels[ch];
            if (ui.record == NULL)
                continue;

            int count = static_cast<int>(ui.record->time_domain->x.size());

            /* FIXME: A rough value to switch off persistent plotting when the
                      window contains too many samples, heavily tanking the
                      performance. */
            bool is_persistence_performant =
                ImPlot::GetPlotLimits().Size().x / ui.record->time_domain->step < 2048;

            if (ui.is_persistence_enabled && is_persistence_performant)
            {
                ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
                for (const auto &c : ui.record->persistence->data)
                {
                    ImPlot::PlotScatter(ui.record->label.c_str(), c->x.data(), c->y.data(),
                                        c->x.size());
                }
                ImPlot::PopStyleVar();
            }
            else
            {
                if (ui.is_sample_markers_enabled)
                    ImPlot::SetNextMarkerStyle(ImPlotMarker_Cross);

                ImPlot::PushStyleColor(ImPlotCol_Line, ui.color);
                ImPlot::PlotLine(ui.record->label.c_str(),
                                ui.record->time_domain->x.data(),
                                ui.record->time_domain->y.data(), count);
                ImPlot::PopStyleColor();
            }

            /* Here we have to resort to using ImPlot internals to gain access
               to whether or not the plot is shown or not. The user can click
               the legend entry to change the visibility state. */
            auto item = ImPlot::GetCurrentContext()->CurrentItems->GetItem(ui.record->label.c_str());
            ui.is_time_domain_visible = (item != NULL) && item->Show;

            if (ui.is_time_domain_visible)
            {
                for (auto &[id, marker] : m_time_domain_markers)
                {
                    if (marker.digitizer != i || marker.channel != ch)
                        continue;

                    SnapX(marker.x, ui.record->time_domain.get(), marker.x, marker.y);

                    ImPlot::DragPoint(0, &marker.x, &marker.y, marker.color,
                                      3.0f + marker.thickness, ImPlotDragToolFlags_NoInputs);
                    DrawMarkerX(marker_id++, &marker.x, marker.color, marker.thickness,
                                Format::Metric(marker.x, "{:g} {}s", 1e-3));
                    DrawMarkerY(marker_id++, &marker.y, marker.color, marker.thickness,
                                Format::Metric(marker.y, "{: 7.1f} {}V", 1e-3),
                                ImPlotDragToolFlags_NoInputs);
                }

                if (ui.is_selected)
                {
                    MaybeAddMarker(i, ch, ui.record->time_domain.get(), m_time_domain_markers,
                                   m_is_adding_time_domain_marker, m_is_dragging_time_domain_marker,
                                   m_next_time_domain_marker_id, m_last_time_domain_marker);
                }
            }
        }
    }
}

int Ui::GetSelectedChannel(ChannelUiState *&ui)
{
    for (auto &d : m_digitizers)
    {
        for (auto &chui : d.ui.channels)
        {
            if (chui.is_selected)
            {
                ui = &chui;
                return ADQR_EOK;
            }
        }
    }

    return ADQR_EAGAIN;
}

void Ui::DrawMarkerX(int id, double *x, const ImVec4 &color, float thickness,
                       const std::string &tag, ImPlotDragToolFlags flags)
{
    ImPlot::DragLineX(id, x, color, thickness, flags);
    ImPlot::TagX(*x, color, "%s", tag.c_str());
}

void Ui::DrawMarkerY(int id, double *y, const ImVec4 &color, float thickness,
                       const std::string &tag, ImPlotDragToolFlags flags)
{
    ImPlot::DragLineY(id, y, color, thickness, flags);
    ImPlot::TagY(*y, color, "%s", tag.c_str());
}

void Ui::MaybeAddMarker(size_t digitizer, size_t channel, const BaseRecord *record,
                        std::map<size_t, Marker> &markers, bool &is_adding_marker,
                        bool &is_dragging_marker, size_t &next_marker_id,
                        std::map<size_t, Marker>::iterator &last_marker)
{
    if (ImPlot::IsPlotHovered() && ImGui::GetIO().KeyCtrl && ImGui::IsMouseClicked(0))
    {
        size_t index;
        GetClosestSampleIndex(ImPlot::GetPlotMousePos().x, ImPlot::GetPlotMousePos().y, record,
                              ImPlot::GetPlotLimits(), index);

        /* FIXME: Probably need to consider the initial x/y-values to be
                  special. Otherwise, the marker can seem to wander a bit if the
                  signal is noisy. */

        auto [it, success] =
            markers.insert({next_marker_id, Marker(next_marker_id, digitizer, channel, index,
                                                   record->x[index], record->y[index])});
        next_marker_id++;
        last_marker = it;
        is_adding_marker = true;
        is_dragging_marker = false;
    }

    if (is_adding_marker && ImGui::IsMouseDragging(0))
    {
        /* FIXME: Very rough test of delta dragging */
        if (!is_dragging_marker)
        {
            size_t index;
            GetClosestSampleIndex(ImPlot::GetPlotMousePos().x, ImPlot::GetPlotMousePos().y, record,
                                  ImPlot::GetPlotLimits(), index);

            last_marker->second.deltas.insert(next_marker_id);
            auto [it, success] =
                markers.insert({next_marker_id, Marker(next_marker_id, digitizer, channel, index,
                                                       record->x[index], record->y[index])});
            next_marker_id++;
            last_marker = it;
        }

        is_dragging_marker = true;
        last_marker->second.x = ImPlot::GetPlotMousePos().x;
    }

    if (is_adding_marker && ImGui::IsMouseReleased(0))
    {
        is_adding_marker = false;
        is_dragging_marker = false;
    }
}

bool Ui::IsHoveredAndDoubleClicked(const std::pair<size_t, Marker> &marker)
{
    /* Construct a rect that covers the markers. If the mouse is double clicked
       while inside these limits, we remove the marker. This needs to be called
       within a current plot that also contains the provided marker. */

    const auto lmarker = ImPlot::PlotToPixels(marker.second.x, marker.second.y);
    const auto limits = ImPlot::GetPlotLimits();
    const auto upper_left = ImPlot::PlotToPixels(limits.X.Min, limits.Y.Max);
    const auto lower_right = ImPlot::PlotToPixels(limits.X.Max, limits.Y.Min);

    const ImVec2 marker_x_upper_left{std::round(lmarker.x) - 4.0f, upper_left.y};
    const ImVec2 marker_x_lower_right{std::round(lmarker.x) + 4.0f, lower_right.y};
    const ImVec2 marker_y_upper_left{upper_left.x, std::round(lmarker.y) - 4.0f};
    const ImVec2 marker_y_lower_right{lower_right.x, std::round(lmarker.y) + 4.0f};

    return ImGui::IsMouseDoubleClicked(0) &&
           (ImGui::IsMouseHoveringRect(marker_x_upper_left, marker_x_lower_right) ||
            ImGui::IsMouseHoveringRect(marker_y_upper_left, marker_y_lower_right));
}

void Ui::RemoveDoubleClickedMarkers(std::map<size_t, Marker> &markers)
{
    for (auto it = markers.begin(); it != markers.end(); )
    {
        if (IsHoveredAndDoubleClicked(*it))
        {
            /* Make sure to remove any delta references to the marker we're
               about to remove. The std::set::erase has no effect if the key
               does not exist. */
            for (auto &[_, marker] : markers)
                marker.deltas.erase(it->first);
            it = markers.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Ui::RenderTimeDomain(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Time Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    m_is_time_domain_collapsed = ImGui::IsWindowCollapsed();
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.0f, 0.1f));
    if (ImPlot::BeginPlot("Time domain", ImVec2(-1, -1), ImPlotFlags_NoTitle))
    {
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);
        ImPlot::SetupAxisFormat(ImAxis_X1, Format::Metric, (void *)"s");
        ImPlot::SetupAxisFormat(ImAxis_Y1, Format::Metric, (void *)"V");
        PlotTimeDomainSelected();
        RemoveDoubleClickedMarkers(m_time_domain_markers);
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar();
    ImGui::End();
}

void Ui::RenderFrequencyDomain(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Frequency Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    m_is_frequency_domain_collapsed = ImGui::IsWindowCollapsed();
    if (ImGui::BeginTabBar("Frequency Domain##tabbar", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("FFT"))
        {
            RenderFourierTransformPlot();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Waterfall"))
        {
            RenderWaterfallPlot();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void Ui::Annotate(const std::pair<double, double> &point, const std::string &label)
{
    /* TODO: We need some form of low-pass filter for the y-coordinates. For
             high update rates (~60 Hz) the annotations jitter quite a lot. Just
             naively rounding them to the nearest integer remove some of the
             jitter but can introduce even larger jumps for noisy data. */
    ImPlot::Annotation(point.first, point.second, ImVec4(0, 0, 0, 0),
                       ImVec2(0, -5), false, "%.2f dBFS", point.second);
    ImPlot::Annotation(point.first, point.second, ImVec4(0, 0, 0, 0),
                       ImVec2(0, -5 - ImGui::GetTextLineHeight()), false,
                       "%s", Format::Metric(point.first, "{:.2f} {}Hz", 1e6).c_str());
    if (label.size() > 0)
    {
        ImPlot::Annotation(point.first, point.second, ImVec4(0, 0, 0, 0),
                           ImVec2(0, -5 - 2 * ImGui::GetTextLineHeight()), true,
                           "%s", label.c_str());
    }
}

void Ui::PlotFourierTransformSelected()
{
    /* We need a (globally) unique id for each marker. */
    int marker_id = 0;

    /* FIXME: Want range based for */
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_digitizers[i].ui.is_selected)
            continue;

        /* FIXME: Want range based for */
        for (size_t ch = 0; ch < m_digitizers[i].ui.channels.size(); ++ch)
        {
            auto &ui = m_digitizers[i].ui.channels[ch];
            if (ui.record == NULL)
                continue;

            int count = static_cast<int>(ui.record->frequency_domain->x.size());
            ImPlot::PushStyleColor(ImPlotCol_Line, ui.color);
            ImPlot::PlotLine(ui.record->label.c_str(), ui.record->frequency_domain->x.data(),
                             ui.record->frequency_domain->y.data(), count);
            ImPlot::PopStyleColor();

            /* Here we have to resort to using ImPlot internals to gain access
               to whether or not the plot is shown or not. The user can click
               the legend entry to change the visibility state. */

            auto item =
                ImPlot::GetCurrentContext()->CurrentItems->GetItem(ui.record->label.c_str());
            ui.is_frequency_domain_visible = (item != NULL) && item->Show;

            if (ui.is_frequency_domain_visible)
            {
                Annotate(ui.record->frequency_domain_metrics.fundamental, "Fund.");
                for (size_t j = 0; j < ui.record->frequency_domain_metrics.harmonics.size(); ++j)
                {
                    Annotate(ui.record->frequency_domain_metrics.harmonics[j],
                             fmt::format("HD{}", j + 2));
                }

                for (auto &[id, marker] : m_frequency_domain_markers)
                {
                    if (marker.digitizer != i || marker.channel != ch)
                        continue;

                    SnapX(marker.x, ui.record->frequency_domain.get(), marker.x, marker.y);

                    ImPlot::DragPoint(0, &marker.x, &marker.y, marker.color,
                                      3.0f + marker.thickness, ImPlotDragToolFlags_NoInputs);
                    DrawMarkerX(marker_id++, &marker.x, marker.color, marker.thickness,
                                Format::Metric(marker.x, "{:.2f} {}Hz", 1e6));
                    DrawMarkerY(marker_id++, &marker.y, marker.color, marker.thickness,
                                fmt::format("{: 8.2f} dBFS", marker.y), ImPlotDragToolFlags_NoInputs);
                }

                if (ui.is_selected)
                {
                    MaybeAddMarker(i, ch, ui.record->frequency_domain.get(),
                                   m_frequency_domain_markers, m_is_adding_frequency_domain_marker,
                                   m_is_dragging_frequency_domain_marker,
                                   m_next_frequency_domain_marker_id,
                                   m_last_frequency_domain_marker);
                }
            }
        }
    }
}

void Ui::RenderFourierTransformPlot()
{
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.0f, 0.1f));
    if (ImPlot::BeginPlot("FFT##plot", ImVec2(-1, -1), ImPlotFlags_NoTitle))
    {
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -100.0, 10.0);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 1e9);
        ImPlot::SetupAxisFormat(ImAxis_X1, Format::Metric, (void *)"Hz");
        PlotFourierTransformSelected();
        RemoveDoubleClickedMarkers(m_frequency_domain_markers);
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar();
}

void Ui::PlotWaterfallSelected()
{
    ChannelUiState *ui;
    if (ADQR_EOK == GetSelectedChannel(ui))
    {
        /* FIXME: Y-axis scale (probably time delta?) */
        const double TOP_RIGHT = ui->record->time_domain->sampling_frequency / 2;
        ImPlot::PlotHeatmap("heat", ui->record->waterfall->data.data(),
                            static_cast<int>(ui->record->waterfall->rows),
                            static_cast<int>(ui->record->waterfall->columns), -100, 0, NULL,
                            ImPlotPoint(0, 0), ImPlotPoint(TOP_RIGHT, 1));
    }
}

void Ui::RenderWaterfallPlot()
{
    ImPlot::PushColormap("Hot");
    if (ImPlot::BeginPlot("Waterfall##plot", ImVec2(-1, -1), ImPlotFlags_NoTitle | ImPlotFlags_NoLegend))
    {
        static const ImPlotAxisFlags FLAGS = ImPlotAxisFlags_NoGridLines;
        ImPlot::SetupAxes(NULL, NULL, FLAGS, FLAGS);
        ImPlot::SetupAxisFormat(ImAxis_X1, Format::Metric, (void *)"Hz");
        PlotWaterfallSelected();
        ImPlot::EndPlot();
    }
    ImPlot::PopColormap();
}

void Ui::RenderTimeDomainMetrics(const ImVec2 &position, const ImVec2 &size)
{
    /* FIXME: Move into functions? */
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Time Domain Metrics", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    bool has_contents = false;
    for (auto &digitizer : m_digitizers)
    {
        if (!digitizer.ui.is_selected)
            continue;

        for (auto &ui : digitizer.ui.channels)
        {
            if (ui.record == NULL)
                continue;

            if (has_contents)
                ImGui::Separator();
            has_contents = true;

            ImGui::ColorEdit4((ui.record->label + "##TimeDomain").c_str(), (float *)&ui.color,
                                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::SameLine();

            int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (ui.is_selected)
                flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);
            bool node_open = ImGui::TreeNodeEx(ui.record->label.c_str(), flags);

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                ClearChannelSelection();
                ui.is_selected = !ui.is_selected;
            }

            if (ImGui::BeginPopupContextItem())
            {
                ImGui::MenuItem("Sample markers", "", &ui.is_sample_markers_enabled);
                ImGui::MenuItem("Persistence", "", &ui.is_persistence_enabled);
                ImGui::EndPopup();
            }

            if (node_open)
            {
                const auto &record = ui.record->time_domain;
                const auto &metrics = ui.record->time_domain_metrics;

                ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings |
                                        ImGuiTableFlags_BordersInnerV;
                if (ImGui::BeginTable("Metrics", 2, flags))
                {
                    const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
                    ImGui::TableSetupColumn("Metric",
                                            ImGuiTableColumnFlags_WidthFixed |
                                                ImGuiTableColumnFlags_NoHide,
                                            16.0f * TEXT_BASE_WIDTH);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);

                    ImGui::TableNextColumn();
                    ImGui::Text("Record number");
                    ImGui::TableNextColumn();
                    ImGui::Text(fmt::format("{: >6d}", record->header.record_number));

                    ImGui::TableNextColumn();
                    ImGui::Text("Maximum");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::TimeDomainY(metrics.max));

                    ImGui::TableNextColumn();
                    ImGui::Text("Minimum");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::TimeDomainY(metrics.min));

                    ImGui::TableNextColumn();
                    ImGui::Text("Mean");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::TimeDomainY(metrics.mean));

                    ImGui::TableNextColumn();
                    ImGui::Text("Sampling rate");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainX(record->sampling_frequency));

                    ImGui::TableNextColumn();
                    ImGui::Text("Sampling period");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::TimeDomainX(record->step));

                    ImGui::TableNextColumn();
                    ImGui::Text("Trigger rate");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainX(record->estimated_trigger_frequency));

                    ImGui::TableNextColumn();
                    ImGui::Text("Throughput");
                    ImGui::TableNextColumn();
                    ImGui::Text(
                        Format::Metric(record->estimated_throughput, "{: 8.1f} {}B/s", 1e6));

                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
            ImGui::PopStyleVar();
        }
    }
    ImGui::End();
}

void Ui::RenderFrequencyDomainMetrics(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Frequency Domain Metrics", NULL,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavFocus);

    bool has_contents = false;
    for (auto &digitizer : m_digitizers)
    {
        if (!digitizer.ui.is_selected)
            continue;

        for (auto &ui : digitizer.ui.channels)
        {
            if (ui.record == NULL)
                continue;

            has_contents = true;
            if (has_contents)
                ImGui::Separator();

            ImGui::ColorEdit4((ui.record->label + "##FrequencyDomain").c_str(), (float *)&ui.color,
                                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);

            int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (ui.is_selected)
                flags |= ImGuiTreeNodeFlags_Selected;

            bool node_open = ImGui::TreeNodeEx(ui.record->label.c_str(), flags);

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                ClearChannelSelection();
                ui.is_selected = !ui.is_selected;
            }

            if (node_open)
            {
                const auto &record = ui.record->frequency_domain;
                const auto &metrics = ui.record->frequency_domain_metrics;

                if (metrics.overlap)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, COLOR_ORANGE);
                    ImGui::SameLine();
                    ImGui::Text("OVERLAP");
                }

                ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings;
                if (ImGui::BeginTable("Metrics", 6, flags))
                {
                    const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
                    const auto METRIC_FLAGS =
                        ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide;
                    ImGui::TableSetupColumn("Metric0", METRIC_FLAGS, 6.0f * TEXT_BASE_WIDTH);
                    ImGui::TableSetupColumn("Value0", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Gap", ImGuiTableColumnFlags_WidthFixed, 2.0f * TEXT_BASE_WIDTH);
                    ImGui::TableSetupColumn("Metric1", METRIC_FLAGS, 4.0f * TEXT_BASE_WIDTH);
                    ImGui::TableSetupColumn("Value1", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value2", ImGuiTableColumnFlags_WidthFixed);

                    ImGui::TableNextColumn();
                    ImGui::Text("SNR");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainDeltaY(metrics.snr));
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::Text("Fund.");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainX(metrics.fundamental.first));
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainY(metrics.fundamental.second));

                    ImGui::TableNextColumn();
                    ImGui::Text("SINAD");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainDeltaY(metrics.sinad));
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::Text("HD2");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainX(metrics.harmonics[0].first));
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainY(metrics.harmonics[0].second));

                    ImGui::TableNextColumn();
                    ImGui::Text("ENOB");
                    ImGui::TableNextColumn();
                    ImGui::Text(fmt::format("{: 7.2f} bits", metrics.enob));
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::Text("HD3");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainX(metrics.harmonics[1].first));
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainY(metrics.harmonics[1].second));

                    ImGui::TableNextColumn();
                    ImGui::Text("THD");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainDeltaY(metrics.thd));
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::Text("HD4");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainX(metrics.harmonics[2].first));
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainY(metrics.harmonics[2].second));

                    ImGui::TableNextColumn();
                    ImGui::Text("SFDR");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainY(metrics.sfdr_dbfs));
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::Text("HD5");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainX(metrics.harmonics[3].first));
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainY(metrics.harmonics[3].second));

                    ImGui::TableNextColumn();
                    ImGui::Text("Noise");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainY(metrics.noise));
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::Text("Spur");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainX(metrics.spur.first));
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainY(metrics.spur.second));

                    ImGui::TableNextColumn();
                    ImGui::Text("Size");
                    ImGui::TableNextColumn();
                    ImGui::Text(fmt::format("{:7} pts", (record->x.size() - 1) * 2));
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::Text("Bin");
                    ImGui::TableNextColumn();
                    ImGui::Text(Format::FrequencyDomainX(record->step));
                    ImGui::TableNextColumn();

                    ImGui::EndTable();
                }

                if (metrics.overlap)
                    ImGui::PopStyleColor();

                ImGui::TreePop();
            }
            ImGui::PopStyleVar();
        }
    }
    ImGui::End();
}

void Ui::RenderApplicationMetrics(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Application Metrics", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    const ImGuiIO &io = ImGui::GetIO();
    ImGui::Text("Average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();
}
