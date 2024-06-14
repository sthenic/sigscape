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
    uint8_t *data = stbi_load("/home/marcus/Documents/git/sigscape/container.jpg", &width, &height, &nof_channels, 0);
    if (data == NULL)
    {
        printf("Failed loading texture 'container.jpg'.\n");
        return;
    }

    /* Texture object 0 */
    glGenTextures(1, &m_texture0);
    glBindTexture(GL_TEXTURE_2D, m_texture0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    data = stbi_load("/home/marcus/Documents/git/sigscape/awesomeface.png", &width, &height, &nof_channels, 0);
    if (data == NULL)
    {
        printf("Failed loading texture 'awesomeface.png'.\n");
        return;
    }

    glGenTextures(1, &m_texture1);
    glBindTexture(GL_TEXTURE_2D, m_texture1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    m_shader.Use();
    m_shader.Set("texture0", 0);
    m_shader.Set("texture1", 1);
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
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_texture1);
    m_vertex_buffer.Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    m_vertex_buffer.Unbind();
}

void Learning::Terminate()
{
    glDeleteTextures(1, &m_texture0);
    glDeleteTextures(1, &m_texture1);
}
