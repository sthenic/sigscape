#include "ui.h"
#include <cinttypes>

const ImVec4 Ui::COLOR_GREEN = {0.0f, 1.0f, 0.5f, 0.6f};
const ImVec4 Ui::COLOR_RED = {1.0f, 0.0f, 0.2f, 0.6f};
const ImVec4 Ui::COLOR_YELLOW = {1.0f, 1.0f, 0.3f, 0.8f};
const ImVec4 Ui::COLOR_ORANGE = {0.86f, 0.38f, 0.1f, 0.8f};
const ImVec4 Ui::COLOR_PURPLE = {0.6f, 0.3f, 1.0f, 0.8f};

Ui::Ui()
    : m_digitizers{}
    , m_records{}
    , m_adq_control_unit()
    , m_show_imgui_demo_window(false)
    , m_show_implot_demo_window(false)
    , m_selected()
    , m_digitizer_ui_state()
#ifdef SIMULATION_ONLY
    , m_mock_adqapi()
#endif
{
#ifdef SIMULATION_ONLY
    m_mock_adqapi.AddDigitizer("SPD-SIM01", 2, PID_ADQ32);
    m_mock_adqapi.AddDigitizer("SPD-SIM02", 2, PID_ADQ32);
#endif
}

Ui::~Ui()
{
    for (const auto &d : m_digitizers)
        d->Stop();
}

void Ui::Initialize(GLFWwindow *window, const char *glsl_version)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImGui::StyleColorsDark();

    m_digitizers.clear();
#ifndef SIMULATION_ONLY
    m_adq_control_unit = CreateADQControlUnit();
    if (m_adq_control_unit == NULL)
    {
        printf("Failed to create an ADQControlUnit.\n");
        return;
    }
#else
    m_adq_control_unit = &m_mock_adqapi;
#endif
    InitializeDigitizers();

    m_selected = std::unique_ptr<bool[]>( new bool[m_digitizers.size()] );
    memset(&m_selected[0], 0, sizeof(m_selected));

    m_digitizer_ui_state = std::unique_ptr<DigitizerUiState[]>( new DigitizerUiState[m_digitizers.size()] );

    for (const auto &d : m_digitizers)
        d->Start();
}

void Ui::Terminate()
{
#ifndef SIMULATION_ONLY
    /* FIXME: Close digitizers? */
    if (m_adq_control_unit != NULL)
        DeleteADQControlUnit(m_adq_control_unit);
#endif

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

void Ui::InitializeDigitizers()
{
    /* Filter out the Gen4 digitizers and construct a digitizer object for each one. */
    struct ADQInfoListEntry *adq_list = NULL;
    int nof_devices = 0;
    if (!ADQControlUnit_ListDevices(m_adq_control_unit, &adq_list, (unsigned int *)&nof_devices))
        return;

    int nof_gen4_digitizers = 0;
    for (int i = 0; i < nof_devices; ++i)
    {
        switch (adq_list[i].ProductID)
        {
        case PID_ADQ32:
        case PID_ADQ36:
            if (ADQControlUnit_OpenDeviceInterface(m_adq_control_unit, i))
                nof_gen4_digitizers++;
            break;
        default:
            break;
        }
    }

    for (int i = 0; i < nof_gen4_digitizers; ++i)
    {
        m_digitizers.push_back(std::make_unique<Digitizer>(m_adq_control_unit, i + 1));

        /* FIXME: array type instead? */
        std::vector<std::shared_ptr<ProcessedRecord>> tmp;
        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
            tmp.push_back(std::shared_ptr<ProcessedRecord>());
        m_records.push_back(tmp);
    }

    if (m_digitizers.size() == 0)
        printf("No Gen4 digitizers.\n");
}

void Ui::PushMessage(DigitizerMessageId id, bool selected)
{
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (selected && !m_selected[i])
            continue;
        m_digitizers[i]->PushMessage({id});
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
            m_digitizers[i]->WaitForProcessedRecord(ch, m_records[i][ch]);
    }
}

void Ui::HandleMessage(size_t i, const DigitizerMessage &message)
{
    switch (message.id)
    {
    case DigitizerMessageId::ENUMERATING:
        m_digitizer_ui_state[i].identifier = "Unknown##" + std::to_string(i);
        m_digitizer_ui_state[i].status = "ENUMERATING##" + std::to_string(i);
        m_digitizer_ui_state[i].color = COLOR_PURPLE;
        break;

    case DigitizerMessageId::SETUP_OK:
        m_digitizer_ui_state[i].identifier = *message.str;
        break;

    case DigitizerMessageId::SETUP_FAILED:
        m_digitizer_ui_state[i].identifier = "Unknown##" + std::to_string(i);
        m_digitizer_ui_state[i].status = "SETUP FAILED##" + std::to_string(i);
        m_digitizer_ui_state[i].color = COLOR_RED;
        break;

    case DigitizerMessageId::NEW_STATE:
        switch (message.state)
        {
        case DigitizerState::NOT_ENUMERATED:
            m_digitizer_ui_state[i].status = "NOT ENUMERATED";
            m_digitizer_ui_state[i].color = COLOR_RED;
            break;
        case DigitizerState::IDLE:
            m_digitizer_ui_state[i].status = "IDLE";
            m_digitizer_ui_state[i].color = COLOR_GREEN;
            break;
        case DigitizerState::ACQUISITION:
            m_digitizer_ui_state[i].status = "ACQUISITION";
            m_digitizer_ui_state[i].color = COLOR_ORANGE;
            break;
        }
        break;

    case DigitizerMessageId::START_ACQUISITION:
    case DigitizerMessageId::STOP_ACQUISITION:
    case DigitizerMessageId::SET_PARAMETERS:
    case DigitizerMessageId::GET_PARAMETERS:
    case DigitizerMessageId::VALIDATE_PARAMETERS:
    case DigitizerMessageId::INITIALIZE_PARAMETERS:
    case DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
        /* These are not expected as a message from a digitizer thread. */
        break;
    }
}

void Ui::HandleMessages()
{
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        DigitizerMessage message;
        int result = m_digitizers[i]->WaitForMessage(message, 0);
        if (result == ADQR_EOK)
            HandleMessage(i, message);
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

    /*  */
    RenderFrequencyDomainMetrics(POSITION_LOWER, SIZE);
}

void Ui::RenderCenter(float width, float height)
{
    /* We show two plots in the center, taking up equal vertical space. */
    const float FRAME_HEIGHT = ImGui::GetFrameHeight();
    const float PLOT_WINDOW_HEIGHT = (height - 1 * FRAME_HEIGHT) / 2;
    const ImVec2 POSITION_UPPER(width * FIRST_COLUMN_RELATIVE_WIDTH, FRAME_HEIGHT);
    const ImVec2 POSITION_LOWER(width * FIRST_COLUMN_RELATIVE_WIDTH, FRAME_HEIGHT + PLOT_WINDOW_HEIGHT);
    const ImVec2 SIZE(width * SECOND_COLUMN_RELATIVE_WIDTH, PLOT_WINDOW_HEIGHT);

    /* The lower plot window, showing time domain data. */
    RenderTimeDomain(POSITION_UPPER, SIZE);

    /* The lower plot window, showing frequency domain data. */
    RenderFrequencyDomain(POSITION_LOWER, SIZE);
}

void Ui::RenderLeft(float width, float height)
{
    const float FRAME_HEIGHT = ImGui::GetFrameHeight();
    const ImVec2 DIGITIZER_SELECTION_POS(0.0f, FRAME_HEIGHT);
    const ImVec2 DIGITIZER_SELECTION_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 200.0);
    RenderDigitizerSelection(DIGITIZER_SELECTION_POS, DIGITIZER_SELECTION_SIZE);

    const ImVec2 COMMAND_PALETTE_POS(0.0f, 200.0 + FRAME_HEIGHT);
    const ImVec2 COMMAND_PALETTE_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 200.0);
    RenderCommandPalette(COMMAND_PALETTE_POS, COMMAND_PALETTE_SIZE);

    const ImVec2 PARAMETERS_POS(0.0f, 400.0f + FRAME_HEIGHT);
    const ImVec2 PARAMETERS_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, height - 400.0 - FRAME_HEIGHT);
    RenderParameters(PARAMETERS_POS, PARAMETERS_SIZE);
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

    if (ImGui::BeginTable("Digitizers", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings))
    {
        if (m_digitizers.size() == 0)
        {
            ImGui::TableNextColumn();
            ImGui::Text("No digitizers found.");
        }
        else
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
                        memset(&m_selected[0], 0, sizeof(m_selected));
                    m_selected[i] ^= 1;
                }

                ImGui::TableNextColumn();
                if (m_digitizer_ui_state[i].status.size() > 0)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, m_digitizer_ui_state[i].color);
                    ImGui::SmallButton(m_digitizer_ui_state[i].status.c_str());
                    ImGui::PopStyleColor();
                }
                ImGui::TableNextRow();
            }
        }
        ImGui::EndTable();
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
    if (ImGui::Button("Set", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::SET_PARAMETERS);

    ImGui::SameLine();
    if (ImGui::Button("Get", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::GET_PARAMETERS);

    /* Second row */
    ImGui::BeginDisabled();
    ImGui::Button("Set Clock\nSystem", COMMAND_PALETTE_BUTTON_SIZE);
    ImGui::SameLine();
    ImGui::Button("Set\nSelection", COMMAND_PALETTE_BUTTON_SIZE);
    ImGui::EndDisabled();

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

void Ui::RenderParameters(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Parameters", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (ImGui::BeginTabBar("Parameters", ImGuiTabBarFlags_None))
    {
        /* TODO: The idea here would be to read & copy the parameters from the selected digitizers[0]-> */
        if (ImGui::BeginTabItem("Top"))
        {
            /* FIXME: Add label w/ path */
            /* FIXME: Could get fancy w/ only copying in ADQR_ELAST -> ADQR_EOK transition. */
            /* It's ok with a static pointer here as long as we keep the
                widget in read only mode. */
            static auto parameters = std::make_shared<std::string>("");
            for (size_t i = 0; i < m_digitizers.size(); ++i)
            {
                if (m_selected[i])
                {
                    m_digitizers[i]->WaitForParameters(parameters);
                    break;
                }
            }

            ImGui::InputTextMultiline("##top", parameters->data(), parameters->size(),
                                      ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Clock System"))
        {
            /* FIXME: Add label w/ path */
            static auto parameters = std::make_shared<std::string>("");
            for (size_t i = 0; i < m_digitizers.size(); ++i)
            {
                if (m_selected[i])
                {
                    m_digitizers[i]->WaitForClockSystemParameters(parameters);
                    break;
                }
            }
            ImGui::InputTextMultiline("##clocksystem", parameters->data(), parameters->size(),
                                      ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void Ui::PlotTimeDomainSelected()
{
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
        {
            if (m_records[i][ch] != NULL)
            {
                ImPlot::PlotLine(m_records[i][ch]->label.c_str(),
                                 m_records[i][ch]->time_domain->x.get(),
                                 m_records[i][ch]->time_domain->y.get(),
                                 m_records[i][ch]->time_domain->count);
            }
        }
    }
}

void Ui::RenderTimeDomain(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Time Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (ImPlot::BeginPlot("Time domain", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
    {
        ImPlot::SetupAxis(ImAxis_X1, "Time");
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

void Ui::PlotFourierTransformSelected()
{
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
        {
            if (m_records[i][ch] != NULL)
            {
                ImPlot::PlotLine(m_records[i][ch]->label.c_str(),
                                 m_records[i][ch]->frequency_domain->x.get(),
                                 m_records[i][ch]->frequency_domain->y.get(),
                                 m_records[i][ch]->frequency_domain->count / 2);
            }
        }
    }
}

void Ui::RenderFourierTransformPlot()
{
    if (ImPlot::BeginPlot("FFT##plot", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
    {
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 0.5);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -80.0, 0.0);
        ImPlot::SetupAxis(ImAxis_X1, "Hz");
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
            if (m_records[i][ch] != NULL)
            {
                ImPlot::PlotHeatmap("heat", m_records[i][ch]->waterfall->data.get(),
                                    m_records[i][ch]->waterfall->rows,
                                    m_records[i][ch]->waterfall->columns, -80, 0, NULL);
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
        ImPlot::SetupAxis(ImAxis_X1, "Hz");
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
    ImGui::Begin("Metrics##timedomain", NULL, ImGuiWindowFlags_NoMove);

    bool has_contents = false;
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
        {
            if (m_records[i][ch] != NULL)
            {
                if (has_contents)
                    ImGui::Separator();

                ImGui::Text("%s", m_records[i][ch]->label.c_str());
                ImGui::Text("Record number: %" PRIu32, m_records[i][ch]->time_domain->header.record_number);
                ImGui::Text("Maximum value: %.4f", m_records[i][ch]->time_domain_metrics.max);
                ImGui::Text("Minimum value: %.4f", m_records[i][ch]->time_domain_metrics.min);
                ImGui::Text("Estimated trigger frequency: %.4f Hz", m_records[i][ch]->time_domain->estimated_trigger_frequency);
            }

            has_contents = true;
        }
    }
    ImGui::End();
}

void Ui::RenderFrequencyDomainMetrics(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Metrics##frequencydomain", NULL, ImGuiWindowFlags_NoMove);

    bool has_contents = false;
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_selected[i])
            continue;

        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
        {
            if (m_records[i][ch] != NULL)
            {
                if (has_contents)
                    ImGui::Separator();

                ImGui::Text("%s", m_records[i][ch]->label.c_str());
                ImGui::Text("Record number: %" PRIu32, m_records[i][ch]->time_domain->header.record_number);
                ImGui::Text("Maximum value: %.4f", m_records[i][ch]->frequency_domain_metrics.max);
                ImGui::Text("Minimum value: %.4f", m_records[i][ch]->frequency_domain_metrics.min);
            }

            has_contents = true;
        }

    }
    ImGui::End();
}

