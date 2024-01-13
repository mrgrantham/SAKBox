#version 330 core

layout(location = 0) out vec4 color;


uniform vec4 uniform_color;
uniform vec2 iResolution;
uniform float time;

void main() {


    // Normalize the fragment coordinates
    vec2 fragCoordNormalized = gl_FragCoord.xy / iResolution.xy;

    // Gradient based on normalized coords
    color =  vec4(fragCoordNormalized.x,fragCoordNormalized.y,0.0,0.0);
}