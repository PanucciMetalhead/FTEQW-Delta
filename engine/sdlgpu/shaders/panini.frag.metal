#include <metal_stdlib>
using namespace metal;
struct Input { float4 position [[position]]; float2 uv; };
struct Panini { float4 projection; };
fragment float4 main0(Input v [[stage_in]], constant Panini &u [[buffer(0)]],
    texture2d<float> scene [[texture(0)]], sampler s [[sampler(0)]]) {
    float2 p = v.uv * 2.0f - 1.0f;
    float scale = (1.0f - u.projection.x) / (1.0f - u.projection.x * p.x * p.x);
    float2 source = p * scale * 0.5f + 0.5f;
    return scene.sample(s, source);
}
