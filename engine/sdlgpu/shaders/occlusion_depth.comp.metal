#include <metal_stdlib>
using namespace metal;

kernel void main0(depth2d<float> sourceDepth [[texture(0)]],
    device float *depths [[buffer(0)]],
    uint2 pixel [[thread_position_in_grid]], uint2 group [[threadgroup_position_in_grid]],
    uint lane [[thread_index_in_threadgroup]])
{
    uint2 size(sourceDepth.get_width(), sourceDepth.get_height());
    threadgroup float tile[64];
    tile[lane] = all(pixel < size) ? sourceDepth.read(pixel) : 1.0f;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (uint step = 32; step != 0; step >>= 1) {
        if (lane < step) tile[lane] = max(tile[lane], tile[lane + step]);
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    if (lane == 0) depths[group.y * ((size.x + 7) / 8) + group.x] = tile[0];
}
