#include <metal_stdlib>
using namespace metal;
struct Input { float4 position [[position]]; float2 uv; };
fragment void main0(Input v [[stage_in]],constant float4 &control [[buffer(0)]],
    texture2d<float> base [[texture(0)]],sampler bs [[sampler(0)]]) {
    float a=base.sample(bs,v.uv).a;
    if((control.x==1 && a<=0) || (control.x==2 && a>=0.5f) || (control.x==3 && a<0.5f)) discard_fragment();
    if((control.x==4 && a<control.y) || (control.x==5 && a>=control.y)) discard_fragment();
}
