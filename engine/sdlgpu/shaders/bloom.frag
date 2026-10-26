#version 450
layout(set=2,binding=0) uniform sampler2D source;
layout(set=3,binding=0,std140) uniform Bloom { vec4 settings; } u;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outputcolor;
vec3 tap(vec2 offset) {
    vec3 c=texture(source,uv+offset*u.settings.xy).rgb;
    return u.settings.z==0 ? max(c-0.7,vec3(0)) : c;
}
void main() {
    vec3 c;
    if(u.settings.z<2) {
        c=(tap(vec2(-1,-1))+tap(vec2(1,-1))+tap(vec2(-1,1))+tap(vec2(1,1)))*0.25;
    } else {
        c=tap(vec2(-1,-1))+tap(vec2(1,-1))+tap(vec2(-1,1))+tap(vec2(1,1));
        c+=2*(tap(vec2(-1,0))+tap(vec2(1,0))+tap(vec2(0,-1))+tap(vec2(0,1)));
        c+=4*tap(vec2(0));
        c*=0.0625;
    }
    outputcolor=vec4(c,u.settings.z==2 ? 0.5 : 1);
}
