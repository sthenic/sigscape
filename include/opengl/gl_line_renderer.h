#pragma once

#include "opengl/shader.h"
#include "opengl/vertex_buffer.h"
#include "implot.h"

/* Forward declaration to avoid including data_types.h here */
struct BaseRecord;

class GlLineRenderer
{
public:
    GlLineRenderer() = default;
    ~GlLineRenderer() = default;

    void Initialize();
    void Render();
    void Terminate();
    void SetViewport(int x, int y, int width, int height);
    void SetLimits(float xmin, float xmax, float ymin, float ymax);
    void SetColor(const ImVec4 &color); /* FIXME: We could pass the color in SetData instead */

    /* Upload data from a BaseRecord (converts double to float, interleaved) */
    void SetData(const BaseRecord *record);

    /* Upload data from separate x and y vectors (converts double to float, interleaved) */
    void SetData(const std::vector<double> &x, const std::vector<double> &y);

    /* Get the data bounds for ImPlot callback */
    ImPlotPoint GetBoundsMin() const { return m_bounds_min; }
    ImPlotPoint GetBoundsMax() const { return m_bounds_max; }

private:
    struct Viewport
    {
        int x;
        int y;
        int width;
        int height;
    } m_viewport{};

    Shader m_shader;
    VertexBuffer m_vertex_buffer;
    std::vector<float> m_interleaved_data;  /* CPU-side cache as interleaved float for GPU */
    ImPlotPoint m_bounds_min{0, 0};
    ImPlotPoint m_bounds_max{0, 0};
    ImVec4 m_color{0.2f, 0.6f, 1.0f, 1.0f};  /* Default color, matches previous hardcoded value */
};
