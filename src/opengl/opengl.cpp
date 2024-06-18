#include "opengl/learning.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <cstddef>
#include <cstdio>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

static constexpr int NOF_POINTS = 1000000;

void Learning::Initialize()
{
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

    std::vector<float> sinewave{};
    sinewave.reserve(2 * NOF_POINTS);

    for (int i = 0; i < NOF_POINTS; ++i)
    {
        float x = 2.0f * static_cast<float>(i) / static_cast<float>(NOF_POINTS) - 1.0f;
        float y = std::sin(2.0 * M_PI * x);
        sinewave.emplace_back(x);
        sinewave.emplace_back(y);
    }

    m_vertex_buffer.SetData(sinewave.data(), sinewave.size() * sizeof(sinewave[0]));

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

void Learning::Render()
{
    // float time = glfwGetTime();
    // auto v = vertices;
    // v[0] = std::sin(4.0 * time);
    // v[1] = std::cos(4.0 * time);
    // m_vertex_buffer.SetData(v.data(), v.size() * sizeof(v[0]));

    m_shader.Use();
    glViewport(m_viewport.x, m_viewport.y, m_viewport.width, m_viewport.height);
    m_vertex_buffer.Bind();
    glDrawArrays(GL_LINE_STRIP, 0, NOF_POINTS);
    // glDrawArrays(GL_POINTS, 0, NOF_POINTS);
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
