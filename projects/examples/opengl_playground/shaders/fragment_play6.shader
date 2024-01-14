#version 330 core

layout(location = 0) out vec4 color;


uniform vec4 uniform_color;
uniform vec2 iResolution;
uniform vec4 iMouse;
uniform float time;

float sdCircle( in vec2 p, in float r ) 
{
    return length(p)-r;
}

void main() {
	vec2 p = (2.0*gl_FragCoord.xy-iResolution.xy)/iResolution.y;
    vec2 m = (2.0*iMouse.xy-iResolution.xy)/iResolution.y;

	float d = sdCircle(p,0.5);
    
	// coloring
    vec3 col = (d>0.0) ? vec3(0.9,0.6,0.3) : vec3(0.65,0.85,1.0);
    col *= 1.0 - exp(-6.0*abs(d));
	col *= 0.8 + 0.2*cos(150.0*d);
	col = mix( col, vec3(1.0), 1.0-smoothstep(0.0,0.01,abs(d)) );

    if( iMouse.z>0.001 )
    {
    d = sdCircle(m,0.5);
    col = mix(col, vec3(1.0,1.0,0.0), 1.0-smoothstep(0.0, 0.005, abs(length(p-m)-abs(d))-0.0025));
    col = mix(col, vec3(1.0,1.0,0.0), 1.0-smoothstep(0.0, 0.005, length(p-m)-0.015));
    }

	color = vec4(col,1.0);


    // // Normalize the fragment coordinates
    // vec2 fragCoordNormalized = gl_FragCoord.xy / iResolution.xy;

    // // Gradient based on normalized coords
    // color =  vec4(fragCoordNormalized.x,fragCoordNormalized.y,0.0,0.0);
}