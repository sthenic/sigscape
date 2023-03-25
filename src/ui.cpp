#include "ui.h"
#include "implot_internal.h" /* To be able to get item visibility state. */

#include "fmt/format.h"
#include "nlohmann/json.hpp"
#include <cinttypes>
#include <cmath>
#include <ctime>
#include <fstream>

const ImVec4 Ui::COLOR_GREEN = {0.0f, 1.0f, 0.5f, 0.6f};
const ImVec4 Ui::COLOR_RED = {1.0f, 0.0f, 0.2f, 0.6f};
const ImVec4 Ui::COLOR_YELLOW = {1.0f, 1.0f, 0.3f, 0.8f};
const ImVec4 Ui::COLOR_ORANGE = {0.86f, 0.38f, 0.1f, 0.8f};
const ImVec4 Ui::COLOR_PURPLE = {0.6f, 0.3f, 1.0f, 0.8f};

const ImVec4 Ui::COLOR_WOW_RED = {0.77f, 0.12f, 0.23f, 0.8f};
const ImVec4 Ui::COLOR_WOW_DARK_MAGENTA = {0.64f, 0.19f, 0.79f, 0.8f};
const ImVec4 Ui::COLOR_WOW_ORANGE = {1.00f, 0.49f, 0.04f, 0.8f};
const ImVec4 Ui::COLOR_WOW_CHROMOPHOBIA_GREEN = {0.20f, 0.58f, 0.50f, 0.8f};
const ImVec4 Ui::COLOR_WOW_GREEN = {0.67f, 0.83f, 0.45f, 0.8f};
const ImVec4 Ui::COLOR_WOW_LIGHT_BLUE = {0.25f, 0.78f, 0.92f, 0.8f};
const ImVec4 Ui::COLOR_WOW_SPRING_GREEN = {0.00f, 1.00f, 0.60f, 0.8f};
const ImVec4 Ui::COLOR_WOW_PINK = {0.96f, 0.55f, 0.73f, 0.8f};
const ImVec4 Ui::COLOR_WOW_WHITE = {1.00f, 1.00f, 1.00f, 0.8f};
const ImVec4 Ui::COLOR_WOW_YELLOW = {1.00f, 0.96f, 0.41f, 0.8f};
const ImVec4 Ui::COLOR_WOW_BLUE = {0.00f, 0.44f, 0.87f, 0.8f};
const ImVec4 Ui::COLOR_WOW_PURPLE = {0.53f, 0.53f, 0.93f, 0.8f};
const ImVec4 Ui::COLOR_WOW_TAN  = {0.78f, 0.61f, 0.43f, 0.8f};

/* ImGui extensions to remove conversion clutter when calling certain functions. */
namespace ImGui
{
static inline void Text(const std::string &str)
{
    ImGui::Text("%s", str.c_str());
}
}

static void WIP()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    ImGui::Text("WORK IN PROGRESS");
    ImGui::PopStyleColor();
}

Ui::ChannelUiState::ChannelUiState(int &nof_channels_total)
    : color{}
    , is_selected(false)
    , is_muted(false)
    , is_solo(false)
    , is_sample_markers_enabled(false)
    , is_persistence_enabled(false)
    , is_harmonics_annotated(true)
    , is_interleaving_spurs_annotated(true)
    , is_time_domain_visible(true)
    , is_frequency_domain_visible(true)
    , should_save_to_file(false)
    , record(NULL)
    , memory{}
{
    if (nof_channels_total == 0)
        is_selected = true;

    color = ImPlot::GetColormapColor(nof_channels_total++);
}

void Ui::ChannelUiState::SaveToFile(const std::filesystem::path &path)
{
    /* FIXME: Display errors in some user friendly way. */
    /* TODO: Prevent saving if the existing extension is not '.json'? */
    /* TODO: Popup if the file already exists? */

    /* Append '.json' if needed. */
    std::filesystem::path lpath(path);
    if (!lpath.has_extension())
        lpath.replace_extension(".json");

    nlohmann::json json;
    json["label"] = record->label;
    json["time_domain"]["x"]["data"] = record->time_domain->x;
    json["time_domain"]["x"]["unit"] = record->time_domain->x_properties.unit;
    json["time_domain"]["y"]["data"] = record->time_domain->y;
    json["time_domain"]["y"]["unit"] = record->time_domain->y_properties.unit;

    json["frequency_domain"]["x"]["data"] = record->frequency_domain->x;
    json["frequency_domain"]["x"]["unit"] = record->frequency_domain->x_properties.unit;
    json["frequency_domain"]["y"]["data"] = record->frequency_domain->y;
    json["frequency_domain"]["y"]["unit"] = record->frequency_domain->y_properties.unit;

    /* TODO: Stringify header? */

    std::ofstream ofs(lpath, std::ios::trunc);
    if (ofs.fail())
    {
        printf("Failed to open file '%s'.\n", lpath.string().c_str());
        return;
    }

    ofs << json.dump(4).c_str() << "\n";
    ofs.close();

    printf("%s\n",
           fmt::format("Saved {} record data in '{}'.", record->label, lpath.string()).c_str());
}

std::string Ui::ChannelUiState::GetDefaultFilename()
{
    auto MakeLowercase = [](unsigned char c){ return static_cast<char>(std::tolower(c)); };
    std::string filename(record->label);
    std::transform(filename.begin(), filename.end(), filename.begin(), MakeLowercase);
    filename.append(fmt::format("_{}.json", NowAsIso8601()));
    std::replace(filename.begin(), filename.end(), ' ', '_');
    return filename;
}

Ui::SensorUiState::SensorUiState(const std::string &label)
    : is_plotted(false)
    , label(label)
    , record()
{
}

Ui::SensorGroupUiState::SensorGroupUiState(const std::string &label,
                                           const std::vector<Sensor> &sensors)
    : label(label)
    , sensors{}
{
    for (const auto &sensor : sensors)
        SensorGroupUiState::sensors.try_emplace(sensor.id, sensor.label);
}

Ui::DigitizerUiState::DigitizerUiState()
    : constant{}
    , identifier("Unknown")
    , state("")
    , event("")
    , state_color(COLOR_GREEN)
    , event_color(COLOR_GREEN)
    , set_top_color(ImGui::GetStyleColorVec4(ImGuiCol_Button))
    , set_clock_system_color(ImGui::GetStyleColorVec4(ImGuiCol_Button))
    , popup_initialize_would_overwrite(false)
    , is_selected(false)
    , dram_fill(0.0f)
    , boot_status()
    , sensor_groups{}
    , channels{}
{
}

Ui::Ui()
    : Screenshot(NULL)
    , m_should_screenshot(false)
    , m_persistent_directories()
    , m_identification()
    , m_adq_control_unit()
    , m_show_imgui_demo_window(false)
    , m_show_implot_demo_window(false)
    , m_processing_parameters{}
    , m_collapsed{}
    , m_nof_channels_total(0)
    , m_digitizers()
    , m_time_domain_markers("Time Domain", "T")
    , m_frequency_domain_markers("Frequency Domain", "F")
    , m_time_domain_units_per_division()
    , m_frequency_domain_units_per_division()
    , m_sensor_units_per_division{0.0, 0.0, "s", "?"}
    , m_should_auto_fit_time_domain(true)
    , m_should_auto_fit_frequency_domain(true)
    , m_should_auto_fit_waterfall(true)
    , m_popup_compatibility_error(false)
    , m_api_revision(0)
    , m_file_browser(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir |
                     ImGuiFileBrowserFlags_CloseOnEsc)
{
}

void Ui::Initialize(GLFWwindow *window, const char *glsl_version,
                    bool (*ScreenshotCallback)(const std::string &filename))
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    ImGui::StyleColorsDark();
    Screenshot = ScreenshotCallback;

    /* Set up the ImGui configuration file. */
    ImGui::GetIO().IniFilename = m_persistent_directories.GetImGuiInitializationFile();

    /* Proceed with identifying the digitizers connected to the system. */
    IdentifyDigitizers();
}

void Ui::Terminate()
{
    for (const auto &d : m_digitizers)
        d.interface->Stop();

    if (m_adq_control_unit != NULL)
        DeleteADQControlUnit(m_adq_control_unit);
    m_adq_control_unit = NULL;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void Ui::Render(float width, float height)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    HandleMessages();
    UpdateRecords();
    UpdateSensors();
    RenderMenuBar();
    RenderLeft(width, height);
    RenderCenter(width, height);
    RenderRight(width, height);
    RenderPopups();

    if (m_show_imgui_demo_window)
        ImGui::ShowDemoWindow();

    if (m_show_implot_demo_window)
        ImPlot::ShowDemoWindow();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    /* We have to call the `Screenshot` callback after the current frame has
       been rendered completely. */
    if (m_should_screenshot && Screenshot)
    {
        const auto filename = fmt::format("{}/sigscape_{}.png",
                                          m_persistent_directories.GetScreenshotDirectory(),
                                          NowAsIso8601());

        if (!Screenshot(filename))
            printf("%s\n", fmt::format("Failed to save PNG image '{}'.", filename).c_str());
        m_should_screenshot = false;
    }
}

void Ui::ClearChannelSelection()
{
    for (auto &digitizer : m_digitizers)
    {
        for (auto &chui : digitizer.ui.channels)
            chui.is_selected = false;
    }
}

bool Ui::IsAnySolo() const
{
    bool result = false;
    for (const auto &digitizer : m_digitizers)
    {
        for (const auto &chui : digitizer.ui.channels)
            result |= chui.is_solo;
    }
    return result;
}

void Ui::IdentifyDigitizers()
{
    /* This function can be called to reinitialize the system so if there's
       already a control unit, be sure to destroy it before starting the
       identification thread. */
    for (const auto &d : m_digitizers)
        d.interface->Stop();

    if (m_adq_control_unit != NULL)
        DeleteADQControlUnit(m_adq_control_unit);
    m_adq_control_unit = NULL;

    /* (Re)start device identification thread. */
    m_identification.SetLogDirectory(m_persistent_directories.GetLogDirectory());
    m_identification.Start();
}

void Ui::PushMessage(const DigitizerMessage &message, bool only_selected)
{
    for (auto &digitizer : m_digitizers)
    {
        if (only_selected && !digitizer.ui.is_selected)
            continue;
        digitizer.interface->PushMessage(message);
    }
}

void Ui::UpdateRecords()
{
    /* FIXME: Skip unselected digitizers? We need a different queue then that
              pops entries at the end once saturated, regardless if they've been
              read or not. */

    /* Attempt to update the set of processed records for each digitizer. */
    for (auto &digitizer : m_digitizers)
    {
        for (int ch = 0; ch < static_cast<int>(digitizer.ui.channels.size()); ++ch)
            digitizer.interface->WaitForProcessedRecord(ch, digitizer.ui.channels[ch].record);
    }
}

void Ui::UpdateSensors()
{
    for (auto &digitizer : m_digitizers)
    {
        std::shared_ptr<std::vector<SensorRecord>> records;
        if (SCAPE_EOK == digitizer.interface->WaitForSensorRecords(records))
        {
            for (auto &record : *records)
            {
                digitizer.ui.sensor_groups[record.group_id].sensors[record.id].record =
                    std::move(record);
            }
        }
    }
}

void Ui::HandleMessage(const IdentificationMessage &message)
{
    /* Stop the identification thread. */
    m_identification.Stop();
    m_api_revision = message.revision;
    m_adq_control_unit = message.handle;

    /* If the API is incompatible, we can't proceed beyond this point. We toggle
       the popup message and return. */
    if (!message.compatible)
    {
        m_popup_compatibility_error = true;
        return;
    }

    /* If we get a message, the identification went well. Prepare to
       reinitialize the entire user interface.*/
    for (const auto &d : m_digitizers)
        d.interface->Stop();

    m_digitizers.clear();
    m_nof_channels_total = 0;
    m_time_domain_markers.clear();
    m_frequency_domain_markers.clear();

    for (auto interface : message.digitizers)
        m_digitizers.emplace_back(interface);

    for (const auto &d : m_digitizers)
    {
        d.interface->Start();
        d.interface->EmplaceMessage(DigitizerMessageId::SET_CONFIGURATION_DIRECTORY,
                                    m_persistent_directories.GetConfigurationDirectory());
    }

    if (m_digitizers.size() > 0)
        m_digitizers.front().ui.is_selected = true;
}

void Ui::HandleMessage(DigitizerUi &digitizer, const DigitizerMessage &message)
{
    switch (message.id)
    {
    case DigitizerMessageId::CONSTANT_PARAMETERS:
        digitizer.ui.identifier = fmt::format("{} {} {}", message.constant_parameters.product_name,
                                              message.constant_parameters.serial_number,
                                              message.constant_parameters.firmware.name);
        digitizer.ui.channels.clear();
        for (int ch = 0; ch < message.constant_parameters.nof_channels; ++ch)
            digitizer.ui.channels.emplace_back(m_nof_channels_total);
        digitizer.ui.constant = std::move(message.constant_parameters);
        break;

    case DigitizerMessageId::CONFIGURATION:
        digitizer.ui.event = "CONFIGURATION";
        digitizer.ui.event_color = COLOR_WOW_TAN;
        break;

    case DigitizerMessageId::CLEAR:
        digitizer.ui.event = "";
        break;

    case DigitizerMessageId::ERR:
        digitizer.ui.event = "ERROR";
        digitizer.ui.event_color = COLOR_RED;
        break;

    case DigitizerMessageId::STATE:
        switch (message.state)
        {
        case DigitizerState::NOT_INITIALIZED:
            digitizer.ui.state = "NOT INITIALIZED";
            digitizer.ui.state_color = COLOR_RED;
            break;
        case DigitizerState::INITIALIZATION:
            digitizer.ui.state = "INITIALIZATION";
            digitizer.ui.state_color = COLOR_PURPLE;
            digitizer.ui.identifier = "Unknown";
            break;
        case DigitizerState::IDLE:
            digitizer.ui.state = "IDLE";
            digitizer.ui.state_color = COLOR_GREEN;
            break;
        case DigitizerState::ACQUISITION:
            digitizer.ui.state = "ACQUISITION";
            digitizer.ui.state_color = COLOR_ORANGE;
            break;
        }
        break;

    case DigitizerMessageId::DIRTY_TOP_PARAMETERS:
        digitizer.ui.set_top_color = COLOR_ORANGE;
        /* FIXME: Remove since debug convenience */
        digitizer.interface->PushMessage(DigitizerMessageId::SET_PARAMETERS);
        break;

    case DigitizerMessageId::DIRTY_CLOCK_SYSTEM_PARAMETERS:
        digitizer.ui.set_clock_system_color = COLOR_ORANGE;
        break;

    case DigitizerMessageId::CLEAN_TOP_PARAMETERS:
        digitizer.ui.set_top_color = COLOR_GREEN;
        break;

    case DigitizerMessageId::CLEAN_CLOCK_SYSTEM_PARAMETERS:
        digitizer.ui.set_clock_system_color = COLOR_GREEN;
        break;

    case DigitizerMessageId::INITIALIZE_WOULD_OVERWRITE:
        digitizer.ui.popup_initialize_would_overwrite = true;
        ImGui::OpenPopup("InitializeWouldOverwrite");
        break;

    case DigitizerMessageId::SENSOR_TREE:
        digitizer.ui.sensor_groups.clear();
        for (const auto &group : message.sensor_tree)
            digitizer.ui.sensor_groups.try_emplace(group.id, group.label, group.sensors);
        break;

    case DigitizerMessageId::BOOT_STATUS:
        digitizer.ui.boot_status.state = message.ivalue;
        digitizer.ui.boot_status.state_description = std::move(message.str);
        digitizer.ui.boot_status.boot_entries = std::move(message.boot_entries);
        break;

    case DigitizerMessageId::NO_ACTIVITY:
        digitizer.ui.event = "NO ACTIVITY";
        digitizer.ui.event_color = COLOR_WOW_PURPLE;
        break;

    case DigitizerMessageId::PARAMETERS_FILENAME:
        ImGui::LogToClipboard();
        ImGui::LogText("%s", message.str.c_str());
        ImGui::LogFinish();
        break;

    case DigitizerMessageId::DRAM_FILL:
        digitizer.ui.dram_fill = static_cast<float>(message.dvalue);
        break;

    case DigitizerMessageId::OVF:
        digitizer.ui.event = "OVERFLOW";
        digitizer.ui.event_color = COLOR_RED;
        break;

    default:
        /* These are not expected as a message from a digitizer thread. */
        printf("%s\n", fmt::format("Unsupported message id '{}'.", message.id).c_str());
        break;
    }
}

void Ui::HandleMessages()
{
    IdentificationMessage identification_message;
    if (SCAPE_EOK == m_identification.WaitForMessage(identification_message, 0))
        HandleMessage(identification_message);

    for (auto &digitizer : m_digitizers)
    {
        DigitizerMessage digitizer_message;
        if (SCAPE_EOK == digitizer.interface->WaitForMessage(digitizer_message, 0))
            HandleMessage(digitizer, digitizer_message);
    }
}

void Ui::RenderMenuBar()
{
    ImGui::BeginMainMenuBar();
    if (ImGui::MenuItem("Screenshot"))
    {
        /* We have to postpone the saving until the very end of the 'global'
           render function, i.e. until the frame buffer has received all its
           contents. */
        m_should_screenshot = true;
    }

    if (ImGui::BeginMenu("Demo"))
    {
        ImGui::MenuItem("ImGui", NULL, &m_show_imgui_demo_window);
        ImGui::MenuItem("ImPlot", NULL, &m_show_implot_demo_window);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Style"))
    {
        static int style_idx = 0;
        if (ImGui::Combo("##combostyle", &style_idx, "Dark\0Light\0"))
        {
            switch (style_idx)
            {
            case 0:
                ImGui::StyleColorsDark();
                break;
            case 1:
                ImGui::StyleColorsLight();
                break;
            }
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void Ui::RenderRight(float width, float height)
{
    const float FRAME_HEIGHT = ImGui::GetFrameHeight();
    const float PLOT_WINDOW_HEIGHT = (height - 1 * FRAME_HEIGHT) / 2;
    const float POSITION_X = width * (FIRST_COLUMN_RELATIVE_WIDTH + SECOND_COLUMN_RELATIVE_WIDTH);
    const float SIZE_X = width * SECOND_COLUMN_RELATIVE_WIDTH;

    ImVec2 TIME_DOMAIN_POSITION{POSITION_X, FRAME_HEIGHT};
    ImVec2 TIME_DOMAIN_SIZE{SIZE_X, PLOT_WINDOW_HEIGHT};

    ImVec2 FREQUENCY_DOMAIN_POSITION{POSITION_X, FRAME_HEIGHT + PLOT_WINDOW_HEIGHT};
    ImVec2 FREQUENCY_DOMAIN_SIZE{SIZE_X, PLOT_WINDOW_HEIGHT};

    if (m_collapsed.time_domain_metrics)
    {
        TIME_DOMAIN_SIZE = ImVec2{SIZE_X, FRAME_HEIGHT};
        FREQUENCY_DOMAIN_POSITION = ImVec2{POSITION_X, 2 * FRAME_HEIGHT};
        FREQUENCY_DOMAIN_SIZE = ImVec2{SIZE_X, height - 2 * FRAME_HEIGHT};
    }

    if (m_collapsed.frequency_domain_metrics)
    {
        TIME_DOMAIN_SIZE = ImVec2{POSITION_X, height - 2 * FRAME_HEIGHT};
        FREQUENCY_DOMAIN_POSITION = ImVec2{POSITION_X, height - FRAME_HEIGHT};
        FREQUENCY_DOMAIN_SIZE = ImVec2{SIZE_X, FRAME_HEIGHT};
    }

    if (m_collapsed.time_domain_metrics && m_collapsed.frequency_domain_metrics)
        FREQUENCY_DOMAIN_POSITION = ImVec2{POSITION_X, 2 * FRAME_HEIGHT};

    /* In the right column we show various metrics. */
    RenderTimeDomainMetrics(TIME_DOMAIN_POSITION, TIME_DOMAIN_SIZE);
    RenderFrequencyDomainMetrics(FREQUENCY_DOMAIN_POSITION, FREQUENCY_DOMAIN_SIZE);
}

void Ui::RenderCenter(float width, float height)
{
    /* We show two plots in the center, taking up equal vertical space. */
    const float FRAME_HEIGHT = ImGui::GetFrameHeight();
    const float PLOT_WINDOW_HEIGHT = (height - 1 * FRAME_HEIGHT) / 2;
    const float POSITION_X = width * FIRST_COLUMN_RELATIVE_WIDTH;
    const float SIZE_X = width * SECOND_COLUMN_RELATIVE_WIDTH;

    ImVec2 TIME_DOMAIN_POSITION{POSITION_X, FRAME_HEIGHT};
    ImVec2 TIME_DOMAIN_SIZE{SIZE_X, PLOT_WINDOW_HEIGHT};
    ImVec2 FREQUENCY_DOMAIN_POSITION{POSITION_X, FRAME_HEIGHT + PLOT_WINDOW_HEIGHT};
    ImVec2 FREQUENCY_DOMAIN_SIZE{SIZE_X, PLOT_WINDOW_HEIGHT};

    if (m_collapsed.time_domain)
    {
        TIME_DOMAIN_SIZE = ImVec2{SIZE_X, FRAME_HEIGHT};
        FREQUENCY_DOMAIN_POSITION = ImVec2{POSITION_X, 2 * FRAME_HEIGHT};
        FREQUENCY_DOMAIN_SIZE = ImVec2{SIZE_X, height - 2 * FRAME_HEIGHT};
    }

    if (m_collapsed.frequency_domain)
    {
        TIME_DOMAIN_SIZE = ImVec2{SIZE_X, height - 2 * FRAME_HEIGHT};
        FREQUENCY_DOMAIN_POSITION = ImVec2{POSITION_X, height - FRAME_HEIGHT};
        FREQUENCY_DOMAIN_SIZE = ImVec2{SIZE_X, FRAME_HEIGHT};
    }

    if (m_collapsed.time_domain && m_collapsed.frequency_domain)
        FREQUENCY_DOMAIN_POSITION = ImVec2{POSITION_X, 2 * FRAME_HEIGHT};

    /* The lower plot window, showing time domain data. */
    RenderTimeDomain(TIME_DOMAIN_POSITION, TIME_DOMAIN_SIZE);

    /* The lower plot window, showing frequency domain data. */
    RenderFrequencyDomain(FREQUENCY_DOMAIN_POSITION, FREQUENCY_DOMAIN_SIZE);
}

void Ui::RenderLeft(float width, float height)
{
    const float FRAME_HEIGHT = ImGui::GetFrameHeight();
    const ImVec2 DIGITIZER_SELECTION_POS(0.0f, FRAME_HEIGHT);
    const ImVec2 DIGITIZER_SELECTION_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 200.0f);
    RenderDigitizerSelection(DIGITIZER_SELECTION_POS, DIGITIZER_SELECTION_SIZE);

    const ImVec2 COMMAND_PALETTE_POS(0.0f, DIGITIZER_SELECTION_POS.y + DIGITIZER_SELECTION_SIZE.y);
    const ImVec2 COMMAND_PALETTE_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 300.0f);
    RenderCommandPalette(COMMAND_PALETTE_POS, COMMAND_PALETTE_SIZE);

    /* Measured from the bottom */
    ImVec2 METRICS_POS(0.0f, height - 3 * FRAME_HEIGHT);
    ImVec2 METRICS_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 3 * FRAME_HEIGHT);

    if (m_collapsed.application_metrics)
    {
        METRICS_POS = ImVec2{0.0f, height - FRAME_HEIGHT};
        METRICS_SIZE = ImVec2{width * FIRST_COLUMN_RELATIVE_WIDTH, FRAME_HEIGHT};
    }

    RenderApplicationMetrics(METRICS_POS, METRICS_SIZE);

    ImVec2 PROCESSING_OPTIONS_POS(0.0f, METRICS_POS.y - 200.0f);
    ImVec2 PROCESSING_OPTIONS_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH, 200.0f);

    if (m_collapsed.processing_options)
    {
        PROCESSING_OPTIONS_POS = ImVec2{0.0f, METRICS_POS.y - FRAME_HEIGHT};
        PROCESSING_OPTIONS_SIZE = ImVec2{width * FIRST_COLUMN_RELATIVE_WIDTH, FRAME_HEIGHT};
    }

    RenderProcessingOptions(PROCESSING_OPTIONS_POS, PROCESSING_OPTIONS_SIZE);

    /* The tools window is the one we allow to stretch to the available space. */
    const ImVec2 TOOLS_POS(0.0f, COMMAND_PALETTE_POS.y + COMMAND_PALETTE_SIZE.y);
    const ImVec2 TOOLS_SIZE(width * FIRST_COLUMN_RELATIVE_WIDTH,
                            height - (FRAME_HEIGHT + DIGITIZER_SELECTION_SIZE.y +
                                      COMMAND_PALETTE_SIZE.y + PROCESSING_OPTIONS_SIZE.y +
                                      METRICS_SIZE.y));
    RenderTools(TOOLS_POS, TOOLS_SIZE);
}

void Ui::RenderPopups()
{
    if (m_popup_compatibility_error)
        RenderPopupCompatibilityError();

    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (m_digitizers[i].ui.popup_initialize_would_overwrite)
            RenderPopupInitializeWouldOverwrite(i);

        /* FIXME: Improve this? Generalize to get/pass the UI objects instead? */
        for (auto &chui : m_digitizers[i].ui.channels)
        {
            if (chui.should_save_to_file)
            {
                m_file_browser.Display();
                if (m_file_browser.HasSelected())
                {
                    chui.SaveToFile(m_file_browser.GetSelected());
                    chui.should_save_to_file = false;
                }
            }
        }
    }
}

void Ui::RenderPopupCompatibilityError()
{
    ImGui::OpenPopup("Incompatible libadq");
    if (ImGui::BeginPopupModal("Incompatible libadq", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("The loaded libadq version (0x%08x) is not \n"
                    "compatible with this version of sigscape.",
                    m_api_revision);

        ImGui::Separator();
        if (ImGui::Button("Ok"))
        {
            m_popup_compatibility_error = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void Ui::RenderPopupInitializeWouldOverwrite(size_t idx)
{
    ImGui::OpenPopup("Overwrite warning");
    if (ImGui::BeginPopupModal("Overwrite warning", NULL,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Initializing parameters for %s would\n"
                    "overwrite (reset) the existing parameters.\n\n"
                    "Proceed?\n\n",
                    m_digitizers[idx].ui.identifier.c_str());
        ImGui::Separator();

        if (ImGui::Button("Yes"))
        {
            m_digitizers[idx].interface->PushMessage(DigitizerMessageId::INITIALIZE_PARAMETERS_FORCE);
            m_digitizers[idx].ui.popup_initialize_would_overwrite = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("No"))
        {
            m_digitizers[idx].ui.popup_initialize_would_overwrite = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void Ui::RenderDigitizerSelection(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Digitizers", NULL,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button("Select all"))
    {
        for (auto &digitizer : m_digitizers)
            digitizer.ui.is_selected = true;
    }
    ImGui::SameLine();

    if (ImGui::Button("Deselect all"))
    {
        for (auto &digitizer : m_digitizers)
            digitizer.ui.is_selected = false;
    }

    std::string label = "Reinitialize all";
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::GetCursorPosX() -
                    ImGui::CalcTextSize(label.c_str()).x);
    if (ImGui::Button(label.c_str()))
        IdentifyDigitizers();
    ImGui::Separator();

    if (m_digitizers.size() == 0)
    {
        ImGui::Text("No digitizers found.");
    }
    else
    {
        for (auto &digitizer : m_digitizers)
        {
            int flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow |
                        ImGuiTreeNodeFlags_SpanAvailWidth;

            if (digitizer.ui.is_selected)
                flags |= ImGuiTreeNodeFlags_Selected;

            /* Specify a certain row height by adding a dummy element. */
            float pos = ImGui::GetCursorPosX();
            ImGui::Dummy(ImVec2(0.0f, 20.0f));
            ImGui::SameLine();
            ImGui::SetCursorPosX(pos);
            ImGui::AlignTextToFramePadding();
            bool node_open = ImGui::TreeNodeEx(digitizer.ui.identifier.c_str(), flags);

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                /* Clear all selections unless CTRL is not held during the process. */
                if (!ImGui::GetIO().KeyCtrl)
                {
                    for (auto &d : m_digitizers)
                        d.ui.is_selected = false;
                }

                /* Toggle the selection state. */
                digitizer.ui.is_selected ^= true;
            }

            if (digitizer.ui.state.size() > 0)
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, digitizer.ui.state_color);
                ImGui::SmallButton(digitizer.ui.state.c_str());
                ImGui::PopStyleColor();
            }

            if (digitizer.ui.event.size() > 0)
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, digitizer.ui.event_color);
                ImGui::SmallButton(digitizer.ui.event.c_str());
                ImGui::PopStyleColor();
            }

            if (node_open)
            {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("DRAM");
                ImGui::SameLine();
                if (digitizer.ui.dram_fill < 0.5f)
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, COLOR_GREEN);
                else if (digitizer.ui.dram_fill < 0.8f)
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, COLOR_ORANGE);
                else
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, COLOR_RED);
                ImGui::ProgressBar(digitizer.ui.dram_fill);
                ImGui::PopStyleColor();

                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
}

void Ui::RenderCommandPalette(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Command Palette", NULL,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    int nof_selected = 0;
    std::for_each(m_digitizers.begin(), m_digitizers.end(), [&](const Ui::DigitizerUi &d) {
        if (d.ui.is_selected)
            nof_selected++;
    });

    if (nof_selected > 0)
    {
        ImGui::Text(fmt::format("Commands apply to {} digitizer{}.", nof_selected,
                                nof_selected > 1 ? "s" : ""));
    }
    else
    {
        ImGui::Text("No digitizer selected.");
    }

    const ImVec2 COMMAND_PALETTE_BUTTON_SIZE{85, 50};
    if (nof_selected == 0)
        ImGui::BeginDisabled();

    /* First row */
    if (ImGui::Button("Start", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::START_ACQUISITION);

    ImGui::SameLine();
    if (ImGui::Button("Stop", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::STOP_ACQUISITION);

    ImGui::SameLine();
    RenderSetTopParametersButton(COMMAND_PALETTE_BUTTON_SIZE);

    ImGui::SameLine();
    if (ImGui::Button("Get", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::GET_PARAMETERS);

    /* Second row */
    ImGui::BeginDisabled();
    if (ImGui::Button("Force", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::FORCE_ACQUISITION);
    ImGui::EndDisabled();

    ImGui::SameLine();
    RenderSetClockSystemParametersButton(COMMAND_PALETTE_BUTTON_SIZE);

    ImGui::SameLine();
    if (ImGui::Button("Initialize", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::INITIALIZE_PARAMETERS);

    ImGui::SameLine();
    if (ImGui::Button("Validate", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::VALIDATE_PARAMETERS);

    if (ImGui::Button("32k 15Hz", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::DEFAULT_ACQUISITION);

    ImGui::SameLine();
    if (ImGui::Button("Internal\nReference", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::SET_INTERNAL_REFERENCE);

    ImGui::SameLine();
    if (ImGui::Button("External\nReference", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::SET_EXTERNAL_REFERENCE);

    ImGui::SameLine();
    if (ImGui::Button("External\nClock", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::SET_EXTERNAL_CLOCK);

    /* Third row */
    /* FIXME: Only first selected? */
    if (ImGui::Button("Copy Top\nFilename", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::GET_TOP_PARAMETERS_FILENAME);

    ImGui::SameLine();
    if (ImGui::Button("Copy Clock\nSystem\nFilename", COMMAND_PALETTE_BUTTON_SIZE))
        PushMessage(DigitizerMessageId::GET_CLOCK_SYSTEM_PARAMETERS_FILENAME);

    if (nof_selected == 0)
        ImGui::EndDisabled();

    ImGui::End();
}

void Ui::RenderSetTopParametersButton(const ImVec2 &size)
{
    ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Button);
    for (auto &digitizer : m_digitizers)
    {
        if (!digitizer.ui.is_selected)
            continue;

        /* FIXME: Figure out color when multiple units are selected. Perhaps we
                  need to store the state... */
        color = digitizer.ui.set_top_color;
        break;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    if (ImGui::Button("Set", size))
        PushMessage(DigitizerMessageId::SET_PARAMETERS);
    ImGui::PopStyleColor(2);
}

void Ui::RenderSetClockSystemParametersButton(const ImVec2 &size)
{
    ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Button);
    for (auto &digitizer : m_digitizers)
    {
        if (!digitizer.ui.is_selected)
            continue;

        /* FIXME: Figure out color when multiple units are selected. Perhaps we
                  need to store the state... */
        color = digitizer.ui.set_clock_system_color;
        break;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    if (ImGui::Button("Set Clock\nSystem", size))
        PushMessage(DigitizerMessageId::SET_CLOCK_SYSTEM_PARAMETERS);
    ImGui::PopStyleColor(2);
}

/* FIXME: Remove if no longer needed. */
void CenteredTextInCell(const std::string &str)
{
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         (ImGui::GetColumnWidth() - ImGui::CalcTextSize(str.c_str()).x) / 2);
    ImGui::Text(str);
}

void Ui::MarkerTree(Markers &markers)
{
    if (markers.empty())
        return;

    ImGui::Spacing();
    ImGui::Separator();
    int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!ImGui::TreeNodeEx(markers.label.c_str(), flags))
        return;

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Remove"))
            markers.clear();

        ImGui::EndPopup();
    }

    ImGui::Spacing();

    /* We don't need a vector to store multiple removal indexes since the user
       will only have one context men u up at a time and this loop runs a
       magnitude faster than the fastest clicking in GUI can achieve.  */
    int id_to_remove = -1;
    bool add_separator = false;

    for (auto &[id, marker] : markers)
    {
        if (add_separator)
            ImGui::Separator();
        add_separator = true;

        ImGui::ColorEdit4(fmt::format("##color{}{}", markers.label, id).c_str(),
                          (float *)&marker.color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        ImGui::SameLine();

        flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        bool node_open =
            ImGui::TreeNodeEx(fmt::format("##node{}", marker.id, markers.label).c_str(), flags);

        /* TODO: Maybe change the color instead. */
        if (ImGui::IsItemHovered())
            marker.thickness = 3.0f;
        else
            marker.thickness = 1.0f; /* FIXME: Restore 'unhovered' thickness instead. */

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Remove"))
                id_to_remove = static_cast<int>(id);

            if (ImGui::MenuItem("Clear deltas", NULL, false, !marker.deltas.empty()))
                marker.deltas.clear();

            ImGui::EndPopup();
        }

        const std::string payload_identifier = fmt::format("MARKER_REFERENCE{}", markers.label);
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload(payload_identifier.c_str(), &id, sizeof(id));
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(payload_identifier.c_str());
            if (payload != NULL)
            {
                size_t payload_id = *static_cast<const size_t *>(payload->Data);
                marker.deltas.insert(payload_id);
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        const float LINE_START = ImGui::GetCursorPosX();
        ImGui::Text(fmt::format("{}{} {}, {}", markers.prefix, id, marker.x.Format(),
                                marker.y.Format()));

        const auto &ui = m_digitizers[marker.digitizer].ui.channels[marker.channel];

        const float LABEL_START_NO_TEXT = ImGui::GetWindowContentRegionMax().x - 30.0f;
        const float LABEL_START = LABEL_START_NO_TEXT - ImGui::CalcTextSize(ui.record->label.c_str()).x;

        /* We need an empty SameLine() here for GetCursorPosX() to yield the correct value. */
        ImGui::SameLine();
        const bool label_collision = LABEL_START - ImGui::GetCursorPosX() < 0;

        ImGui::SameLine(label_collision ? LABEL_START_NO_TEXT : LABEL_START);
        if (!label_collision)
        {
            ImGui::Text(ui.record->label);
            ImGui::SameLine();
        }

        ImGui::ColorEdit4(fmt::format("##channel{}{}", markers.label, id).c_str(),
                          (float *)&ui.color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                              ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker);

        if (label_collision && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", ui.record->label.c_str());

        if (node_open)
        {
            if (!marker.deltas.empty())
            {
                flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings |
                        ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX;
                if (ImGui::BeginTable(fmt::format("##table{}{}", markers.label, id).c_str(), 2, flags))
                {
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

                    for (auto delta_id : marker.deltas)
                    {
                        const auto &delta_marker = markers.at(delta_id);
                        const double delta_x = delta_marker.x.value - marker.x.value;
                        const double delta_y = delta_marker.y.value - marker.y.value;

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::SetCursorPosX(LINE_START - ImGui::CalcTextSize("d").x);
                        ImGui::Text(fmt::format("d{}{}", markers.prefix, delta_marker.id));
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text(fmt::format(" {}, {}",
                                                marker.x.FormatDelta(delta_x, true),
                                                marker.y.FormatDelta(delta_y, true)));
                    }

                    ImGui::EndTable();
                }
            }

            ImGui::TreePop();
        }
    }

    if (id_to_remove >= 0)
    {
        /* Make sure to remove any delta references to the marker we're about to
           remove. The std::set::erase has no effect if the key does not exist.*/
        for (auto &[_, marker] : markers)
            marker.deltas.erase(id_to_remove);
        markers.erase(id_to_remove);
    }

    ImGui::TreePop();
}

void Ui::RenderMarkers()
{
    if (m_time_domain_markers.empty() && m_frequency_domain_markers.empty())
    {
        ImGui::Text("No markers to show.");
        return;
    }

    if (ImGui::SmallButton("Remove all"))
    {
        m_time_domain_markers.clear();
        m_frequency_domain_markers.clear();
    }

    MarkerTree(m_time_domain_markers);
    MarkerTree(m_frequency_domain_markers);
}

void Ui::RenderMemory()
{
    if (ImGui::SmallButton("Remove all"))
    {
        for (auto &digitizer : m_digitizers)
        {
            for (auto &chui : digitizer.ui.channels)
                chui.memory.clear();
        }
    }

    /* FIXME: Implement listing */
    WIP();
}

void Ui::RenderSensorGroup(SensorGroupUiState &group, bool is_first)
{
    if (!is_first)
    {
        ImGui::Spacing();
        ImGui::Separator();
    }

    int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    bool node_open = ImGui::TreeNodeEx(group.label.c_str(), flags);

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Plot"))
        {
            for (auto &[sensor_id, sensor] : group.sensors)
                sensor.is_plotted = true;
        }
        ImGui::EndPopup();
    }

    if (!node_open)
      return;

    flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings |
            ImGuiTableFlags_BordersInnerV;

    if (ImGui::BeginTable(fmt::format("Sensors##{}", group.label).c_str(), 2, flags))
    {
        ImGui::TableSetupColumn("Sensor", ImGuiTableColumnFlags_WidthFixed,
                                24 * ImGui::CalcTextSize("x").x);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);

        for (auto &[sensor_id, sensor] : group.sensors)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(sensor.label.c_str(), sensor.is_plotted,
                                  ImGuiSelectableFlags_SpanAllColumns))
            {
                printf("Sensor %d selected\n", sensor_id);
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                /* The payload is the _address_ of the `is_plotted` state,
                   allowing us to toggle it at the drag-and-drop target. So we
                   make sure to copy the _value_ of the pointer, which holds the
                   address we're after. */
                bool *payload = &sensor.is_plotted;
                ImGui::SetDragDropPayload("SENSOR", &payload, sizeof(payload));
                ImGui::EndDragDropSource();
            }

            if (sensor.record.status == 0 && ImGui::BeginPopupContextItem())
            {
                ImGui::MenuItem("Plot", "", &sensor.is_plotted);
                if (ImGui::MenuItem("Capture"))
                    printf("Capture sensor %d\n", sensor_id);
                ImGui::EndPopup();
            }

            if (sensor.record.status != 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("%s", sensor.record.note.c_str());

            ImGui::TableSetColumnIndex(1);
            if (sensor.record.status != 0)
            {
                ImGui::Text(fmt::format("{:>7} {}", "Error", sensor.record.status));
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
            }
            else
            {
                ImGui::Text(fmt::format("{:7.3f} {}", sensor.record.y.back(),
                                        sensor.record.y_properties.unit));
            }
        }

        ImGui::EndTable();
    }

    ImGui::TreePop();
}

void Ui::RenderSensors()
{
    DigitizerUiState *ui = NULL;
    for (auto &digitizer : m_digitizers)
    {
        if (digitizer.ui.is_selected && digitizer.ui.sensor_groups.size() > 0)
        {
            ui = &digitizer.ui;
            break;
        }
    }

    if (ui == NULL)
    {
        ImGui::Text("No data to display.");
        return;
    }

    ImGui::Text(ui->identifier);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x -
                    2 * ImGui::GetStyle().ItemInnerSpacing.x -
                    ImGui::CalcTextSize("Unplot").x);

    if (ImGui::SmallButton("Unplot"))
    {
        for (auto &[group_id, group] : ui->sensor_groups)
        {
            for (auto &[sensor_id, sensor] : group.sensors)
                sensor.is_plotted = false;
        }
    }

    ImGui::Separator();

    bool is_first = true;
    for (auto &[group_id, group] : ui->sensor_groups)
    {
        RenderSensorGroup(group, is_first);
        is_first = false;
    }
}

void Ui::RenderBootStatus()
{
    DigitizerUiState *ui = NULL;
    for (auto &digitizer : m_digitizers)
    {
        if (digitizer.ui.is_selected && digitizer.ui.boot_status.boot_entries.size() > 0)
        {
            ui = &digitizer.ui;
            break;
        }
    }

    if (ui == NULL)
    {
        ImGui::Text("No data to display.");
        return;
    }

    ImGui::Text(ui->identifier);
    ImGui::Separator();
    ImGui::Text(fmt::format("In state '{}' ({})", ui->boot_status.state_description,
                            ui->boot_status.state));
    ImGui::Separator();

    int flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings |
                ImGuiTableFlags_BordersInnerV;

    /* Increase the horizontal cell padding. */
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                        ImGui::GetStyle().CellPadding + ImVec2(3.0, 0.0));

    if (ImGui::BeginTable("Boot", 2, flags))
    {
        ImGui::TableSetupColumn("Boot", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);

        for (const auto &boot_entry : ui->boot_status.boot_entries)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Selectable(boot_entry.label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);

            if (boot_entry.status != 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("%s", boot_entry.note.c_str());

            ImGui::TableSetColumnIndex(1);
            if (boot_entry.status != 0)
            {
                ImGui::Text(fmt::format("Error {}", boot_entry.status));
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
            }
            else
            {
                ImGui::Text("OK");
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
}

void Ui::RenderStaticInformation()
{
    auto Row = [](const std::string &label, const std::string &value)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text(label);
        ImGui::TableSetColumnIndex(1);
        ImGui::Text(value);
    };

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings |
                            ImGuiTableFlags_BordersInnerV;

    /* "Sigscape version" is the worst case width */
    const float WIDTH = ImGui::CalcTextSize("Sigscape version").x;

    if (ImGui::CollapsingHeader("Software", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("Software", 2, flags))
        {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, WIDTH);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

            Row("sigscape", fmt::format("v{}", SIGSCAPE_VERSION));
            Row("libadq", fmt::format("0x{:08x}", m_api_revision));

            ImGui::EndTable();
        }
    }

    DigitizerUiState *ui = NULL;
    for (auto &digitizer : m_digitizers)
    {
        if (digitizer.ui.is_selected)
        {
            ui = &digitizer.ui;
            break;
        }
    }

    if (ui == NULL)
        return;

    if (ImGui::CollapsingHeader("Digitizer",
                                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf))
    {
        if (ImGui::BeginTable(fmt::format("Digitizer", ui->identifier).c_str(), 2, flags))
        {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, WIDTH);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);


            Row("Name", ui->constant.product_name);
            Row("Serial number", ui->constant.serial_number);
            Row("Options", ui->constant.product_options);
            Row("Channels", fmt::format("{}", ui->constant.nof_channels));
            Row("DRAM", fmt::format("{:.2f} MiB", ui->constant.dram_size / 1024.0 / 1024.0));

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Firmware", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("Firmware", 2, flags))
        {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, WIDTH);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

            auto Stringify = [](enum ADQFirmwareType firmware) -> std::string
            {
                switch (firmware)
                {
                case ADQ_FIRMWARE_TYPE_FWDAQ:
                    return "FWDAQ";
                case ADQ_FIRMWARE_TYPE_FWATD:
                    return "FWATD";
                default:
                    return "Unknown";
                }
            };

            Row("Type", Stringify(ui->constant.firmware.type));
            Row("Name", ui->constant.firmware.name);
            Row("Revision", ui->constant.firmware.revision);
            Row("Customization", ui->constant.firmware.customization);
            Row("Part number", ui->constant.firmware.part_number);

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Channels", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("Channels", 1 + ui->constant.nof_channels, flags))
        {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
            for (int i = 0; i < ui->constant.nof_channels; ++i)
            {
                ImGui::TableSetupColumn(fmt::format("CH {}", ui->constant.channel[i].label).c_str(),
                                        ImGuiTableColumnFlags_WidthStretch);
            }
            ImGui::TableHeadersRow();

            auto ChannelRow = [&](const std::string &label,
                                  const std::function<std::string(int)> &f)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(label);
                for (int i = 0; i < ui->constant.nof_channels; ++i)
                {
                    ImGui::TableSetColumnIndex(i + 1);
                    ImGui::Text(f(i));
                }
            };

            ChannelRow("Base sampling rate", [&](int i) -> std::string {
                return Format::Metric(ui->constant.channel[i].base_sampling_rate, "{:7.2f} {}Hz", 1e6);
            });

            ChannelRow("Input range", [&](int i) -> std::string {
                return Format::Metric(ui->constant.channel[i].input_range[0] / 1e3, "{:7.2f} {}V", 1e-3);
            });

            ChannelRow("ADC cores", [&](int i) -> std::string {
                return fmt::format("{}", ui->constant.channel[i].nof_adc_cores);
            });

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Clock System"))
    {
        WIP(); /* FIXME: */
    }

    if (ImGui::CollapsingHeader("Interface", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("Interface", 2, flags))
        {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

            auto Stringify = [](enum ADQCommunicationInterface interface) -> std::string
            {
                switch (interface)
                {
                case ADQ_COMMUNICATION_INTERFACE_PCIE:
                    return "PCIe";
                case ADQ_COMMUNICATION_INTERFACE_USB:
                    return "USB";
                default:
                    return "Unknown";
                }
            };

            Row("Type", Stringify(ui->constant.communication_interface.type));
            Row("Link generation", fmt::format("{}", ui->constant.communication_interface.link_generation));
            Row("Link width", fmt::format("{}", ui->constant.communication_interface.link_width));

            ImGui::EndTable();
        }
    }
}

void Ui::RenderTools(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Tools", NULL,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("Tools##TabBar", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Markers"))
        {
            RenderMarkers();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Memory"))
        {
            RenderMemory();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Sensors"))
        {
            RenderSensors();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Boot"))
        {
            RenderBootStatus();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Static"))
        {
            RenderStaticInformation();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void Ui::RenderProcessingOptions(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Processing Options", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    m_collapsed.processing_options = ImGui::IsWindowCollapsed();
    bool push_parameters = false;

    static int window_idx = 2;
    static int window_idx_prev = 2;
    const char *window_labels[] = {"No window", "Blackman-Harris", "Flat top", "Hamming", "Hanning"};
    ImGui::Combo("Window", &window_idx, window_labels, IM_ARRAYSIZE(window_labels));

    if (window_idx != window_idx_prev)
    {
        switch (window_idx)
        {
        case 0:
            m_processing_parameters.window_type = WindowType::NONE;
            break;

        case 1:
            m_processing_parameters.window_type = WindowType::BLACKMAN_HARRIS;
            break;

        case 2:
            m_processing_parameters.window_type = WindowType::FLAT_TOP;
            break;

        case 3:
            m_processing_parameters.window_type = WindowType::HAMMING;
            break;

        case 4:
            m_processing_parameters.window_type = WindowType::HANNING;
            break;
        }

        window_idx_prev = window_idx;
        push_parameters = true;
    }

    static int scaling_idx = 0;
    static int scaling_idx_prev = 0;
    const char *scaling_labels[] = {"Amplitude", "Energy"};
    ImGui::Combo("Scaling", &scaling_idx, scaling_labels, IM_ARRAYSIZE(scaling_labels));

    if (scaling_idx != scaling_idx_prev)
    {
        switch (scaling_idx)
        {
        case 0:
            m_processing_parameters.fft_scaling = FrequencyDomainScaling::AMPLITUDE;
            break;

        case 1:
            m_processing_parameters.fft_scaling = FrequencyDomainScaling::ENERGY;
            break;
        }

        scaling_idx_prev = scaling_idx;
        push_parameters = true;
    }

    static const ImS32 LIMIT_LOW = 0;
    static const ImS32 LIMIT_HIGH = 16;
    static ImS32 nof_skirt_bins = m_processing_parameters.nof_skirt_bins;
    if (ImGui::SliderScalar("Skirt bins", ImGuiDataType_S8, &nof_skirt_bins, &LIMIT_LOW,
                            &LIMIT_HIGH, NULL, ImGuiSliderFlags_NoInput))
    {
        m_processing_parameters.nof_skirt_bins = static_cast<int>(nof_skirt_bins);
        push_parameters = true;
    }

    /* TODO: Right now, any visible time domain markers end up in incorrect
             positions when the x/y conversion is toggled. Just clear them for
             now. */
    if (ImGui::Checkbox("Convert samples to time", &m_processing_parameters.convert_horizontal))
    {
        m_time_domain_markers.clear();
        push_parameters = true;
    }

    if (ImGui::Checkbox("Convert codes to volts", &m_processing_parameters.convert_vertical))
    {
        m_time_domain_markers.clear();
        push_parameters = true;
    }

    if (ImGui::Checkbox("Full-scale ENOB", &m_processing_parameters.fullscale_enob))
        push_parameters = true;

    if (push_parameters)
        PushMessage({DigitizerMessageId::SET_PROCESSING_PARAMETERS, m_processing_parameters}, false);

    ImGui::End();
}

/* FIXME: Remove */
void Ui::Reduce(double xsize, double sampling_period, int &count, int &stride)
{
    /* Determine how many samples fit in the current view. If this number
       exceeds MAX_NOF_SAMPLES, repeatedly increase the stride by a factor of
       two until we're in range. */
    static const int MAX_NOF_SAMPLES = 32768;
    const int MAX_NOF_SAMPLES_IN_WINDOW = static_cast<int>(xsize / sampling_period + 0.5);

    stride = 1;
    if ((count < MAX_NOF_SAMPLES) || (MAX_NOF_SAMPLES_IN_WINDOW < MAX_NOF_SAMPLES))
        return;

    /* Reduce the number of visible samples. */
    do
    {
        stride += 1;
    } while ((count / stride) > MAX_NOF_SAMPLES);

    count /= stride;
}

void Ui::SnapX(double x, const BaseRecord *record, double &snap_x, double &snap_y)
{
    if (x < record->x.front())
    {
        snap_x = record->x.front();
        snap_y = record->y.front();
    }
    else if (x > record->x.back())
    {
        snap_x = record->x.back();
        snap_y = record->y.back();
    }
    else
    {
        /* Get the distance to the first sample, which we now know is a positive
           value in the range spanned by the x-vector. */
        double distance = x - record->x.front();
        size_t index = static_cast<size_t>(std::round(distance / record->step));
        snap_x = record->x[index];
        snap_y = record->y[index];
    }
}

void Ui::GetClosestSampleIndex(double x, double y, const BaseRecord *record, const ImPlotRect &view,
                               size_t &index)
{
    /* Find the closest sample to the coordinates (x,y) by minimizing the
       Euclidian distance. We have to normalize the data fpr this method to give
       the desired results---namely, we have to normalize using the _plot_
       limits since that is the perceived reference frame of the user. */

    const double kx = (view.X.Max - view.X.Min) / 2;
    const double mx = -(view.X.Min + kx);
    const double ky = (view.Y.Max - view.Y.Min) / 2;
    const double my = -(view.Y.Min + ky);

    const double x_normalized = (x + mx) / kx;
    const double x_step_normalized = record->step / kx;
    const double y_normalized = (y + my) / ky;

    const double x0_normalized = (record->x.front() + mx) / kx;
    const double center = std::round((x_normalized - x0_normalized) / x_step_normalized);

    /* Create a symmetric span around the rounded x-coordinate and then clip the
       limits to the range where there's data. */
    const double span = 16.0;
    const double low_limit = 0.0;
    const double high_limit = static_cast<double>(record->x.size() - 1);
    double span_low = center - span;
    double span_high = center + span;

    if (span_low < low_limit)
        span_low = low_limit;
    else if (span_low > (high_limit - span))
        span_low = std::max(high_limit - span, low_limit);

    if (span_high < (low_limit + span))
        span_high = std::min(low_limit + span, high_limit);
    else if (span_high > high_limit)
        span_high = high_limit;

    const size_t low = static_cast<size_t>(span_low);
    const size_t high = static_cast<size_t>(span_high);
    double distance_min = std::numeric_limits<double>::max();

    for (size_t i = low; i <= high; ++i)
    {
        const double xi = (record->x[i] + mx) / kx;
        const double yi = (record->y[i] + my) / ky;

        const double x2 = std::pow(x_normalized - xi, 2);
        const double y2 = std::pow(y_normalized - yi, 2);
        const double distance = x2 + y2;

        if (distance < distance_min)
        {
            index = i;
            distance_min = distance;
        }
    }
}

std::vector<std::tuple<size_t, size_t, Ui::ChannelUiState *>> Ui::FilterUiStates()
{
    /* We pass through all UI states and filter out the ones we should pass on
       for plotting. First, we skip any unselected digitizers. Second, we skip
       any channels w/o data. We take special care to put the selected channel
       at the end to cause this data to be plotted last, and thus effectively
       'on top' of all the other traces.

       Additionally, we consider the two channel properties 'solo' and 'muted'.
       This concept is borrowed from the world of music editing software. They
       do what it says on the tin, but here are the edge cases: if a channel is
       both muted and solo, it is included. If several channels are marked solo,
       they are all included. */

    std::vector<std::tuple<size_t, size_t, ChannelUiState *>> result{};
    std::tuple<size_t, size_t, ChannelUiState *> selected{0, 0, NULL};
    bool any_solo = false;

    for (size_t i = 0; i < m_digitizers.size(); ++i)
    {
        if (!m_digitizers[i].ui.is_selected)
            continue;

        for (size_t ch = 0; ch < m_digitizers[i].ui.channels.size(); ++ch)
        {
            auto &ui = m_digitizers[i].ui.channels[ch];
            if (ui.record == NULL)
                continue;

            if (ui.is_solo)
                any_solo = true;

            if (ui.is_selected)
                selected = {i, ch, &ui};
            else
                result.emplace_back(i, ch, &ui);
        }
    }

    if (std::get<2>(selected) != NULL)
        result.push_back(selected);

    /* Filter again based on the solo/muted state. */
    auto MaskSoloMuted = [&](std::tuple<size_t, size_t, ChannelUiState *> &x) -> bool
    {
        auto &[i, ch, ui] = x;
        if (ui->is_solo)
            return false;
        else if (ui->is_muted || any_solo)
            return true;
        else
            return false;
    };

    result.erase(std::remove_if(result.begin(), result.end(), MaskSoloMuted), result.end());
    return result;
}

void Ui::GetUnitsPerDivision(const std::string &title, UnitsPerDivision &units_per_division)
{
    /* WARNING: This function makes use of ImPlot's internal API and may _only_
                be called after EndPlot(). Otherwise, the ticker is invalid. */

    const auto &axis_x = ImPlot::GetPlot(title.c_str())->Axes[ImAxis_X1];
    const auto &axis_y = ImPlot::GetPlot(title.c_str())->Axes[ImAxis_Y1];

    /* A small workaround is needed to handle the fact that using a 'time
       scaled' axis (which we use for the sensor plot) repeats the first tick
       twice giving a delta of zero when we use [1] - [0]. For all practical
       purposes we should always have a tick count >2 so we expect to hit the
       first case pretty much every time. */
    if (axis_x.Ticker.TickCount() > 2)
        units_per_division.x = axis_x.Ticker.Ticks[2].PlotPos - axis_x.Ticker.Ticks[1].PlotPos;
    else if (axis_x.Ticker.TickCount() > 1)
        units_per_division.x = axis_x.Ticker.Ticks[1].PlotPos - axis_x.Ticker.Ticks[0].PlotPos;
    else
        units_per_division.x = axis_x.Range.Size();

    if (axis_y.Ticker.TickCount() > 2)
        units_per_division.y = axis_y.Ticker.Ticks[2].PlotPos - axis_y.Ticker.Ticks[1].PlotPos;
    else if (axis_y.Ticker.TickCount() > 1)
        units_per_division.y = axis_y.Ticker.Ticks[1].PlotPos - axis_y.Ticker.Ticks[0].PlotPos;
    else
        units_per_division.y = axis_y.Range.Size();
}

void Ui::RenderUnitsPerDivision(const std::string &str)
{
    /* This function will output the input `str` in a 'legend' style frame in
       the current plot's lower left corner. The function must be called inside
       Begin/EndPlot(). */

    const ImVec2 lower_left_corner{ImPlot::GetPlotPos().x,
                                   ImPlot::GetPlotPos().y + ImPlot::GetPlotSize().y};
    const ImVec2 text_size{ImGui::CalcTextSize(str.c_str())};
    const ImVec2 offset{10, -10};
    const ImVec2 padding{5, 5};

    const ImRect rect{
        /* Upper left corner */
        lower_left_corner + offset + ImVec2{0, -text_size.y - 2 * padding.y},
        /* Lower right corner */
        lower_left_corner + offset + ImVec2{text_size.x + 2 * padding.x, 0}
    };

    const ImVec2 text{rect.Min + padding};

    ImPlot::GetPlotDrawList()->AddRectFilled(rect.Min, rect.Max,
                                             ImPlot::GetStyleColorU32(ImPlotCol_LegendBg));
    ImPlot::GetPlotDrawList()->AddRect(rect.Min, rect.Max,
                                       ImPlot::GetStyleColorU32(ImPlotCol_LegendBorder));
    ImPlot::GetPlotDrawList()->AddText(text, ImPlot::GetStyleColorU32(ImPlotCol_LegendText),
                                       str.c_str());
}

void Ui::PlotTimeDomainSelected()
{
    /* We need a (globally) unique id for each marker. */
    int marker_id = 0;
    bool first = true;

    for (auto &[i, ch, ui] : FilterUiStates())
    {
        /* One-shot configuration to sample units (assuming to be equal for all
           records in this domain) and to set up the plot axes. */
        if (first)
        {
            const auto &record = ui->record->time_domain;
            ImPlot::SetupAxisFormat(ImAxis_X1, Format::Metric, record->x_properties.unit.data());
            ImPlot::SetupAxisFormat(ImAxis_Y1, Format::Metric, record->y_properties.unit.data());
            m_time_domain_units_per_division.x_unit = record->x_properties.delta_unit;
            m_time_domain_units_per_division.y_unit = record->y_properties.delta_unit;
            first = false;
        }

        /* Unset the automatic auto fit as soon as we know we have something to
           plot (it's already been armed at this point). */
        if (m_should_auto_fit_time_domain)
          m_should_auto_fit_time_domain = false;

        /* FIXME: A rough value to switch off persistent plotting when the
                  window contains too many samples, heavily tanking the
                  performance. */

        bool is_persistence_performant =
            ImPlot::GetPlotLimits().Size().x / ui->record->time_domain->step < 2048;

        if (ui->is_persistence_enabled && is_persistence_performant)
        {
            ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
            for (const auto &c : ui->record->persistence->data)
            {
                ImPlot::PlotScatter(ui->record->label.c_str(), c->x.data(), c->y.data(),
                                    static_cast<int>(c->x.size()));
            }
            ImPlot::PopStyleVar();
        }
        else
        {
            if (ui->is_sample_markers_enabled)
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Cross);

            ImPlot::PushStyleColor(ImPlotCol_Line, ui->color);
            ImPlot::PlotLine(ui->record->label.c_str(), ui->record->time_domain->x.data(),
                             ui->record->time_domain->y.data(),
                             static_cast<int>(ui->record->time_domain->x.size()));
            ImPlot::PopStyleColor();
        }

        /* Plot any waveforms in memory. */
        if (!ui->memory.empty())
        {
            /* TODO: Ideally RGB -> HSV -> / increase brightness / -> RGB */
            auto memory_color = ui->color;
            memory_color.x *= 1.5;
            memory_color.y *= 1.5;
            memory_color.z *= 1.5;
            ImPlot::PushStyleColor(ImPlotCol_Line, memory_color);
            for (size_t m = 0; m < ui->memory.size(); ++m)
            {
                const auto &memory_record = ui->memory[m];
                const std::string label = fmt::format("M{} {}", m, memory_record->label);
                ImPlot::PlotLine(label.c_str(), memory_record->time_domain->x.data(),
                                 memory_record->time_domain->y.data(),
                                 static_cast<int>(memory_record->time_domain->x.size()));
            }
            ImPlot::PopStyleColor();
        }

        /* Here we have to resort to using ImPlot internals to gain access to
           whether or not the plot is shown or not. The user can click the
           legend entry to change the visibility state. */

        auto item = ImPlot::GetCurrentContext()->CurrentItems->GetItem(ui->record->label.c_str());
        ui->is_time_domain_visible = (item != NULL) && item->Show;

        if (ui->is_time_domain_visible)
        {
            for (auto &[id, marker] : m_time_domain_markers)
            {
                if (marker.digitizer != i || marker.channel != ch)
                    continue;

                SnapX(marker.x.value, ui->record->time_domain.get(), marker.x.value, marker.y.value);

                ImPlot::DragPoint(0, &marker.x.value, &marker.y.value, marker.color,
                                  3.0f + marker.thickness, ImPlotDragToolFlags_NoInputs);
                DrawMarkerX(marker_id++, &marker.x.value, marker.color, marker.thickness,
                            marker.x.Format(".2"));
                DrawMarkerY(marker_id++, &marker.y.value, marker.color, marker.thickness,
                            marker.y.Format(), ImPlotDragToolFlags_NoInputs);

                ImPlot::Annotation(marker.x.value, ImPlot::GetPlotLimits().Y.Max,
                                   ImVec4(0, 0, 0, 0), ImVec2(10, 10), false, "T%zu", id);
            }

            if (ui->is_selected)
                MaybeAddMarker(i, ch, ui->record->time_domain.get(), m_time_domain_markers);
        }
    }
}

void Ui::DrawMarkerX(int id, double *x, const ImVec4 &color, float thickness,
                     const std::string &tag, ImPlotDragToolFlags flags)
{
    ImPlot::DragLineX(id, x, color, thickness, flags);
    ImPlot::TagX(*x, color, "%s", tag.c_str());
}

void Ui::DrawMarkerY(int id, double *y, const ImVec4 &color, float thickness,
                     const std::string &tag, ImPlotDragToolFlags flags)
{
    ImPlot::DragLineY(id, y, color, thickness, flags);
    ImPlot::TagY(*y, color, "%s", tag.c_str());
}

void Ui::MaybeAddMarker(size_t digitizer, size_t channel, const BaseRecord *record,
                        Markers &markers)
{
    if (ImPlot::IsPlotHovered() && ImGui::GetIO().KeyCtrl && ImGui::IsMouseClicked(0))
    {
        size_t index;
        GetClosestSampleIndex(ImPlot::GetPlotMousePos().x, ImPlot::GetPlotMousePos().y, record,
                              ImPlot::GetPlotLimits(), index);

        /* FIXME: Probably need to consider the initial x/y-values to be
                  special. Otherwise, the marker can seem to wander a bit if the
                  signal is noisy. */
        markers.insert(digitizer, channel, index, record->ValueX(record->x[index]),
                       record->ValueY(record->y[index]));
        markers.is_adding = true;
        markers.is_dragging = false;
    }

    if (markers.is_adding && ImGui::IsMouseDragging(0))
    {
        /* FIXME: Very rough test of delta dragging */
        if (!markers.is_dragging)
        {
            size_t index;
            GetClosestSampleIndex(ImPlot::GetPlotMousePos().x, ImPlot::GetPlotMousePos().y, record,
                                  ImPlot::GetPlotLimits(), index);
            markers.insert(digitizer, channel, index, record->ValueX(record->x[index]),
                           record->ValueY(record->y[index]), true);
        }

        markers.is_dragging = true;
        markers.last().x.value = ImPlot::GetPlotMousePos().x;
    }

    if (markers.is_adding && ImGui::IsMouseReleased(0))
    {
        markers.is_adding = false;
        markers.is_dragging = false;
    }
}

bool Ui::IsHoveredAndDoubleClicked(const Marker &marker)
{
    /* Construct a rect that covers the markers. If the mouse is double clicked
       while inside these limits, we remove the marker. This needs to be called
       within a current plot that also contains the provided marker. */

    const auto lmarker = ImPlot::PlotToPixels(marker.x.value, marker.y.value);
    const auto limits = ImPlot::GetPlotLimits();
    const auto upper_left = ImPlot::PlotToPixels(limits.X.Min, limits.Y.Max);
    const auto lower_right = ImPlot::PlotToPixels(limits.X.Max, limits.Y.Min);

    const ImVec2 marker_x_upper_left{std::round(lmarker.x) - 4.0f, upper_left.y};
    const ImVec2 marker_x_lower_right{std::round(lmarker.x) + 4.0f, lower_right.y};
    const ImVec2 marker_y_upper_left{upper_left.x, std::round(lmarker.y) - 4.0f};
    const ImVec2 marker_y_lower_right{lower_right.x, std::round(lmarker.y) + 4.0f};

    return ImGui::IsMouseDoubleClicked(0) &&
           (ImGui::IsMouseHoveringRect(marker_x_upper_left, marker_x_lower_right) ||
            ImGui::IsMouseHoveringRect(marker_y_upper_left, marker_y_lower_right));
}

void Ui::RemoveDoubleClickedMarkers(Markers &markers)
{
    for (auto it = markers.begin(); it != markers.end(); )
    {
        if (IsHoveredAndDoubleClicked(it->second))
        {
            /* Make sure to remove any delta references to the marker we're
               about to remove. The std::set::erase has no effect if the key
               does not exist. */
            for (auto &[_, marker] : markers)
                marker.deltas.erase(it->first);
            it = markers.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Ui::RenderChannelPlot()
{
    if (m_should_auto_fit_time_domain)
        ImPlot::SetNextAxesToFit();

    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.0f, 0.1f));
    if (ImPlot::BeginPlot("Channels##Plot", ImVec2(-1, -1), ImPlotFlags_NoTitle))
    {
        ImPlot::SetupLegend(ImPlotLocation_NorthEast, ImPlotLegendFlags_Sort);
        PlotTimeDomainSelected();
        RemoveDoubleClickedMarkers(m_time_domain_markers);
        RenderUnitsPerDivision(m_time_domain_units_per_division.Format());

        ImPlot::EndPlot();

        /* Update the units per division. It's imperative that this is carried
           out after ImPlot::EndPlot() has been called. */
        GetUnitsPerDivision("Channels##Plot", m_time_domain_units_per_division);
    }
    ImPlot::PopStyleVar();
}

void Ui::PlotSensorsSelected()
{
    /* FIXME: Rework all this selection logic. */
    for (const auto &digitizer : m_digitizers)
    {
        const auto &sensor_groups = digitizer.ui.sensor_groups;
        if (sensor_groups.size() == 0)
            continue;

        for (const auto &[group_id, group] : sensor_groups)
        {
            for (const auto &[sensor_id, sensor] : group.sensors)
            {
                if (!sensor.is_plotted)
                    continue;

                const auto label = fmt::format("{}:{}", digitizer.ui.identifier, sensor.label);
                ImPlot::PlotLine(label.c_str(), sensor.record.x.data(), sensor.record.y.data(),
                                 static_cast<int>(sensor.record.x.size()));
            }
        }
    }
}

void Ui::RenderSensorPlot()
{
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.0f, 0.1f));
    if (ImPlot::BeginPlot("Sensors##Plot", ImVec2(-1, -1), ImPlotFlags_NoTitle))
    {
        ImPlot::SetupLegend(ImPlotLocation_NorthEast, ImPlotLegendFlags_Sort);
        ImPlot::SetupAxisFormat(ImAxis_X1, Format::Metric, (void *)"s");
        ImPlot::SetupAxis(ImAxis_X1, NULL, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::GetStyle().Use24HourClock = true;
        ImPlot::GetStyle().UseISO8601 = true;
        ImPlot::GetStyle().UseLocalTime = true;

        if (ImPlot::BeginDragDropTargetPlot())
        {
            const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SENSOR");
            if (payload != NULL)
            {
                /* The payload is an address to the `is_plotted` state (boolean)
                   for the dragged sensor. */
                auto data = *static_cast<bool **>(payload->Data);
                *data = true;
            }
            ImPlot::EndDragDropTarget();
        }

        PlotSensorsSelected();

        /* FIXME: Vertical units? Probably multiple axes. */
        RenderUnitsPerDivision(m_sensor_units_per_division.Format());

        ImPlot::EndPlot();

        /* Update the units per division. It's imperative that this is carried
           out after ImPlot::EndPlot() has been called. */
        GetUnitsPerDivision("Sensors##Plot", m_sensor_units_per_division);
    }
    ImPlot::PopStyleVar();
}

void Ui::RenderTimeDomain(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Time Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    m_collapsed.time_domain = ImGui::IsWindowCollapsed();
    if (ImGui::BeginTabBar("Time Domain##TabBar"))
    {
        if (ImGui::BeginTabItem("Channels"))
        {
            RenderChannelPlot();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Sensors"))
        {
            RenderSensorPlot();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();
}

void Ui::RenderFrequencyDomain(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Frequency Domain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    m_collapsed.frequency_domain = ImGui::IsWindowCollapsed();
    if (ImGui::BeginTabBar("Frequency Domain##TabBar"))
    {
        if (ImGui::BeginTabItem("FFT"))
        {
            RenderFourierTransformPlot();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Waterfall"))
        {
            RenderWaterfallPlot();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void Ui::Annotate(const std::tuple<Value, Value> &point, const std::string &label)
{
    /* TODO: We need some form of low-pass filter for the y-coordinates. For
             high update rates (~60 Hz) the annotations jitter quite a lot. Just
             naively rounding them to the nearest integer remove some of the
             jitter but can introduce even larger jumps for noisy data. */
    const auto &[x, y] = point;
    ImPlot::Annotation(x.value, y.value, ImVec4(0, 0, 0, 0), ImVec2(0, -5), false, "%s",
                       y.Format(".2").c_str());
    ImPlot::Annotation(x.value, y.value, ImVec4(0, 0, 0, 0),
                       ImVec2(0, -5 - ImGui::GetTextLineHeight()), false, "%s",
                       x.Format(".2").c_str());

    if (label.size() > 0)
    {
        ImPlot::Annotation(x.value, y.value, ImVec4(0, 0, 0, 0),
                           ImVec2(0, -5 - 2 * ImGui::GetTextLineHeight()), true, "%s",
                           label.c_str());
    }
}

void Ui::PlotFourierTransformSelected()
{
    /* We need a (globally) unique id for each marker. */
    int marker_id = 0;
    bool first = true;

    for (auto &[i, ch, ui] : FilterUiStates())
    {
        /* One-shot configuration to sample units (assuming to be equal for all
           records in this domain) and to set up the plot axes. */
        if (first)
        {
            const auto &record = ui->record->frequency_domain;
            ImPlot::SetupAxisFormat(ImAxis_X1, Format::Metric, record->x_properties.unit.data());
            m_frequency_domain_units_per_division.x_unit = record->x_properties.delta_unit;
            m_frequency_domain_units_per_division.y_unit = record->y_properties.delta_unit;
            first = false;
        }

        /* Unset the automatic auto fit as soon as we know we have something to
           plot (it's already been armed at this point). */
        if (m_should_auto_fit_frequency_domain)
          m_should_auto_fit_frequency_domain = false;

        ImPlot::PushStyleColor(ImPlotCol_Line, ui->color);
        ImPlot::PlotLine(ui->record->label.c_str(), ui->record->frequency_domain->x.data(),
                         ui->record->frequency_domain->y.data(),
                         static_cast<int>(ui->record->frequency_domain->x.size()));
        ImPlot::PopStyleColor();

        /* Plot any waveforms in memory. */
        if (!ui->memory.empty())
        {
            /* TODO: Ideally RGB -> HSV -> / increase brightness / -> RGB */
            auto memory_color = ui->color;
            memory_color.x *= 1.5;
            memory_color.y *= 1.5;
            memory_color.z *= 1.5;
            ImPlot::PushStyleColor(ImPlotCol_Line, memory_color);
            for (size_t m = 0; m < ui->memory.size(); ++m)
            {
                const auto &memory_record = ui->memory[m];
                const std::string label = fmt::format("M{} {}", m, memory_record->label);
                ImPlot::PlotLine(label.c_str(), memory_record->frequency_domain->x.data(),
                                 memory_record->frequency_domain->y.data(),
                                 static_cast<int>(memory_record->frequency_domain->x.size()));
            }
            ImPlot::PopStyleColor();
        }

        /* Here we have to resort to using ImPlot internals to gain access to
           whether or not the plot is shown or not. The user can click the
           legend entry to change the visibility state. */

        auto item = ImPlot::GetCurrentContext()->CurrentItems->GetItem(ui->record->label.c_str());
        ui->is_frequency_domain_visible = (item != NULL) && item->Show;

        if (ui->is_frequency_domain_visible)
        {
            Annotate(ui->record->frequency_domain->fundamental, "Fund.");

            if (ui->is_interleaving_spurs_annotated)
            {
                Annotate(ui->record->frequency_domain->gain_phase_spur, "TIx");
                Annotate(ui->record->frequency_domain->offset_spur, "TIo");
            }

            if (ui->is_harmonics_annotated)
            {
                for (size_t j = 0; j < ui->record->frequency_domain->harmonics.size(); ++j)
                {
                    Annotate(ui->record->frequency_domain->harmonics[j],
                            fmt::format("HD{}", j + 2));
                }
            }

            for (auto &[id, marker] : m_frequency_domain_markers)
            {
                if (marker.digitizer != i || marker.channel != ch)
                    continue;

                SnapX(marker.x.value, ui->record->frequency_domain.get(), marker.x.value, marker.y.value);

                ImPlot::DragPoint(0, &marker.x.value, &marker.y.value, marker.color,
                                  3.0f + marker.thickness, ImPlotDragToolFlags_NoInputs);
                DrawMarkerX(marker_id++, &marker.x.value, marker.color, marker.thickness,
                            marker.x.Format(".2"));
                DrawMarkerY(marker_id++, &marker.y.value, marker.color, marker.thickness,
                            marker.y.Format(), ImPlotDragToolFlags_NoInputs);

                ImPlot::Annotation(marker.x.value, ImPlot::GetPlotLimits().Y.Max,
                                   ImVec4(0, 0, 0, 0), ImVec2(10, 10), false, "F%zu", id);
            }

            if (ui->is_selected)
            {
                MaybeAddMarker(i, ch, ui->record->frequency_domain.get(),
                               m_frequency_domain_markers);
            }
        }
    }
}

void Ui::RenderFourierTransformPlot()
{
    if (m_should_auto_fit_frequency_domain)
        ImPlot::SetNextAxesToFit();

    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.0f, 0.2f));
    if (ImPlot::BeginPlot("FFT##plot", ImVec2(-1, -1), ImPlotFlags_NoTitle))
    {
        ImPlot::SetupLegend(ImPlotLocation_NorthEast, ImPlotLegendFlags_Sort);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -100.0, 10.0);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 1e9);
        PlotFourierTransformSelected();
        RemoveDoubleClickedMarkers(m_frequency_domain_markers);
        RenderUnitsPerDivision(m_frequency_domain_units_per_division.Format());

        ImPlot::EndPlot();

        /* Update the units per division. It's imperative that this is carried
           out after ImPlot::EndPlot() has been called. */
        GetUnitsPerDivision("FFT##plot", m_frequency_domain_units_per_division);
    }
    ImPlot::PopStyleVar();
}

void Ui::PlotWaterfallSelected(double &scale_min, double &scale_max)
{
    /* Only plot the selected channel, which will be at the back of the filtered UI states. */
    const auto filtered_ui = FilterUiStates();
    if (!filtered_ui.empty())
    {
        /* TODO: Y-axis scale (probably time delta?) */
        const auto &[i, ch, ui] = filtered_ui.back();
        ImPlot::SetupAxisFormat(ImAxis_X1, Format::Metric,
                                ui->record->frequency_domain->x_properties.unit.data());

        /* Unset the automatic auto fit as soon as we know we have something to
           plot (it's already been armed at this point). */
        if (m_should_auto_fit_waterfall)
          m_should_auto_fit_waterfall = false;

        scale_min = std::round(ui->record->frequency_domain->noise_moving_average.value);
        scale_max = 0.0;

        const double TOP_RIGHT = ui->record->time_domain->sampling_frequency.value / 2;
        ImPlot::PlotHeatmap("heat", ui->record->waterfall->data.data(),
                            static_cast<int>(ui->record->waterfall->rows),
                            static_cast<int>(ui->record->waterfall->columns),
                            scale_min, scale_max, NULL,
                            ImPlotPoint(0, 0), ImPlotPoint(TOP_RIGHT, 1));
        return;
    }
}

void Ui::RenderWaterfallPlot()
{
    const float LEGEND_WIDTH = 70.0f;
    const float PLOT_WIDTH = ImGui::GetWindowContentRegionMax().x - LEGEND_WIDTH;
    const int PLOT_FLAGS = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend;
    double scale_min = -100;
    double scale_max = 0;

    if (m_should_auto_fit_waterfall)
        ImPlot::SetNextAxesToFit();

    ImPlot::PushColormap("Plasma");
    if (ImPlot::BeginPlot("Waterfall##plot", ImVec2(PLOT_WIDTH, -1.0f), PLOT_FLAGS))
    {
        const ImPlotAxisFlags X1_FLAGS = ImPlotAxisFlags_NoGridLines;
        const ImPlotAxisFlags Y1_FLAGS = ImPlotAxisFlags_NoGridLines |
                                         ImPlotAxisFlags_NoTickLabels |
                                         ImPlotAxisFlags_NoTickMarks;
        ImPlot::SetupAxes(NULL, NULL, X1_FLAGS, Y1_FLAGS);
        PlotWaterfallSelected(scale_min, scale_max);
        ImPlot::EndPlot();
    }
    ImGui::SameLine();
    ImPlot::ColormapScale(" ##waterfallscale", scale_min, scale_max, ImVec2(LEGEND_WIDTH, -1.0f));
    ImPlot::PopColormap();
}

void Ui::RenderHeaderButtons(ChannelUiState &ui)
{
    ImGui::ColorEdit4((ui.record->label + "##ColorPicker").c_str(), (float *)&ui.color,
                       ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    ImGui::SameLine();

    auto Button = [](const std::string &label, bool &state)
    {
        auto button_color = COLOR_WOW_RED;
        button_color.w = 0.8f;

        auto hovered_color = button_color;
        hovered_color.w = 1.0f;

        auto active_color = button_color;
        active_color.w = 0.8f;

        bool lstate = state;
        if (lstate)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, button_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, active_color);
        }

        const ImVec2 SIZE = {ImGui::GetTextLineHeightWithSpacing(),
                             ImGui::GetTextLineHeightWithSpacing()};

        /* Slight tweaking to align w/ color box. */
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1);

        if (ImGui::Button(label.c_str(), SIZE))
            state ^= true;

        if (lstate)
        {
            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
        }
    };

    Button(fmt::format("S##Solo{}", ui.record->label), ui.is_solo);
    ImGui::SameLine();

    Button(fmt::format("M##Muted{}", ui.record->label), ui.is_muted);
    ImGui::SameLine();
}


std::vector<std::vector<std::string>> Ui::FormatTimeDomainMetrics(
    const ProcessedRecord *processed_record)
{
    const auto &record = processed_record->time_domain;
    const double peak_to_peak = record->max.value - record->min.value; /* FIXME: +1.0 'unit' */
    const double peak_to_peak_range = record->range_max.value - record->range_min.value;

    return {
        {
            "Record number",
            fmt::format("{: >8d}", record->header.record_number),
        },
        {
            "Maximum",
            record->max.Format(),
            record->range_max.Format(),
        },
        {
            "Minimum",
            record->min.Format(),
            record->range_min.Format(),
        },
        {
            "Peak-to-peak",
            record->max.Format(peak_to_peak),
            record->range_max.Format(peak_to_peak_range),
            fmt::format("{:6.2f} %", 100.0 * peak_to_peak / peak_to_peak_range),
        },
        {
            "Mean",
            record->mean.Format(),
            record->range_mid.Format(),
        },
        {
            "RMS",
            record->rms.Format(),
        },
        /* TODO: Hide these behind some 'show extra' toggle? */
        {
            "Sampling rate",
            record->sampling_frequency.Format(),
        },
        {
            "Sampling period",
            record->sampling_period.Format(),
        },
        {
            "Trigger rate",
            record->estimated_trigger_frequency.Format(),
        },
        {
            "Throughput",
            record->estimated_throughput.Format(),
        },
    };
}

std::vector<std::vector<std::string>> Ui::FormatFrequencyDomainMetrics(
    const ProcessedRecord *processed_record)
{
    const auto &record = processed_record->frequency_domain;
    return {
        {
            "SNR",
            record->snr.Format(),
            "Fund.",
            std::get<0>(record->fundamental).Format(),
            std::get<1>(record->fundamental).Format(), /* FIXME: mDBFS wrong */
        },
        {
            "SINAD",
            record->sinad.Format(),
            "Spur",
            std::get<0>(record->spur).Format(),
            std::get<1>(record->spur).Format(),
        },
        {
            "ENOB",
            record->enob.Format() + "  ", /* Padding for table */
            "HD2",
            std::get<0>(record->harmonics.at(0)).Format(),
            std::get<1>(record->harmonics.at(0)).Format(),
        },
        {
            "THD",
            record->thd.Format(),
            "HD3",
            std::get<0>(record->harmonics.at(1)).Format(),
            std::get<1>(record->harmonics.at(1)).Format(),
        },
        {
            "SFDR",
            record->sfdr_dbfs.Format(),
            "HD4",
            std::get<0>(record->harmonics.at(2)).Format(),
            std::get<1>(record->harmonics.at(2)).Format(),
        },
        {
            "NPSD",
            record->npsd.Format(),
            "HD5",
            std::get<0>(record->harmonics.at(3)).Format(),
            std::get<1>(record->harmonics.at(3)).Format(),
        },
        {
            "Size",
            record->size.Format(),
            "TIx",
            std::get<0>(record->gain_phase_spur).Format(),
            std::get<1>(record->gain_phase_spur).Format(),
        },
        {
            "RBW",
            record->rbw.Format(),
            "TIo",
            std::get<0>(record->offset_spur).Format(),
            std::get<1>(record->offset_spur).Format(),
        },
    };
}

void Ui::CopyFreqencyDomainMetricsToClipboard(const ProcessedRecord *processed_record)
{
    const auto &record = processed_record->frequency_domain;
    ImGui::LogToClipboard();
    ImGui::LogText("%s\n", processed_record->label.c_str());
    ImGui::LogText("SNR,%s\n", record->snr.FormatCsv().c_str());
    ImGui::LogText("SINAD,%s\n", record->sinad.FormatCsv().c_str());
    ImGui::LogText("ENOB,%s\n", record->enob.FormatCsv().c_str());
    ImGui::LogText("THD,%s\n", record->thd.FormatCsv().c_str());
    ImGui::LogText("SFDR,%s\n", record->sfdr_dbfs.FormatCsv().c_str());
    ImGui::LogText("NPSD,%s\n", record->npsd.FormatCsv().c_str());
    ImGui::LogText("Fund.,%s,%s\n", std::get<0>(record->fundamental).FormatCsv().c_str(),
                                    std::get<1>(record->fundamental).FormatCsv().c_str());
    ImGui::LogText("HD2,%s,%s\n", std::get<0>(record->harmonics.at(0)).FormatCsv().c_str(),
                                  std::get<1>(record->harmonics.at(0)).FormatCsv().c_str());
    ImGui::LogText("HD3,%s,%s\n", std::get<0>(record->harmonics.at(1)).FormatCsv().c_str(),
                                  std::get<1>(record->harmonics.at(1)).FormatCsv().c_str());
    ImGui::LogText("HD4,%s,%s\n", std::get<0>(record->harmonics.at(2)).FormatCsv().c_str(),
                                  std::get<1>(record->harmonics.at(2)).FormatCsv().c_str());
    ImGui::LogText("HD5,%s,%s\n", std::get<0>(record->harmonics.at(3)).FormatCsv().c_str(),
                                  std::get<1>(record->harmonics.at(3)).FormatCsv().c_str());
    ImGui::LogText("TIx,%s,%s\n", std::get<0>(record->gain_phase_spur).FormatCsv().c_str(),
                                  std::get<1>(record->gain_phase_spur).FormatCsv().c_str());
    ImGui::LogText("TIo,%s,%s\n", std::get<0>(record->offset_spur).FormatCsv().c_str(),
                                  std::get<1>(record->offset_spur).FormatCsv().c_str());
    ImGui::LogFinish();
}

void Ui::RenderTimeDomainMetrics(const ImVec2 &position, const ImVec2 &size)
{
    /* FIXME: Move into functions? */
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Time Domain Metrics", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    m_collapsed.time_domain_metrics = ImGui::IsWindowCollapsed();

    bool has_contents = false;
    for (auto &digitizer : m_digitizers)
    {
        if (!digitizer.ui.is_selected)
            continue;

        for (auto &ui : digitizer.ui.channels)
        {
            if (ui.record == NULL)
                continue;

            if (has_contents)
                ImGui::Separator();
            has_contents = true;

            RenderHeaderButtons(ui);

            int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (ui.is_selected)
                flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);
            bool node_open = ImGui::TreeNodeEx(ui.record->label.c_str(), flags);

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                ClearChannelSelection();
                ui.is_selected = !ui.is_selected;
            }

            if (ImGui::BeginPopupContextItem())
            {
                ImGui::MenuItem("Solo", "", &ui.is_solo);
                ImGui::MenuItem("Mute", "", &ui.is_muted);
                ImGui::MenuItem("Sample markers", "", &ui.is_sample_markers_enabled);

                if (ImGui::MenuItem("Add to memory"))
                    ui.memory.push_back(ui.record);

                ImGui::MenuItem("Persistence", "", &ui.is_persistence_enabled);

                if (ImGui::MenuItem("Save to file..."))
                {
                    /* Open the file browsing dialog with a prefilled filename for fast saving. */
                    ui.should_save_to_file = true;
                    m_file_browser.SetTitle(
                        fmt::format("Save {} record to file...", digitizer.ui.identifier));
                    m_file_browser.SetTypeFilters({".json"});
                    m_file_browser.Open();
                    m_file_browser.SetInputName(ui.GetDefaultFilename());
                }
                ImGui::EndPopup();
            }

            if (ui.record->time_domain->header.record_status & ADQ_RECORD_STATUS_OVERRANGE)
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, COLOR_RED);
                ImGui::SmallButton("OVERRANGE");
                ImGui::PopStyleColor();
            }

            if (node_open)
            {
                if ((ui.is_muted || IsAnySolo()) && !ui.is_solo)
                {
                    auto text_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    text_color.w *= 0.5f;
                    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
                }

                flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings |
                        ImGuiTableFlags_BordersInnerV;

                /* Increase the horizontal cell padding. */
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                                    ImGui::GetStyle().CellPadding + ImVec2(3.0, 0.0));

                if (ImGui::BeginTable("Metrics", 4, flags))
                {
                    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Extra0", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Extra1", ImGuiTableColumnFlags_WidthFixed);

                    for (const auto &row : FormatTimeDomainMetrics(ui.record.get()))
                    {
                        ImGui::TableNextRow();
                        for (const auto &column : row)
                        {
                            ImGui::TableNextColumn();
                            ImGui::Text(column);
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::PopStyleVar();

                if ((ui.is_muted || IsAnySolo()) && !ui.is_solo)
                    ImGui::PopStyleColor();

                ImGui::TreePop();
            }
            ImGui::PopStyleVar();
        }
    }
    ImGui::End();
}

void Ui::RenderFrequencyDomainMetrics(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Frequency Domain Metrics", NULL,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavFocus);
    m_collapsed.frequency_domain_metrics = ImGui::IsWindowCollapsed();

    bool has_contents = false;
    for (auto &digitizer : m_digitizers)
    {
        if (!digitizer.ui.is_selected)
            continue;

        for (auto &ui : digitizer.ui.channels)
        {
            if (ui.record == NULL)
                continue;

            if (has_contents)
                ImGui::Separator();
            has_contents = true;

            RenderHeaderButtons(ui);

            int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (ui.is_selected)
                flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);
            bool node_open = ImGui::TreeNodeEx(ui.record->label.c_str(), flags);

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                ClearChannelSelection();
                ui.is_selected = !ui.is_selected;
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ui.is_harmonics_annotated ^ ui.is_interleaving_spurs_annotated)
                {
                    if (ImGui::MenuItem("Annotations off"))
                    {
                        ui.is_harmonics_annotated = false;
                        ui.is_interleaving_spurs_annotated = false;
                    }
                }
                else
                {
                    bool toggle_on = !ui.is_harmonics_annotated;
                    if (ImGui::MenuItem(fmt::format("Annotations {}", toggle_on ? "on" : "off").c_str()))
                    {
                        ui.is_harmonics_annotated = toggle_on;
                        ui.is_interleaving_spurs_annotated = toggle_on;
                    }
                }

                ImGui::MenuItem("Annotate harmonics", "", &ui.is_harmonics_annotated);
                ImGui::MenuItem("Annotate interleaving spurs", "",
                                &ui.is_interleaving_spurs_annotated);

                if (ImGui::MenuItem("Copy metrics"))
                    CopyFreqencyDomainMetricsToClipboard(ui.record.get());

                ImGui::EndPopup();
            }

            if (node_open)
            {
                if (ui.record->frequency_domain->overlap)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, COLOR_ORANGE);
                    ImGui::SameLine();
                    ImGui::Text("OVERLAP");
                }

                if ((ui.is_muted || IsAnySolo()) && !ui.is_solo)
                {
                    auto text_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    text_color.w *= 0.5f;
                    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
                }

                /* Increase the horizontal cell padding. */
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                                    ImGui::GetStyle().CellPadding + ImVec2(3.0, 0.0));

                flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                        ImGuiTableFlags_NoSavedSettings;

                if (ImGui::BeginTable("Metrics", 5, flags))
                {
                    ImGui::TableSetupColumn("Metric0", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value0", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Metric1", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value1", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value2", ImGuiTableColumnFlags_WidthFixed);

                    for (const auto &row : FormatFrequencyDomainMetrics(ui.record.get()))
                    {
                        ImGui::TableNextRow();
                        for (const auto &column : row)
                        {
                            ImGui::TableNextColumn();
                            ImGui::Text(column);
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::PopStyleVar();

                if ((ui.is_muted || IsAnySolo()) && !ui.is_solo)
                    ImGui::PopStyleColor();
                if (ui.record->frequency_domain->overlap)
                    ImGui::PopStyleColor();

                ImGui::TreePop();
            }
            ImGui::PopStyleVar();
        }
    }
    ImGui::End();
}

void Ui::RenderApplicationMetrics(const ImVec2 &position, const ImVec2 &size)
{
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Application Metrics", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    m_collapsed.application_metrics = ImGui::IsWindowCollapsed();
    const ImGuiIO &io = ImGui::GetIO();
    ImGui::Text("Average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();
}

std::string Ui::NowAsIso8601()
{
    time_t now;
    time(&now);
    std::string now_as_iso8601(32, '\0');
    size_t result = std::strftime(now_as_iso8601.data(), now_as_iso8601.size(), "%FT%H%M%S",
                                  std::localtime(&now));
    if (result > 0)
    {
        /* Remember to strip the null terminator. */
        now_as_iso8601.resize(result);
        return now_as_iso8601;
    }
    else
    {
        /* FIXME: Communicate in some other way? */
        printf("Failed to generate an ISO8601 filename.\n");
        return "";
    }
}
