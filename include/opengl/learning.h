#pragma once

#include "GL/gl3w.h"
#include "opengl/shader.h"
#include "opengl/vertex_buffer.h"

class Learning
{
public:
    Learning() = default;
    ~Learning() = default;

    void Initialize();
    void Render();
    void Terminate();

private:
    GLuint m_texture0{0};
    GLuint m_texture1{0};
    Shader m_shader;
    VertexBuffer m_vertex_buffer{{}};
};
