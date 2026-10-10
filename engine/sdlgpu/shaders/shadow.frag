#version 450
layout(set=2,binding=0) uniform sampler2D base;
layout(set=3,binding=0,std140) uniform Fragment { vec4 control; } u;
layout(location=0) in vec2 v_uv;
void main() {
    float a=texture(base,v_uv).a;
    if((u.control.x==1 && a<=0) || (u.control.x==2 && a>=0.5) || (u.control.x==3 && a<0.5)) discard;
    if((u.control.x==4 && a<u.control.y) || (u.control.x==5 && a>=u.control.y)) discard;
}
