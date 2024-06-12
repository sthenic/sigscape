#pragma once

#include "GL/gl3w.h"

#include <filesystem>
#include <string>
#include <stdexcept>
#include <vector>
#include <map>

class ShaderException : public std::runtime_error
{
public:
    ShaderException(const std::string &str)
        : std::runtime_error(str)
    {}
};

class Shader
{
public:
    /* Allow default construction of an uninitialized shader, or a shader where
       both the vertex shader and the fragment shader are specified. */
    Shader() = default;
    Shader(const std::filesystem::path &vertex_path, const std::filesystem::path &fragment_path);

    ~Shader();

    /* Only allow moving. */
    Shader(const Shader &other) = delete;
    Shader &operator=(const Shader &other) = delete;
    Shader(Shader &&other);
    Shader &operator=(Shader &&other);

    GLuint GetId() const;
    void Use();
    int Set(const std::string &name, bool value);
    int Set(const std::string &name, int value);
    int Set(const std::string &name, float value);
    int Set(const std::string &name, const std::vector<float> &value);

private:
    GLuint m_id{0};

    /* Wrapper around a vertex or fragment shader read from file to leverage RAII. */
    class _Shader
    {
    public:
        _Shader(const std::filesystem::path &path, GLenum type);
        ~_Shader();

        GLuint GetId() const;

    private:
        GLuint m_id{0};
        static std::string ReadShaderSource(const std::filesystem::path &path);
    };
};

