#include <metal_stdlib>
using namespace metal;
struct Vertex { float3 position [[attribute(0)]]; float2 uv [[attribute(1)]]; };
struct Output { float4 position [[position]]; float2 uv; };
vertex Output main0(Vertex v [[stage_in]], constant float4x4 &mvp [[buffer(0)]]) {
    Output o; o.position=mvp*float4(v.position,1); o.uv=v.uv; return o;
}
