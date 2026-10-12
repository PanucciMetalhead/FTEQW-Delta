#include "fteqtv/endian.h"
#undef NDEBUG /* Assertions must also execute in optimized test builds. */
#include <assert.h>
#include <stdio.h>

int main(void)
{
    uint32_t value = 0, raw32, actual32;
    uint16_t raw16, actual16;
    int32_t signed32;
    int16_t signed16;
    unsigned char bytes[4];
    unsigned int i, calls = 0;

    for (i = 0; i < 100000; ++i)
    {
        value = value * UINT32_C(1664525) + UINT32_C(1013904223);
        bytes[0] = (unsigned char)value;
        bytes[1] = (unsigned char)(value >> 8);
        bytes[2] = (unsigned char)(value >> 16);
        bytes[3] = (unsigned char)(value >> 24);
        memcpy(&raw32, bytes, sizeof(raw32));
        signed32 = LittleLong(raw32);
        memcpy(&actual32, &signed32, sizeof(actual32));
        assert(actual32 == value);
        memcpy(&raw16, bytes, sizeof(raw16));
        signed16 = LittleShort(raw16);
        memcpy(&actual16, &signed16, sizeof(actual16));
        assert(actual16 == (uint16_t)value);
    }
    memset(bytes, 255, sizeof(bytes));
    memcpy(&raw32, bytes, sizeof(raw32));
    memcpy(&raw16, bytes, sizeof(raw16));
    assert(LittleLong(raw32) == -1);
    assert(LittleShort(raw16) == -1);
    (void)LittleLong(calls++);
    (void)LittleShort((uint16_t)calls++);
    assert(calls == 2);
    puts("C17 endian checks passed (100000 patterns and signed sentinels)");
    return 0;
}
