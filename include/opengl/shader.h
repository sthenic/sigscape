#pragma once

#include "GL/gl3w.h"

#include <filesystem>
#include <string>
#include <stdexcept>
#include <vector>
#include <map>
#include <array>
#include <variant>

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
    Shader(
        const std::filesystem::path &vertex_path, const std::filesystem::path &fragment_path,
        const std::filesystem::path &geometry_path = "");

    ~Shader();

    /* Only allow moving. */
    Shader(const Shader &other) = delete;
    Shader &operator=(const Shader &other) = delete;
    Shader(Shader &&other);
    Shader &operator=(Shader &&other);

    [[nodiscard]] GLuint GetId() const;
    void Use() const;

    /* Type variant to implement a `Set` function for a few explicit types we
       can pass as uniform values. */
    using Uniform = std::variant<
        bool, int, float, std::array<float, 2>, std::array<float, 3>, std::array<float, 4>>;
    int Set(const std::string &name, Uniform value);

private:
    GLuint m_id{0};
    std::map<std::string, GLuint> m_uniform_cache{};

    GLuint GetUniformLocation(const std::string &name);

    /* Wrapper around a vertex or fragment shader read from file to leverage RAII. */
    class _Shader
    {
    public:
        _Shader() = default;
        _Shader(const std::filesystem::path &path, GLenum type);
        ~_Shader();

        _Shader(const _Shader &other) = delete;
        _Shader &operator=(const _Shader &other) = delete;
        _Shader(_Shader &&other);
        _Shader &operator=(_Shader &&other);

        GLuint GetId() const;

    private:
        GLuint m_id{0};
        static std::string ReadShaderSource(const std::filesystem::path &path);
    };
};

