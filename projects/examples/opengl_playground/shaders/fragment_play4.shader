#version 330 core

layout(location = 0) out vec4 color;


uniform vec4 uniform_color;
uniform vec2 iResolution;
uniform float time;

void main() {

    // Get the resolution of the viewport
    // vec2 viewportSize = vec2(gl_FragCoord.w, gl_FragCoord.w);
    vec2 viewportSize = vec2(300.0f, 300.0f);

    // Normalize the fragment coordinates
    // vec2 fragCoordNormalized = gl_FragCoord.xy / viewportSize;
    vec2 fragCoordNormalized = gl_FragCoord.xy / iResolution.xy;
    // float offset = sin(time) * 0.5;  // Adjust the multiplier and add more complexity if needed
    // color =  vec4(offset,offset,offset,1.0f);
    color =  vec4(fragCoordNormalized.x,fragCoordNormalized.y,0.0,0.0);
}