#pragma once

#include "GL/gl3w.h"

class Texture
{
public:
    Texture() = default;
    Texture(int width, int height, const void *data = nullptr);
    ~Texture();

    /* Only allow moving. */
    Texture(const Texture &other) = delete;
    Texture &operator=(const Texture &other) = delete;
    Texture(Texture &&other);
    Texture &operator=(Texture &&other);

    [[nodiscard]] GLuint GetId() const;

private:
    GLuint m_id{0};
};
