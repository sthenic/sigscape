#ifndef UI_H_TLXUOY
#define UI_H_TLXUOY

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "implot.h"
#include "GL/gl3w.h"
#include <GLFW/glfw3.h>

#include "digitizer.h"
#include "identification.h"

class Ui
{
public:
    Ui();
    ~Ui();

    void Initialize(GLFWwindow *window, const char *glsl_version);
    void Render(float width, float height);
    void Terminate();

private:
    Identification m_identification;
    std::vector<std::shared_ptr<Digitizer>> m_digitizers;
    void *m_adq_control_unit;
    bool m_show_imgui_demo_window;
    bool m_show_implot_demo_window;
    bool m_is_time_domain_collapsed;
    bool m_is_frequency_domain_collapsed;
    std::unique_ptr<bool[]> m_selected;

    struct ChannelUiState
    {
        ChannelUiState();

        bool sample_markers;
        std::shared_ptr<ProcessedRecord> record;
    };

    struct DigitizerUiState
    {
        DigitizerUiState(int nof_channels);

        std::string identifier;
        std::string status;
        std::string extra;
        ImVec4 status_color;
        ImVec4 set_top_color;
        ImVec4 set_clock_system_color;

        std::vector<ChannelUiState> channels;
    };
    std::vector<DigitizerUiState> m_digitizer_ui_state;

    void PushMessage(const DigitizerMessage &message, bool selected = true);

    void UpdateRecords();
    void HandleMessage(const IdentificationMessage &message);
    void HandleMessage(size_t i, const DigitizerMessage &message);
    void HandleMessages();

    void RenderMenuBar();
    void RenderRight(float width, float height);
    void RenderCenter(float width, float height);
    void RenderLeft(float width, float height);

    void RenderDigitizerSelection(const ImVec2 &position, const ImVec2 &size);
    void RenderCommandPalette(const ImVec2 &position, const ImVec2 &size);
    void RenderSetTopParametersButton(const ImVec2 &size);
    void RenderSetClockSystemParametersButton(const ImVec2 &size);
    void RenderProcessingOptions(const ImVec2 &position, const ImVec2 &size);

    void Reduce(double xsize, double sampling_frequency, int &count, int &stride);
    static void MetricFormatter(double value, char *tick_label, int size, void *data);
    void PlotTimeDomainSelected();
    void RenderTimeDomain(const ImVec2 &position, const ImVec2 &size);
    void RenderFrequencyDomain(const ImVec2 &position, const ImVec2 &size);

    void Annotate(const std::pair<double, double> &point, const std::string &label = "");
    void PlotFourierTransformSelected();
    void RenderFourierTransformPlot();

    void PlotWaterfallSelected();
    void RenderWaterfallPlot();

    void RenderTimeDomainMetrics(const ImVec2 &position, const ImVec2 &size);
    void RenderFrequencyDomainMetrics(const ImVec2 &position, const ImVec2 &size);
    void RenderApplicationMetrics(const ImVec2 &position, const ImVec2 &size);

    static constexpr float FIRST_COLUMN_RELATIVE_WIDTH = 0.2f;
    static constexpr float SECOND_COLUMN_RELATIVE_WIDTH = 0.6f;
    static constexpr float THIRD_COLUMN_RELATIVE_WIDTH = 0.2f;

    static const ImVec4 COLOR_GREEN;
    static const ImVec4 COLOR_RED;
    static const ImVec4 COLOR_YELLOW;
    static const ImVec4 COLOR_ORANGE;
    static const ImVec4 COLOR_PURPLE;

    static const ImVec4 COLOR_WOW_RED;
    static const ImVec4 COLOR_WOW_DARK_MAGENTA;
    static const ImVec4 COLOR_WOW_ORANGE;
    static const ImVec4 COLOR_WOW_CHROMOPHOBIA_GREEN;
    static const ImVec4 COLOR_WOW_GREEN;
    static const ImVec4 COLOR_WOW_LIGHT_BLUE;
    static const ImVec4 COLOR_WOW_SPRING_GREEN;
    static const ImVec4 COLOR_WOW_PINK;
    static const ImVec4 COLOR_WOW_WHITE;
    static const ImVec4 COLOR_WOW_YELLOW;
    static const ImVec4 COLOR_WOW_BLUE;
    static const ImVec4 COLOR_WOW_PURPLE;
    static const ImVec4 COLOR_WOW_TAN;
};

#endif
