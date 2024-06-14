#version 330 core
in vec3 our_color;
in vec2 our_tex_coord;

out vec4 FragColor;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform float mix_value;
uniform vec3 mix_color;

void main()
{
    FragColor = mix(texture(texture0, our_tex_coord) * vec4(mix_color, 1.0), texture(texture1, our_tex_coord), mix_value);
}
