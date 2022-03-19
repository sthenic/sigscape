#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "implot.h"

#include "data_processing.h"
#include "simulator.h"

#include "ADQAPI.h"

#include "GL/gl3w.h"
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <filesystem>

static char text[1024 * 16] = "";

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char **)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return -1;

    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

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

    bool show_imgui_demo_window = true;
    bool show_implot_demo_window = false;

    DataAcquisitionSimulator acquisition_a;
    DataAcquisitionSimulator acquisition_b;
    DataProcessing processing_a(acquisition_a);
    DataProcessing processing_b(acquisition_b);
    ProcessedRecord stored_processed_record_a(65536, true);
    ProcessedRecord stored_processed_record_b(65536, true);

    Simulator::SineWave sine_a;
    sine_a.amplitude = 0.5;
    sine_a.frequency = 17e6;
    sine_a.noise_std_dev = 0.01;
    sine_a.sampling_frequency = 2.5e9;
    sine_a.harmonic_distortion = true;

    Simulator::SineWave sine_b;
    sine_b.amplitude = 0.5;
    sine_b.frequency = 800e3;
    sine_b.noise_std_dev = 0.01;
    sine_b.sampling_frequency = 2.5e9;

    auto Initialize = [&]()
    {
        acquisition_a.Initialize(10000, 2, sine_a);
        processing_a.Initialize();
        acquisition_b.Initialize(10000, 30, sine_b);
        processing_b.Initialize();
    };

    auto Start = [&]()
    {
        Initialize();
        processing_a.Start();
        processing_b.Start();
    };

    auto Stop = [&]()
    {
        processing_a.Stop();
        processing_b.Stop();
    };

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
        int display_w, display_h;
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
            if (ImGui::Combo("", &style_idx, "Dark\0Light\0"))
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
        ImGui::SetNextWindowPos(ImVec2(0.0, frame_height));
        ImGui::SetNextWindowSize(ImVec2(display_w / 2, 200.0));
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

        ImGui::SetNextWindowPos(ImVec2(0.0, 200.0 + frame_height));
        ImGui::SetNextWindowSize(ImVec2(display_w / 2, 200.0));
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

        const ImVec2 COMMAND_PALETTE_BUTTON_SIZE{90, 50};
        if (ImGui::Button("Start", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("Start!\n");
            Start();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled();
        if (ImGui::Button("Stop", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("Stop!\n");
            Stop();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Set", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("Set!\n");
        }
        ImGui::SameLine();
        if (ImGui::Button("Get", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("Get!\n");
        }
        ImGui::SameLine();
        if (ImGui::Button("Initialize", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("Initialize!\n");
        }
        ImGui::SameLine();
        if (ImGui::Button("Validate", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("Validate!\n");
        }
        if (ImGui::Button("SetPorts", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("SetPorts!\n");
        }
        ImGui::SameLine();
        if (ImGui::Button("SetSelection", COMMAND_PALETTE_BUTTON_SIZE))
        {
            printf("SetSelection!\n");
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0.0, 400.0 + frame_height));
        ImGui::SetNextWindowSize(ImVec2(display_w / 2, display_h - 400.0 - frame_height));
        ImGui::Begin("Parameters", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::InputTextMultiline("##parameters", text, IM_ARRAYSIZE(text),
                                  ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_AllowTabInput);
        ImGui::End();

        float plot_window_height = (display_h - 1 * frame_height) / 2;

        struct ProcessedRecord *processed_record = NULL;
        if (processing_a.WaitForBuffer(processed_record, 0) == 0)
        {
            stored_processed_record_a = *processed_record;
            processing_a.ReturnBuffer(processed_record);
        }
        if (processing_b.WaitForBuffer(processed_record, 0) == 0)
        {
            stored_processed_record_b = *processed_record;
            processing_b.ReturnBuffer(processed_record);
        }

        ImGui::SetNextWindowPos(ImVec2(display_w / 2, frame_height));
        ImGui::SetNextWindowSize(ImVec2(display_w / 2, plot_window_height));
        ImGui::Begin("Time Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        if (ImPlot::BeginPlot("Time domain", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
        {
            ImPlot::SetupAxis(ImAxis_X1, "Time");
            ImPlot::SetupAxis(ImAxis_Y1, "f(x)");
            ImPlot::PlotLine("CHA", stored_processed_record_a.time_domain->x,
                             stored_processed_record_a.time_domain->y,
                             stored_processed_record_a.time_domain->count);
            ImPlot::PlotLine("CHB", stored_processed_record_b.time_domain->x,
                             stored_processed_record_b.time_domain->y,
                             stored_processed_record_b.time_domain->count);
            ImPlot::EndPlot();
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(display_w / 2, frame_height + plot_window_height));
        ImGui::SetNextWindowSize(ImVec2(display_w / 2, plot_window_height));
        ImGui::Begin("Frequency Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        if (ImPlot::BeginPlot("FFT", ImVec2(-1, -1), ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
        {
            ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 0.5);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -80.0, 0.0);
            ImPlot::SetupAxis(ImAxis_X1, "Hz");
            ImPlot::SetupAxis(ImAxis_Y1, "FFT");
            ImPlot::PlotLine("CHA", stored_processed_record_a.frequency_domain->x,
                             stored_processed_record_a.frequency_domain->y,
                             stored_processed_record_a.frequency_domain->count / 2);
            ImPlot::PlotLine("CHB", stored_processed_record_b.frequency_domain->x,
                             stored_processed_record_b.frequency_domain->y,
                             stored_processed_record_b.frequency_domain->count / 2);
            ImPlot::EndPlot();
        }
        ImGui::End();

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
    processing_a.Stop();
    processing_b.Stop();

    if (adq_cu != NULL)
    {
        DeleteADQControlUnit(adq_cu);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
