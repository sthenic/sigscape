#include "ui.h"
#include "implot_internal.h" /* To be able to get item visibility state. */

#include "fmt/format.h"
#include <cinttypes>

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

// Ui::MarkerPair::MarkerPair()
//     : direction(HORIZONTAL)
//     , start(0.0)
//     , stop(0.0)
// {}

Ui::ChannelUiState::ChannelUiState()
    : color{}
    , sample_markers(true)
    , is_time_domain_visible(true)
    , is_frequency_domain_visible(true)
    , record(NULL)
    , markers{}
{}

void Ui::ChannelUiState::ColorSquare() const
{
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::SmallButton(" ");
    ImGui::PopItemFlag();
    ImGui::PopStyleColor();
}

Ui::DigitizerUiState::DigitizerUiState(int nof_channels)
    : identifier("")
    , state("")
    , event("")
    , state_color(COLOR_GREEN)
    , event_color(COLOR_GREEN)
    , set_top_color(ImGui::GetStyleColorVec4(ImGuiCol_Button))
    , set_clock_system_color(ImGui::GetStyleColorVec4(ImGuiCol_Button))
    , channels{}
{
    for (int i = 0; i < nof_channels; ++i)
        channels.push_back(ChannelUiState());
}

Ui::Ui()
    : m_identification()
    , m_digitizers()
    , m_adq_control_unit()
    , m_show_imgui_demo_window(false)
    , m_show_implot_demo_window(false)
    , m_is_time_domain_collapsed(false)
    , m_is_frequency_domain_collapsed(false)
    , m_selected()
    , m_digitizer_ui_state{}
{}

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
        d->Stop();

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

    if (m_show_imgui_demo_window)
        ImGui::ShowDemoWindow();

    if (m_show_implot_demo_window)
        ImPlot::ShowDemoWindow();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Ui::PushMessage(const DigitizerMessage &message, bool selected)
{
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (selected && !m_selected[i])
            continue;
        m_digitizers[i]->PushMessage(message);

        if (message.id == DigitizerMessageId::STOP_ACQUISITION)
        {
            for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
                m_digitizer_ui_state[i].channels[ch].markers.clear();
        }
    }
}

void Ui::UpdateRecords()
{
    /* FIXME: Skip unselected digitizers? We need a different queue then that
              pops entries at the end once saturated, regardless if they've been
              read or not. */

    /* Attempt to update the set of processed records for each digitizer. */
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        /* FIXME: Skip nonexistent channels. */
        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
            m_digitizers[i]->WaitForProcessedRecord(ch, m_digitizer_ui_state[i].channels[ch].record);
    }
}

void Ui::HandleMessage(const IdentificationMessage &message)
{
    /* If we get a message, the identification went well. */
    for (const auto &d : m_digitizers)
        d->Stop();

    m_digitizers = message.digitizers;
    m_adq_control_unit = message.handle;

    m_digitizer_ui_state.clear();
    for (size_t i = 0; i < m_digitizers.size(); ++i)
        m_digitizer_ui_state.push_back(DigitizerUiState(ADQ_MAX_NOF_CHANNELS));

    m_selected = std::unique_ptr<bool[]>( new bool[m_digitizers.size()] );
    memset(&m_selected[0], 0, sizeof(m_selected[0]) * m_digitizers.size());

    for (const auto &d : m_digitizers)
        d->Start();
}

void Ui::HandleMessage(size_t i, const DigitizerMessage &message)
{
    switch (message.id)
    {
    case DigitizerMessageId::IDENTIFIER:
        m_digitizer_ui_state[i].identifier = *message.str;
        break;

    case DigitizerMessageId::CONFIGURATION:
        m_digitizer_ui_state[i].event = "CONFIGURATION";
        m_digitizer_ui_state[i].event_color = COLOR_WOW_TAN;
        break;

    case DigitizerMessageId::CLEAR:
        m_digitizer_ui_state[i].event = "";
        break;

    case DigitizerMessageId::ERROR:
        m_digitizer_ui_state[i].event = "ERROR";
        m_digitizer_ui_state[i].event_color = COLOR_RED;
        break;

    case DigitizerMessageId::STATE:
        switch (message.state)
        {
        case DigitizerState::NOT_ENUMERATED:
            m_digitizer_ui_state[i].state = "NOT ENUMERATED";
            m_digitizer_ui_state[i].state_color = COLOR_RED;
            break;
        case DigitizerState::ENUMERATION:
            m_digitizer_ui_state[i].state = "ENUMERATION";
            m_digitizer_ui_state[i].state_color = COLOR_PURPLE;
            break;
        case DigitizerState::IDLE:
            m_digitizer_ui_state[i].state = "IDLE";
            m_digitizer_ui_state[i].state_color = COLOR_GREEN;
            break;
        case DigitizerState::ACQUISITION:
            m_digitizer_ui_state[i].state = "ACQUISITION";
            m_digitizer_ui_state[i].state_color = COLOR_ORANGE;
            break;
        }
        break;

    case DigitizerMessageId::DIRTY_TOP_PARAMETERS:
        m_digitizer_ui_state[i].set_top_color = COLOR_ORANGE;
        break;

    case DigitizerMessageId::DIRTY_CLOCK_SYSTEM_PARAMETERS:
        m_digitizer_ui_state[i].set_clock_system_color = COLOR_ORANGE;
        break;

    case DigitizerMessageId::CLEAN_TOP_PARAMETERS:
        m_digitizer_ui_state[i].set_top_color = COLOR_GREEN;
        break;

    case DigitizerMessageId::CLEAN_CLOCK_SYSTEM_PARAMETERS:
        m_digitizer_ui_state[i].set_clock_system_color = COLOR_GREEN;
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

    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        DigitizerMessage digitizer_message;
        if (ADQR_EOK == m_digitizers[i]->WaitForMessage(digitizer_message, 0))
            HandleMessage(i, digitizer_message);
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

    const ImVec2 COMMAND_PALETTE_POS(0.0f, 200.0f + FRAME_HEIGHT);
    const ImVec2 COMMAND_PALETTE_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, height - 400.0f - 4 * FRAME_HEIGHT);
    RenderCommandPalette(COMMAND_PALETTE_POS, COMMAND_PALETTE_SIZE);

    const ImVec2 PROCESSING_OPTIONS_POS(0.0f, height - 200.0f - 3 * FRAME_HEIGHT);
    const ImVec2 PROCESSING_OPTIONS_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 200.0f);
    RenderProcessingOptions(PROCESSING_OPTIONS_POS, PROCESSING_OPTIONS_SIZE);

    const ImVec2 METRICS_POS(0.0f, height - 3 * FRAME_HEIGHT);
    const ImVec2 METRICS_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 3 * FRAME_HEIGHT);
    RenderApplicationMetrics(METRICS_POS, METRICS_SIZE);
}

void Ui::RenderDigitizerSelection(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Digitizers", NULL, ImGuiWindowFlags_NoMove);

    if (ImGui::Button("Select All"))
    {
        for (size_t i = 0; i < m_digitizers.size(); ++i)
            m_selected[i] = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Deselect All"))
    {
        for (size_t i = 0; i < m_digitizers.size(); ++i)
            m_selected[i] = false;
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

            for (size_t i = 0; i < m_digitizers.size(); ++i)
            {
                ImGui::TableNextColumn();
                if (ImGui::Selectable(m_digitizer_ui_state[i].identifier.c_str(), m_selected[i],
                                      ImGuiSelectableFlags_SpanAllColumns))
                {
                    if (!ImGui::GetIO().KeyCtrl)
                        memset(&m_selected[0], 0, sizeof(m_selected[0]) * m_digitizers.size());
                    m_selected[i] ^= 1;
                }

                ImGui::TableNextColumn();
                if (m_digitizer_ui_state[i].state.size() > 0)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, m_digitizer_ui_state[i].state_color);
                    ImGui::SmallButton(m_digitizer_ui_state[i].state.c_str());
                    ImGui::PopStyleColor();
                }

                ImGui::TableNextColumn();
                if (m_digitizer_ui_state[i].event.size() > 0)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, m_digitizer_ui_state[i].event_color);
                    ImGui::SmallButton(m_digitizer_ui_state[i].event.c_str());
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
    ImGui::Begin("Command Palette");

    /* FIXME: Maybe this is overkill. */
    std::stringstream ss;
    bool any_selected = false;
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (m_selected[i])
        {
            if (!any_selected)
                ss << "Commands will be applied to ";
            else
                ss << ", ";

            ss << m_digitizer_ui_state[i].identifier;
            any_selected = true;
        }
    }

    if (!any_selected)
        ss << "No digitizer selected.";

    ImGui::Text("%s", ss.str().c_str());

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
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        /* FIXME: Figure out color when multiple units are selected. Perhaps we
                  need to store the state... */
        color = m_digitizer_ui_state[i].set_top_color;
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
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        /* FIXME: Figure out color when multiple units are selected. Perhaps we
                  need to store the state... */
        color = m_digitizer_ui_state[i].set_clock_system_color;
        break;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    if (ImGui::Button("Set Clock\nSystem", size))
        PushMessage(DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS);
    ImGui::PopStyleColor(2);
}

void Ui::RenderProcessingOptions(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Processing Options");

    static int window_idx = 1;
    static int window_idx_prev = 1;
    const char *window_labels[] = {"No window", "Blackman-Harris", "Hamming", "Hanning"};
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
            PushMessage({DigitizerMessageId::SET_WINDOW_TYPE, WindowType::HAMMING}, false);
            break;

        case 3:
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

void Ui::MetricFormatterBase(double value, char *tick_label, int size, const char *format,
                             const char *unit, double start)
{
    static const std::vector<std::pair<double, const char *>> LIMITS = {
        {1e9, "G"},
        {1e6, "M"},
        {1e3, "k"},
        {1, ""},
        {1e-3, "m"},
        {1e-6, "u"},
        {1e-9, "n"}
    };

    if (value == 0)
    {
        std::snprintf(tick_label, size, "0 %s", unit);
        return;
    }

    /* Loop through the limits (descending order) checking if the input value is
       larger than the limit. If it is, we pick the corresponding prefix. If
       we've exhausted the search, we pick the last entry (smallest prefix). */
    for (const auto &limit : LIMITS)
    {
        if (limit.first > start)
            continue;

        if (std::fabs(value) >= limit.first)
        {
            std::snprintf(tick_label, size, format, value / limit.first, limit.second, unit);
            return;
        }
    }

    std::snprintf(tick_label, size, format, value / LIMITS.back().first, LIMITS.back().second, unit);
}

void Ui::MetricFormatterAxis(double value, char *tick_label, int size, void *data)
{
    MetricFormatterBase(value, tick_label, size, "%g %s%s", (const char *)data);
}

void SnapHorizontalMarkers(double x1, double x2, const ProcessedRecord *record,
                           double &x1_snap, double &x2_snap,
                           double &y1_snap, double &y2_snap)
{
    const double &sampling_period = record->time_domain->sampling_period;
    double nearest_sample_x1 = std::round(x1 / sampling_period);
    double nearest_sample_x2 = std::round(x2 / sampling_period);

    if (nearest_sample_x1 < 0.0)
        nearest_sample_x1 = 0.0;
    else if (nearest_sample_x1 >= record->time_domain->count)
        nearest_sample_x1 = record->time_domain->count - 1;

    if (nearest_sample_x2 < 0.0)
        nearest_sample_x2 = 0.0;
    else if (nearest_sample_x2 >= record->time_domain->count)
        nearest_sample_x2 = record->time_domain->count - 1;

    x1_snap = nearest_sample_x1 * sampling_period;
    x2_snap = nearest_sample_x2 * sampling_period;

    y1_snap = record->time_domain->y[static_cast<size_t>(nearest_sample_x1)];
    y2_snap = record->time_domain->y[static_cast<size_t>(nearest_sample_x2)];
}

void Ui::PlotTimeDomainSelected()
{
    /* We need a (globally) unique id for each marker. */
    int marker_id = 0;

    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
        {
            auto &ui = m_digitizer_ui_state[i].channels[ch];
            if (ui.record == NULL)
                continue;

            int count = static_cast<int>(ui.record->time_domain->count);
            if (ui.sample_markers)
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Cross);
            ImPlot::PlotLine(ui.record->label.c_str(),
                                ui.record->time_domain->x.get(),
                                ui.record->time_domain->y.get(), count);
            ui.color = ImPlot::GetLastItemColor();

            /* Here we have to resort to using ImPlot internals to gain
                access to whether or not the plot is shown or not. The user
                can click the legend entry to change the visibility state. */
            auto item = ImPlot::GetCurrentContext()->CurrentItems->GetItem(ui.record->label.c_str());
            ui.is_time_domain_visible = (item != NULL) && item->Show;

            if (ImPlot::BeginLegendPopup(ui.record->label.c_str()))
            {
                ImGui::Text("%s", ui.record->label.c_str());
                ImGui::Separator();
                ImGui::Checkbox("Sample markers", &ui.sample_markers);
                ImPlot::EndLegendPopup();
            }

            if (ui.is_time_domain_visible)
            {
                for (auto &m : ui.markers)
                {
                    static double y1, y2;
                    SnapHorizontalMarkers(m.start, m.stop, ui.record.get(), m.start, m.stop, y1, y2);
                    RenderMarkerX(marker_id++, &m.start);
                    RenderMarkerX(marker_id++, &m.stop);
                    RenderMarkerY(marker_id++, &y1, ImPlotDragToolFlags_NoInputs);
                    RenderMarkerY(marker_id++, &y2, ImPlotDragToolFlags_NoInputs);
                }
            }
        }
    }
}

int Ui::GetFirstVisibleChannel(ChannelUiState *&ui)
{
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
        {
            auto &lui = m_digitizer_ui_state[i].channels[ch];
            if (lui.record != NULL)
            {
                ui = &lui;
                return ADQR_EOK;
            }
        }
    }

    return ADQR_EAGAIN;
}

void Ui::RenderMarkerX(int id, double *x, ImPlotDragToolFlags flags)
{
    char label[32];
    ImPlot::DragLineX(id, x, ImVec4(1, 1, 1, 1), 1.0F, flags);
    MetricFormatterBase(*x, label, sizeof(label), "%g %s%s", "s");
    ImPlot::TagX(*x, ImVec4(1, 1, 1, 1), "%s", label);
}

void Ui::RenderMarkerY(int id, double *y, ImPlotDragToolFlags flags)
{
    char label[32];
    ImPlot::DragLineY(id, y, ImVec4(1, 1, 1, 1), 1.0F, flags);
    MetricFormatterBase(*y, label, sizeof(label), "% 7.1f %s%s", "V", 1e-3);
    ImPlot::TagY(*y, ImVec4(1, 1, 1, 1), "%s", label);
}

void Ui::NewMarkers()
{
    static std::vector<std::pair<double, double>> markers;
    static ImPlotPoint start{};
    static ImPlotPoint stop{};

    /* FIXME: Just as a temporary solution, not the prettiest.  */
    ChannelUiState *ui;
    if (ADQR_EOK != GetFirstVisibleChannel(ui))
        return;

    static bool armed = false;
    if (ImGui::GetIO().KeyCtrl && ImGui::IsMouseClicked(0))
    {
        start = ImPlot::GetPlotMousePos();
        stop = ImPlot::GetPlotMousePos();
        armed = true;
    }

    if (armed && ImGui::IsMouseDragging(0))
    {
        stop = ImPlot::GetPlotMousePos();
    }

    if (armed && ImGui::IsMouseReleased(0))
    {
        ui->markers.push_back({Ui::MarkerPair::HORIZONTAL, start.x, stop.x});
        armed = false;
    }

    if (armed)
    {
        static double snapped_x1 = 0.0;
        static double snapped_x2 = 0.0;
        static double snapped_y1 = 0.0;
        static double snapped_y2 = 0.0;
        SnapHorizontalMarkers(start.x, stop.x, ui->record.get(), snapped_x1, snapped_x2,
                              snapped_y1, snapped_y2);

        RenderMarkerX(0, &snapped_x1, ImPlotDragToolFlags_NoInputs);
        RenderMarkerX(1, &snapped_x2, ImPlotDragToolFlags_NoInputs);
        RenderMarkerY(1, &snapped_y1, ImPlotDragToolFlags_NoInputs);
        RenderMarkerY(1, &snapped_y2, ImPlotDragToolFlags_NoInputs);
    }
}

void Ui::RenderTimeDomain(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Time Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    m_is_time_domain_collapsed = ImGui::IsWindowCollapsed();
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.0f, 0.1f));
    if (ImPlot::BeginPlot("Time domain", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
    {
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);
        ImPlot::SetupAxisFormat(ImAxis_X1, MetricFormatterAxis, (void *)"s");
        ImPlot::SetupAxisFormat(ImAxis_Y1, MetricFormatterAxis, (void *)"V");
        PlotTimeDomainSelected();
        NewMarkers();
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
                       "%.2f MHz", point.first / 1e6);
    if (label.size() > 0)
    {
        ImPlot::Annotation(point.first, point.second, ImVec4(0, 0, 0, 0),
                           ImVec2(0, -5 - 2 * ImGui::GetTextLineHeight()), true,
                           "%s", label.c_str());
    }
}

void Ui::PlotFourierTransformSelected()
{
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
        {
            auto &ui = m_digitizer_ui_state[i].channels[ch];
            if (ui.record != NULL)
            {
                int count = static_cast<int>(ui.record->frequency_domain->count);
                ImPlot::PlotLine(ui.record->label.c_str(),
                                 ui.record->frequency_domain->x.get(),
                                 ui.record->frequency_domain->y.get(), count);
                ui.color = ImPlot::GetLastItemColor();

                /* Here we have to resort to using ImPlot internals to gain
                   access to whether or not the plot is shown or not. The user
                   can click the legend entry to change the visibility state. */
                auto item = ImPlot::GetCurrentContext()->CurrentItems->GetItem(ui.record->label.c_str());
                ui.is_frequency_domain_visible = (item != NULL) && item->Show;

                if (ui.is_frequency_domain_visible)
                {
                    Annotate(ui.record->frequency_domain_metrics.fundamental, "Fund.");
                    for (size_t j = 0; j < ui.record->frequency_domain_metrics.harmonics.size(); ++j)
                    {
                        Annotate(ui.record->frequency_domain_metrics.harmonics[j],
                                 fmt::format("HD{}", j + 2));
                    }
                }
            }
        }
    }
}

void Ui::RenderFourierTransformPlot()
{
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.0f, 0.1f));
    if (ImPlot::BeginPlot("FFT##plot", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
    {
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -100.0, 10.0);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 1e9);
        ImPlot::SetupAxisFormat(ImAxis_X1, MetricFormatterAxis, (void *)"Hz");
        PlotFourierTransformSelected();
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar();
}

void Ui::PlotWaterfallSelected()
{
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        /* FIXME: Plot for the first channel for the first selected digitizer.
                  We have to figure out what to do here since we cannot plot
                  multiple waterfalls at the same time. */

        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
        {
            const auto &record = m_digitizer_ui_state[i].channels[ch].record;
            if (record != NULL)
            {
                /* FIXME: Y-axis scale (probably time delta?) */
                const double TOP_RIGHT = record->time_domain->sampling_frequency / 2;
                ImPlot::PlotHeatmap("heat", record->waterfall->data.get(),
                                    static_cast<int>(record->waterfall->rows),
                                    static_cast<int>(record->waterfall->columns),
                                    -100, 0, NULL, ImPlotPoint(0,0), ImPlotPoint(TOP_RIGHT, 1));
                return;
            }
        }
    }
}

void Ui::RenderWaterfallPlot()
{
    ImPlot::PushColormap("Hot");
    if (ImPlot::BeginPlot("Waterfall##plot", ImVec2(-1, -1), ImPlotFlags_NoTitle | ImPlotFlags_NoLegend))
    {
        static const ImPlotAxisFlags FLAGS = ImPlotAxisFlags_NoGridLines;
        ImPlot::SetupAxes(NULL, NULL, FLAGS, FLAGS);
        ImPlot::SetupAxisFormat(ImAxis_X1, MetricFormatterAxis, (void *)"Hz");
        PlotWaterfallSelected();
        ImPlot::EndPlot();
    }
    ImPlot::PopColormap();
}

template<typename T>
void Ui::MetricsRow(const std::string &label, const std::string &str, T value)
{
    ImGui::TableNextColumn();
    ImGui::Text("%s", label.c_str());
    ImGui::TableNextColumn();
    ImGui::Text(str.c_str(), value);
}

void Ui::RenderTimeDomainMetrics(const ImVec2 &position, const ImVec2 &size)
{
    /* FIXME: Move into functions? */
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Time Domain Metrics", NULL, ImGuiWindowFlags_NoMove);

    bool has_contents = false;
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
        {
            const auto &ui = m_digitizer_ui_state[i].channels[ch];
            if ((ui.record != NULL) && ui.is_time_domain_visible)
            {
                if (has_contents)
                    ImGui::Separator();

                ui.ColorSquare();
                ImGui::SameLine();
                ImGui::Text("%s", ui.record->label.c_str());

                ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings |
                                        ImGuiTableFlags_BordersInnerV;
                if (ImGui::BeginTable("Metrics", 2, flags))
                {
                    const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
                    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed
                                                      | ImGuiTableColumnFlags_NoHide,
                                            14.0f * TEXT_BASE_WIDTH);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);

                    const std::string format = "% 8.3f";
                    MetricsRow("Record number", "%" PRIu32, ui.record->time_domain->header.record_number);
                    MetricsRow("Max", format + " V", ui.record->time_domain_metrics.max);
                    MetricsRow("Min", format + " V", ui.record->time_domain_metrics.min);
                    MetricsRow("Trigger rate", format + " Hz", ui.record->time_domain->estimated_trigger_frequency);

                    ImGui::EndTable();
                }
                has_contents = true;
            }
        }
    }
    ImGui::End();
}

void Ui::RenderFrequencyDomainMetrics(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Frequency Domain Metrics", NULL, ImGuiWindowFlags_NoMove);

    bool has_contents = false;
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
        {
            const auto &ui = m_digitizer_ui_state[i].channels[ch];
            if ((ui.record != NULL) && ui.is_frequency_domain_visible)
            {
                if (has_contents)
                    ImGui::Separator();

                ui.ColorSquare();
                ImGui::SameLine();
                ImGui::Text("%s", ui.record->label.c_str());

                const auto &metrics = ui.record->frequency_domain_metrics;
                if (metrics.overlap)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, COLOR_ORANGE);
                    ImGui::SameLine();
                    ImGui::Text("OVERLAP");
                }

                ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings |
                                        ImGuiTableFlags_BordersInnerV;
                if (ImGui::BeginTable("Metrics", 2, flags))
                {
                    const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
                    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed
                                                      | ImGuiTableColumnFlags_NoHide,
                                            8.0f * TEXT_BASE_WIDTH);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);

                    const std::string format = "% 8.3f";
                    MetricsRow("SNR", format + " dB", metrics.snr);
                    MetricsRow("SINAD", format + " dB", metrics.sinad);
                    MetricsRow("THD", format + " dB", metrics.thd);
                    MetricsRow("ENOB", format + " bits", metrics.enob);
                    MetricsRow("SFDR", format + " dBc", metrics.sfdr_dbc);
                    MetricsRow("SFDR", format + " dBFS", metrics.sfdr_dbfs);
                    MetricsRow("Noise", format + " dBFS", metrics.noise);

                    ImGui::TableNextRow();
                    ImGui::EndTable();
                }

                if (metrics.overlap)
                    ImGui::PopStyleColor();

                has_contents = true;
            }
        }
    }
    ImGui::End();
}

void Ui::RenderApplicationMetrics(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Application Metrics");
    const ImGuiIO &io = ImGui::GetIO();
    ImGui::Text("Average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();
}
