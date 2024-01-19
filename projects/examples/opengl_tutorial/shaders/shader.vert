#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 color;

uniform float scale;

void main() {
    vec3 scaled = aPos.xyz + (aPos.xyz * scale);

    gl_Position = vec4(scaled, 1.0);
    color = aColor;
}