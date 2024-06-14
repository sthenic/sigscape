#include "opengl/frame_buffer.h"

FrameBuffer::FrameBuffer()
{
    glGenFramebuffers(1, &m_id);
}

FrameBuffer::~FrameBuffer()
{
    if (m_id > 0)
        glDeleteFramebuffers(1, &m_id);
}

FrameBuffer::FrameBuffer(FrameBuffer &&other)
{
    m_id = other.m_id;
    other.m_id = 0;
}

FrameBuffer &FrameBuffer::operator=(FrameBuffer &&other)
{
    m_id = other.m_id;
    other.m_id = 0;
    return *this;
}

void FrameBuffer::SetTexture(GLuint id) const
{
    /* Attach the target texture as the default color buffer. */
    glBindFramebuffer(GL_FRAMEBUFFER, m_id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::Bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_id);
}

void FrameBuffer::Unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
