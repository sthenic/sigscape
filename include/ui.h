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
#include "marker.h"

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
    void *m_adq_control_unit;
    bool m_show_imgui_demo_window;
    bool m_show_implot_demo_window;
    bool m_is_time_domain_collapsed;
    bool m_is_frequency_domain_collapsed;
    int m_nof_channels_total;

    /* Representation of a digitizer channel's state in the UI. */
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
    };

    /* Representation of a digitizer's state in the UI. */
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
        bool is_selected;

        std::vector<ChannelUiState> channels;
    };

    /* Representation of a digitizer in the UI. */
    struct DigitizerUi
    {
        DigitizerUi(std::shared_ptr<Digitizer> interface)
            : interface(interface)
            , ui{}
        {}

        std::shared_ptr<Digitizer> interface;
        DigitizerUiState ui;
    };

    std::vector<DigitizerUi> m_digitizers;

    Markers m_time_domain_markers;
    Markers m_frequency_domain_markers;

    void ClearChannelSelection();

    void PushMessage(const DigitizerMessage &message, bool selected = true);

    void UpdateRecords();
    void HandleMessage(const IdentificationMessage &message);
    void HandleMessage(DigitizerUi &digitizer, const DigitizerMessage &message);
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

    typedef std::string (*Formatter)(double value, bool show_sign);
    void MarkerTree(Markers &markers, Formatter FormatX, Formatter FormatY);

    std::vector<std::tuple<size_t, size_t, ChannelUiState *>> GetUiWithSelectedLast();
    void PlotTimeDomainSelected();

    int GetSelectedChannel(ChannelUiState *&ui);
    void DrawMarkerX(int id, double *x, const ImVec4 &color, float thickness,
                     const std::string &format, ImPlotDragToolFlags flags = 0);
    void DrawMarkerY(int id, double *y, const ImVec4 &color, float thickness,
                     const std::string &format, ImPlotDragToolFlags flags = 0);

    static void MaybeAddMarker(size_t digitizer, size_t channel, const BaseRecord *record,
                               Markers &markers);

    static bool IsHoveredAndDoubleClicked(const Marker &marker);
    static void RemoveDoubleClickedMarkers(Markers &markers);

    static void SnapX(double x, const BaseRecord *record, double &snap_x, double &snap_y);
    static void GetClosestSampleIndex(double x, double y, const BaseRecord *record,
                                      const ImPlotRect &view, size_t &index);

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
