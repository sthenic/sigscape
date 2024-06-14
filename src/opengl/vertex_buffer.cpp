#include "opengl/vertex_buffer.h"

#include <cstring>
#include <cstdio>
#include <algorithm>

VertexBuffer::Attribute::Attribute(Type type, size_t nof_values, bool is_normalized)
    : m_type{type}
    , m_nof_values{nof_values}
    , m_is_normalized{is_normalized}
{
}

size_t VertexBuffer::Attribute::GetNofValues() const
{
    return m_nof_values;
}

size_t VertexBuffer::Attribute::GetSize() const
{
    size_t size = 0;
    switch (m_type)
    {
    case Type::FLOAT:
        size = sizeof(float);
        break;

    default:
        break;
    }

    return size * m_nof_values;
}

GLenum VertexBuffer::Attribute::GetType() const
{
    switch (m_type)
    {
    case Type::FLOAT:
        return GL_FLOAT;

    default:
        return 0;
    }
}

bool VertexBuffer::Attribute::IsNormalized() const
{
    return m_is_normalized;
}

VertexBuffer::VertexBuffer(const std::vector<Attribute> &attributes)
{
    /* Create and bind a vertex array object. */
    glGenVertexArrays(1, &m_vertex_array);
    glBindVertexArray(m_vertex_array);

    /* Create and bind a vertex buffer object. */
    glGenBuffers(1, &m_vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);

    /* Create data store for the vertex buffer. Allocate one byte for now but
       make sure to set `GL_DYNAMIC_DRAW` to indicate that the contents will
       change frequently and used many times. The store will be resized when
       it's filled with data. */
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);

    /* Specify the properties of the attribute pointer. */
    size_t total_size = 0;
    std::for_each(attributes.cbegin(), attributes.cend(), [&](const auto &a) {
        total_size += a.GetSize();
    });

    GLuint index = 0;
    size_t offset = 0;
    for (const auto &a : attributes)
    {
        const auto nof_values = static_cast<GLint>(a.GetNofValues());
        const auto is_normalized = a.IsNormalized() ? GL_TRUE : GL_FALSE;

        glVertexAttribPointer(
            index, nof_values, a.GetType(), is_normalized, total_size,
            reinterpret_cast<const void *>(offset));

        glEnableVertexAttribArray(index);

        offset += a.GetSize();
        index++;
    }

    /* Unbind buffers. */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

VertexBuffer::~VertexBuffer()
{
    if (m_vertex_buffer > 0)
        glDeleteBuffers(1, &m_vertex_buffer);
    if (m_vertex_array > 0)
        glDeleteBuffers(1, &m_vertex_array);
}

VertexBuffer::VertexBuffer(VertexBuffer &&other)
{
    m_vertex_buffer = other.m_vertex_buffer;
    m_vertex_array = other.m_vertex_array;
    other.m_vertex_buffer = 0;
    other.m_vertex_array = 0;
}

VertexBuffer &VertexBuffer::operator=(VertexBuffer &&other)
{
    m_vertex_buffer = other.m_vertex_buffer;
    m_vertex_array = other.m_vertex_array;
    other.m_vertex_buffer = 0;
    other.m_vertex_array = 0;
    return *this;
}

void VertexBuffer::Bind() const
{
    glBindVertexArray(m_vertex_array);
}

void VertexBuffer::Unbind() const
{
    glBindVertexArray(0);
}

void VertexBuffer::SetData(const void *data, size_t len) const
{
    glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);

    /* Most of the time, this function will be called to update the data in the
       existing buffer. For that, we grab a pointer to the buffer and copy the
       input with `std::memcpy`. Less often, the size will change. If that's the
       case we reallocate the data store to the target size and copy the new
       contents at the same time. */

    GLint size{};
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);

    if (size > 0 && static_cast<size_t>(size) == len)
    {
        auto ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        if (ptr != nullptr)
        {
            std::memcpy(ptr, data, len);

            /* Once we've copied the data, we unmap the buffer to synchronize
               the changes. According to the documentation, we have to look out
               for the (rare) return value `GL_FALSE` and reinitialize the data
               store in that case. */
            if (GL_FALSE == glUnmapBuffer(GL_ARRAY_BUFFER))
                glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(len), data, GL_DYNAMIC_DRAW);
        }
    }
    else
    {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(len), data, GL_DYNAMIC_DRAW);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

