#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "implot.h"

#include "simulated_digitizer.h"

#ifndef SIMULATION_ONLY
#include "ADQAPI.h"
#endif

#include "GL/gl3w.h"
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstring>

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

static int display_w = 0;
static int display_h = 0;

static const float FIRST_COLUMN_RELATIVE_WIDTH = 0.2;
static const float SECOND_COLUMN_RELATIVE_WIDTH = 0.6;
static const float THIRD_COLUMN_RELATIVE_WIDTH = 0.2;

#define FIRST_COLUMN_SIZE (display_w * FIRST_COLUMN_RELATIVE_WIDTH)
#define SECOND_COLUMN_SIZE (display_w * SECOND_COLUMN_RELATIVE_WIDTH)
#define THIRD_COLUMN_SIZE (display_w * THIRD_COLUMN_RELATIVE_WIDTH)

#define FIRST_COLUMN_POSITION (0.0)
#define SECOND_COLUMN_POSITION (display_w * FIRST_COLUMN_RELATIVE_WIDTH)
#define THIRD_COLUMN_POSITION (display_w * (FIRST_COLUMN_RELATIVE_WIDTH + SECOND_COLUMN_RELATIVE_WIDTH))

void DoPlot(Digitizer &digitizer)
{
    const float FRAME_HEIGHT = ImGui::GetFrameHeight();
    const float PLOT_WINDOW_HEIGHT = (display_h - 1 * FRAME_HEIGHT) / 2;

    /* FIXME: Figure out something other than this manual unrolling. */
    std::shared_ptr<ProcessedRecord> processed_record0 = NULL;
    std::shared_ptr<ProcessedRecord> processed_record1 = NULL;
    int result0 = digitizer.WaitForProcessedRecord(0, processed_record0);
    int result1 = digitizer.WaitForProcessedRecord(1, processed_record1);

    ImGui::SetNextWindowPos(ImVec2(SECOND_COLUMN_POSITION, FRAME_HEIGHT));
    ImGui::SetNextWindowSize(ImVec2(SECOND_COLUMN_SIZE, PLOT_WINDOW_HEIGHT));
    ImGui::Begin("Time Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (ImPlot::BeginPlot("Time domain", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
    {
        ImPlot::SetupAxis(ImAxis_X1, "Time");
        ImPlot::SetupAxis(ImAxis_Y1, "f(x)");
        if (result0 >= 0)
        {
            ImPlot::PlotLine("CHA", processed_record0->time_domain->x,
                             processed_record0->time_domain->y,
                             processed_record0->time_domain->count);
        }
        if (result1 >= 0)
        {
            ImPlot::PlotLine("CHB", processed_record1->time_domain->x,
                             processed_record1->time_domain->y,
                             processed_record1->time_domain->count);
        }
        ImPlot::EndPlot();
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(SECOND_COLUMN_POSITION, FRAME_HEIGHT + PLOT_WINDOW_HEIGHT));
    ImGui::SetNextWindowSize(ImVec2(SECOND_COLUMN_SIZE, PLOT_WINDOW_HEIGHT));
    ImGui::Begin("Frequency Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (ImPlot::BeginPlot("FFT", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
    {
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 0.5);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -80.0, 0.0);
        ImPlot::SetupAxis(ImAxis_X1, "Hz");
        ImPlot::SetupAxis(ImAxis_Y1, "FFT");
        if (result0 >= 0)
        {
            ImPlot::PlotLine("CHA", processed_record0->frequency_domain->x,
                             processed_record0->frequency_domain->y,
                             processed_record0->frequency_domain->count / 2);
        }
        if (result1 >= 0)
        {
            ImPlot::PlotLine("CHB", processed_record1->frequency_domain->x,
                             processed_record1->frequency_domain->y,
                             processed_record1->frequency_domain->count / 2);
        }
        ImPlot::EndPlot();
    }
    ImGui::End();

    /* Metric windows */
    ImGui::SetNextWindowPos(ImVec2(THIRD_COLUMN_POSITION, FRAME_HEIGHT));
    ImGui::SetNextWindowSize(ImVec2(THIRD_COLUMN_SIZE, PLOT_WINDOW_HEIGHT));
    ImGui::Begin("Metrics##timedomain", NULL, ImGuiWindowFlags_NoMove);
    if (result0 >= 0)
    {
        ImGui::Text("Maximum value: %.4f", processed_record0->time_domain_metrics.max_value);
        ImGui::Text("Minimum value: %.4f", processed_record0->time_domain_metrics.min_value);
    }
    if (result1 >= 0)
    {
        ImGui::Text("Maximum value: %.4f", processed_record1->time_domain_metrics.max_value);
        ImGui::Text("Minimum value: %.4f", processed_record1->time_domain_metrics.min_value);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(THIRD_COLUMN_POSITION, FRAME_HEIGHT + PLOT_WINDOW_HEIGHT));
    ImGui::SetNextWindowSize(ImVec2(THIRD_COLUMN_SIZE, PLOT_WINDOW_HEIGHT));
    ImGui::Begin("Metrics##frequencydomain", NULL, ImGuiWindowFlags_NoMove);
    if (result0 >= 0)
    {
        ImGui::Text("Maximum value: %.4f", processed_record0->frequency_domain_metrics.max_value);
        ImGui::Text("Minimum value: %.4f", processed_record0->frequency_domain_metrics.min_value);
    }
    if (result1 >= 0)
    {
        ImGui::Text("Maximum value: %.4f", processed_record1->frequency_domain_metrics.max_value);
        ImGui::Text("Minimum value: %.4f", processed_record1->frequency_domain_metrics.min_value);
    }
    ImGui::End();

}

int main(int, char **)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return -1;

#if defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWwindow *window = glfwCreateWindow(1920, 1200, "ADQ Rapid", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    if (gl3wInit())
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImGui::StyleColorsDark();

    bool show_imgui_demo_window = false;
    bool show_implot_demo_window = false;

    SimulatedDigitizer digitizer;
    digitizer.Initialize();
    digitizer.Start();

#ifdef SIMULATION_ONLY
    int nof_devices = 4;
#else
    void *adq_cu = CreateADQControlUnit();
    if (adq_cu == NULL)
    {
        printf("Failed to create an ADQControlUnit.\n");
    }

    struct ADQInfoListEntry *adq_list = NULL;
    int nof_devices = 0;
    if (!ADQControlUnit_ListDevices(adq_cu, &adq_list, (unsigned int *)&nof_devices))
    {
        printf("Failed to list devices.\n");
    }
    printf("Found %d devices.\n", nof_devices);

    struct ADQInfoListEntry dummy_adq_list[4];
    for (int i = 0; i < sizeof(dummy_adq_list) / sizeof(dummy_adq_list[0]); ++i)
    {
        dummy_adq_list[i].ProductID = PID_ADQ3;
    }

    if (nof_devices == 0)
    {
        adq_list = dummy_adq_list;
        nof_devices = sizeof(dummy_adq_list) / sizeof(dummy_adq_list[0]);
    }
#endif

    // std::string data_directory = "/home/marcus/.local/share/adq-rapid";
    // std::filesystem::path data_directory_path(data_directory);
    // if (std::filesystem::exists(data_directory_path))
    // {
    //     printf("Path %s exists!\n", data_directory_path.c_str());
    // }
    // else
    // {
    //     printf("Path does not exist, creating.\n");
    //     std::error_code e;
    //     if (std::filesystem::create_directory(data_directory_path, e))
    //     {
    //         printf("Created %s.\n", data_directory_path.c_str());
    //     }
    //     else
    //     {
    //         printf("Failed to create %s, error code %d.\n", data_directory_path.c_str(), e.value());
    //     }
    // }

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
        // const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

        ImGui::BeginMainMenuBar();
        if (ImGui::MenuItem("Quit"))
        {
            glfwSetWindowShouldClose(window, 1);
        }
        if (ImGui::BeginMenu("Demo"))
        {
            ImGui::MenuItem("ImGui", NULL, &show_imgui_demo_window);
            ImGui::MenuItem("ImPlot", NULL, &show_implot_demo_window);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Style"))
        {
            static int style_idx = 0;
            if (ImGui::Combo("##combostyle", &style_idx, "Dark\0Light\0"))
            {
                switch (style_idx)
                {
                case 0: ImGui::StyleColorsDark(); break;
                case 1: ImGui::StyleColorsLight(); break;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();

        float frame_height = ImGui::GetFrameHeight();
        ImGui::SetNextWindowPos(ImVec2(FIRST_COLUMN_POSITION, frame_height));
        ImGui::SetNextWindowSize(ImVec2(FIRST_COLUMN_SIZE, 200.0));
        ImGui::Begin("Digitizers", NULL, ImGuiWindowFlags_NoMove);
        static bool selected[16] = {0};

        if (ImGui::Button("Select All"))
        {
            for (int i = 0; i < nof_devices; ++i)
                selected[i] = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Deselect All"))
        {
            for (int i = 0; i < nof_devices; ++i)
                selected[i] = false;
        }
        ImGui::Separator();

        if (ImGui::BeginTable("Digitizers", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings))
        {
            if (nof_devices == 0)
            {
                ImGui::TableNextColumn();
                ImGui::Text("No digitizers found.");
            }
            else
            {
                ImGui::TableSetupColumn("Identifier", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, TEXT_BASE_WIDTH * 20.0f);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
                ImGui::TableSetupColumn("Extra");
                ImGui::TableHeadersRow();

                for (int i = 0; i < nof_devices; ++i)
                {
                    char label[32];
                    std::sprintf(label, "Device %d", i);
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(label, selected[i], ImGuiSelectableFlags_SpanAllColumns))
                    {
                        if (!ImGui::GetIO().KeyCtrl)
                            memset(selected, 0, sizeof(selected));
                        selected[i] ^= 1;
                    }
                    ImGui::TableNextColumn();
                    ImVec4 color(0.0f, 1.0f, 0.5f, 0.6f);
                    ImGui::PushStyleColor(ImGuiCol_Button, color);
                    std::sprintf(label, "OK##%d", i);
                    if (ImGui::SmallButton(label))
                    {
                        printf("OK! %d\n", i);
                    }
                    ImGui::PopStyleColor();
                    ImGui::TableNextRow();
                }
            }
            ImGui::EndTable();
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(FIRST_COLUMN_POSITION, 200.0 + frame_height));
        ImGui::SetNextWindowSize(ImVec2(FIRST_COLUMN_SIZE, 200.0));
        ImGui::Begin("Command Palette");
        std::stringstream ss;
        bool any_selected = false;
        for (int i = 0; i < nof_devices; ++i)
        {
            if (selected[i])
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
            digitizer.PushMessage(DigitizerMessage(DigitizerMessageId::START_ACQUISITION));
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("Stop!\n");
            digitizer.PushMessage(DigitizerMessage(DigitizerMessageId::STOP_ACQUISITION));
        }
        ImGui::SameLine();
        if (ImGui::Button("Set", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("Set!\n");
            digitizer.PushMessage(DigitizerMessage(DigitizerMessageId::SET_PARAMETERS));
        }
        ImGui::SameLine();
        if (ImGui::Button("Get", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("Get!\n");
            digitizer.PushMessage(DigitizerMessage(DigitizerMessageId::GET_PARAMETERS));
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
            digitizer.PushMessage(DigitizerMessage(DigitizerMessageId::INITIALIZE_PARAMETERS));
        }
        ImGui::SameLine();
        if (ImGui::Button("Validate", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("Validate!\n");
            digitizer.PushMessage(DigitizerMessage(DigitizerMessageId::VALIDATE_PARAMETERS));
        }
        ImGui::End();


        ImGui::SetNextWindowPos(ImVec2(FIRST_COLUMN_POSITION, 400.0 + frame_height));
        ImGui::SetNextWindowSize(ImVec2(FIRST_COLUMN_SIZE, display_h - 400.0 - frame_height));
        ImGui::Begin("Configuration", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        if (ImGui::BeginTabBar("Parameters", ImGuiTabBarFlags_None))
        {
            /* TODO: The idea here would be to read & copy the parameters from the selected digitizer. */
            if (ImGui::BeginTabItem("Parameters"))
            {
                /* FIXME: Add label w/ path */
                /* FIXME: Could get fancy w/ only copying in ADQR_ELAST -> ADQR_EOK transition. */
                /* It's ok with a static pointer here as long as we keep the
                   widget in read only mode. */
                static auto parameters = std::make_shared<std::string>("");
                digitizer.WaitForParameters(parameters);
                ImGui::InputTextMultiline("##parameters", parameters->data(), parameters->size(),
                                          ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Clock System"))
            {
                /* FIXME: Add label w/ path */
                static auto parameters = std::make_shared<std::string>("");
                digitizer.WaitForClockSystemParameters(parameters);
                ImGui::InputTextMultiline("##clocksystem", parameters->data(), parameters->size(),
                                          ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        DoPlot(digitizer);

        if (show_imgui_demo_window)
            ImGui::ShowDemoWindow(&show_imgui_demo_window);

        if (show_implot_demo_window)
            ImPlot::ShowDemoWindow(&show_implot_demo_window);

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    printf("Stopping\n");

    /* FIXME: wait until threads close */
    digitizer.PushMessage(DigitizerMessage(DigitizerMessageId::STOP_ACQUISITION));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    digitizer.Stop();
#ifndef SIMULATION_ONLY
    if (adq_cu != NULL)
    {
        DeleteADQControlUnit(adq_cu);
    }
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
