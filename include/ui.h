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

    struct Marker
    {
        double x;
        double y;
    };

    struct ChannelUiState
    {
        ChannelUiState();

        ImVec4 color;
        bool sample_markers;
        bool is_time_domain_visible;
        bool is_frequency_domain_visible;
        bool is_adding_time_domain_marker;
        bool is_adding_frequency_domain_marker;
        std::shared_ptr<ProcessedRecord> record;
        std::vector<Marker> time_domain_markers;
        std::vector<Marker> frequency_domain_markers;

        void ColorSquare() const;
    };

    struct DigitizerUiState
    {
        DigitizerUiState(int nof_channels);

        std::string identifier;
        std::string state;
        std::string event;
        ImVec4 state_color;
        ImVec4 event_color;
        ImVec4 set_top_color;
        ImVec4 set_clock_system_color;
        bool popup_initialize_would_overwrite;

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
    void RenderPopups();

    void RenderPopupInitializeWouldOverwrite(size_t idx);
    void RenderDigitizerSelection(const ImVec2 &position, const ImVec2 &size);
    void RenderCommandPalette(const ImVec2 &position, const ImVec2 &size);
    void RenderSetTopParametersButton(const ImVec2 &size);
    void RenderSetClockSystemParametersButton(const ImVec2 &size);
    void RenderProcessingOptions(const ImVec2 &position, const ImVec2 &size);

    void Reduce(double xsize, double sampling_frequency, int &count, int &stride);
    static std::string MetricFormatter(double value, const std::string &format,
                                       double highest_prefix = 1e9);
    static void MetricFormatter(double value, char *tick_label, int size, void *data);
    void PlotTimeDomainSelected();
    int GetFirstChannelWithData(ChannelUiState *&ui);
    void RenderMarkerX(int id, double *x, const std::string &format, double highest_prefix,
                       ImPlotDragToolFlags flags = 0);
    void RenderMarkerY(int id, double *y, const std::string &format, double highest_prefix,
                       ImPlotDragToolFlags flags = 0);
    void MaybeAddMarker(std::vector<Marker> &markers, bool &is_adding_marker);
    static void SnapX(double x, double step, const std::vector<double> &y, double &snap_x,
                      double &snap_y);
    void RenderTimeDomain(const ImVec2 &position, const ImVec2 &size);
    void RenderFrequencyDomain(const ImVec2 &position, const ImVec2 &size);

    void Annotate(const std::pair<double, double> &point, const std::string &label = "");
    void PlotFourierTransformSelected();
    void RenderFourierTransformPlot();

    void PlotWaterfallSelected();
    void RenderWaterfallPlot();

    void MetricsRow(const std::string &label, const std::string &str);
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
