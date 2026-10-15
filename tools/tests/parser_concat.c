/* Real string helpers: empty/first append must never pass NULL to memcpy. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned live;
static void *BZ_Malloc(size_t size)
{
    void *p = malloc(size); assert(p); ++live; return p;
}
static void BZ_Free(void *p) { if (p) { assert(live); --live; free(p); } }
/* macOS libc lacks glibc's nonnull attributes: enforce the same contract here. */
static void *checked_memcpy(void *dest, const void *src, size_t size)
{
    assert(dest && src); return memcpy(dest, src, size);
}
#define memcpy checked_memcpy
/* ENGINE_FUNCTIONS */
int main(void)
{
    for (int i = 0; i < 256; ++i)
    {
        char *s = NULL;
        Z_StrCat(&s, ""); assert(s && !*s && live == 1);
        Z_StrCatLen(&s, NULL, 0); assert(!*s && live == 1);
        Z_StrCat(&s, "Jolt"); assert(!strcmp(s, "Jolt"));
        Z_StrCatLen(&s, "abcDEF", 3); assert(!strcmp(s, "Joltabc"));
        Z_StrCat(&s, s); assert(!strcmp(s, "JoltabcJoltabc"));
        Z_StrCatLen(&s, s + 4, 3); assert(!strcmp(s, "JoltabcJoltabcabc"));
        Z_StrCatLen(&s, "", 0); assert(!strcmp(s, "JoltabcJoltabcabc") && live == 1);
        BZ_Free(s); assert(!live);
        s = NULL;
        Z_StrCatLen(&s, NULL, 0); assert(s && !*s && live == 1);
        BZ_Free(s); assert(!live);
        s = NULL;
        Z_StrCat(&s, "first"); assert(!strcmp(s, "first"));
        BZ_Free(s); assert(!live);
        s = NULL;
        Z_StrCatLen(&s, "bounded", 4); assert(!strcmp(s, "boun"));
        BZ_Free(s); assert(!live);
    }
    puts("String append: NULL initial/zero-length inputs, bounded and aliased appends, repeated allocation/free passed");
    return 0;
}
