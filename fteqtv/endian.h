/* Fixed-width little-endian disk fields; each argument is evaluated once. */
#ifndef QTV_ENDIAN_H
#define QTV_ENDIAN_H

#include <stdint.h>
#include <string.h>

static inline int32_t LittleLong(uint32_t value)
{
    const unsigned char *p = (const unsigned char *)&value;
    uint32_t bits = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    int32_t result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static inline int16_t LittleShort(uint16_t value)
{
    const unsigned char *p = (const unsigned char *)&value;
    uint16_t bits = (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
    int16_t result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

#endif
