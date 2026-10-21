/* Actual checkpoint allocation: exact VM handles and foreign-world isolation. */
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define MAX_SKEL_OBJECTS 8
#define MAX_BONES 16
#define MAX_PRECACHE_MODELS 32
#define countof(a) (sizeof(a) / sizeof((a)[0]))
#define QDECL
typedef bool qboolean;
typedef enum { SKEL_RELATIVE, SKEL_ABSOLUTE } skeltype_t;
typedef struct {
    void *body, *geom;
} rbebody_t;
typedef struct {
    void *joint;
} rbejoint_t;
typedef struct rbejointinfo_s rbejointinfo_t;
/* ENGINE_SKEL_VIEW */
enum { MLS_NOTLOADED, MLS_LOADED };
typedef struct model_s {
    int loadstate, bones;
} model_t;
typedef struct world_s world_t;
struct world_s {
    void *progs;
    model_t *(*Get_CModel)(world_t *, int);
};
/* ENGINE_SKEL_TYPE */
static skelobject_t skelobjects[MAX_SKEL_OBJECTS];
static int numskelobjectsused;
static qboolean pendingkill;
static unsigned native_live, vm_count;
static void *vm_allocations[64];
static void *BZ_Malloc(size_t bytes) {
    void *p = calloc(1, bytes);
    assert(p);
    ++native_live;
    return p;
}
static void BZ_Free(void *p) {
    assert(p && native_live);
    --native_live;
    free(p);
}
static void *PR_AddString(void *progs, const char *text, size_t bytes, bool copy) {
    assert(progs && text && !copy && vm_count < countof(vm_allocations));
    void *p = calloc(1, bytes);
    assert(p);
    vm_allocations[vm_count++] = p;
    return p;
}
static model_t model = {MLS_LOADED, 3};
static model_t *getmodel(world_t *world, int index) {
    (void)world;
    return index == 1 ? &model : NULL;
}
static int Mod_GetNumBones(model_t *mod, qboolean absolute) {
    (void)absolute;
    return mod->bones;
}
/* ENGINE_FUNCTIONS */
int main(void) {
    int vm;
    world_t first = {&vm, getmodel}, second = {&vm, getmodel};
    float pose[36] = {0};
    for (int bone = 0; bone < 3; bone++)
        pose[bone * 12] = pose[bone * 12 + 5] = pose[bone * 12 + 10] = 1;
    for (int cycle = 0; cycle < 256; cycle++) {
        rbeskel_t v = {0};
        v.id = 5;
        v.modelindex = 1;
        v.type = SKEL_RELATIVE;
        v.numbones = 3;
        v.bonematrix = pose;
        assert(skel_checkpoint_allocate(&first, &v));
        skelobject_t *s = skel_get(&first, 5);
        assert(s && s->model == &model && s->bonematrix != pose && s->world == &first &&
               !s->ownedmatrix);
        assert(!memcmp(s->bonematrix, pose, sizeof(pose)) && numskelobjectsused == 5);
        assert(!skel_checkpoint_allocate(&first, &v) && !skel_checkpoint_allocate(&second, &v));
        assert(vm_count == 1 && skel_get(&first, 5) == s && !skel_get(&second, 5));
        s->inuse = 2;
        assert(!skel_checkpoint_allocate(&second, &v));
        s->inuse = 1;
        rbeskel_t bad = v;
        bad.id = 0;
        assert(!skel_checkpoint_allocate(&first, &bad));
        bad = v;
        bad.id = MAX_SKEL_OBJECTS + 1;
        assert(!skel_checkpoint_allocate(&first, &bad));
        bad = v;
        bad.id = 3;
        bad.numbones = MAX_BONES + 1;
        assert(!skel_checkpoint_allocate(&first, &bad));
        bad = v;
        bad.id = 3;
        bad.numbones = 2;
        assert(!skel_checkpoint_allocate(&first, &bad));
        bad = v;
        bad.id = 3;
        bad.modelindex = 2;
        assert(!skel_checkpoint_allocate(&first, &bad));
        bad = v;
        bad.id = 3;
        bad.type = 7;
        assert(!skel_checkpoint_allocate(&first, &bad));
        bad = v;
        bad.id = 3;
        pose[3] = NAN;
        assert(!skel_checkpoint_allocate(&first, &bad));
        pose[3] = 0;
        model.loadstate = MLS_NOTLOADED;
        assert(!skel_checkpoint_allocate(&first, &bad));
        model.loadstate = MLS_LOADED;
        v.id = 2;
        assert(skel_checkpoint_allocate(&second, &v));
        skel_reset(&first);
        assert(skel_get(&second, 2) && !skel_get(&first, 5));
        skel_reset(&second);
        assert(numskelobjectsused == 0);
        for (unsigned i = 0; i < vm_count; i++)
            free(vm_allocations[i]);
        vm_count = 0;
        /* A cached native allocation owned by another world is protected too. */
        first.progs = NULL;
        s = skel_create(&first, 3);
        assert(s && s->inuse);
        s->inuse = 0;
        v.id = (int)(s - skelobjects) + 1;
        assert(!skel_checkpoint_allocate(&second, &v));
        skel_reset(&first);
        assert(!native_live);
        first.progs = &vm;
    }
    puts("Skeleton checkpoints: exact handles, copied VM poses, model/count/type bounds, nonfinite "
         "input, live/cached foreign ownership and reset passed");
    return 0;
}
