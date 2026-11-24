#include <metal_stdlib>
using namespace metal;

struct Command { uint indices, instances, firstIndex; int vertexOffset; uint firstInstance; };
struct Candidate { uint4 rect; float nearDepth; Command draw; };
kernel void main0(constant uint4 &params [[buffer(0)]],
    const device float *depths [[buffer(1)]], const device Candidate *candidates [[buffer(2)]],
    device Command *draws [[buffer(3)]], uint id [[thread_position_in_grid]])
{
    if (id >= params.x) return;
    Candidate c = candidates[id];
    float farDepth = 0;
    for (uint y = c.rect.y; y <= c.rect.w; ++y)
        for (uint x = c.rect.x; x <= c.rect.z; ++x)
            farDepth = max(farDepth, depths[y * params.y + x]);
    if (c.nearDepth > farDepth + 0.00001f) c.draw.indices = 0;
    draws[id] = c.draw;
}
