/* Data-free regressions for conservative GPU light/shadow culling. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "engine/sdlgpu/gpu_cull.h"
#include "engine/sdlgpu/gpu_cluster.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
static uint32_t seed = 0x12345678;
static float random_unit(void)
{
    seed = seed*1664525u+1013904223u;
    return (seed >> 8)*(1.0f/16777216.0f);
}

int main(void)
{
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const float origin[3] = {0,0,0};
    float mins[3] = {-0.5f,-0.5f,0.1f}, maxs[3] = {0.5f,0.5f,0.9f};
    unsigned i, axis, samples = 0;
    CHECK(GPU_ShadowBoundsVisible(mins,maxs,origin,10,identity));
    for (axis = 0; axis < 3; ++axis) {
        float oldmin = mins[axis], oldmax = maxs[axis];
        mins[axis] = 2; maxs[axis] = 3;
        CHECK(!GPU_ShadowBoundsVisible(mins,maxs,origin,10,identity));
        mins[axis] = -3; maxs[axis] = -2;
        CHECK(!GPU_ShadowBoundsVisible(mins,maxs,origin,10,identity));
        mins[axis] = oldmin; maxs[axis] = oldmax;
    }
    mins[2] = -0.5f; /* Near-plane crossing and camera enclosure must survive. */
    CHECK(GPU_ShadowBoundsVisible(mins,maxs,origin,10,identity));
    mins[0] = 0.25f; maxs[0] = 0.5f; mins[1] = maxs[1] = mins[2] = maxs[2] = 0;
    CHECK(GPU_ShadowBoundsVisible(mins,maxs,origin,0.25f,identity));
    CHECK(!GPU_ShadowBoundsVisible(mins,maxs,origin,0.24f,identity));
    CHECK(GPU_ClusterSlice(0) == 0);
    CHECK(GPU_ClusterSlice(GPU_CLUSTER_FAR*2) == GPU_CLUSTER_Z-1);
    /* Every sampled illuminated point must fall inside its light's assigned
     * cell range, including off-centre/flipped projections and near crossings. */
    for (i = 0; i < 20000; ++i) {
        float projection[16] = {0}, center[3], radius, point[3], depth, grid[2];
        int lo[3], hi[3], cell[3];
        projection[0] = 0.3f+random_unit()*2;
        projection[5] = (i & 1 ? -1 : 1)*(0.3f+random_unit()*2);
        projection[8] = random_unit()-0.5f; projection[9] = random_unit()-0.5f;
        projection[10] = -1.0001f; projection[11] = -1; projection[14] = -0.1f;
        center[0] = (random_unit()-0.5f)*4000;
        center[1] = (random_unit()-0.5f)*4000;
        center[2] = -powf(2,random_unit()*17);
        radius = powf(2,random_unit()*12);
        GPU_ClusterRange(center,radius,projection,lo,hi);
        for (axis = 0; axis < 3; ++axis) point[axis] = center[axis]+(random_unit()*2-1)*radius*0.57735f;
        depth = -point[2];
        if (depth < GPU_CLUSTER_NEAR || depth >= GPU_CLUSTER_FAR) continue;
        grid[0] = ((projection[0]*point[0]+projection[8]*point[2])/depth*0.5f+0.5f)*GPU_CLUSTER_X;
        grid[1] = ((projection[5]*point[1]+projection[9]*point[2])/depth*0.5f+0.5f)*GPU_CLUSTER_Y;
        if (grid[0] < 0 || grid[0] >= GPU_CLUSTER_X || grid[1] < 0 || grid[1] >= GPU_CLUSTER_Y) continue;
        cell[0] = (int)grid[0]; cell[1] = (int)grid[1]; cell[2] = GPU_ClusterSlice(depth);
        for (axis = 0; axis < 3; ++axis) CHECK(cell[axis] >= lo[axis] && cell[axis] <= hi[axis]);
        ++samples;
    }
    CHECK(samples > 1000);
    printf("Shadow planes/sphere boundaries and %u conservative cluster samples passed.\n",samples);
    return 0;
}
