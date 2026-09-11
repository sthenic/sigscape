#version 330 core

out vec4 FragColor;
uniform vec4 in_color;

void main()
{
    FragColor = in_color;
}
