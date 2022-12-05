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

#include <map>
#include <set>

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
    int m_nof_channels_total;

    struct Marker
    {
        Marker() = default;
        Marker(size_t id, size_t digitizer, size_t channel, size_t sample, double x, double y);

        size_t id;
        size_t digitizer;
        size_t channel;
        size_t sample;
        ImVec4 color;
        float thickness;
        double x;
        double y;
        std::set<size_t> deltas;
    };

    struct ChannelUiState
    {
        ChannelUiState(int &nof_channels_total);

        ImVec4 color;
        bool is_selected;
        bool is_sample_markers_enabled;
        bool is_persistence_enabled;
        bool is_time_domain_visible;
        bool is_frequency_domain_visible;
        std::shared_ptr<ProcessedRecord> record;

        void ColorSquare() const;
    };

    struct DigitizerUiState
    {
        DigitizerUiState();

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

    std::map<size_t, Marker>::iterator m_last_frequency_domain_marker;
    std::map<size_t, Marker> m_frequency_domain_markers;
    bool m_is_dragging_frequency_domain_marker;
    bool m_is_adding_frequency_domain_marker;
    size_t m_next_frequency_domain_marker_id;

    std::map<size_t, Marker>::iterator m_last_time_domain_marker;
    std::map<size_t, Marker> m_time_domain_markers;
    bool m_is_dragging_time_domain_marker;
    bool m_is_adding_time_domain_marker;
    size_t m_next_time_domain_marker_id;

    void ClearChannelSelection();

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
    void RenderMarkers(const ImVec2 &position, const ImVec2 &size);
    void RenderProcessingOptions(const ImVec2 &position, const ImVec2 &size);

    void Reduce(double xsize, double sampling_frequency, int &count, int &stride);
    static std::string MetricFormatter(double value, const std::string &format,
                                       double highest_prefix = 1e9);
    static int MetricFormatter(double value, char *tick_label, int size, void *data);
    static std::string FormatTimeDomainX(double value, bool show_sign);
    static std::string FormatTimeDomainY(double value, bool show_sign);
    static std::string FormatFrequencyDomainX(double value, bool show_sign);
    static std::string FormatFrequencyDomainY(double value, bool show_sign);
    typedef std::string (*Formatter)(double value, bool show_sign);

    void MarkerTree(std::map<size_t, Marker> &markers, const std::string &label,
                    const std::string &prefix, Formatter format_x, Formatter format_y);

    void PlotTimeDomainSelected();

    int GetSelectedChannel(ChannelUiState *&ui);
    void DrawMarkerX(int id, double *x, const ImVec4 &color, float thickness,
                     const std::string &format, ImPlotDragToolFlags flags = 0);
    void DrawMarkerY(int id, double *y, const ImVec4 &color, float thickness,
                     const std::string &format, ImPlotDragToolFlags flags = 0);

    template <typename T>
    static void MaybeAddMarker(size_t digitizer, size_t channel, const T &record,
                               std::map<size_t, Marker> &markers, bool &is_adding_marker,
                               bool &is_dragging_marker, size_t &next_marker_id,
                               std::map<size_t, Marker>::iterator &last_marker);

    static bool IsHoveredAndDoubleClicked(const std::pair<size_t, Marker> &marker);
    static void RemoveDoubleClickedMarkers(std::map<size_t, Marker> &markers);

    template <typename T>
    static void SnapX(double x, const T &record, double &snap_x, double &snap_y);

    template <typename T>
    static void GetClosestSampleIndex(double x, double y, const T &record, const ImPlotRect &view,
                                      size_t &index);

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
