/* Exercise actual skeleton lifetime code with native and VM allocation hosts. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Engine loops predate signedness/unused-argument cleanup; test their behavior. */
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define countof(a) (sizeof(a)/sizeof((a)[0]))
#define MAX_SKEL_OBJECTS 8
#define MAX_BONES 16
typedef bool qboolean;
typedef struct model_s model_t;
typedef struct { void *progs; } world_t;
typedef enum { SKEL_RELATIVE, SKEL_ABSOLUTE } skeltype_t;
/* ENGINE_SKEL_TYPE */
static skelobject_t skelobjects[MAX_SKEL_OBJECTS];
static int numskelobjectsused;
static qboolean pendingkill;
static unsigned int native_live, vm_count;
static void *vm_allocations[64];
static void *BZ_Malloc(size_t size)
{
    void *p = calloc(1, size); assert(p); ++native_live; return p;
}
static void BZ_Free(void *p)
{
    assert(p && native_live);
    for (unsigned int i = 0; i < vm_count; ++i) assert(p != vm_allocations[i]);
    --native_live; free(p);
}
static void *PR_AddString(void *progs, const char *value, size_t size, bool copy)
{
    assert(progs && value && !copy && vm_count < countof(vm_allocations));
    void *p = calloc(1, size); assert(p); vm_allocations[vm_count++] = p; return p;
}
/* ENGINE_FUNCTIONS */
static void delete_object(world_t *w, skelobject_t *s)
{
    s->inuse = 2; pendingkill = true; skel_dodelete(w);
}
static void free_vm(void)
{
    for (unsigned int i = 0; i < vm_count; ++i) free(vm_allocations[i]);
    vm_count = 0;
}
int main(void)
{
    world_t native = {0}, other = {0}; int vm;
    assert(!skel_create(&native, 0) && !skel_create(&native, MAX_BONES + 1));
    assert(!skel_get(&native, 0) && !skel_get(&native, -1));
    for (int cycle = 0; cycle < 32; ++cycle)
    {
        /* Exhaust the table: the failed allocation must never touch slot 8. */
        for (int i = 0; i < MAX_SKEL_OBJECTS; ++i)
        {
            skelobject_t *s = skel_create(&native, 3);
            assert(s == &skelobjects[i] && s->ownedmatrix && s->numbones == 3);
            for (int j = 0; j < 36; ++j) s->bonematrix[j] = (float)j;
            assert(skel_get(&native, i + 1) == s);
            assert(!skel_get(&other, i + 1));
        }
        assert(native_live == MAX_SKEL_OBJECTS && !skel_create(&native, 3));
        assert(!skel_get(&native, MAX_SKEL_OBJECTS + 1));
        /* Non-tail cache reuse transitions heap storage to VM storage. */
        delete_object(&native, &skelobjects[0]);
        assert(!skel_get(&native, 1));
        native.progs = &vm;
        assert(skel_create(&native, 3) == &skelobjects[0]);
        assert(!skelobjects[0].ownedmatrix && native_live == MAX_SKEL_OBJECTS - 1 && vm_count == 1);
        skel_reset(&other); assert(native_live == MAX_SKEL_OBJECTS - 1);
        skel_reset(&native); assert(native_live == 0 && numskelobjectsused == 0);
        skel_reset(&native); free_vm();
        native.progs = NULL;
        /* Deleting the tail shrinks the used table: this takes the second loop. */
        skelobject_t *s = skel_create(&native, 2);
        delete_object(&native, s); assert(numskelobjectsused == 0 && native_live == 1);
        native.progs = &vm;
        assert(skel_create(&native, 2) == s);
        assert(!s->ownedmatrix && native_live == 0 && vm_count == 1);
        skel_reset(&native); free_vm(); native.progs = NULL;
        /* Reusing a native cache without a VM must keep its allocation. */
        s = skel_create(&native, MAX_BONES);
        float *matrix = s->bonematrix;
        delete_object(&native, s);
        assert(skel_create(&native, MAX_BONES)->bonematrix == matrix && native_live == 1);
        skel_reset(&native); assert(native_live == 0);
        /* Resetting one world preserves another world's still-live skeleton. */
        s = skel_create(&native, 1);
        skelobject_t *second = skel_create(&other, 1);
        int second_index = (int)(second - skelobjects) + 1;
        skel_reset(&native);
        assert(skel_get(&other, second_index) == second && native_live == 1);
        skel_reset(&other); assert(native_live == 0 && numskelobjectsused == 0);
    }
    puts("Skeleton lifetime: native/VM ownership, both cache transitions, exhaustion, world isolation and repeated cleanup passed");
    return 0;
}
