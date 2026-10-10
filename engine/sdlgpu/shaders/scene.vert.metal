#include <metal_stdlib>
using namespace metal;
struct Input { float3 position [[attribute(0)]]; float2 uv [[attribute(1)]]; float2 lmuv [[attribute(2)]]; float4 color [[attribute(3)]]; float3 normal [[attribute(4)]]; float3 tangent [[attribute(5)]]; float3 bitangent [[attribute(6)]]; };
struct Vertex { float4x4 mvp,model; };
struct Output { float4 position [[position]]; float2 uv; float2 lmuv; float4 color; float3 worldpos; float3 normal; float3 tangent; float3 bitangent; };
vertex Output main0(Input v [[stage_in]],constant Vertex &u [[buffer(0)]]) {
    Output o; o.position=u.mvp*float4(v.position,1);
    o.uv=v.uv; o.lmuv=v.lmuv; o.color=v.color;
    o.worldpos=(u.model*float4(v.position,1)).xyz;
    float3x3 matrix=float3x3(u.model[0].xyz,u.model[1].xyz,u.model[2].xyz);
    o.normal=matrix*v.normal; o.tangent=matrix*v.tangent; o.bitangent=matrix*v.bitangent;
    return o;
}
