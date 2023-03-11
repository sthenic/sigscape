#ifndef UI_H_TLXUOY
#define UI_H_TLXUOY

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "implot.h"
#include "imfilebrowser.h"
#include "GL/gl3w.h"
#include <GLFW/glfw3.h>

#include "persistent_configuration.h"
#include "digitizer.h"
#include "identification.h"
#include "marker.h"
#include "format.h"

#include <vector>
#include <filesystem>

class Ui
{
public:
    Ui();
    ~Ui() = default;

    void Initialize(GLFWwindow *window, const char *glsl_version,
                    bool (* SaveToFile)(const std::string &filename));
    void Render(float width, float height);
    void Terminate();

private:
    bool (* SaveToFile)(const std::string &filename);
    bool m_save_to_file;
    PersistentConfiguration m_persistent_configuration;
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
        void SaveToFile(const std::filesystem::path &path);
        std::string GetDefaultFilename();

        ImVec4 color;
        bool is_selected;
        bool is_muted;
        bool is_solo;
        bool is_sample_markers_enabled;
        bool is_persistence_enabled;
        bool is_time_domain_visible;
        bool is_frequency_domain_visible;
        bool should_save_to_file;
        bool should_auto_fit_time_domain;
        bool should_auto_fit_frequency_domain;
        bool should_auto_fit_waterfall;
        std::shared_ptr<ProcessedRecord> record;
        std::vector<std::shared_ptr<ProcessedRecord>> memory;
    };

    /* Representation of the state of the digitizer's sensors in the UI. */
    struct SensorUiState
    {
        SensorUiState() = default;
        SensorUiState(const std::string &label);

        bool is_plotted;
        std::string label;
        SensorRecord record;
    };

    struct SensorGroupUiState
    {
        SensorGroupUiState() = default;
        SensorGroupUiState(const std::string &label, const std::vector<Sensor> &sensors);

        std::string label;
        std::map<uint32_t, SensorUiState> sensors;
    };

    struct BootStatusUiState
    {
        BootStatusUiState() = default;

        int state;
        std::string state_description;
        std::vector<BootEntry> boot_entries;
    };

    /* Representation of a digitizer's state in the UI. */
    struct DigitizerUiState
    {
        DigitizerUiState();

        std::string identifier;
        std::string state;
        std::string event; /* TODO: Make this into a queue? */
        ImVec4 state_color;
        ImVec4 event_color;
        ImVec4 set_top_color;
        ImVec4 set_clock_system_color;
        bool popup_initialize_would_overwrite;
        bool is_selected;
        float dram_fill;

        BootStatusUiState boot_status;
        std::map<uint32_t, SensorGroupUiState> sensor_groups;
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

    struct UnitsPerDivision
    {
        double x;
        double y;
    };

    UnitsPerDivision m_time_domain_units_per_division;
    UnitsPerDivision m_frequency_domain_units_per_division;
    UnitsPerDivision m_sensor_units_per_division;

    ImGui::FileBrowser m_file_browser;

    void ClearChannelSelection();
    bool IsAnySolo() const;

    void IdentifyDigitizers();
    void PushMessage(const DigitizerMessage &message, bool only_selected = true);

    void UpdateRecords();
    void UpdateSensors();
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
    void RenderTools(const ImVec2 &position, const ImVec2 &size);
    void RenderMarkers();
    void RenderMemory();
    void RenderSensorGroup(SensorGroupUiState &group, bool is_first);
    void RenderBootStatus();
    void RenderSensors();
    void RenderProcessingOptions(const ImVec2 &position, const ImVec2 &size);

    void Reduce(double xsize, double sampling_frequency, int &count, int &stride);

    void MarkerTree(Markers &markers, Format::Formatter FormatX, Format::Formatter FormatY);

    std::vector<std::tuple<size_t, size_t, ChannelUiState *>> FilterUiStates();
    static void GetUnitsPerDivision(const std::string &title, UnitsPerDivision &units_per_division);
    static void RenderUnitsPerDivision(const std::string &str);

    void PlotTimeDomainSelected();

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

    void PlotSensorsSelected();
    void RenderChannelPlot();
    void RenderSensorPlot();
    void RenderTimeDomain(const ImVec2 &position, const ImVec2 &size);
    void RenderFrequencyDomain(const ImVec2 &position, const ImVec2 &size);

    void Annotate(const std::pair<double, double> &point, const std::string &label = "");
    void PlotFourierTransformSelected();
    void RenderFourierTransformPlot();

    void PlotWaterfallSelected(double &scale_min, double &scale_max);
    void RenderWaterfallPlot();

    static void RenderHeaderButtons(ChannelUiState &ui);

    void RenderTimeDomainMetrics(const ImVec2 &position, const ImVec2 &size);
    void RenderFrequencyDomainMetrics(const ImVec2 &position, const ImVec2 &size);
    void RenderApplicationMetrics(const ImVec2 &position, const ImVec2 &size);

    static std::string NowAsIso8601();

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
