#include "ui.h"
#include <cinttypes>

Ui::Ui()
    : m_digitizers{}
    , m_records{}
    , m_adq_control_unit()
    , m_show_imgui_demo_window(false)
    , m_show_implot_demo_window(false)
    , m_selected()
{
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

    m_digitizers.clear();
#ifndef SIMULATION_ONLY
    m_adq_control_unit = CreateADQControlUnit();
    if (m_adq_control_unit == NULL)
        printf("Failed to create an ADQControlUnit.\n");
    InitializeGen4Digitizers();
#endif

    if (m_digitizers.size() == 0)
        InitializeSimulatedDigitizers();

    m_selected = std::unique_ptr<bool[]>( new bool[m_digitizers.size()] );
    memset(&m_selected[0], 0, sizeof(m_selected));

    for (const auto &d : m_digitizers)
    {
        d->Start();
    }
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

void Ui::InitializeGen4Digitizers()
{
#ifndef SIMULATION_ONLY
    /* Filter out the Gen4 digitizers and construct a digitizer object for each one. */
    struct ADQInfoListEntry *adq_list = NULL;
    int nof_devices = 0;
    if (!ADQControlUnit_ListDevices(m_adq_control_unit, &adq_list, (unsigned int *)&nof_devices))
        return;

    int nof_gen4_digitizers = 0;
    for (int i = 0; i < nof_devices; ++i)
    {
        if (adq_list[i].ProductID == PID_ADQ3)
        {
            if (ADQControlUnit_OpenDeviceInterface(m_adq_control_unit, i))
                nof_gen4_digitizers++;
        }
    }

    for (int i = 0; i < nof_gen4_digitizers; ++i)
        m_digitizers.push_back(std::make_unique<Gen4Digitizer>(m_adq_control_unit, i + 1));

    /* FIXME: Remove */
    if (m_digitizers.size() == 0)
        printf("No Gen4 digitizers.\n");
#endif
}

void Ui::InitializeSimulatedDigitizers()
{
    printf("Using simulator.\n");
    for (int i = 0; i < 1; ++i)
    {
        m_digitizers.push_back(std::make_unique<SimulatedDigitizer>(i));

        /* FIXME: array type instead? */
        std::vector<std::shared_ptr<ProcessedRecord>> tmp;
        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
            tmp.push_back(std::shared_ptr<ProcessedRecord>());
        m_records.push_back(tmp);
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
        for (int ch = 0; ch < ADQ_MAX_NOF_CHANNELS; ++ch)
            m_digitizers[i]->WaitForProcessedRecord(ch, m_records[i][ch]);
    }
}

void Ui::HandleMessages()
{
    /* FIXME: Implement */
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
    /* FIXME: Move into functions? */
    ImGui::SetNextWindowPos(POSITION_UPPER);
    ImGui::SetNextWindowSize(SIZE);
    ImGui::Begin("Metrics##timedomain", NULL, ImGuiWindowFlags_NoMove);
    if (m_records[0][0] != NULL)
    {
        ImGui::Text("Record number: %" PRIu64, m_records[0][0]->time_domain->header.record_number);
        ImGui::Text("Maximum value: %.4f", m_records[0][0]->time_domain_metrics.max);
        ImGui::Text("Minimum value: %.4f", m_records[0][0]->time_domain_metrics.min);
        ImGui::Text("Estimated trigger frequency: %.4f Hz", m_records[0][0]->time_domain->estimated_trigger_frequency);
    }
    if (m_records[0][1] != NULL)
    {
        ImGui::Text("Record number: %" PRIu64, m_records[0][1]->time_domain->header.record_number);
        ImGui::Text("Maximum value: %.4f", m_records[0][1]->time_domain_metrics.max);
        ImGui::Text("Minimum value: %.4f", m_records[0][1]->time_domain_metrics.min);
        ImGui::Text("Estimated trigger frequency: %.4f Hz", m_records[0][1]->time_domain->estimated_trigger_frequency);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(POSITION_LOWER);
    ImGui::SetNextWindowSize(SIZE);
    ImGui::Begin("Metrics##frequencydomain", NULL, ImGuiWindowFlags_NoMove);
    if (m_records[0][0] != NULL)
    {
        ImGui::Text("Record number: %" PRIu64, m_records[0][0]->frequency_domain->header.record_number);
        ImGui::Text("Maximum value: %.4f", m_records[0][0]->frequency_domain_metrics.max);
        ImGui::Text("Minimum value: %.4f", m_records[0][0]->frequency_domain_metrics.min);
    }
    if (m_records[0][1] != NULL)
    {
        ImGui::Text("Record number: %" PRIu64, m_records[0][1]->frequency_domain->header.record_number);
        ImGui::Text("Maximum value: %.4f", m_records[0][1]->frequency_domain_metrics.max);
        ImGui::Text("Minimum value: %.4f", m_records[0][1]->frequency_domain_metrics.min);
    }
    ImGui::End();
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
                                    TEXT_BASE_WIDTH * 20.0f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed,
                                    TEXT_BASE_WIDTH * 12.0f);
            ImGui::TableSetupColumn("Extra");
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < m_digitizers.size(); ++i)
            {
                char label[32];
                std::sprintf(label, "Initializing... %zu", i);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(label, m_selected[i], ImGuiSelectableFlags_SpanAllColumns))
                {
                    if (!ImGui::GetIO().KeyCtrl)
                        memset(&m_selected[0], 0, sizeof(m_selected));
                    m_selected[i] ^= 1;
                }
                ImGui::TableNextColumn();
                ImVec4 color(0.0f, 1.0f, 0.5f, 0.6f);
                ImGui::PushStyleColor(ImGuiCol_Button, color);
                std::sprintf(label, "OK##%zu", i);
                if (ImGui::SmallButton(label))
                {
                    printf("OK! %zu\n", i);
                }
                ImGui::PopStyleColor();
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
    std::stringstream ss;
    bool any_selected = false;
    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (m_selected[i])
        {
            if (!any_selected)
                ss << "Commands will be applied to device ";
            else
                ss << ", ";

            ss << i;
            any_selected = true;
        }
    }

    if (!any_selected)
        ss << "No digitizer available.";

    ImGui::Text("%s", ss.str().c_str());

    const ImVec2 COMMAND_PALETTE_BUTTON_SIZE{85, 50};
    if (ImGui::Button("Start", COMMAND_PALETTE_BUTTON_SIZE))
    {
        printf("Start!\n");
        m_digitizers[0]->PushMessage(DigitizerMessage(DigitizerMessageId::START_ACQUISITION));
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop", COMMAND_PALETTE_BUTTON_SIZE))
    {
        printf("Stop!\n");
        m_digitizers[0]->PushMessage(DigitizerMessage(DigitizerMessageId::STOP_ACQUISITION));
    }
    ImGui::SameLine();
    if (ImGui::Button("Set", COMMAND_PALETTE_BUTTON_SIZE))
    {
        printf("Set!\n");
        m_digitizers[0]->PushMessage(DigitizerMessage(DigitizerMessageId::SET_PARAMETERS));
    }
    ImGui::SameLine();
    if (ImGui::Button("Get", COMMAND_PALETTE_BUTTON_SIZE))
    {
        printf("Get!\n");
        m_digitizers[0]->PushMessage(DigitizerMessage(DigitizerMessageId::GET_PARAMETERS));
    }
    ImGui::BeginDisabled();
    if (ImGui::Button("SetPorts", COMMAND_PALETTE_BUTTON_SIZE))
    {
        printf("SetPorts!\n");
    }
    ImGui::SameLine();
    if (ImGui::Button("Set\nSelection", COMMAND_PALETTE_BUTTON_SIZE))
    {
        printf("SetSelection!\n");
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Initialize", COMMAND_PALETTE_BUTTON_SIZE))
    {
        printf("Initialize!\n");
        m_digitizers[0]->PushMessage(DigitizerMessage(DigitizerMessageId::INITIALIZE_PARAMETERS));
    }
    ImGui::SameLine();
    if (ImGui::Button("Validate", COMMAND_PALETTE_BUTTON_SIZE))
    {
        printf("Validate!\n");
        m_digitizers[0]->PushMessage(DigitizerMessage(DigitizerMessageId::VALIDATE_PARAMETERS));
    }
    ImGui::End();
}

void Ui::RenderParameters(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Configuration", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (ImGui::BeginTabBar("Parameters", ImGuiTabBarFlags_None))
    {
        /* TODO: The idea here would be to read & copy the parameters from the selected digitizers[0]-> */
        if (ImGui::BeginTabItem("Parameters"))
        {
            /* FIXME: Add label w/ path */
            /* FIXME: Could get fancy w/ only copying in ADQR_ELAST -> ADQR_EOK transition. */
            /* It's ok with a static pointer here as long as we keep the
                widget in read only mode. */
            static auto parameters = std::make_shared<std::string>("");
            m_digitizers[0]->WaitForParameters(parameters);
            ImGui::InputTextMultiline("##parameters", parameters->data(), parameters->size(),
                                        ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Clock System"))
        {
            /* FIXME: Add label w/ path */
            static auto parameters = std::make_shared<std::string>("");
            m_digitizers[0]->WaitForClockSystemParameters(parameters);
            ImGui::InputTextMultiline("##clocksystem", parameters->data(), parameters->size(),
                                        ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

}

void Ui::RenderTimeDomain(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Time Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (ImPlot::BeginPlot("Time domain", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
    {
        ImPlot::SetupAxis(ImAxis_X1, "Time");
        if (m_records[0][0] != NULL)
        {
            ImPlot::PlotLine("CHA", m_records[0][0]->time_domain->x.get(),
                             m_records[0][0]->time_domain->y.get(),
                             m_records[0][0]->time_domain->count);
        }
        if (m_records[0][1] != NULL)
        {
            ImPlot::PlotLine("CHB", m_records[0][1]->time_domain->x.get(),
                             m_records[0][1]->time_domain->y.get(),
                             m_records[0][1]->time_domain->count);
        }
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

void Ui::RenderFourierTransformPlot()
{
    if (ImPlot::BeginPlot("FFT##plot", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
    {
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 0.5);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -80.0, 0.0);
        ImPlot::SetupAxis(ImAxis_X1, "Hz");
        if (m_records[0][0] != NULL)
        {
            ImPlot::PlotLine("CHA", m_records[0][0]->frequency_domain->x.get(),
                             m_records[0][0]->frequency_domain->y.get(),
                             m_records[0][0]->frequency_domain->count / 2);
        }
        if (m_records[0][1] != NULL)
        {
            ImPlot::PlotLine("CHB", m_records[0][1]->frequency_domain->x.get(),
                             m_records[0][1]->frequency_domain->y.get(),
                             m_records[0][1]->frequency_domain->count / 2);
        }
        ImPlot::EndPlot();
    }
}

void Ui::RenderWaterfallPlot()
{
    ImPlot::PushColormap("Hot");
    if (ImPlot::BeginPlot("Waterfall##plot", ImVec2(-1, -1), ImPlotFlags_NoTitle | ImPlotFlags_NoLegend))
    {
        ImPlot::SetupAxis(ImAxis_X1, "Hz");
        if (m_records[0][0] != NULL)
        {
            ImPlot::PlotHeatmap("heat", m_records[0][0]->waterfall->data.get(),
                                m_records[0][0]->waterfall->rows,
                                m_records[0][0]->waterfall->columns, -80, 0, NULL);
        }
        ImPlot::EndPlot();
    }
    ImPlot::PopColormap();
}


