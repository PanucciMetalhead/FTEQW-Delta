/* Actual original MDL pose storage, including odd vertex counts and frame groups. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef unsigned char qbyte;
typedef uintptr_t quintptr_t;
typedef int64_t qint64_t;
#define FTE_ALIGN(n) __attribute__((aligned(n)))
/* ENGINE_VECTOR_TYPES */
/* ENGINE_VBO_TYPE */
/* ENGINE_POSE_TYPE */
typedef struct {int memgroup;} model_t;
static size_t bytes;
static void *ZG_Malloc(int *group, size_t size) { (void)group; bytes=size; void *p=calloc(1,size); assert(p); return p; }
/* ENGINE_FUNCTIONS */
int main(void)
{
    model_t model = {0};
    for (int count = 0; count <= 64; ++count)
    for (int frames = 0; frames <= 20; ++frames)
    {
        galiaspose_t *poses = Q1MDL_AllocPoses(&model, frames, count);
        for (int f = 0; f < frames; ++f)
        {
            assert((uintptr_t)poses[f].ofsverts % _Alignof(vecV_t) == 0);
            assert((qbyte *)poses[f].ofsverts >= (qbyte *)(poses+frames));
            qbyte *end = (qbyte *)(poses[f].ofsverts+count);
#ifndef SERVERONLY
            assert((qbyte *)poses[f].ofsnormals == end);
            assert(poses[f].ofssvector == poses[f].ofsnormals+count);
            assert(poses[f].ofstvector == poses[f].ofssvector+count);
            end = (qbyte *)(poses[f].ofstvector+count);
#endif
            assert(end <= (qbyte *)poses+bytes);
            if (f+1 < frames) assert(end <= (qbyte *)poses[f+1].ofsverts);
            for (int i = 0; i < count; ++i)
            {
                for (int axis = 0; axis < 4; ++axis) poses[f].ofsverts[i][axis] = f*1000+i+axis;
#ifndef SERVERONLY
                for (int axis = 0; axis < 3; ++axis) {
                    poses[f].ofsnormals[i][axis] = f;
                    poses[f].ofssvector[i][axis] = 1;
                    poses[f].ofstvector[i][axis] = 2;
                }
#endif
            }
        }
        for (int f = 0; f < frames; ++f)
        for (int i = 0; i < count; ++i) {
            assert(poses[f].ofsverts[i][0] == f*1000+i && poses[f].ofsverts[i][3] == f*1000+i+3);
#ifndef SERVERONLY
            assert(poses[f].ofsnormals[i][2] == f && poses[f].ofssvector[i][2] == 1 && poses[f].ofstvector[i][2] == 2);
#endif
        }
        free(poses);
    }
    puts("MDL pose storage: empty/odd/even vertex counts and 0–20 poses preserve aligned independent position/normal arrays");
    return 0;
}
