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

static const std::vector<float> vertices = {
    0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 1.5f, /* top right */
    0.5f,  0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.5f, 0.0f, /* bottom right */
    -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -0.5f, 0.0f, /* bottom left */
    // -0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, /* top left */
};

void Learning::Initialize()
{
    m_shader = Shader(
        "/home/marcus/Documents/git/sigscape/src/opengl/shader.vs",
        "/home/marcus/Documents/git/sigscape/src/opengl/shader.fs");

    m_vertex_buffer = VertexBuffer({
        {VertexBuffer::Attribute::Type::FLOAT, 3},
        {VertexBuffer::Attribute::Type::FLOAT, 3},
        {VertexBuffer::Attribute::Type::FLOAT, 2},
    });

    m_vertex_buffer.SetData(vertices.data(), vertices.size() * sizeof(vertices[0]));

    /* Load texture */
    int width;
    int height;
    int nof_channels;
    stbi_set_flip_vertically_on_load(true);
    uint8_t *data = stbi_load("/home/marcus/Documents/git/sigscape/awesomeface.png", &width, &height, &nof_channels, 0);
    if (data == NULL)
    {
        printf("Failed loading texture 'awesomeface.png'.\n");
        return;
    }

    m_texture = Texture(width, height, data);
    m_frame_buffer.SetTexture(m_texture.GetId());

    m_shader.Use();
    m_shader.Set("our_texture", 0);
}

void Learning::Render()
{
    float time = glfwGetTime();
    auto v = vertices;
    v[0] = std::sin(4.0 * time);
    v[1] = std::cos(4.0 * time);
    m_vertex_buffer.SetData(v.data(), v.size() * sizeof(v[0]));

    m_shader.Use();
    m_shader.Set("mix_value", 0.2f);
    m_shader.Set("mix_color", std::array{1.0f, 0.5f, 0.2f});

    m_frame_buffer.Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture.GetId());
    m_vertex_buffer.Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    m_vertex_buffer.Unbind();
    m_frame_buffer.Unbind();
}

void Learning::Terminate()
{
}

GLuint Learning::GetId() const
{
    return m_texture.GetId();
}
