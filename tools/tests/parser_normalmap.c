/* Actual generated normal-map RGBA packing, including all alpha byte values. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char qbyte;
#define bound(lo,v,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
static unsigned int LittleLong(unsigned int value)
{
    const uint16_t one = 1;
    if (*(const qbyte *)&one)
        return value;
    return ((value & 255u) << 24) | ((value & 0xff00u) << 8) |
           ((value >> 8) & 0xff00u) | ((value >> 24) & 255u);
}
/* ENGINE_FUNCTIONS */
int main(void)
{
    qbyte pixels[16];
    unsigned int output[16];
    for (int height = 0; height < 256; ++height)
    for (int offset = 0; offset < 2; ++offset)
    {
        memset(pixels, height, sizeof(pixels));
        assert(Image_GenerateNormalMap(pixels, output, 4, 4, 4, offset) == output);
        const qbyte *bytes = (const qbyte *)output;
        for (int i = 0; i < 16; ++i)
        {
            assert(bytes[4*i] == 128 && bytes[4*i+1] == 128 && bytes[4*i+2] == 255);
            assert(bytes[4*i+3] == (offset ? height : 255));
        }
    }
    /* A height ramp changes the normal directions, including the wrapped edge. */
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            pixels[4*y+x] = x * 64;
    Image_GenerateNormalMap(pixels, output, 4, 4, 4, 1);
    const qbyte *bytes = (const qbyte *)output;
    for (int i = 0; i < 16; ++i)
    {
        assert(bytes[4*i+1] == 128);
        assert(bytes[4*i] < 128 || i % 4 == 3);
        if (i % 4 == 3) assert(bytes[4*i] > 128);
        assert(bytes[4*i+2] > 128 && bytes[4*i+2] < 255);
        assert(bytes[4*i+3] == pixels[i]);
    }
    puts("Generated normal maps: all 256 height/alpha values, flat normals and wrapped ramp passed");
    return 0;
}
