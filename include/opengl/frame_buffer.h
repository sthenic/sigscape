#pragma once

#include "GL/gl3w.h"

class FrameBuffer
{
public:
    FrameBuffer();
    ~FrameBuffer();

    /* Only allow moving. */
    FrameBuffer(const FrameBuffer &other) = delete;
    FrameBuffer &operator=(const FrameBuffer &other) = delete;
    FrameBuffer(FrameBuffer &&other);
    FrameBuffer &operator=(FrameBuffer &&other);

    void SetTexture(GLuint id) const;
    void Bind() const;
    void Unbind() const;

private:
    GLuint m_id{0};
};
