#version 330 core

layout(location = 0) out vec4 color;


uniform vec4 uniform_color;
uniform vec2 iResolution;
uniform vec4 iMouse;
uniform float time;

float dot2( vec2 v ) { return dot(v,v); }
float sdfCoolS( in vec2 p )
{
    // symmetries
    float six = (p.y<.0) ? -p.x : p.x;
    p.x = abs(p.x);
    p.y = abs(p.y) - .2;
    float rex = p.x - min(round(p.x/.4),.4);
    float aby = abs(p.y-.2)-.6;
    
    // line segments
    float d = dot2(vec2(six,-p.y)-clamp(.5*(six-p.y),.0,.2));
    d = min(d,dot2(vec2(p.x,-aby)-clamp(.5*(p.x-aby),.0,.4)));
    d = min(d,dot2(vec2(rex,p.y  -clamp(p.y         ,.0,.4))));
    
    // interior vs exterior
    float s = 2.*p.x+aby+abs(aby+.4)-.4;

    return sqrt(d) * sign(s);
}

void main() {
    const float scale = 1.25;
    
	vec2  p = scale*(2.0*gl_FragCoord.xy-iResolution.xy)/iResolution.y;
    vec2  m = scale*(2.0*iMouse.xy-iResolution.xy)/iResolution.y;
    float px = scale*2.0/iResolution.y;
    
    float d = sdfCoolS(p);
    
    // colorize
    vec3 col = (d>0.0) ? vec3(0.9,0.6,0.3) : vec3(0.65,0.85,1.0);
	col *= 1.0 - exp2(-20.0*abs(d));
	col *= 0.7 + 0.2*cos(120.0*d);
	col = mix( col, vec3(1.0), 1.0-smoothstep(0.0,3.0*px,abs(d)) );
    
    // mouse
    if( iMouse.z>0.001 )
    {
        float d = sdfCoolS(m);
        float l = length(p-m);
        col = mix(col, vec3(1.0,1.0,0.0), 1.0-smoothstep(0.0, 2.0*px, abs(l-abs(d))));
        col = mix(col, vec3(1.0,1.0,0.0), 1.0-smoothstep(0.0, 2.0*px, l-px*3.0));
    }
    
	color = vec4(col,1.0);
}