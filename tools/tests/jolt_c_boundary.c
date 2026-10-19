/* Compile and call the production boundary as ISO C17, linked to C++ Jolt. */
#include "plugins/jolt/jolt_api.h"
#include <stdio.h>
int main(void)
{
    fte_jolt_world *world=fte_jolt_create(4,120,8);
    fte_jolt_desc body={0};
    body.shape=FTE_JOLT_BOX;body.motion=FTE_JOLT_DYNAMIC;
    body.size[0]=body.size[1]=body.size[2]=32;body.mass=1;
    body.gravity_scale=1;body.state.position[2]=1024;body.state.rotation[3]=1;
    body.solid_mask=body.hit_mask=1;
    fte_jolt_body handle=fte_jolt_add(world,&body);
    fte_jolt_state state;
    int passed=world && handle && fte_jolt_step(world,1.0/60,800) && fte_jolt_get(world,handle,&state) && state.position[2]<1024;
    if (passed) {
        fte_jolt_joint_desc settings={0};
        settings.type=FTE_JOLT_FIXED;settings.body2=handle;
        fte_jolt_joint joint=fte_jolt_joint_add(world,&settings);
        passed=joint && fte_jolt_joint_enable(world,joint,0) && fte_jolt_joint_remove(world,joint);
    }
    if (passed) {
        body.motion=FTE_JOLT_KINEMATIC;
        fte_jolt_body platform=fte_jolt_add(world,&body);
        state=body.state;state.position[0]=16;
        passed=platform && fte_jolt_move(world,platform,&state) &&
            fte_jolt_step(world,1.0/60,0) && fte_jolt_get(world,platform,&state) &&
            state.position[0]>15.99f && state.position[0]<16.01f;
    }
    fte_jolt_destroy(world);
    if (!passed) return 1;
    puts("JOLT_C17_BOUNDARY_OK");
    return 0;
}
