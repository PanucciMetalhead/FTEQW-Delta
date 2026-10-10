#include <metal_stdlib>
using namespace metal;
struct Output { float4 position [[position]]; float2 uv; };
vertex Output main0(uint id [[vertex_id]]) {
    float2 p=float2((id<<1)&2,id&2); Output o; o.uv=p;
    o.position=float4(p.x*2-1,1-p.y*2,0,1); return o;
}
