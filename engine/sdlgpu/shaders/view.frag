#version 450
layout(set=2,binding=0) uniform sampler2D scene;
layout(set=3,binding=0,std140) uniform View { vec4 projection; vec4 quality; } u;
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
vec2 underwater(vec2 uv, float mode, float time) {
    if (mode == 2.0) {
        // WinQuake D_WarpScreen. Keep its 128-pixel period, integer offsets
        // and six-pixel edge compression in 320x200 reference coordinates.
        const vec2 reference = vec2(320.0, 200.0);
        vec2 pixel = uv * reference;
        vec2 phase = floor(pixel.yx) + floor(time * 20.0);
        vec2 offset = floor(3.0 + 3.0 * sin(mod(phase, 128.0) * (6.28318 / 128.0)));
        return (max(pixel - 0.5, vec2(0.0)) + offset) / (reference + 6.0);
    }
    // The older FTE shader used a scrolling sine/cosine lookup and edge fade.
    vec2 edge = min(uv, 1.0 - uv);
    float fade = sin(min(1.0, min(edge.x, edge.y) * 16.0) * 1.5707963);
    vec2 phase = (uv.yx + time * 0.25) * 6.2831853;
    return uv + vec2(sin(phase.x), cos(phase.y)) * (0.016 * min(mode, 4.0) * fade);
}
void main() {
    float warp = u.quality.z;
    vec2 source = warp > 0.0 ? underwater(uv, warp, u.quality.w) : uv;
    if (u.projection.x == 0.0) {
        vec2 texel = u.projection.yz;
        if (warp == 2.0)
            source = (floor(source / texel) + 0.5) * texel;
        outputcolor = textureLod(scene, source, 0.0);
        return;
    }
    vec2 p = source * 2.0 - 1.0;
    float scale = (1.0 - u.projection.x) / (1.0 - u.projection.x * p.x * p.x);
    source = p * scale * 0.5 + 0.5;
    outputcolor = reconstruct(scene, source, u.projection.yz, u.quality.x, u.projection.w);
}
