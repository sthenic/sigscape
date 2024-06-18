#version 330 core
layout (location = 0) in vec2 in_pos;
// layout (location = 1) in vec3 in_col;

uniform float xmin;
uniform float xmax;
uniform float ymin;
uniform float ymax;

// out vec3 vs_out_col;

void main()
{
    float dx = 2.0 / (xmax - xmin);
    float dy = 2.0 / (ymax - ymin);
    gl_Position = vec4((in_pos.x - xmin) * dx - 1.0, (in_pos.y - ymin) * dy - 1.0, 0.0, 1.0);
    // vs_out_col = in_col;
}
