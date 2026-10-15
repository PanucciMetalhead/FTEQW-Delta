/* Actual brush mesh allocation, actual vector and mesh layouts. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef unsigned char qbyte;
typedef uintptr_t quintptr_t;
typedef int qboolean;
#ifdef TEST_WIDE_INDEX
typedef unsigned int index_t;
#else
typedef unsigned short index_t;
#endif
typedef unsigned char bone_vec4_t[4];
#define MAXRLIGHTMAPS 4
#define FTE_ALIGN(n) __attribute__((aligned(n)))
/* ENGINE_VECTOR_TYPES */
/* ENGINE_MESH_TYPE */
typedef struct { int memgroup; } model_t;
static void *allocation;
static size_t allocation_size;
static void *ZG_Malloc(int *group, size_t size)
{
    (void)group;
    allocation_size = size;
    allocation = calloc(1, size);
    assert(allocation);
    return allocation;
}
/* ENGINE_FUNCTIONS */
int main(void)
{
    model_t mod = {0};
    for (int count = 3; count <= 256; ++count)
    {
        mesh_t *mesh = ModQ1_AllocSurfaceMesh(&mod, count);
        assert(mesh->numvertexes == count && mesh->numindexes == (count - 2) * 3);
        assert((uintptr_t)mesh->xyz_array % _Alignof(vecV_t) == 0);
        assert((uintptr_t)mesh->colors4f_array[0] % _Alignof(avec4_t) == 0);
        assert((qbyte *)mesh->st_array >= (qbyte *)(mesh->xyz_array + count));
        assert((qbyte *)mesh->colors4f_array[0] >= (qbyte *)(mesh->tnormals_array + count));
        assert((qbyte *)(mesh->indexes + mesh->numindexes) <= (qbyte *)allocation + allocation_size);
        for (int i = 0; i < count; ++i)
        {
            for (int axis = 0; axis < 4; ++axis)
                mesh->xyz_array[i][axis] = i + axis;
            for (int axis = 0; axis < 2; ++axis)
            {
                mesh->st_array[i][axis] = -i - axis;
                mesh->lmst_array[0][i][axis] = i - axis;
            }
            for (int axis = 0; axis < 3; ++axis)
            {
                mesh->normals_array[i][axis] = 1;
                mesh->snormals_array[i][axis] = 2;
                mesh->tnormals_array[i][axis] = 3;
            }
            for (int axis = 0; axis < 4; ++axis)
                mesh->colors4f_array[0][i][axis] = 4;
        }
        for (int i = 0; i < mesh->numindexes; ++i)
            mesh->indexes[i] = i % count;
        for (int i = 0; i < count; ++i)
        {
            assert(mesh->xyz_array[i][0] == i && mesh->xyz_array[i][3] == i + 3);
            assert(mesh->st_array[i][1] == -i - 1 && mesh->lmst_array[0][i][1] == i - 1);
            assert(mesh->normals_array[i][2] == 1 && mesh->snormals_array[i][2] == 2);
            assert(mesh->tnormals_array[i][2] == 3 && mesh->colors4f_array[0][i][3] == 4);
        }
        free(allocation);
    }
    puts("Brush mesh storage: 3–256 vertices, aligned positions/colors and independent arrays passed");
    return 0;
}
