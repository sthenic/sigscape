#include "opengl/gl_line_renderer.h"
#include "data_types.h"

#include "GL/gl3w.h"

#include <vector>
#include <cstddef>
#include <limits>

void GlLineRenderer::Initialize()
{
    m_shader = Shader(
        "/home/marcus/Documents/git/sigscape/src/opengl/shader.vs",
        "/home/marcus/Documents/git/sigscape/src/opengl/shader.fs");

    m_vertex_buffer = VertexBuffer({
        {VertexBuffer::Attribute::Type::FLOAT, 2},
    });
}

void GlLineRenderer::SetData(const BaseRecord *record)
{
    /* TODO: There's an optimization to be made here if we use
             std::pair<double, double> to store the samples. */
    return SetData(record->x, record->y);
}

void GlLineRenderer::SetData(const std::vector<double> &x, const std::vector<double> &y)
{
    if (x.size() != y.size())
        return;

    const size_t count = x.size();
    m_interleaved_data.resize(count * 2);

    float x_min = std::numeric_limits<float>::max();
    float x_max = std::numeric_limits<float>::lowest();
    float y_min = std::numeric_limits<float>::max();
    float y_max = std::numeric_limits<float>::lowest();

    for (size_t i = 0; i < count; ++i)
    {
        const float xi = static_cast<float>(x[i]);
        const float yi = static_cast<float>(y[i]);
        m_interleaved_data[i * 2 + 0] = xi;
        m_interleaved_data[i * 2 + 1] = yi;

        x_min = std::min(x_min, xi);
        x_max = std::max(x_max, xi);
        y_min = std::min(y_min, yi);
        y_max = std::max(y_max, yi);
    }

    m_bounds_min = ImPlotPoint{x_min, y_min};
    m_bounds_max = ImPlotPoint{x_max, y_max};

    if (!m_interleaved_data.empty())
    {
        m_vertex_buffer.SetData(m_interleaved_data.data(),
                                m_interleaved_data.size() * sizeof(float));
    }
}

void GlLineRenderer::Render()
{
    if (m_interleaved_data.empty())
        return;

    m_shader.Use();
    m_shader.Set("in_color", std::array{m_color.x, m_color.y, m_color.z, m_color.w});

    glViewport(m_viewport.x, m_viewport.y, m_viewport.width, m_viewport.height);
    m_vertex_buffer.Bind();
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(m_interleaved_data.size() / 2));
    m_vertex_buffer.Unbind();
}

void GlLineRenderer::Terminate()
{
}

void GlLineRenderer::SetViewport(int x, int y, int width, int height)
{
    m_viewport = {x, y, width, height};
}

void GlLineRenderer::SetLimits(float xmin, float xmax, float ymin, float ymax)
{
    m_shader.Use();
    m_shader.Set("xmin", xmin);
    m_shader.Set("xmax", xmax);
    m_shader.Set("ymin", ymin);
    m_shader.Set("ymax", ymax);
}

void GlLineRenderer::SetColor(const ImVec4 &color)
{
    m_color = color;
}
