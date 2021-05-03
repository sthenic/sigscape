#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "implot.h"

#include "data_processing.h"
#include "simulator.h"

#include "GL/gl3w.h"
#include <GLFW/glfw3.h>

static char text[1024 * 16] =
    "{\n"
    "    \"_id\": \"608472ef48e54b8f8c0b6fd8\",\n"
    "    \"index\": 0,\n"
    "    \"guid\": \"13424586-b852-4ac9-b530-12bb04b29000\",\n"
    "    \"isActive\": true,\n"
    "    \"balance\": \"$3,814.57\",\n"
    "    \"picture\": \"http://placehold.it/32x32\",\n"
    "    \"age\": 40,\n"
    "    \"eyeColor\": \"green\",\n"
    "    \"name\": \"Tabatha Guerrero\",\n"
    "    \"gender\": \"female\",\n"
    "    \"company\": \"CYCLONICA\",\n"
    "    \"email\": \"tabathaguerrero@cyclonica.com\",\n"
    "    \"phone\": \"+1 (826) 529-3897\",\n"
    "    \"address\": \"222 Brevoort Place, Navarre, Oregon, 5132\",\n"
    "    \"about\": \"Minim est aliqua amet et veniam. Exercitation minim dolor id nisi veniam "
    "quis nostrud irure. Incididunt nostrud occaecat labore quis commodo ad esse non laborum velit "
    "ipsum pariatur aliquip aute. Eu aliquip laboris laborum proident elit sit exercitation culpa "
    "exercitation in sit proident. Mollit proident consequat culpa pariatur velit exercitation "
    "voluptate dolor qui adipisicing. Sint in culpa aliquip ea eu nulla proident.\r\n\",\n"
    "    \"registered\": \"2016-11-02T02:24:09 -01:00\",\n"
    "    \"latitude\": -9.873761,\n"
    "    \"longitude\": -132.827924,\n"
    "    \"tags\": [\n"
    "        \"ad\",\n"
    "        \"proident\",\n"
    "        \"anim\",\n"
    "        \"eu\",\n"
    "        \"ipsum\",\n"
    "        \"labore\",\n"
    "        \"nulla\"\n"
    "    ],\n"
    "    \"friends\": [\n"
    "        {\n"
    "            \"id\": 0,\n"
    "            \"name\": \"Prince Battle\"\n"
    "        },\n"
    "        {\n"
    "            \"id\": 1,\n"
    "            \"name\": \"Orr Torres\"\n"
    "        },\n"
    "        {\n"
    "            \"id\": 2,\n"
    "            \"name\": \"Jennings Skinner\"\n"
    "        }\n"
    "    ],\n"
    "    \"greeting\": \"Hello, Tabatha Guerrero! You have 9 unread messages.\",\n"
    "    \"favoriteFruit\": \"banana\"\n"
    "}\n";

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

    bool show_demo_window = false;
    bool show_plot_demo_window = false;

    DataAcquisitionSimulator acquisition_a;
    DataAcquisitionSimulator acquisition_b;
    DataProcessing processing_a(acquisition_a);
    DataProcessing processing_b(acquisition_b);
    ProcessedRecord stored_processed_record_a(65536, true);
    ProcessedRecord stored_processed_record_b(65536, true);

    acquisition_a.Initialize(10000, 2);
    processing_a.Initialize();

    Simulator::SineWave sine_b;
    sine_b.amplitude = 0.5;
    acquisition_b.Initialize(10000, 30, sine_b);
    processing_b.Initialize();

    acquisition_a.Start();
    processing_a.Start();
    acquisition_b.Start();
    processing_b.Start();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        // glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z
        // * clear_color.w, clear_color.w);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::BeginMainMenuBar();
        if (ImGui::MenuItem("Run"))
        {
            printf("Pressed run\n");
        }
        if (ImGui::MenuItem("Quit"))
        {
            printf("Closing\n");
            glfwSetWindowShouldClose(window, 1);
        }
        ImGui::EndMainMenuBar();

        float frame_height = ImGui::GetFrameHeight();
        ImGui::SetNextWindowPos(ImVec2(0.0, frame_height));
        ImGui::SetNextWindowSize(ImVec2(display_w / 2, display_h - frame_height));
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
        if (ImPlot::BeginPlot("Line Plot", "x", "f(x)", ImVec2(-1, -1),
                              ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle))
        {
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

        ImPlot::SetNextPlotLimitsX(0.0, 0.5);
        ImPlot::SetNextPlotLimitsY(-80.0, 0.0);
        if (ImPlot::BeginPlot("Line Plot", "x", "f(x)", ImVec2(-1, -1),
                              ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle,
                              ImPlotAxisFlags_None, ImPlotAxisFlags_None))
        {
            ImPlot::PlotLine("CHA", stored_processed_record_a.frequency_domain->x,
                             stored_processed_record_a.frequency_domain->y,
                             stored_processed_record_a.frequency_domain->count / 2);
            ImPlot::PlotLine("CHB", stored_processed_record_b.frequency_domain->x,
                             stored_processed_record_b.frequency_domain->y,
                             stored_processed_record_b.frequency_domain->count / 2);
            ImPlot::EndPlot();
        }
        ImGui::End();

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        if (show_plot_demo_window)
            ImPlot::ShowDemoWindow(&show_plot_demo_window);

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    printf("Stopping\n");
    processing_a.Stop();
    acquisition_a.Stop();
    processing_b.Stop();
    acquisition_b.Stop();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
