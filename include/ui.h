#ifndef UI_H_TLXUOY
#define UI_H_TLXUOY

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "implot.h"
#include "GL/gl3w.h"
#include <GLFW/glfw3.h>

#include "simulated_digitizer.h"

#ifndef SIMULATOR_ONLY
#include "gen4_digitizer.h"
#endif

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
    void *m_adq_control_unit;
    bool m_show_imgui_demo_window;
    bool m_show_implot_demo_window;
    std::unique_ptr<bool[]> m_selected;

    /* FIXME: Extremely temporary pointers to the current data. */
    std::shared_ptr<ProcessedRecord> m_record0;
    std::shared_ptr<ProcessedRecord> m_record1;

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

    /* FIXME: Try const */
    static constexpr float FIRST_COLUMN_RELATIVE_WIDTH = 0.2f;
    static constexpr float SECOND_COLUMN_RELATIVE_WIDTH = 0.6f;
    static constexpr float THIRD_COLUMN_RELATIVE_WIDTH = 0.2f;
};

#endif
