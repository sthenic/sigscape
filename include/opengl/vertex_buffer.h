#pragma once

#include "GL/gl3w.h"
#include <cstddef>
#include <vector>

/* A wrapper class for a vertex buffer object and an associated vertex array
   object to gather attributes. */

/* TODO: Support element array buffer? */
/* FIXME: Probably need multiple (separate) vertex buffer objects? */

class VertexBuffer
{
public:
    class Attribute
    {
    public:
        enum class Type
        {
            FLOAT,
        };

        Attribute(Type type, size_t nof_values, bool is_normalized = false);

        size_t GetNofValues() const;
        size_t GetSize() const;
        GLenum GetType() const;
        bool IsNormalized() const;

    private:
        Type m_type;
        size_t m_nof_values;
        bool m_is_normalized;
    };

    VertexBuffer() = default;
    explicit VertexBuffer(const std::vector<Attribute> &attributes);
    ~VertexBuffer();

    /* Only allow moving. */
    VertexBuffer(const VertexBuffer &other) = delete;
    VertexBuffer &operator=(const VertexBuffer &other) = delete;
    VertexBuffer(VertexBuffer &&other);
    VertexBuffer &operator=(VertexBuffer &&other);

    void Bind() const;
    void Unbind() const;
    void SetData(const void *data, size_t len) const;

private:
    GLuint m_vertex_buffer{0};
    GLuint m_vertex_array{0};
};
