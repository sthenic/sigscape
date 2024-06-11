#include "learning_opengl.h"
#include "GL/gl3w.h"

#include <cstddef>
#include <cstdio>
#include <vector>

void Learning::Initialize()
{
    static const std::vector<float> vertices = {
         0.5f,  0.5f, 0.0f, /* top right */
         0.5f, -0.5f, 0.0f, /* bottom right */
        -0.5f, -0.5f, 0.0f, /* bottom left */
        -0.5f,  0.5f, 0.0f, /* top left */
    };

    static const std::vector<unsigned int> indices = {
        0, 1, 3, /* first triangle */
        1, 2, 3, /* second triangle */
    };

    static const char *vertex_shader_source = R"(
#version 330 core
layout (location = 0) in vec3 pos;

void main()
{
    gl_Position = vec4(pos.x, pos.y, pos.z, 1.0);
}
)";
    static const char *fragment_shader_source = R"(
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
}
)";

    /* Create the vertex shader. */
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);

    int success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(vertex_shader, sizeof(log), NULL, log);
        printf("Vertex shader compilation failed: '%s'\n", log);
    }

    /* Create the fragment shader. */
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(fragment_shader, sizeof(log), NULL, log);
        printf("Fragment shader compilation failed: '%s'\n", log);
    }

    /* Create shader program */
    m_shader_program = glCreateProgram();

    glAttachShader(m_shader_program, vertex_shader);
    glAttachShader(m_shader_program, fragment_shader);
    glLinkProgram(m_shader_program);

    glGetProgramiv(m_shader_program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(m_shader_program, sizeof(log), NULL, log);
        printf("Shader program linking failed: '%s'\n", log);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

void Learning::Render()
{
    glUseProgram(m_shader_program);
    glBindVertexArray(m_VAO);
    // glDrawArrays(GL_TRIANGLES, 0, 3);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Learning::Terminate()
{
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_EBO);
    glDeleteProgram(m_shader_program);
}
