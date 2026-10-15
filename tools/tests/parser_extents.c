/* Actual surface texture/lightmap bounds, with negative texture coordinates. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef struct { float position[3]; } mvertex_t;
typedef struct { unsigned int v[2]; } medge_t;
typedef struct { float vecs[2][4]; } mtexinfo_t;
typedef struct {
    int numedges, firstedge, lmshift, texturemins[2], extents[2];
    mtexinfo_t *texinfo;
} msurface_t;
typedef struct {
    int *surfedges;
    int numedges;
    medge_t *edges;
    mvertex_t *vertexes;
} model_t;
#define DotProduct_Double(a,b) ((double)(a)[0]*(b)[0]+(double)(a)[1]*(b)[1]+(double)(a)[2]*(b)[2])
/* ENGINE_FUNCTIONS */
int main(void)
{
    mvertex_t verts[4] = {{{-103.5f, -47.25f, 0}}, {{79.75f, -47.25f, 0}},
                          {{79.75f, 33.5f, 0}}, {{-103.5f, 33.5f, 0}}};
    medge_t edges[4] = {{{0,1}}, {{1,2}}, {{2,3}}, {{3,0}}};
    int surfedges[4] = {0,1,2,3};
    model_t model = {surfedges, 4, edges, verts};
    mtexinfo_t tex = {{{1,0,0,0}, {0,1,0,0}}};
    msurface_t surface = {4,0,0,{0,0},{0,0},&tex};
    for (int shift = 0; shift <= 8; ++shift)
    for (int offset = -300; offset <= 300; ++offset)
    {
        float minimum[2] = {-103.5f + offset, -47.25f - offset};
        float maximum[2] = {79.75f + offset, 33.5f - offset};
        int scale = 1 << shift;
        surface.lmshift = shift;
        tex.vecs[0][3] = offset;
        tex.vecs[1][3] = -offset;
        CalcSurfaceExtents(&model, &surface);
        for (int axis = 0; axis < 2; ++axis)
        {
            int low = (int)floor(minimum[axis] / scale) * scale;
            int high = (int)ceil(maximum[axis] / scale) * scale;
            assert(surface.texturemins[axis] == low);
            assert(surface.extents[axis] == high - low);
            assert(low <= minimum[axis] && high >= maximum[axis]);
            assert(low % scale == 0 && surface.extents[axis] % scale == 0);
        }
    }
    puts("Surface extents: 5,409 negative/positive texture origins preserve lightmap grid bounds");
    return 0;
}
