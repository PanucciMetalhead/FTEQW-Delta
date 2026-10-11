#version 450
layout(set=2,binding=0) uniform sampler2D scene;
layout(set=3,binding=0,std140) uniform Panini { vec4 projection; } u;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outputcolor;
void main() {
    // Inverse standard Panini (D=1), fitted to the perspective source's
    // horizontal field of view. Vertical lines remain straight. Applying
    // the same scale to both coordinates avoids stretching at the center.
    vec2 p = uv * 2.0 - 1.0;
    float scale = (1.0 - u.projection.x) / (1.0 - u.projection.x * p.x * p.x);
    vec2 source = p * scale * 0.5 + 0.5;
    outputcolor = texture(scene, source);
}
