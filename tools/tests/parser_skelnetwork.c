/* Exercise production skeletal wire encoding, interpolation and pose lifetime. */
#include <assert.h>
#include <math.h>
#include <stdarg.h>
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
#define MAX_BONES 64
#define QDECL
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
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
typedef float vec3_t[3], vec4_t[4];
typedef struct {int skeletalobject;} lerpents_t;
world_t csqc_world;
#define VectorScale(a,s,b) do {for(int vi=0;vi<3;vi++)(b)[vi]=(a)[vi]*(s);} while(0)
#define Vector4Scale(a,s,b) do {for(int vi=0;vi<4;vi++)(b)[vi]=(a)[vi]*(s);} while(0)
#define VectorMA(a,s,b,c) do {for(int vi=0;vi<3;vi++)(c)[vi]=(a)[vi]+(s)*(b)[vi];} while(0)
static void Con_Printf(const char *fmt,...) {(void)fmt;}
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
    float matrices[24*12],scale[3]={1,1,1};short packed[24*7];
    lerpents_t le={0};
    for(int i=0;i<24;i++) {
        float pos[3]={i*.37f-4,i*.61f-8,i*.29f-2};
        float q[4]={sinf(i*.39f),cosf(i*.17f),sinf(i*.73f),cosf(i*.43f)};
        float n=sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
        for(int j=0;j<4;j++)q[j]/=n;
        GenMatrixPosQuat4Scale(pos,q,scale,matrices+i*12);
    }
    Bones_To_PosQuat4(24,matrices,packed);
    rag_lerpdeltaent(&le,24,packed,1,NULL);
    skelobject_t *s=skel_get(&csqc_world,le.skeletalobject);
    assert(s && s->ownedmatrix && s->type==SKEL_RELATIVE && s->numbones==24);
    for(int i=0;i<24*12;i++)
        assert(fabsf(s->bonematrix[i]-matrices[i])<((i%4==3)?.016f:.0004f));
    // Independent midpoint expectation: 0 -> 90 degrees around Z, with a
    // sign-flipped quaternion endpoint representing exactly the same rotation.
    short old[7]={-64,128,192,0,0,0,-32767};
    short next[7]={320,512,576,0,0,23170,23170};
    rag_lerpdeltaent(&le,1,next,.5f,old);
    s=skel_get(&csqc_world,le.skeletalobject);
    assert(s && s->numbones==1 && pendingkill);
    assert(fabsf(s->bonematrix[3]-2)<.001f && fabsf(s->bonematrix[7]-5)<.001f && fabsf(s->bonematrix[11]-6)<.001f);
    assert(fabsf(s->bonematrix[0]-.70710678f)<.001f && fabsf(s->bonematrix[4]-.70710678f)<.001f);
    skel_dodelete(&csqc_world);
    rag_lerpdeltaent(&le,1,NULL,1,NULL);
    assert(!le.skeletalobject && pendingkill);
    skel_dodelete(&csqc_world);skel_reset(&csqc_world);assert(!native_live);
    // A stale index after world teardown must not be dereferenced.
    le.skeletalobject=MAX_SKEL_OBJECTS;
    rag_lerpdeltaent(&le,24,packed,1,NULL);
    assert(skel_get(&csqc_world,le.skeletalobject));
    rag_removedeltaent(&le);skel_dodelete(&csqc_world);skel_reset(&csqc_world);
    // Exhaustion rejects allocation without scribbling over the table.
    for(int i=0;i<MAX_SKEL_OBJECTS;i++)assert(skel_create(&csqc_world,24));
    le.skeletalobject=0;rag_lerpdeltaent(&le,24,packed,1,NULL);assert(!le.skeletalobject);
    skel_reset(&csqc_world);assert(!native_live);
    // Keep the same VM ownership checks active for decoded network poses.
    int vm;csqc_world.progs=&vm;le.skeletalobject=0;
    rag_lerpdeltaent(&le,24,packed,1,NULL);s=skel_get(&csqc_world,le.skeletalobject);
    assert(s && !s->ownedmatrix && vm_count==1);
    delete_object(&csqc_world,s);skel_reset(&csqc_world);free_vm();
    puts("Skeletal networking: real pose encoder/decoder, quantization bounds, sign-safe interpolation, replacement and lifetime checks passed");
    return 0;
}
