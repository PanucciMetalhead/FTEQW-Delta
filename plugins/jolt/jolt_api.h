/* FTEQW Delta rigid-body adapter. GPL-2.0-or-later; Jolt remains MIT licensed. */
#ifndef FTE_JOLT_API_H
#define FTE_JOLT_API_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Main-thread API. Quake coordinates are Z-up, 32 units/metre. Angular velocity
 * is radians/second in world XYZ; orientation is a normalized XYZW quaternion.
 * No engine structures, allocators or C++ objects cross this boundary. */
typedef struct fte_jolt_world fte_jolt_world;
typedef uint64_t fte_jolt_body;
typedef uint64_t fte_jolt_joint;
enum { FTE_JOLT_FIXED=-1, FTE_JOLT_POINT=1, FTE_JOLT_HINGE=2, FTE_JOLT_SLIDER=3, FTE_JOLT_UNIVERSAL=4, FTE_JOLT_HINGE2=5 };
enum { FTE_JOLT_POINT_SHARED=0, FTE_JOLT_POINT_WORLD=1, FTE_JOLT_POINT_LEGACY=2 };
/* A zero body handle anchors that side to the world. Limits contain zero;
 * hinge limits/velocity use radians, slider limits/velocity use Quake units.
 * Connected bodies do not collide, even while their constraint is disabled. */
typedef struct {
    int type;
    fte_jolt_body body1,body2;
    float anchor[3],axis[3];
    float lower,upper,max_force,velocity,stiffness,damping;
    /* Two-axis joints: axis is attached to body1; axis2 to body2. They must
     * initially be orthogonal unit vectors. Both angular ranges contain zero.
     * Hinge2 may add linear suspension along axis (Quake force/distance units).
     * Zero suspension stiffness/damping keeps the anchor rigid. */
    float axis2[3],lower2,upper2,max_force2,velocity2,stiffness2,damping2;
    float suspension_stiffness,suspension_damping;
    /* Point only: SHARED uses anchor for both world-space points (default).
     * WORLD supplies two independent world-space points. LEGACY matches FTE's
     * ODE SetBallAnchor/SetBallAnchor2 calls: anchor2 is body2-local COM offset
     * when both bodies exist, otherwise world-space on the world side. With
     * body1=world, anchor remains the point on body2 (ODE's reversed ordering).
     * Explicit points must remain within +/-10000000 Quake units. */
    int point_mode;
    float anchor2[3];
} fte_jolt_joint_desc;
enum { FTE_JOLT_STATIC, FTE_JOLT_KINEMATIC, FTE_JOLT_DYNAMIC };
enum {
    FTE_JOLT_BOX=1, FTE_JOLT_SPHERE, FTE_JOLT_CAPSULE_X,
    FTE_JOLT_CAPSULE_Y, FTE_JOLT_CAPSULE_Z, FTE_JOLT_CYLINDER_X,
    FTE_JOLT_CYLINDER_Y, FTE_JOLT_CYLINDER_Z, FTE_JOLT_MESH,
    FTE_JOLT_CONVEX, FTE_JOLT_COMPOUND
};
typedef struct {
    float position[3], rotation[4], velocity[3], spin[3];
} fte_jolt_state;
typedef struct {
    int shape, motion;
    float size[3], mass, friction, restitution, gravity_scale;
    float linear_damping, angular_damping;
    unsigned entity, owner, solid_mask, hit_mask, collision_family;
    int is_proxy;
    fte_jolt_state state;
    const float *vertices;
    const int *indices;
    size_t vertex_count, triangle_count;
    /* Convex uses vertices (4..4096, not triangle topology). Compound uses a
     * bounded authored ftejolt 1 text recipe, copied while add() runs. Coordinates
     * are local Quake units, with scale 0 meaning 1 for zero-initialized callers. */
    const char *compound_text;
    size_t compound_length;
    float shape_scale;
} fte_jolt_desc;
typedef struct {
    fte_jolt_body body1, body2;
    unsigned entity1, entity2;
    float position[3], normal[3]; /* normal pushes body2 away from body1 */
} fte_jolt_contact;
typedef struct {
    unsigned bodies, active, steps, contact_overflows, joints, collision_steps, animation_motors;
    double simulated_time, dropped_time;
} fte_jolt_stats;
fte_jolt_world *fte_jolt_create(unsigned max_bodies, unsigned hz, unsigned max_steps);
void fte_jolt_destroy(fte_jolt_world *world);
const char *fte_jolt_error(const fte_jolt_world *world);
const char *fte_jolt_version(void);
fte_jolt_body fte_jolt_add(fte_jolt_world *world, const fte_jolt_desc *desc);
fte_jolt_joint fte_jolt_joint_add(fte_jolt_world *world,const fte_jolt_joint_desc *desc);
/* Update only suspension, preserving the original anchors and angular frame.
 * Valid only for hinge2. Rejected settings leave the existing joint unchanged. */
int fte_jolt_joint_suspension(fte_jolt_world *world,fte_jolt_joint joint,float stiffness,float damping);
int fte_jolt_joint_remove(fte_jolt_world *world,fte_jolt_joint joint);
int fte_jolt_joint_enable(fte_jolt_world *world,fte_jolt_joint joint,int enabled);
/* Read current world-space points in caller body1/body2 order. */
int fte_jolt_joint_anchors(fte_jolt_world *world,fte_jolt_joint joint,float point1[3],float point2[3]);
int fte_jolt_joint_frame(fte_jolt_world *world,fte_jolt_joint joint,fte_jolt_state *frame);
int fte_jolt_remove(fte_jolt_world *world, fte_jolt_body body);
int fte_jolt_get(fte_jolt_world *world, fte_jolt_body body, fte_jolt_state *state);
int fte_jolt_set(fte_jolt_world *world, fte_jolt_body body, const fte_jolt_state *state);
/* Queue an animation pose for a kinematic or fractionally driven dynamic body.
 * The next advancing step interpolates to the latest target across its fixed
 * ticks; partial steps retain the target. Kinematic velocities are derived from
 * poses and cleared after arrival; fractional bodies retain physical velocity.
 * fte_jolt_set is an explicit teleport and cancels a pending target. */
int fte_jolt_move(fte_jolt_world *world, fte_jolt_body body, const fte_jolt_state *target);
/* Animation strength in [0,1]: 0 passive, 1 exact kinematic animation;
 * intermediate values use dynamic position/orientation motors with finite force.
 * Retains body/joint identity and pose. A fractional release retains actual
 * physical velocity. Targets are submitted with move(), held between updates,
 * and limited to +/-10000000 Quake units for stable world-space motors. */
int fte_jolt_animate(fte_jolt_world *, fte_jolt_body, float strength);
/* Switch dynamic/kinematic convex bodies in place (also disables any motor).
 * Kinematic release inherits the last completed target's velocity; an advancing
 * idle tick clears that velocity. Dynamic release retains physical velocity.
 * Static bodies and triangle meshes cannot become dynamic. */
int fte_jolt_motion(fte_jolt_world *, fte_jolt_body, int motion);
int fte_jolt_material(fte_jolt_world *world, fte_jolt_body body,
    float friction, float restitution, float gravity_scale);
int fte_jolt_filter(fte_jolt_world *world, fte_jolt_body body,
    unsigned owner, unsigned solid_mask, unsigned hit_mask);
int fte_jolt_enable(fte_jolt_world *world, fte_jolt_body body, int enabled);
/* Forces/torques persist across the substeps of the next advancing step call.
 * If no fixed step elapses they wait for the next advancing call. */
int fte_jolt_force(fte_jolt_world *world, fte_jolt_body body,
    const float force[3], const float point[3]);
int fte_jolt_torque(fte_jolt_world *world, fte_jolt_body body, const float torque[3]);
int fte_jolt_impulse(fte_jolt_world *world, fte_jolt_body body, const float impulse[3]);
int fte_jolt_step(fte_jolt_world *world, double dt, double gravity);
void fte_jolt_get_stats(const fte_jolt_world *world, fte_jolt_stats *stats);
size_t fte_jolt_contacts(const fte_jolt_world *world);
int fte_jolt_get_contact(const fte_jolt_world *world, size_t index, fte_jolt_contact *contact);
int fte_jolt_quat(const float forward[3], const float left[3], const float up[3], float quat[4]);
void fte_jolt_axes(const float quat[4], float forward[3], float left[3], float up[3]);
#ifdef __cplusplus
}
#endif
#endif
