/* Production rig defaults must not depend on the allocator's previous contents. */
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wsign-compare"
#define MAX_BONES 16
#define GEOMTYPE_BOX 1
#define countof(a) (sizeof(a) / sizeof((a)[0]))
#define min(a, b) ((a) < (b) ? (a) : (b))
typedef bool qboolean;
typedef float vec3_t[3];
typedef struct {
    int unused;
} galiasbone_t;
typedef struct {
    int bones;
} model_t;
/* ENGINE_RIG_TYPES */
/* ENGINE_DOLL_TYPES */
static galiasbone_t bones[3];
static unsigned allocations;
static void *dirty_malloc(size_t size) {
    void *p = malloc(size);
    assert(p);
    memset(p, 0x7f, size);
    ++allocations;
    return p;
}
static void release(void *p) {
    if (p) {
        assert(allocations);
        --allocations;
        free(p);
    }
}
static char *copy_string(const char *text) {
    char *p = dirty_malloc(strlen(text) + 1);
    strcpy(p, text);
    return p;
}
static int Mod_GetNumBones(model_t *model, qboolean absolute) {
    (void)absolute;
    return model->bones;
}
static galiasbone_t *Mod_GetBoneInfo(model_t *model, int *count) {
    *count = model->bones;
    return bones;
}
#define malloc dirty_malloc
#define BZ_Malloc dirty_malloc
#define strdup copy_string
/* ENGINE_FUNCTIONS */
int main(void) {
    model_t model = {3};
    assert(!rag_createdoll(&model, "bad", 0) && !allocations);
    assert(!rag_createdoll(&model, "bad", 2) && !allocations);
    for (int i = 0; i < 256; ++i) {
        dollcreatectx_t *ctx = rag_createdoll(&model, "default", 3);
        assert(ctx && !ctx->errors);
        doll_t *d = ctx->d;
        assert(skel_checkpoint_doll(d));
        assert(d->refanim == -1 && d->refanimtime == 0 && !d->drawn && !d->numdefaultanimated &&
               !d->registered && !d->next && !d->uses && !d->definition && !d->numbodies &&
               !d->body && !d->numjoints && !d->joint && d->numbones == 3);
        assert(!ctx->body && !ctx->joint);
        for (int j = 0; j < 3; ++j)
            assert(d->bone[j].bodyidx == -1);
        assert(ctx->defbody.mass == 1 && ctx->defbody.animate == 1 &&
               ctx->defbody.dimensions[0] == 4 && ctx->defjoint.startenabled);
        release(d->bone);
        release(d->name);
        release(d);
        release(ctx);
        assert(!allocations);
    }
    puts("Rig defaults: dirty allocation, skin reference time, parser selectors, "
         "checkpoint validation and repeated cleanup passed");
    return 0;
}
