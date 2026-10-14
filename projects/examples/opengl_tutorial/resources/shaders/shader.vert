#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTex;
layout (location = 3) in vec3 aNormal;

out vec3 color;
out vec2 texCoord;

uniform float scale;

uniform mat4 model;
uniform mat4 cameraMatrix;

void main() {
    gl_Position = cameraMatrix * model * vec4(aPos, 1.0);
    color = aColor;
    texCoord = aTex;
}