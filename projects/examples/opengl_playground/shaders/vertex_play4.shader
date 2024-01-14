#version 330 core

layout(location = 0) in vec4 position_t;
layout(location = 1) in vec3 normal_t;

uniform float iTime;

void main() {

  // float offset = sin(iTime)*0.1;  // Adjust the multiplier and add more complexity if needed
  // gl_Position = position_t + vec4(offset,offset,0.0,0.0);
  gl_Position = position_t;
  // gl_Position.y += sin(iTime + position_t.x * 8.0)/8.0;
  // gl_Position.x += sin(iTime + position_t.x * 8.0)/8.0;
  // gl_Position = vec4(1.0,1.0,1.0,1.0);
}