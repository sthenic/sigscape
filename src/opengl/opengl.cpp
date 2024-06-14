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

void Learning::Initialize()
{
    static const std::vector<float> vertices = {
         0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, /* top right */
         0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, /* bottom right */
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, /* bottom left */
        -0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, /* top left */
    };

    static const std::vector<unsigned int> indices = {
        0, 1, 3, /* first triangle */
        1, 2, 3, /* second triangle */
    };

    m_shader = Shader(
        "/home/marcus/Documents/git/sigscape/src/opengl/shader.vs",
        "/home/marcus/Documents/git/sigscape/src/opengl/shader.fs");

    /* Vertex array object */
    glGenVertexArrays(1, &m_VAO);
    glBindVertexArray(m_VAO);

    /* Create a vertex buffer object. */
    glGenBuffers(1, &m_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);

    glBufferData(
        GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);

    /* Create a element buffer object */
    glGenBuffers(1, &m_EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
    m_shader.Use();
    m_shader.Set("texture0", 0);
    m_shader.Set("texture1", 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Learning::Render()
{
    float time = glfwGetTime();
    m_shader.Use();
    m_shader.Set("mix_value", 0.2f);
    m_shader.Set("mix_color", std::array{1.0f, 0.5f, 0.2f});
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_texture1);
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Learning::Terminate()
{
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_EBO);
    glDeleteTextures(1, &m_texture0);
    glDeleteTextures(1, &m_texture1);
}
