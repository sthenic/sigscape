#version 330 core
in vec3 our_color;
in vec2 our_tex_coord;

out vec4 FragColor;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform float mix_value;

void main()
{
    FragColor = mix(texture(texture0, our_tex_coord), texture(texture1, vec2(1.0 - our_tex_coord.x, our_tex_coord.y)), mix_value);
}
