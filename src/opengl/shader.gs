#version 330 core

layout (lines) in;
layout (triangle_strip, max_vertices = 6) out;

uniform float thickness;

void main()
{
    /* Extract the (x,y) coordinates of the two points on the line. */
    vec2 p1 = gl_in[0].gl_Position.xy / thickness;
    vec2 p2 = gl_in[1].gl_Position.xy / thickness;

    /* Construct a vector that runs between the two points, then normalize it. */
    vec2 n = p2 - p1;
    vec2 v = normalize(n);

    /* To create a line with a certain thickness, we emit six vertices which
       define four triangles. */

    gl_Position = vec4(vec2(p1.x + v.y, p1.y - v.x) * thickness, 0.0, 1.0);
    EmitVertex();

    gl_Position = vec4(vec2(p2.x + v.y, p2.y - v.x) * thickness, 0.0, 1.0);
    EmitVertex();

    gl_Position = vec4(p1 * thickness, 0.0, 1.0);
    EmitVertex();

    gl_Position = vec4(p2 * thickness, 0.0, 1.0);
    EmitVertex();

    gl_Position = vec4(vec2(p1.x - v.y, p1.y + v.x) * thickness, 0.0, 1.0);
    EmitVertex();

    gl_Position = vec4(vec2(p2.x - v.y, p2.y + v.x) * thickness, 0.0, 1.0);
    EmitVertex();

    EndPrimitive();
}
