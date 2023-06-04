#include "ui.h"
#include "log.h"

#include "GL/gl3w.h"
#include <GLFW/glfw3.h>

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

int main(int, char **)
{
    signal(SIGINT, signal_handler);

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

    GLFWwindow *window = glfwCreateWindow(1920, 1080, "sigscape", NULL, NULL);
    if (window == NULL)
        return -1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); /* Enable VSYNC */

    if (gl3wInit())
    {
        Log::log->error("Failed to initialize the OpenGL loader.");
        return -1;
    }

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
