#version 330 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 color;

uniform float offset_x;
uniform float offset_y;

out vec3 our_color;
out vec4 our_position;

void main()
{
    our_position = vec4(pos.x + offset_x, pos.y + offset_y, pos.z, 1.0);
    our_color = color;
    gl_Position = our_position;
}
