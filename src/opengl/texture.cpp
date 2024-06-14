#include "opengl/texture.h"

Texture::Texture(int width, int height, const void *data)
{
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    /* Allocate storage for the texture based on the input parameters. If `data`
       is `NULL`, nothing will be written but we'll still get a memory region of
       the specified size. */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::~Texture()
{
    if (m_id > 0)
        glDeleteTextures(1, &m_id);
}

Texture::Texture(Texture &&other)
{
    m_id = other.m_id;
    other.m_id = 0;
}

Texture &Texture::operator=(Texture &&other)
{
    m_id = other.m_id;
    other.m_id = 0;
    return *this;
}

GLuint Texture::GetId() const
{
    return m_id;
}


