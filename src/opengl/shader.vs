#version 330 core
layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_color;

uniform float xmin;
uniform float xmax;
uniform float ymin;
uniform float ymax;

void main()
{
    float dx = 2.0 / (xmax - xmin);
    float dy = 2.0 / (ymax - ymin);
    gl_Position = vec4((in_pos.x - xmin) * dx - 1.0, (in_pos.y - ymin) * dy - 1.0, 0.0, 1.0);
}
