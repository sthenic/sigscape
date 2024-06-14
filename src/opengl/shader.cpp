#include "opengl/shader.h"
#include "error.h"

#include "fmt/format.h"
#include <fstream>
#include <type_traits>

Shader::Shader(Shader &&other)
{
    /* We simply claim the shader id so it doesn't get deleted twice. */
    m_id = other.m_id;
    other.m_id = 0;
}

Shader &Shader::operator=(Shader &&other)
{
    m_id = other.m_id;
    other.m_id = 0;
    return *this;
}

Shader::Shader(const std::filesystem::path &vertex_path, const std::filesystem::path &fragment_path)
{
    /* Throws on error. The destructors cleans up any allocated resources. */
    const _Shader vertex_shader{vertex_path, GL_VERTEX_SHADER};
    const _Shader fragment_shader{fragment_path, GL_FRAGMENT_SHADER};

    /* Create the shader program. */
    m_id = glCreateProgram();
    glAttachShader(m_id, vertex_shader.GetId());
    glAttachShader(m_id, fragment_shader.GetId());

    /* Link the program then check for any errors. */
    glLinkProgram(m_id);

    GLint success{};
    glGetProgramiv(m_id, GL_LINK_STATUS, &success);

    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(m_id, sizeof(log), NULL, log);
        throw ShaderException(fmt::format(
            "Linking of shaders '{}' and '{}' failed: {}.", vertex_path.string(),
            fragment_path.string(), log));
    }
}

Shader::~Shader()
{
    if (m_id > 0)
        glDeleteProgram(m_id);
}

GLuint Shader::GetId() const
{
    return m_id;
}

void Shader::Use() const
{
    glUseProgram(m_id);
}

int Shader::Set(const std::string &name, Uniform value)
{
    try
    {
        /* Dispatch to the appropriate OpenGL function depending on the type. */
        const auto location = GetUniformLocation(name);
        std::visit([&](auto &&arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int>)
            {
                glUniform1i(location, static_cast<GLint>(arg));
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                glUniform1f(location, arg);
            }
            else if constexpr (std::is_same_v<T, std::array<float, 2>>)
            {
                printf("I'm here\n");
                glUniform2f(location, arg[0], arg[1]);
            }
            else if constexpr (std::is_same_v<T, std::array<float, 3>>)
            {
                glUniform3f(location, arg[0], arg[1], arg[2]);
            }
            else if constexpr (std::is_same_v<T, std::array<float, 4>>)
            {
                glUniform4f(location, arg[0], arg[1], arg[2], arg[3]);
            }
        }, value);

        return SCAPE_EOK;
    }
    catch (const ShaderException &e)
    {
        return SCAPE_EINTERNAL;
    }
}

GLuint Shader::GetUniformLocation(const std::string &name)
{
    if (const auto match = m_uniform_cache.find(name); match != m_uniform_cache.end())
    {
        return match->second;
    }
    else
    {
        const auto location = glGetUniformLocation(m_id, name.c_str());
        if (location < 0)
            throw ShaderException(fmt::format("Failed to find uniform '{}'.", name));

        m_uniform_cache[name] = location;
        return location;
    }
}

Shader::_Shader::_Shader(const std::filesystem::path &path, GLenum type)
{
    /* Throws on error. */
    const std::string source{std::move(ReadShaderSource(path))};

    /* Create a new shader of the target type. */
    switch (type)
    {
    case GL_VERTEX_SHADER:
    case GL_FRAGMENT_SHADER:
        m_id = glCreateShader(type);
        break;

    default:
        throw ShaderException(fmt::format("Unknown shader type '{}'.", type));
    }

    /* Compile the shader then check for any errors. */
    auto code = source.c_str();
    glShaderSource(m_id, 1, &code, NULL);
    glCompileShader(m_id);

    GLint success{};
    glGetShaderiv(m_id, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(m_id, sizeof(log), NULL, log);
        throw ShaderException(
            fmt::format("Compilation of shader '{}' failed: {}.", path.string(), log));
    }
}

Shader::_Shader::~_Shader()
{
    if (m_id > 0)
        glDeleteShader(m_id);
}

GLuint Shader::_Shader::GetId() const
{
    return m_id;
}

std::string Shader::_Shader::ReadShaderSource(const std::filesystem::path &path)
{
    try
    {
        /* FIXME: Ensure throwable? */
        std::ifstream ifs{path, std::ios::in};
        const auto size = std::filesystem::file_size(path);
        std::string result(size, '\0');
        ifs.read(result.data(), size);
        return result;
    }
    catch (const std::exception &e)
    {
        throw ShaderException(
            fmt::format("Failed to read shader file '{}': {}.", path.string(), e.what()));
    }
}
