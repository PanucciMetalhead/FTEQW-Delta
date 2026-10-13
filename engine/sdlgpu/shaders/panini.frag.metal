#include <metal_stdlib>
using namespace metal;
struct Input { float4 position [[position]]; float2 uv; };
struct Panini { float4 projection; float4 quality; };
// Catmull-Rom reconstruction: group the positive middle weights into
// linear samples, leaving the negative outer lobes separate (nine reads).
float4 cubic(texture2d<float> scene, sampler s, float2 uv, float2 texel) {
    float2 pixel = uv / texel - 0.5;
    float2 base = floor(pixel), f = pixel - base;
    float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    float2 w3 = f * f * (-0.5 + 0.5 * f);
    float2 middle = w1 + w2;
    float2 a = (base - 0.5) * texel;
    float2 b = (base + 0.5 + w2 / middle) * texel;
    float2 c = (base + 2.5) * texel;
    return (scene.sample(s, float2(a.x,a.y), level(0.0)) * w0.x + scene.sample(s, float2(b.x,a.y), level(0.0)) * middle.x + scene.sample(s, float2(c.x,a.y), level(0.0)) * w3.x) * w0.y
         + (scene.sample(s, float2(a.x,b.y), level(0.0)) * w0.x + scene.sample(s, float2(b.x,b.y), level(0.0)) * middle.x + scene.sample(s, float2(c.x,b.y), level(0.0)) * w3.x) * middle.y
         + (scene.sample(s, float2(a.x,c.y), level(0.0)) * w0.x + scene.sample(s, float2(b.x,c.y), level(0.0)) * middle.x + scene.sample(s, float2(c.x,c.y), level(0.0)) * w3.x) * w3.y;
}
float4 filtered(texture2d<float> scene, sampler s, float2 uv, float2 dx, float2 dy, float cubicfilter) {
    // Level zero preserves the original bilinear mode without reading mips.
    return cubicfilter > 0.5 ? scene.sample(s, uv, gradient2d(dx,dy)) : scene.sample(s, uv, level(0.0));
}
float4 reconstruct(texture2d<float> scene, sampler s, float2 source, float2 texel, float cubicfilter, float sharpen) {
    float2 dx = dfdx(source), dy = dfdy(source);
    float footprint = max(length(dx / texel), length(dy / texel));
    float4 linear = filtered(scene, s, source, dx, dy, cubicfilter);
    float4 color = linear;
    // Bicubic where pixels are enlarged; anisotropic mip filtering where
    // the wide-angle edges shrink. Blend the transition to avoid a seam.
    if (cubicfilter > 0.5 && footprint < 1.5)
        color = mix(cubic(scene, s, source, texel), linear, smoothstep(1.0,1.5,footprint));
    if (sharpen > 0.0 && footprint < 2.0) {
        float3 left = filtered(scene, s, source - float2(texel.x,0.0), dx, dy, cubicfilter).rgb;
        float3 right = filtered(scene, s, source + float2(texel.x,0.0), dx, dy, cubicfilter).rgb;
        float3 up = filtered(scene, s, source - float2(0.0,texel.y), dx, dy, cubicfilter).rgb;
        float3 down = filtered(scene, s, source + float2(0.0,texel.y), dx, dy, cubicfilter).rgb;
        float3 low = min(color.rgb, min(linear.rgb, min(min(left,right),min(up,down))));
        float3 high = max(color.rgb, max(linear.rgb, max(max(left,right),max(up,down))));
        float3 allowance = (high - low) * 0.05;
        float strength = sharpen * (1.0-smoothstep(1.0,2.0,footprint));
        color.rgb = clamp(color.rgb + strength * (linear.rgb - (left+right+up+down)*0.25), low-allowance, high+allowance);
    }
    // Preserve HDR highlights: only alpha has a unit upper bound.
    return float4(max(color.rgb, float3(0.0)), clamp(color.a,0.0,1.0));
}
fragment float4 main0(Input v [[stage_in]], constant Panini &u [[buffer(0)]],
    texture2d<float> scene [[texture(0)]], sampler s [[sampler(0)]]) {
    float2 p = v.uv * 2.0f - 1.0f;
    float scale = (1.0f - u.projection.x) / (1.0f - u.projection.x * p.x * p.x);
    float2 source = p * scale * 0.5f + 0.5f;
    return reconstruct(scene, s, source, u.projection.yz, u.quality.x, u.projection.w);
}
