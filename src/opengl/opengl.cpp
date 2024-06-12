#include "opengl/learning.h"
#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <cstddef>
#include <cstdio>
#include <vector>
#include <cmath>

void Learning::Initialize()
{
    static const std::vector<float> vertices = {
        //  0.5f,  0.5f, 0.0f, /* top right */
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, /* bottom right */
        -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, /* bottom left */
        // -0.5f,  0.5f, 0.0f, /* top left */
         0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f,
    };

    static const std::vector<unsigned int> indices = {
        0, 1, 2,
        // 0, 1, 3, /* first triangle */
        // 1, 2, 3, /* second triangle */
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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Learning::Render()
{
    float offset_x = std::cos(glfwGetTime()) / 2.0f;
    float offset_y = std::sin(glfwGetTime()) / 2.0f;
    m_shader.Use();
    m_shader.Set("offset_x", offset_x);
    m_shader.Set("offset_y", offset_y);
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    // glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Learning::Terminate()
{
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_EBO);
}
