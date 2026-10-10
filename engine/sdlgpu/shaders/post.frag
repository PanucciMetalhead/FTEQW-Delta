#version 450
layout(set=2,binding=0) uniform sampler2D scene;
layout(set=3,binding=0,std140) uniform Post { vec4 settings, color, reserved; } u;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outputcolor;
void main() {
    vec3 c=texture(scene,uv).rgb;

    if(u.reserved.x>0) {
        vec3 nw=texture(scene,uv+vec2(-1,-1)*u.settings.xy).rgb;
        vec3 ne=texture(scene,uv+vec2( 1,-1)*u.settings.xy).rgb;
        vec3 sw=texture(scene,uv+vec2(-1, 1)*u.settings.xy).rgb;
        vec3 se=texture(scene,uv+vec2( 1, 1)*u.settings.xy).rgb;
        vec3 luma=vec3(0.299,0.587,0.114);
        float a=dot(nw,luma),b=dot(ne,luma),d=dot(sw,luma),e=dot(se,luma),m=dot(c,luma);
        float lo=min(m,min(min(a,b),min(d,e))),hi=max(m,max(max(a,b),max(d,e)));
        if(hi-lo>max(0.0312,hi*0.125)) {
            vec2 direction=vec2(-((a+b)-(d+e)),(a+d)-(b+e));
            float reduce=max((a+b+d+e)*0.03125,0.0078125);
            direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),vec2(-8),vec2(8))*u.settings.xy;
            vec3 r1=0.5*(texture(scene,uv+direction*(1.0/3.0-0.5)).rgb+texture(scene,uv+direction*(2.0/3.0-0.5)).rgb);
            vec3 r2=r1*0.5+0.25*(texture(scene,uv-direction*0.5).rgb+texture(scene,uv+direction*0.5).rgb);
            float l=dot(r2,luma); c=(l<lo || l>hi)?r1:r2;
        }
    }
    if(u.settings.z>0) {
        vec3 bloom=vec3(0); float total=0;
        for(int x=-3;x<=3;x++) for(int y=-3;y<=3;y++) {
            float w=exp(-float(x*x+y*y)/6);
            bloom+=max(vec3(0),texture(scene,uv+vec2(x,y)*u.settings.xy*3).rgb-0.7)*w; total+=w;
        }
        c+=bloom/total*u.settings.z;
    }
    c=pow(max(c,vec3(0)),vec3(1/max(u.color.z,0.01)));
    c=pow(max(c,vec3(0)),vec3(u.settings.w))*u.color.x+u.color.y;
    outputcolor=vec4(c,1);
}
