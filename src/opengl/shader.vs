#version 330 core
layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec2 in_tex_coord;

out vec3 our_color;
out vec2 our_tex_coord;

void main()
{
    gl_Position = vec4(in_pos, 1.0);
    our_color = in_color;
    our_tex_coord = in_tex_coord;
}
