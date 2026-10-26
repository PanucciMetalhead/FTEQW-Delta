/* Conservative shadow bounds tests. GPL-2.0-or-later. */
#ifndef FTE_GPU_CULL_H
#define FTE_GPU_CULL_H

/* Column-major clip matrix, SDL's 0..w depth range. A positive support
 * vertex outside a plane proves the entire AABB is outside that plane.
 * Boxes crossing the near plane or containing the camera remain visible. */
static inline int GPU_ShadowBoundsVisible(const float mins[3], const float maxs[3],
    const float origin[3], float radius, const float matrix[16])
{
    float distance = 0;
    unsigned i, axis;
    for (i = 0; i < 3; ++i) {
        float delta = origin[i] < mins[i] ? mins[i]-origin[i] :
            origin[i] > maxs[i] ? origin[i]-maxs[i] : 0;
        distance += delta*delta;
    }
    if (distance > radius*radius) return 0;
    for (axis = 0; axis < 6; ++axis) {
        float plane[4], support;
        for (i = 0; i < 4; ++i) {
            if (axis == 4) plane[i] = matrix[i*4+2];
            else if (axis == 5) plane[i] = matrix[i*4+3]-matrix[i*4+2];
            else plane[i] = matrix[i*4+3]+(axis & 1 ? -1 : 1)*matrix[i*4+axis/2];
        }
        support = plane[3];
        for (i = 0; i < 3; ++i) support += plane[i]*(plane[i] >= 0 ? maxs[i] : mins[i]);
        if (support < -0.0001f) return 0;
    }
    return 1;
}
#endif
