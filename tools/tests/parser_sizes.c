/* Preserve the protocol's signed-height box encoding and point special case. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define bound(lo,v,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
/* ENGINE_FUNCTIONS */
int main(void)
{
    float mins[3] = {-17,-17,-9}, maxs[3] = {17,17,0};
    float decodedmin[3], decodedmax[3];
    for (int height = -32768; height <= 32767; ++height)
    {
        maxs[2] = height;
        uint32_t expected = 17u | (9u << 8) | ((uint32_t)(height + 32768) << 16);
        int packed = COM_EncodeSize(mins, maxs);
        assert((uint32_t)packed == expected);
        COM_DecodeSize(packed, decodedmin, decodedmax);
        for (int axis = 0; axis < 3; ++axis)
            assert(decodedmin[axis] == mins[axis] && decodedmax[axis] == maxs[axis]);
    }
    float point[3] = {0,0,0};
    assert(COM_EncodeSize(point,point) == 0);
    mins[0] = -1000; mins[2] = -1000; maxs[2] = 40000;
    assert((uint32_t)COM_EncodeSize(mins,maxs) == 0xffffffffu);
    maxs[2] = -40000;
    assert((uint32_t)COM_EncodeSize(mins,maxs) == 0x0000ffffu);
    puts("Solid box packing: all 65,536 signed heights, bounds and point-sized special case preserve protocol bits");
    return 0;
}
