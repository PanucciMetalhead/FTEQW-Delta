#version 450
layout(set=2,binding=0) uniform sampler2D scene;
layout(set=3,binding=0,std140) uniform Panini { vec4 projection; vec4 quality; } u;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outputcolor;
// Catmull-Rom reconstruction: group the positive middle weights into
// linear samples, leaving the negative outer lobes separate (nine reads).
vec4 cubic(sampler2D scene, vec2 uv, vec2 texel) {
    vec2 pixel = uv / texel - 0.5;
    vec2 base = floor(pixel), f = pixel - base;
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);
    vec2 middle = w1 + w2;
    vec2 a = (base - 0.5) * texel;
    vec2 b = (base + 0.5 + w2 / middle) * texel;
    vec2 c = (base + 2.5) * texel;
    return (textureLod(scene, vec2(a.x,a.y), 0.0) * w0.x + textureLod(scene, vec2(b.x,a.y), 0.0) * middle.x + textureLod(scene, vec2(c.x,a.y), 0.0) * w3.x) * w0.y
         + (textureLod(scene, vec2(a.x,b.y), 0.0) * w0.x + textureLod(scene, vec2(b.x,b.y), 0.0) * middle.x + textureLod(scene, vec2(c.x,b.y), 0.0) * w3.x) * middle.y
         + (textureLod(scene, vec2(a.x,c.y), 0.0) * w0.x + textureLod(scene, vec2(b.x,c.y), 0.0) * middle.x + textureLod(scene, vec2(c.x,c.y), 0.0) * w3.x) * w3.y;
}
vec4 filtered(sampler2D scene, vec2 uv, vec2 dx, vec2 dy, float cubicfilter) {
    // Level zero preserves the original bilinear mode without reading mips.
    return cubicfilter > 0.5 ? textureGrad(scene, uv, dx, dy) : textureLod(scene, uv, 0.0);
}
vec4 reconstruct(sampler2D scene, vec2 source, vec2 texel, float cubicfilter, float sharpen) {
    vec2 dx = dFdx(source), dy = dFdy(source);
    float footprint = max(length(dx / texel), length(dy / texel));
    vec4 linear = filtered(scene, source, dx, dy, cubicfilter);
    vec4 color = linear;
    // Bicubic where pixels are enlarged; anisotropic mip filtering where
    // the wide-angle edges shrink. Blend the transition to avoid a seam.
    if (cubicfilter > 0.5 && footprint < 1.5)
        color = mix(cubic(scene, source, texel), linear, smoothstep(1.0,1.5,footprint));
    if (sharpen > 0.0 && footprint < 2.0) {
        vec3 left = filtered(scene, source - vec2(texel.x,0.0), dx, dy, cubicfilter).rgb;
        vec3 right = filtered(scene, source + vec2(texel.x,0.0), dx, dy, cubicfilter).rgb;
        vec3 up = filtered(scene, source - vec2(0.0,texel.y), dx, dy, cubicfilter).rgb;
        vec3 down = filtered(scene, source + vec2(0.0,texel.y), dx, dy, cubicfilter).rgb;
        vec3 low = min(color.rgb, min(linear.rgb, min(min(left,right),min(up,down))));
        vec3 high = max(color.rgb, max(linear.rgb, max(max(left,right),max(up,down))));
        vec3 allowance = (high - low) * 0.05;
        float strength = sharpen * (1.0-smoothstep(1.0,2.0,footprint));
        color.rgb = clamp(color.rgb + strength * (linear.rgb - (left+right+up+down)*0.25), low-allowance, high+allowance);
    }
    // Preserve HDR highlights: only alpha has a unit upper bound.
    return vec4(max(color.rgb, vec3(0.0)), clamp(color.a,0.0,1.0));
}
void main() {
    vec2 p = uv * 2.0 - 1.0;
    float scale = (1.0 - u.projection.x) / (1.0 - u.projection.x * p.x * p.x);
    vec2 source = p * scale * 0.5 + 0.5;
    outputcolor = reconstruct(scene, source, u.projection.yz, u.quality.x, u.projection.w);
}
