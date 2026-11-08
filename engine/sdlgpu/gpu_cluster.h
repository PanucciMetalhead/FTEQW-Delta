/* Frustum cluster bounds shared by the renderer and regression tests. */
#ifndef FTE_GPU_CLUSTER_H
#define FTE_GPU_CLUSTER_H
#include <math.h>
#define GPU_CLUSTER_X 16
#define GPU_CLUSTER_Y 9
#define GPU_CLUSTER_Z 24
#define GPU_CLUSTER_COUNT (GPU_CLUSTER_X*GPU_CLUSTER_Y*GPU_CLUSTER_Z)
#define GPU_CLUSTER_LIMIT 128
#define GPU_CLUSTER_NEAR 1.0f
#define GPU_CLUSTER_FAR 65536.0f

static inline int GPU_ClusterSlice(float depth)
{
    int z;
    if (depth <= GPU_CLUSTER_NEAR) return 0;
    if (depth >= GPU_CLUSTER_FAR) return GPU_CLUSTER_Z-1;
    z = (int)(logf(depth/GPU_CLUSTER_NEAR)*(GPU_CLUSTER_Z/logf(GPU_CLUSTER_FAR/GPU_CLUSTER_NEAR)));
    return z < 0 ? 0 : z >= GPU_CLUSTER_Z ? GPU_CLUSTER_Z-1 : z;
}

/* Project an enclosing view-space cube, conservatively expanding by a tile
 * at boundaries. Near-plane intersections cover every tile; fragments outside
 * the grid use the full light list, so finite grid bounds never drop a light. */
static inline void GPU_ClusterRange(const float center[3], float radius,
    const float projection[16], int lo[3], int hi[3])
{
    float minimum[2] = {1,1}, maximum[2] = {-1,-1};
    unsigned corner, axis;
    lo[2] = GPU_ClusterSlice(-center[2]-radius);
    hi[2] = GPU_ClusterSlice(-center[2]+radius);
    if (-center[2]-radius <= GPU_CLUSTER_NEAR) {
        lo[0] = lo[1] = 0; hi[0] = GPU_CLUSTER_X-1; hi[1] = GPU_CLUSTER_Y-1; return;
    }
    for (corner = 0; corner < 8; ++corner) {
        float p[3], w;
        for (axis = 0; axis < 3; ++axis) p[axis] = center[axis]+(corner & (1u<<axis) ? radius : -radius);
        w = projection[3]*p[0]+projection[7]*p[1]+projection[11]*p[2]+projection[15];
        if (w <= 0) {
            lo[0] = lo[1] = 0; hi[0] = GPU_CLUSTER_X-1; hi[1] = GPU_CLUSTER_Y-1; return;
        }
        for (axis = 0; axis < 2; ++axis) {
            float ndc = (projection[axis]*p[0]+projection[4+axis]*p[1]+projection[8+axis]*p[2]+projection[12+axis])/w;
            if (ndc < minimum[axis]) minimum[axis] = ndc;
            if (ndc > maximum[axis]) maximum[axis] = ndc;
        }
    }
    for (axis = 0; axis < 2; ++axis) {
        int size = axis ? GPU_CLUSTER_Y : GPU_CLUSTER_X;
        lo[axis] = (int)floorf((minimum[axis]*0.5f+0.5f)*size)-1;
        hi[axis] = (int)floorf((maximum[axis]*0.5f+0.5f)*size)+1;
        if (lo[axis] < 0) lo[axis] = 0;
        if (hi[axis] >= size) hi[axis] = size-1;
    }
}
#endif
