#pragma once

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "implot.h"
#include "imfilebrowser.h"
#include "GL/gl3w.h"
#include <GLFW/glfw3.h>

#include "embedded_python_thread.h"
#include "persistent_directories.h"
#include "directory_watcher.h"
#include "digitizer.h"
#include "identification.h"
#include "marker.h"
#include "format.h"
#include "record_frame.h"
#if defined(_WIN32)
#include "hotplug_windows.h"
#else
#include "hotplug_linux.h"
#endif

#include <vector>
#include <set>
#include <filesystem>

class Ui
{
public:
    Ui();
    ~Ui() = default;

    void Initialize(GLFWwindow *window, const char *glsl_version);
    void Render(float width, float height);
    void Terminate();

private:
    std::function<bool(const std::filesystem::path &filename)> Screenshot;
    bool m_should_screenshot;
    PersistentDirectories m_persistent_directories;
    DirectoryWatcher m_python_directory_watcher;
    std::shared_ptr<EmbeddedPythonThread> m_python;
    std::set<std::filesystem::path> m_python_files;
    Identification m_identification;
#if defined(_WIN32)
    HotplugWindows m_hotplug;
#else
    HotplugLinux m_hotplug;
#endif
    void *m_adq_control_unit;
    bool m_show_imgui_demo_window;
    bool m_show_implot_demo_window;
    DataProcessingParameters m_processing_parameters;
    struct
    {
        bool time_domain;
        bool frequency_domain;
        bool log;
        bool time_domain_metrics;
        bool frequency_domain_metrics;
        bool processing_options;
        bool record_frame_options;
        bool application_metrics;
    } m_collapsed;
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
        bool is_plot_frame_enabled;
        bool is_harmonics_annotated;
        bool is_interleaving_spurs_annotated;
        bool is_time_domain_visible;
        bool is_frequency_domain_visible;
        bool should_save_to_file;
        RecordFrame frame;
        std::shared_ptr<ProcessedRecord> record; /* FIXME: Remove and replace w/ frame access. */
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

    /* Forward declaration to let a command store an action affecting the UI state. */
    struct DigitizerUiState;
    struct CommandUiState
    {
        CommandUiState(
            const DigitizerMessage &message,
            std::function<void(DigitizerUiState *ui)> Preamble = [](auto) {});

        void MaybeRestoreColor();

        static constexpr int RESTORE_COLOR_MS = 2000;
        std::chrono::high_resolution_clock::time_point timestamp;
        std::function<void(DigitizerUiState *ui)> Preamble;
        const DigitizerMessage message;
        ImVec4 color;
    };

    /* Representation of a digitizer's state in the UI. */
    struct DigitizerUiState
    {
        DigitizerUiState();

        ADQConstantParameters constant;
        std::string identifier;
        std::string state;
        std::string event;
        ImVec4 state_color;
        ImVec4 event_color;
        bool popup_initialize_would_overwrite;
        bool is_selected;
        float dram_fill;

        BootStatusUiState boot_status;
        std::map<uint32_t, SensorGroupUiState> sensor_groups;
        std::vector<ChannelUiState> channels;
        std::map<std::string, CommandUiState> commands;
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
        std::string Format()
        {
            return fmt::format(
                "{}/div\n{}/div",
                Format::Metric(y, "{:6.2f} {}" + y_unit),
                Format::Metric(x, "{:6.2f} {}" + x_unit)
            );
        }

        double x;
        double y;
        std::string x_unit;
        std::string y_unit;
    };

    /* FIXME: Maybe wrap into a struct for plot properties? + markers */
    UnitsPerDivision m_time_domain_units_per_division;
    UnitsPerDivision m_frequency_domain_units_per_division;
    UnitsPerDivision m_sensor_units_per_division;
    bool m_should_auto_fit_time_domain;
    bool m_should_auto_fit_frequency_domain;
    bool m_should_auto_fit_waterfall;
    bool m_should_save_sensors_to_file;
    bool m_popup_add_python_script;
    struct
    {
        std::string revision;
        bool compatible;
        bool popup;
        bool pyadq_compatible;
    } m_libadq;
    ImGui::FileBrowser m_file_browser;

    void InitializeEmbeddedPython();

    void ClearChannelSelection();
    void MaybeResetChannelSelection();
    bool IsAnySolo() const;
    bool IsAnySensorError() const;
    bool IsAnyBootError() const;
    bool IsAnySensorPlotted() const;

    void IdentifyDigitizers();
    void PushMessage(const DigitizerMessage &message, bool only_selected = true);

    void UpdateRecords();
    void UpdateSensors();
    void HandleMessage(const IdentificationMessage &message);
    void HandleMessage(DigitizerUi &digitizer, const DigitizerMessage &message);
    void HandleMessage(const DirectoryWatcherMessage &message);
    void HandleMessages();
    void HandleHotplugEvents();

    void RenderMenuBar();
    void RenderRight(float width, float height);
    void RenderCenter(float width, float height);
    void RenderLeft(float width, float height);
    void RenderPopups();

    void RenderPopupCompatibilityError();
    void RenderPopupInitializeWouldOverwrite(size_t idx);
    void RenderPopupAddPythonScript();
    void RenderDigitizerSelection(const ImVec2 &position, const ImVec2 &size);
    void RenderPythonCommandPalette(bool enable);
    void RenderDefaultCommandPalette(bool enable);
    void RenderCommandPalette(const ImVec2 &position, const ImVec2 &size);
    void RenderTools(const ImVec2 &position, const ImVec2 &size);
    void RenderMarkers();
    void RenderMemory();
    void RenderSensorGroup(SensorGroupUiState &group, bool is_first);
    void RenderBootStatus();
    void RenderStaticInformation();
    void RenderSensors();
    void RenderProcessingOptions(const ImVec2 &position, const ImVec2 &size);
    void RenderRecordFrameOptions(const ImVec2 &position, const ImVec2 &size);

    void Reduce(double xsize, double sampling_frequency, int &count, int &stride);

    void MarkerTree(Markers &markers, bool inverse_delta);

    ChannelUiState *GetSelectedChannel();
    std::vector<DigitizerUi *> GetSelectedDigitizers();
    std::vector<std::tuple<size_t, size_t, ChannelUiState *>> FilterUiStates(); /* FIXME: References here instead of pointers? */
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
    void RenderLog(const ImVec2 &position, const ImVec2 &size);

    void Annotate(const std::tuple<Value, Value> &point, const std::string &label = "");
    void PlotFourierTransformSelected();
    void RenderFourierTransformPlot();

    void PlotWaterfallSelected(double &scale_min, double &scale_max);
    void RenderWaterfallPlot();

    void RenderHeaderButtons(ChannelUiState &ui);

    void CopyFreqencyDomainMetricsToClipboard(const ProcessedRecord *processed_record);
    void SaveSensorsToFile(const std::filesystem::path &path);

    void RenderTimeDomainMetrics(const ImVec2 &position, const ImVec2 &size);
    void RenderFrequencyDomainMetrics(const ImVec2 &position, const ImVec2 &size);
    void RenderApplicationMetrics(const ImVec2 &position, const ImVec2 &size);

    static std::string NowAsIso8601();

    static constexpr float FIRST_COLUMN_RELATIVE_WIDTH = 0.2f;
    static constexpr float SECOND_COLUMN_RELATIVE_WIDTH = 0.58f;
    static constexpr float THIRD_COLUMN_RELATIVE_WIDTH = 0.22f;

    static constexpr ImVec2 COMMAND_PALETTE_BUTTON_SIZE = {85, 50};

    static const ImVec4 COLOR_GREEN;
    static const ImVec4 COLOR_RED;
    static const ImVec4 COLOR_YELLOW;
    static const ImVec4 COLOR_ORANGE;
    static const ImVec4 COLOR_PURPLE;
    static const ImVec4 COLOR_TAN;

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
