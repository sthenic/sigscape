#ifndef UI_H_TLXUOY
#define UI_H_TLXUOY

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "implot.h"
#include "GL/gl3w.h"
#include <GLFW/glfw3.h>

#include "digitizer.h"

class Ui
{
public:
    Ui();
    ~Ui();

    void Initialize(GLFWwindow *window, const char *glsl_version);
    void Render(float width, float height);
    void Terminate();

private:
    std::vector<std::unique_ptr<Digitizer>> m_digitizers;
    std::vector<std::vector<std::shared_ptr<ProcessedRecord>>> m_records;
    void *m_adq_control_unit;
    bool m_show_imgui_demo_window;
    bool m_show_implot_demo_window;
    std::unique_ptr<bool[]> m_selected;

    struct DigitizerUiState
    {
        std::string identifier;
        std::string status;
        std::string extra;
        ImVec4 color;
        ImVec4 text_color;
    };
    std::unique_ptr<DigitizerUiState[]> m_digitizer_ui_state;

#ifdef SIMULATION_ONLY
    MockAdqApi m_mock_adqapi;
#endif

    void InitializeDigitizers();
    void PushMessage(DigitizerMessageId id, bool selected = true);

    void UpdateRecords();
    void HandleMessage(size_t i, const DigitizerMessage &message);
    void HandleMessages();

    void RenderMenuBar();
    void RenderRight(float width, float height);
    void RenderCenter(float width, float height);
    void RenderLeft(float width, float height);

    void RenderDigitizerSelection(const ImVec2 &position, const ImVec2 &size);
    void RenderCommandPalette(const ImVec2 &position, const ImVec2 &size);
    void RenderParameters(const ImVec2 &position, const ImVec2 &size);
    void RenderTimeDomain(const ImVec2 &position, const ImVec2 &size);
    void RenderFrequencyDomain(const ImVec2 &position, const ImVec2 &size);
    void RenderFourierTransformPlot();
    void RenderWaterfallPlot();

    static constexpr float FIRST_COLUMN_RELATIVE_WIDTH = 0.2f;
    static constexpr float SECOND_COLUMN_RELATIVE_WIDTH = 0.6f;
    static constexpr float THIRD_COLUMN_RELATIVE_WIDTH = 0.2f;

    static const ImVec4 COLOR_GREEN;
    static const ImVec4 COLOR_RED;
    static const ImVec4 COLOR_YELLOW;
    static const ImVec4 COLOR_ORANGE;
};

#endif
