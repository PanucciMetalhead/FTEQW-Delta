#include <metal_stdlib>
using namespace metal;
struct Input { float4 position [[position]]; float2 uv; };
struct Bloom { float4 settings; };
float3 tap(texture2d<float> source,sampler s,float2 uv,float2 offset,constant Bloom &u) {
    float3 c=source.sample(s,uv+offset*u.settings.xy).rgb;
    return u.settings.z==0 ? max(c-0.7f,float3(0)) : c;
}
fragment float4 main0(Input v [[stage_in]],constant Bloom &u [[buffer(0)]],
    texture2d<float> source [[texture(0)]],sampler s [[sampler(0)]]) {
    float3 c;
    if(u.settings.z<2) {
        c=(tap(source,s,v.uv,float2(-1,-1),u)+tap(source,s,v.uv,float2(1,-1),u)+
           tap(source,s,v.uv,float2(-1,1),u)+tap(source,s,v.uv,float2(1,1),u))*0.25f;
    } else {
        c=tap(source,s,v.uv,float2(-1,-1),u)+tap(source,s,v.uv,float2(1,-1),u)+
           tap(source,s,v.uv,float2(-1,1),u)+tap(source,s,v.uv,float2(1,1),u);
        c+=2*(tap(source,s,v.uv,float2(-1,0),u)+tap(source,s,v.uv,float2(1,0),u)+
              tap(source,s,v.uv,float2(0,-1),u)+tap(source,s,v.uv,float2(0,1),u));
        c+=4*tap(source,s,v.uv,float2(0),u);
        c*=0.0625f;
    }
    return float4(c,u.settings.z==2 ? 0.5f : 1.0f);
}
