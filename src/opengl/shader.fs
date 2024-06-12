#version 330 core
out vec4 FragColor;
in vec3 our_color;
in vec4 our_position;

void main()
{
    FragColor = our_position;//vec4(our_color, 1.0f);
}
