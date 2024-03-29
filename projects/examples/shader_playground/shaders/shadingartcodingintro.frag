#version 330 core
// Link to adapted shadertoy example
// https://www.shadertoy.com/view/mtyGWy

// Tutorial origin
// https://www.youtube.com/watch?v=f4s1h2YETNY

layout(location = 0) out vec4 color;


uniform vec4 uniform_color;
uniform vec2 iResolution;
uniform vec4 iMouse;
uniform float iTime;

vec3 palette( float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0,1.0,1.0);
    vec3 d = vec3(0.263,0.416,0.557);
    return a + b*cos (6.28318*(c*t+d));
}

void main() {
    vec2 uv = (gl_FragCoord.xy * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv;
    vec3 finalColor = vec3(0.0);
    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;
        float d = length(uv) * exp(-length(uv0));
        vec3 col = palette(length(uv0) + i*.4 + iTime*.4);
        d = sin(d*8. + iTime)/8.f;
        d = abs(d);
        d = 0.01 / d;
        finalColor += col * d;
    }
    color = vec4(finalColor,1.0);
}