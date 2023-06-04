#include "ui.h"
#include "log.h"

#include "png.h"
#include "GL/gl3w.h"
#include <GLFW/glfw3.h>

#include <vector>
#include <string>
#include <csignal>

/* Global GLFW window to enable callback to save to file. */
/* TODO: Maybe not the best idea in the world. */
static GLFWwindow *window = NULL;
static bool should_exit = false;

static void signal_handler(int)
{
    should_exit = true;
}

static void GlfwErrorCallback(int error, const char *description)
{
    Log::log->error("Glfw error {}: {}.", error, description);
}

static bool SaveAsPng(const std::string &filename, uint8_t *pixels, int width, int height)
{
    png_structp png = NULL;
    png_infop info = NULL;
    png_colorp palette = NULL;
    png_bytepp rows = NULL;
    FILE *fp = NULL;

    try
    {
        png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (png == NULL)
            throw std::runtime_error("libpng: png_create_write_struct failed.");

        info = png_create_info_struct(png);
        if (info == NULL)
            throw std::runtime_error("libpng: png_create_info_struct failed.");

        fp = std::fopen(filename.c_str(), "wb");
        if (fp == NULL)
            throw std::runtime_error(fmt::format("Failed to open '{}' for writing.", filename));

        png_init_io(png, fp);
        png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

        palette = static_cast<png_colorp>(png_malloc(png, PNG_MAX_PALETTE_LENGTH * sizeof(png_color)));
        if (palette == NULL)
            throw std::runtime_error("libpng: failed to allocate memory for the palette.");

        png_set_PLTE(png, info, palette, PNG_MAX_PALETTE_LENGTH);
        png_write_info(png, info);
        png_set_packing(png);

        rows = static_cast<png_bytepp>(png_malloc(png, height * sizeof(png_bytep)));
        if (rows == NULL)
            throw std::runtime_error("libpng: failed to allocate memory for image data.");

        for (int i = 0; i < height; ++i)
            rows[i] = static_cast<png_bytep>(pixels + (height - i - 1) * width * 3);

        png_write_image(png, rows);
        png_write_end(png, info);

        png_free(png, rows);
        png_free(png, palette);
        png_destroy_write_struct(&png, &info); /* Also destroys `info`. */
        std::fclose(fp);
        return true;
    }
    catch (const std::runtime_error &e)
    {
        Log::log->error(e.what());

        if (rows != NULL)
            png_free(png, rows);
        if (palette != NULL)
            png_free(png, palette);
        if (png != NULL)
            png_destroy_write_struct(&png, &info);
        if (fp != NULL)
            std::fclose(fp);

        return false;
    }
}

static bool Screenshot(const std::string &filename)
{
    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);

    std::vector<uint8_t> pixels(width * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    return SaveAsPng(filename, pixels.data(), width, height);
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

    window = glfwCreateWindow(1920, 1080, "sigscape", NULL, NULL);
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
    ui.Initialize(window, glsl_version, Screenshot);

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
