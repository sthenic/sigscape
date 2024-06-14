#version 330 core
in vec3 our_color;
in vec2 our_tex_coord;

out vec4 FragColor;

uniform sampler2D our_texture;
uniform float mix_value;
uniform vec3 mix_color;

void main()
{
    FragColor = texture(our_texture, our_tex_coord) * vec4(our_tex_coord, 0.5, 1.0);
}
