#pragma once

#include "opengl/shader.h"
#include "opengl/vertex_buffer.h"
#include "opengl/texture.h"
#include "opengl/frame_buffer.h"

class Learning
{
public:
    Learning() = default;
    ~Learning() = default;

    void Initialize();
    void Render();
    void Terminate();
    void SetViewport(int x, int y, int width, int height);
    void SetLimits(float xmin, float xmax, float ymin, float ymax);

private:
    static constexpr size_t NOF_WAVES = 10;
    static constexpr int NOF_POINTS = 1000000;

    struct Viewport
    {
        int x;
        int y;
        int width;
        int height;
    } m_viewport{};
    Shader m_shader;
    VertexBuffer m_vertex_buffer;
    std::vector<std::vector<std::pair<float, float>>> m_waves;
    size_t m_current_wave = 0;
};
