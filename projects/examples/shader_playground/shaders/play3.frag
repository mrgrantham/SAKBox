#version 330 core

layout(location = 0) out vec4 color;

uniform vec4 uniform_color;
uniform float iTime;

void main() {

    float offset = sin(iTime) * 0.5;  // Adjust the multiplier and add more complexity if needed
    color =  vec4(0.5f,offset,0.0f,1.0f);
    // color =  vec4(1.0,0.5,0.5,0.5);
}