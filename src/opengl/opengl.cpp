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

static constexpr int NOF_POINTS = 1000000;
// static std::vector<float> sinewave{};
static std::vector<std::pair<float, float>> sinewave{};

void Learning::Initialize()
{
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

    sinewave.reserve(2 * NOF_POINTS);

    for (int i = 0; i < NOF_POINTS; ++i)
    {
        float x = 2.0f * static_cast<float>(i) / static_cast<float>(NOF_POINTS) - 1.0f;
        float y = std::sin(2.0 * M_PI * x);
        sinewave.emplace_back(x, y);
    }

    m_vertex_buffer.SetData(sinewave.data(), sinewave.size() * sizeof(sinewave[0]));
}

void Learning::Render()
{
    m_shader.Use();
    m_shader.Set("in_color", std::array{0.2f, 0.6f, 1.0f});
    m_vertex_buffer.SetData(sinewave.data(), sinewave.size() * sizeof(sinewave[0]));

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
