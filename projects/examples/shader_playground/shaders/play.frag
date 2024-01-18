#version 330 core

layout(location = 0) out vec4 color;

uniform vec4 uniform_color;

void main() {
    color =  uniform_color;
    // color =  vec4(1.0,0.5,0.5,0.5);
}