#include <metal_stdlib>
using namespace metal;
struct Input { float4 position [[position]]; float2 uv; };
struct Post { float4 settings,color,reserved; };
fragment float4 main0(Input v [[stage_in]],constant Post &u [[buffer(0)]], texture2d<float> scene [[texture(0)]],sampler s [[sampler(0)]]) {
    float3 c=scene.sample(s,v.uv).rgb;

    if(u.reserved.x>0) {
        float3 nw=scene.sample(s,v.uv+float2(-1,-1)*u.settings.xy).rgb;
        float3 ne=scene.sample(s,v.uv+float2( 1,-1)*u.settings.xy).rgb;
        float3 sw=scene.sample(s,v.uv+float2(-1, 1)*u.settings.xy).rgb;
        float3 se=scene.sample(s,v.uv+float2( 1, 1)*u.settings.xy).rgb;
        float3 luma=float3(0.299f,0.587f,0.114f);
        float a=dot(nw,luma),b=dot(ne,luma),d=dot(sw,luma),e=dot(se,luma),m=dot(c,luma);
        float lo=min(m,min(min(a,b),min(d,e))),hi=max(m,max(max(a,b),max(d,e)));
        if(hi-lo>max(0.0312f,hi*0.125f)) {
            float2 direction=float2(-((a+b)-(d+e)),(a+d)-(b+e));
            float reduce=max((a+b+d+e)*0.03125f,0.0078125f);
            direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),float2(-8),float2(8))*u.settings.xy;
            float3 r1=0.5f*(scene.sample(s,v.uv+direction*(1.0f/3.0f-0.5f)).rgb+scene.sample(s,v.uv+direction*(2.0f/3.0f-0.5f)).rgb);
            float3 r2=r1*0.5f+0.25f*(scene.sample(s,v.uv-direction*0.5f).rgb+scene.sample(s,v.uv+direction*0.5f).rgb);
            float l=dot(r2,luma); c=(l<lo || l>hi)?r1:r2;
        }
    }
    if(u.settings.z>0) {
        float3 bloom=float3(0); float total=0;
        for(int x=-3;x<=3;x++) for(int y=-3;y<=3;y++) {
            float w=exp(-float(x*x+y*y)/6);
            bloom+=max(float3(0),scene.sample(s,v.uv+float2(x,y)*u.settings.xy*3).rgb-0.7f)*w; total+=w;
        }
        c+=bloom/total*u.settings.z;
    }
    c=pow(max(c,float3(0)),float3(1/max(u.color.z,0.01f)));
    c=pow(max(c,float3(0)),float3(u.settings.w))*u.color.x+u.color.y;
    return float4(c,1);
}
