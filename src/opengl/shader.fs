#version 330 core
in vec3 our_color;
in vec2 our_tex_coord;

out vec4 FragColor;

uniform float mix_value;
uniform vec3 mix_color;

void main()
{
    FragColor = vec4(1.0, 0.2, 0.5, 1.0);
}
