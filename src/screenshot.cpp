#include "screenshot.h"

#include "log.h"
#include "png.h"

#include <vector>
#include <stdexcept>

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

bool Screenshot::Screenshot(GLFWwindow *window, const std::string &filename)
{
    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);

    std::vector<uint8_t> pixels(width * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    return SaveAsPng(filename, pixels.data(), width, height);
}
