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
    struct Viewport
    {
        int x;
        int y;
        int width;
        int height;
    } m_viewport{};
    Shader m_shader;
    VertexBuffer m_vertex_buffer;
};
