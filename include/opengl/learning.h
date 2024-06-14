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

    [[nodiscard]] GLuint GetId() const;

private:
    Shader m_shader;
    VertexBuffer m_vertex_buffer;
    Texture m_texture;
    FrameBuffer m_frame_buffer;
};
