#include "opengl/learning.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <cstddef>
#include <cstdio>
#include <vector>
#include <utility>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstdlib>  // for rand()
#include <ctime>    // for time()

void Learning::Initialize()
{
    /* Seed random number generator for noise */
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    /* FIXME: If we want line thickness we'll have to figure out mitering. */
    // m_shader = Shader(
    //     "/home/marcus/Documents/git/sigscape/src/opengl/shader.vs",
    //     "/home/marcus/Documents/git/sigscape/src/opengl/shader.fs",
    //     "/home/marcus/Documents/git/sigscape/src/opengl/shader.gs");

    m_shader = Shader(
        "/home/marcus/Documents/git/sigscape/src/opengl/shader.vs",
        "/home/marcus/Documents/git/sigscape/src/opengl/shader.fs");

    m_vertex_buffer = VertexBuffer({
        {VertexBuffer::Attribute::Type::FLOAT, 2},
    });

    /* Preallocate and generate NOF_WAVES sine waves with random noise */
    m_waves.resize(NOF_WAVES);
    for (size_t w = 0; w < NOF_WAVES; ++w)
    {
        auto &wave = m_waves[w];
        wave.reserve(NOF_POINTS);

        for (int i = 0; i < NOF_POINTS; ++i)
        {
            float x = 2.0f * static_cast<float>(i) / static_cast<float>(NOF_POINTS) - 1.0f;
            float y = std::sin(2.0f * M_PI * x);

            /* Add random noise in range [-0.1, 0.1] */
            float noise = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 0.1f;
            y += noise;

            wave.emplace_back(x, y);
        }
    }

    /* Initialize with the first wave */
    m_current_wave = 0;
    m_vertex_buffer.SetData(m_waves[m_current_wave].data(), m_waves[m_current_wave].size() * sizeof(m_waves[m_current_wave][0]));
}

void Learning::Render()
{
    m_shader.Use();
    m_shader.Set("in_color", std::array{0.2f, 0.6f, 1.0f});

    /* Cycle to next wave and upload its data */
    const auto &current_wave = m_waves[m_current_wave];
    m_vertex_buffer.SetData(current_wave.data(), current_wave.size() * sizeof(current_wave[0]));
    m_current_wave = (m_current_wave + 1) % NOF_WAVES;

    glViewport(m_viewport.x, m_viewport.y, m_viewport.width, m_viewport.height);
    m_vertex_buffer.Bind();
    /* FIXME:: Could we do super simple frustrum culling here? */
    glDrawArrays(GL_LINE_STRIP, 0, NOF_POINTS);
    m_vertex_buffer.Unbind();
}

void Learning::Terminate()
{
}

void Learning::SetViewport(int x, int y, int width, int height)
{
    m_viewport = {x, y, width, height};
}

void Learning::SetLimits(float xmin, float xmax, float ymin, float ymax)
{
    m_shader.Use();
    m_shader.Set("xmin", xmin);
    m_shader.Set("xmax", xmax);
    m_shader.Set("ymin", ymin);
    m_shader.Set("ymax", ymax);
}
