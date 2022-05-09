#include "ui.h"
#include "implot_internal.h" /* To be able to get item visibility status. */

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

Ui::ChannelUiState::ChannelUiState()
    : color{}
    , sample_markers(false)
    , is_shown(true)
    , record(NULL)
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
    , status("")
    , extra("")
    , status_color(COLOR_GREEN)
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
    case DigitizerMessageId::ENUMERATING:
        m_digitizer_ui_state[i].identifier = "Unknown##" + std::to_string(i);
        m_digitizer_ui_state[i].status = "ENUMERATING##" + std::to_string(i);
        m_digitizer_ui_state[i].status_color = COLOR_PURPLE;
        break;

    case DigitizerMessageId::SETUP_OK:
        m_digitizer_ui_state[i].identifier = *message.str;
        break;

    case DigitizerMessageId::SETUP_FAILED:
        m_digitizer_ui_state[i].identifier = "Unknown##" + std::to_string(i);
        m_digitizer_ui_state[i].status = "SETUP FAILED##" + std::to_string(i);
        m_digitizer_ui_state[i].status_color = COLOR_RED;
        break;

    case DigitizerMessageId::NEW_STATE:
        switch (message.state)
        {
        case DigitizerState::NOT_ENUMERATED:
            m_digitizer_ui_state[i].status = "NOT ENUMERATED";
            m_digitizer_ui_state[i].status_color = COLOR_RED;
            break;
        case DigitizerState::IDLE:
            m_digitizer_ui_state[i].status = "IDLE";
            m_digitizer_ui_state[i].status_color = COLOR_GREEN;
            break;
        case DigitizerState::CONFIGURATION:
            m_digitizer_ui_state[i].status = "CONFIGURATION";
            m_digitizer_ui_state[i].status_color = COLOR_PURPLE;
            break;
        case DigitizerState::ACQUISITION:
            m_digitizer_ui_state[i].status = "ACQUISITION";
            m_digitizer_ui_state[i].status_color = COLOR_ORANGE;
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

    case DigitizerMessageId::START_ACQUISITION:
    case DigitizerMessageId::STOP_ACQUISITION:
    case DigitizerMessageId::SET_PARAMETERS:
    case DigitizerMessageId::GET_PARAMETERS:
    case DigitizerMessageId::VALIDATE_PARAMETERS:
    case DigitizerMessageId::INITIALIZE_PARAMETERS:
    case DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
    case DigitizerMessageId::SET_WINDOW_TYPE:
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
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed,
                                    TEXT_BASE_WIDTH * 20.0f);
            ImGui::TableSetupColumn("Extra");
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
                if (m_digitizer_ui_state[i].status.size() > 0)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, m_digitizer_ui_state[i].status_color);
                    ImGui::SmallButton(m_digitizer_ui_state[i].status.c_str());
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

void Ui::MetricFormatter(double value, char *tick_label, int size, void *data)
{
    const char *UNIT = (const char *)data;
    static double LIMITS[] = {1e9, 1e6, 1e3, 1, 1e-3, 1e-6, 1e-9};
    static const char* PREFIXES[] = {"G","M","k","","m","u","n"};
    static const size_t NOF_LIMITS = sizeof(LIMITS) / sizeof(LIMITS[0]);

    if (value == 0)
    {
        std::snprintf(tick_label, size, "0 %s", UNIT);
        return;
    }

    /* Loop through the limits (descending order) checking if the input value is
       larger than the limit. If it is, we pick the corresponding prefix. If
       we've exhausted the search, we pick the last entry (smallest prefix). */
    for (size_t i = 0; i < NOF_LIMITS; ++i)
    {
        if (std::fabs(value) >= LIMITS[i])
        {
            std::snprintf(tick_label,size,"%g %s%s", value / LIMITS[i], PREFIXES[i], UNIT);
            return;
        }
    }

    std::snprintf(tick_label, size, "%g %s%s", value / LIMITS[NOF_LIMITS - 1],
                  PREFIXES[NOF_LIMITS - 1], UNIT);
}

void Ui::PlotTimeDomainSelected()
{
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
        {
            auto &state = m_digitizer_ui_state[i].channels[ch];
            if (state.record != NULL)
            {
                int count = static_cast<int>(state.record->time_domain->count);
                if (state.sample_markers)
                    ImPlot::SetNextMarkerStyle(ImPlotMarker_Cross);
                ImPlot::PlotLine(state.record->label.c_str(),
                                 state.record->time_domain->x.get(),
                                 state.record->time_domain->y.get(), count);
                if (ImPlot::BeginLegendPopup(state.record->label.c_str()))
                {
                    ImGui::Text("%s", state.record->label.c_str());
                    ImGui::Separator();
                    ImGui::Checkbox("Sample markers", &state.sample_markers);
                    ImPlot::EndLegendPopup();
                }
            }
        }
    }
}

void Ui::RenderTimeDomain(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Time Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    m_is_time_domain_collapsed = ImGui::IsWindowCollapsed();
    if (ImPlot::BeginPlot("Time domain", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
    {
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);
        ImPlot::SetupAxisFormat(ImAxis_X1, MetricFormatter, (void *)"s");
        // ImPlot::SetupAxisFormat(ImAxis_Y1, MetricFormatter, (void *)"V");
        PlotTimeDomainSelected();
        ImPlot::EndPlot();
    }
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
    ImPlot::Annotation(point.first, point.second, ImVec4(0, 0, 0, 0),
                       ImVec2(0, -5), true, "%.2f dBFS", point.second);
    ImPlot::Annotation(point.first, point.second, ImVec4(0, 0, 0, 0),
                       ImVec2(0, -5 - ImGui::GetTextLineHeight()), true,
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
            const auto &record = m_digitizer_ui_state[i].channels[ch].record;
            auto &color = m_digitizer_ui_state[i].channels[ch].color;
            auto &is_shown = m_digitizer_ui_state[i].channels[ch].is_shown;
            if (record != NULL)
            {
                int count = static_cast<int>(record->frequency_domain->count);
                ImPlot::PlotLine(record->label.c_str(),
                                 record->frequency_domain->x.get(),
                                 record->frequency_domain->y.get(), count);
                color = ImPlot::GetLastItemColor();

                /* Here we have to resort to using ImPlot internals to gain
                   access to whether or not the plot is shown or not. The user
                   can click the legend entry to change the visibility state. */
                auto item = ImPlot::GetCurrentContext()->CurrentItems->GetItem(record->label.c_str());
                is_shown = (item != NULL) && item->Show;
                if (is_shown)
                {
                    Annotate(record->frequency_domain_metrics.fundamental, "Fund.");
                    for (size_t j = 0; j < record->frequency_domain_metrics.harmonics.size(); ++j)
                    {
                        Annotate(record->frequency_domain_metrics.harmonics[j],
                                 "HD" + std::to_string(j+2));
                    }
                }
            }
        }
    }
}

void Ui::RenderFourierTransformPlot()
{
    if (ImPlot::BeginPlot("FFT##plot", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
    {
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -80.0, 0.0);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 1e9);
        ImPlot::SetupAxisFormat(ImAxis_X1, MetricFormatter, (void *)"Hz");
        PlotFourierTransformSelected();
        ImPlot::EndPlot();
    }
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
        ImPlot::SetupAxisFormat(ImAxis_X1, MetricFormatter, (void *)"Hz");
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
            if ((ui.record != NULL) && ui.is_shown)
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
                                            12.0f * TEXT_BASE_WIDTH);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);

                    const auto &format = "% 8.3f";
                    MetricsRow("Record number", "%" PRIu32, ui.record->time_domain->header.record_number);
                    MetricsRow("Max", format, ui.record->time_domain_metrics.max);
                    MetricsRow("Min", format, ui.record->time_domain_metrics.min);
                    MetricsRow("Frequency", format, ui.record->time_domain->estimated_trigger_frequency);

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
            if ((ui.record != NULL) && ui.is_shown)
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
