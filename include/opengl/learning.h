#pragma once

#include "GL/gl3w.h"
#include "opengl/shader.h"
#include "opengl/vertex_buffer.h"
#include "opengl/texture.h"

class Learning
{
public:
    Learning() = default;
    ~Learning() = default;

    void Initialize();
    void Render();
    void Terminate();

private:
    Shader m_shader;
    VertexBuffer m_vertex_buffer;
    Texture m_texture;
};
