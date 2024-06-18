#include "ui.h"
#include "log.h"
#include "persistent_directories.h"

#include "GL/gl3w.h"
#include <GLFW/glfw3.h>
#include "fmt/format.h"

#if defined(_WIN32)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "sigscape16.h"
#include "sigscape32.h"
#include "sigscape48.h"
#endif

#include <csignal>

static bool should_exit = false;
static void signal_handler(int)
{
    should_exit = true;
}

static void GlfwErrorCallback(int error, const char *description)
{
    Log::log->error("Glfw error {}: {}.", error, description);
}

#if defined(_WIN32)
struct ManagedGlfwImage : public GLFWimage
{
    ManagedGlfwImage(const std::string &path)
    {
        pixels = stbi_load(path.c_str(), &width, &height, NULL, 4);
        if (pixels == NULL)
            throw std::runtime_error(fmt::format("Failed to load image '{}'.", path));
    }

    ManagedGlfwImage(const uint8_t *buffer, size_t len)
    {
        pixels = stbi_load_from_memory(buffer, static_cast<int>(len), &width, &height, NULL, 4);
        if (pixels == NULL)
            throw std::runtime_error("Failed to load image from memory.");
    }

    ~ManagedGlfwImage()
    {
        if (pixels != NULL)
            stbi_image_free(pixels);
    }

    ManagedGlfwImage(const ManagedGlfwImage &other) = delete;
    ManagedGlfwImage &operator=(const ManagedGlfwImage &other) = delete;
    ManagedGlfwImage &operator=(ManagedGlfwImage &&other) = delete;

    ManagedGlfwImage(ManagedGlfwImage &&other)
    {
        pixels = other.pixels;
        width = other.width;
        height = other.height;
        other.pixels = NULL;
        other.width = 0;
        other.height = 0;
    };
};

static void SetWindowIcon(GLFWwindow *window)
{
    try
    {
        /* On Windows, the image data is embedded into the application. We load
           the images and provide them to the function responsible for the
           window icon. Recommended sizes are 16x16, 32x32 and 48x48. */
        std::vector<ManagedGlfwImage> images{};
        images.emplace_back(sigscape16_png, sizeof(sigscape16_png));
        images.emplace_back(sigscape32_png, sizeof(sigscape32_png));
        images.emplace_back(sigscape48_png, sizeof(sigscape48_png));
        glfwSetWindowIcon(window, static_cast<int>(images.size()), images.data());
        Log::log->trace("Initialized window icon.");
    }
    catch (const std::runtime_error &e)
    {
        Log::log->error(e.what());
    }
}
#endif

int main(int, char **)
{
    signal(SIGINT, signal_handler);

    /* Set up the logger's file sink once we can determine the persistent
       directories. Pushing back another sink is not thread safe, but at this
       point were running in a single thread. */
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        (PersistentDirectories().GetLogDirectory() / "sigscape.log").string(), true);
    Log::log->sinks().push_back(file_sink);
    Log::log->flush_on(spdlog::level::info);

    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
        return -1;

#if defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow *window = glfwCreateWindow(1920, 1080, "sigscape", NULL, NULL);
    if (window == NULL)
        return -1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); /* Enable VSYNC */

#if defined (_WIN32)
    SetWindowIcon(window);
#endif

    if (gl3wInit())
    {
        Log::log->error("Failed to initialize the OpenGL loader.");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    /* FIXME: https://github.com/ocornut/imgui/issues/5485 */
    /* FIXME: https://github.com/ocornut/imgui/issues/6892 */
    /* FIXME: https://github.com/ocornut/imgui/issues/4214 */
    /* FIXME: https://github.com/ocornut/imgui/issues/984 */
    /* FIXME: https://gamedev.stackexchange.com/questions/150214/render-in-a-imgui-window/207560 */

    glEnable(GL_MULTISAMPLE);

    Ui ui;
    ui.Initialize(window, glsl_version);
    while (!glfwWindowShouldClose(window) && !should_exit)
    {
        int display_width;
        int display_height;
        glfwPollEvents();
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwGetFramebufferSize(window, &display_width, &display_height);
        glViewport(0, 0, display_width, display_height);
        ui.Render(static_cast<float>(display_width), static_cast<float>(display_height));
        glfwSwapBuffers(window);
    }

    ui.Terminate();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
