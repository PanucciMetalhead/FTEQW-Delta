/* Actual grouped allocation/free in release and debug, including SIMD alignment. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define QDECL
#define FTE_ALIGN(n) __attribute__((aligned(n)))
typedef float vec4_t[4];
typedef FTE_ALIGN(16) vec4_t avec4_t;
typedef struct { void *first; int totalbytes; } zonegroup_t;
static void *Z_Malloc(size_t size) { void *p = calloc(1, size); assert(p); return p; }
static void BZ_Free(void *p) { free(p); }
_Noreturn void Sys_Error(const char *message) { (void)message; abort(); }
/* ENGINE_ZONE_ALLOCATOR */
/* ENGINE_FUNCTIONS */
_Static_assert(sizeof(zonegroupblock_t) % _Alignof(avec4_t) == 0, "Grouped payload must retain SIMD alignment");
int main(void)
{
    for (int size = 16; size <= 2048; ++size)
    {
        zonegroup_t group = {0};
        void *blocks[3];
        for (int j = 0; j < 3; ++j)
        {
            blocks[j] = ZG_Malloc(&group, size);
            assert((uintptr_t)blocks[j] % _Alignof(avec4_t) == 0);
            for (int i = 0; i < size; ++i)
                assert(((unsigned char *)blocks[j])[i] == 0);
            avec4_t *vector = blocks[j];
            for (int i = 0; i < 4; ++i) (*vector)[i] = i + j;
        }
        ZG_Free(&group, blocks[1]); /* Remove a middle block, preserving both links. */
        assert(((zonegroupblock_t *)group.first)->next->prev == group.first);
        ZG_Free(&group, blocks[2]); /* Remove head. */
        assert(((zonegroupblock_t *)group.first)->prev == NULL);
        ZG_FreeGroup(&group);
        assert(group.first == NULL && group.totalbytes == 0);
        blocks[0] = ZG_Malloc(&group, size);
        ZG_Free(&group, blocks[0]);
        assert(group.first == NULL);
        ZG_FreeGroup(&group);
        assert(group.totalbytes == 0);
    }
    puts("Grouped allocation: 16–2048-byte payloads retain SIMD alignment; middle/head/group free and reuse passed");
    return 0;
}
