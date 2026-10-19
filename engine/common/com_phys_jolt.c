/* FTEQW Delta, GPL-2.0-or-later. Experimental rigid-body Jolt plugin.
 * Classic movement/physics run in the engine; only MOVETYPE_PHYSICS is read back.
 * Jolt callbacks record contacts; QuakeC callbacks run after simulation. */
#include "../../plugins/plugin.h"
#include "../../plugins/engine.h"
#include "pr_common.h"
#include "../../plugins/jolt/jolt_api.h"

#ifdef USERBE
static rbeplugfuncs_t *rbeapi;
static cvar_t *enabled, *mesh_pitch;
static qboolean jolt_registered;
typedef struct jolt_rag_body_s {
    struct jolt_rag_body_s *next;
    world_t *world;
    wedict_t *entity;
    fte_jolt_body handle;
} jolt_rag_body_t;
static void joint_free(rbejoint_t *slot);
typedef struct jolt_entity_s {
    fte_jolt_body handle;
    vec3_t center, mins, maxs;
    float mass, scale, damping_linear, damping_angular;
    int shape, motion, modelindex;
    qboolean warned;
} jolt_entity_t;
typedef struct jolt_command_s {
    struct jolt_command_s *next;
    enum rbecommands_e command;
    wedict_t *edict;
    vec3_t v1,v2;
} jolt_command_t;
typedef struct jolt_context_s {
    rigidbodyengine_t pub; /* Must remain first: world->rbe is the context. */
    fte_jolt_world *simulation;
    qboolean failed, warned_dropped;
    jolt_rag_body_t *rag_bodies;
    jolt_command_t *commands, *tail;
    unsigned pending_commands;
} jolt_context_t;
static jolt_context_t *context(world_t *world) {return (jolt_context_t*)world->rbe;}
static qboolean equal3(const float *a,const float *b) {return a[0]==b[0] && a[1]==b[1] && a[2]==b[2];}
static unsigned dimension_mask(float mask) {
    double bits;
    if (!isfinite(mask)) return 0;
    bits=fmod(trunc((double)mask),4294967296.0);
    if (bits<0) bits+=4294967296.0;
    return (unsigned)bits;
}
static float mesh_sign(world_t *w,wedict_t *ed) {
    model_t *model=ed->v->modelindex?w->Get_CModel(w,(int)ed->v->modelindex):NULL;
    return model && model->type==mod_alias && mesh_pitch->value<0 ? -1 : 1;
}
static void remember(wedict_t *ed) {
    VectorCopy(ed->v->origin,ed->rbe.origin);VectorCopy(ed->v->angles,ed->rbe.angles);
    VectorCopy(ed->v->velocity,ed->rbe.velocity);VectorCopy(ed->v->avelocity,ed->rbe.avelocity);
}
static int geometry(wedict_t *ed) {
    int shape=(int)ed->xv->geomtype;
    if (!shape) switch ((int)ed->v->solid) {
    case SOLID_NOT:case SOLID_TRIGGER:case SOLID_BSPTRIGGER: return GEOMTYPE_NONE;
    case SOLID_BSP:case SOLID_PHYSICS_TRIMESH:return GEOMTYPE_TRIMESH;
    case SOLID_PHYSICS_SPHERE:return GEOMTYPE_SPHERE;
    case SOLID_PHYSICS_CAPSULE:return GEOMTYPE_CAPSULE;
    case SOLID_PHYSICS_CYLINDER:return GEOMTYPE_CYLINDER;
    default:return GEOMTYPE_BOX;
    }
    return shape;
}
static int adapter_shape(int shape,const vec3_t size) {
    int axis=0;
    if (size[1]>size[axis]) axis=1;
    if (size[2]>size[axis]) axis=2;
    switch (shape) {
    case GEOMTYPE_BOX:return FTE_JOLT_BOX;
    case GEOMTYPE_SPHERE:return FTE_JOLT_SPHERE;
    case GEOMTYPE_TRIMESH:return FTE_JOLT_MESH;
    case GEOMTYPE_CAPSULE:return FTE_JOLT_CAPSULE_X+axis;
    case GEOMTYPE_CYLINDER:return FTE_JOLT_CYLINDER_X+axis;
    case GEOMTYPE_CAPSULE_X:case GEOMTYPE_CAPSULE_Y:case GEOMTYPE_CAPSULE_Z:return FTE_JOLT_CAPSULE_X+shape-GEOMTYPE_CAPSULE_X;
    case GEOMTYPE_CYLINDER_X:case GEOMTYPE_CYLINDER_Y:case GEOMTYPE_CYLINDER_Z:return FTE_JOLT_CYLINDER_X+shape-GEOMTYPE_CYLINDER_X;
    default:return 0;
    }
}
static void QDECL remove_joint(world_t *world,wedict_t *ed) {
    (void)world;joint_free(&ed->rbe.joint);ed->rbe.joint_type=0;
}
static void destroy_geometry(world_t *world,wedict_t *ed) {
    jolt_entity_t *body=(jolt_entity_t*)ed->rbe.body.body;
    if (body) {if (body->handle) fte_jolt_remove(context(world)->simulation,body->handle);free(body);}
    ed->rbe.body.body=NULL;ed->rbe.body.geom=NULL;ed->rbe.physics=qfalse;
    rbeapi->ReleaseCollisionMesh(ed);
}
/* The engine calls RemoveFromEntity before every SSQC/CSQC edict free.
 * Purge even commands queued before the first body exists; a recycled edict
 * must never receive the previous occupant's forces. Shape rebuilds retain
 * pending commands and use destroy_geometry directly. */
static void QDECL remove_body(world_t *world,wedict_t *ed) {
    jolt_context_t *ctx=context(world);
    jolt_command_t **link=&ctx->commands;
    ctx->tail=NULL;
    while (*link) {
        jolt_command_t *cmd=*link;
        if (cmd->edict==ed) {*link=cmd->next;free(cmd);--ctx->pending_commands;}
        else {ctx->tail=cmd;link=&cmd->next;}
    }
    {
        jolt_rag_body_t *body;
        for(body=ctx->rag_bodies;body;body=body->next) if(body->entity==ed) {
            if(body->handle)fte_jolt_remove(ctx->simulation,body->handle);
            body->handle=0;body->entity=NULL;
        }
    }
    destroy_geometry(world,ed);
}
static void QDECL end_world(world_t *world) {
    jolt_context_t *ctx=context(world);
    while (ctx->commands) {jolt_command_t *cmd=ctx->commands;ctx->commands=cmd->next;free(cmd);}
    fte_jolt_destroy(ctx->simulation);world->rbe=NULL;free(ctx);
}
static qboolean make_state(world_t *world,wedict_t *ed,jolt_entity_t *body,fte_jolt_state *state) {
    vec3_t angles,forward,right,left,up;
    int i;float sign=mesh_sign(world,ed);
    memset(state,0,sizeof(*state));VectorCopy(ed->v->angles,angles);angles[PITCH]*=sign;
    if ((int)ed->v->solid==SOLID_BBOX || (int)ed->v->solid==SOLID_SLIDEBOX || (int)ed->v->solid==SOLID_CORPSE)
        VectorClear(angles);
    rbeapi->AngleVectors(angles,forward,right,up);VectorNegate(right,left);
    if (!fte_jolt_quat(forward,left,up,state->rotation)) return qfalse;
    for (i=0;i<3;i++) state->position[i]=ed->v->origin[i]+forward[i]*body->center[0]+left[i]*body->center[1]+up[i]*body->center[2];
    if (body->motion==FTE_JOLT_DYNAMIC) {
        VectorCopy(ed->v->velocity,state->velocity);
        state->spin[0]=ed->v->avelocity[PITCH]*sign*(M_PI/180.0);
        state->spin[1]=ed->v->avelocity[ROLL]*(M_PI/180.0);
        state->spin[2]=ed->v->avelocity[YAW]*(M_PI/180.0);
    }
    return qtrue;
}
static void sync_entity(world_t *world,wedict_t *ed) {
    jolt_context_t *ctx=context(world);
    jolt_entity_t *body=(jolt_entity_t*)ed->rbe.body.body;
    int shape=geometry(ed),motion=(int)ed->v->movetype==MOVETYPE_PHYSICS?FTE_JOLT_DYNAMIC:
        ed->entnum && ((int)ed->v->movetype==MOVETYPE_PUSH || (int)ed->v->movetype==MOVETYPE_H2PUSHPULL)?FTE_JOLT_KINEMATIC:FTE_JOLT_STATIC;
    int modelindex=shape==GEOMTYPE_TRIMESH?(int)ed->v->modelindex:0;
    float mass=ed->xv->mass>0?ed->xv->mass:1,scale=ed->xv->scale?ed->xv->scale:1;
    vec3_t mins,maxs,size;model_t *model=NULL;
    float gravity=ed->xv->gravity?ed->xv->gravity:1;
    float friction=ed->xv->friction>0?ed->xv->friction:1;
    float restitution=bound(0,ed->xv->bouncefactor,1);
    float linear=fmaxf(0,ed->xv->damp_linear),angular=fmaxf(0,ed->xv->damp_angular);
    fte_jolt_state state;
    qboolean changed;
    if (shape==GEOMTYPE_NONE) {if (body) destroy_geometry(world,ed);return;}
    if (shape==GEOMTYPE_TRIMESH) {
        model=world->Get_CModel(world,modelindex);
        if (!model || !model->surfaces) {if (body) destroy_geometry(world,ed);return;}
        VectorScale(model->mins,scale,mins);VectorScale(model->maxs,scale,maxs);
    } else {VectorCopy(ed->v->mins,mins);VectorCopy(ed->v->maxs,maxs);}
    VectorSubtract(maxs,mins,size);
    if (shape!=GEOMTYPE_TRIMESH && (size[0]<=0 || size[1]<=0 || size[2]<=0)) {if (body) destroy_geometry(world,ed);return;}
    changed=!body || body->shape!=shape || body->motion!=motion || body->modelindex!=modelindex ||
        body->mass!=mass || body->scale!=scale || body->damping_linear!=linear || body->damping_angular!=angular ||
        !equal3(body->mins,mins) || !equal3(body->maxs,maxs);
    if (changed) {
        fte_jolt_desc desc;int i;
        destroy_geometry(world,ed);body=(jolt_entity_t*)calloc(1,sizeof(*body));
        if (!body) {ctx->failed=qtrue;return;}
        ed->rbe.body.body=body;
        body->shape=shape;body->motion=motion;body->modelindex=modelindex;body->mass=mass;body->scale=scale;
        body->damping_linear=linear;body->damping_angular=angular;
        VectorCopy(mins,body->mins);VectorCopy(maxs,body->maxs);
        for (i=0;i<3;i++) body->center[i]=(mins[i]+maxs[i])*.5f;
        memset(&desc,0,sizeof(desc));
        desc.shape=adapter_shape(shape,size);desc.motion=motion;desc.mass=mass;desc.friction=friction;
        desc.restitution=restitution;desc.gravity_scale=gravity;desc.linear_damping=linear;desc.angular_damping=angular;
        desc.entity=ed->entnum;desc.collision_family=(unsigned)ed->entnum+1;desc.is_proxy=1;desc.owner=ed->v->owner;desc.solid_mask=dimension_mask(ed->xv->dimension_solid);desc.hit_mask=dimension_mask(ed->xv->dimension_hit);
        VectorCopy(size,desc.size);
        if (make_state(world,ed,body,&desc.state)) {
            qboolean mesh_ok=qtrue;
            if (shape==GEOMTYPE_TRIMESH) {
                vec3_t zero={0,0,0};
                mesh_ok=rbeapi->GenerateCollisionMesh(world,model,ed,zero);
                if (mesh_ok) {
                    for (i=0;i<ed->rbe.numvertices;i++) {
                        int axis;for (axis=0;axis<3;axis++) ed->rbe.vertex3f[3*i+axis]=ed->rbe.vertex3f[3*i+axis]*scale-body->center[axis];
                    }
                    desc.vertices=ed->rbe.vertex3f;desc.indices=ed->rbe.element3i;desc.vertex_count=ed->rbe.numvertices;desc.triangle_count=ed->rbe.numtriangles;
                }
            }
            if (mesh_ok) body->handle=fte_jolt_add(ctx->simulation,&desc);
            rbeapi->ReleaseCollisionMesh(ed);
        }
        if (!body->handle) {
            Con_Printf("Jolt entity %i rejected: %s\n",ed->entnum,fte_jolt_error(ctx->simulation));
            body->warned=qtrue;return;
        }
        ed->rbe.physics=qtrue;remember(ed);
    }
    if (!body->handle) return;
    if (body->motion==FTE_JOLT_KINEMATIC) {
        if (make_state(world,ed,body,&state) && fte_jolt_move(ctx->simulation,body->handle,&state)) remember(ed);
    } else if (!equal3(ed->v->origin,ed->rbe.origin) || !equal3(ed->v->angles,ed->rbe.angles) ||
        !equal3(ed->v->velocity,ed->rbe.velocity) || !equal3(ed->v->avelocity,ed->rbe.avelocity)) {
        if (make_state(world,ed,body,&state) && fte_jolt_set(ctx->simulation,body->handle,&state)) remember(ed);
    }
    fte_jolt_material(ctx->simulation,body->handle,friction,restitution,gravity);
    fte_jolt_filter(ctx->simulation,body->handle,ed->v->owner,dimension_mask(ed->xv->dimension_solid),dimension_mask(ed->xv->dimension_hit));
}
typedef struct {
    fte_jolt_world *simulation;
    fte_jolt_joint handle;
    fte_jolt_joint_desc settings;
    int enemy,aiment;
} jolt_joint_t;
static void joint_free(rbejoint_t *slot) {
    jolt_joint_t *joint=(jolt_joint_t*)slot->joint;
    if (!joint) return;
    if (joint->handle) fte_jolt_joint_remove(joint->simulation,joint->handle);
    free(joint);slot->joint=NULL;
}
static qboolean joint_body(world_t *world,int number,fte_jolt_body *out) {
    wedict_t *ed;jolt_entity_t *body;
    *out=0;if (!number) return qtrue;
    if (number<0 || (unsigned)number>=world->num_edicts) return qfalse;
    ed=WEDICT_NUM_PB(world->progs,number);body=ED_ISFREE(ed)?NULL:(jolt_entity_t*)ed->rbe.body.body;
    if (!body || !body->handle) return qfalse;
    *out=body->handle;return qtrue;
}
static void joint_compliance(fte_jolt_joint_desc *desc,float erp,float cfm) {
    /* ODE's timestep relation: ERP=h*k/(h*k+c), CFM=1/(h*k+c).
     * These map limit softness, not the entire solver's contact response. */
    if (isfinite(erp) && isfinite(cfm) && cfm>0 && erp>0 && erp<=1) {
        desc->stiffness=erp*120/cfm;desc->damping=(1-erp)/cfm;
    }
}
static void sync_joint(world_t *world,wedict_t *ed) {
    jolt_context_t *ctx=context(world);jolt_joint_t *joint=(jolt_joint_t*)ed->rbe.joint.joint;
    fte_jolt_joint_desc desc;vec3_t right,up;int type=(int)ed->xv->jointtype;
    int enemy=ed->v->enemy,aiment=ed->v->aiment;
    if ((int)ed->v->movetype==MOVETYPE_PHYSICS) type=0;
    if (!type) {if (joint) remove_joint(world,ed);return;}
    memset(&desc,0,sizeof(desc));desc.type=type;
    if (!joint_body(world,enemy,&desc.body1)||!joint_body(world,aiment,&desc.body2)) {
        if (joint) remove_joint(world,ed);return;
    }
    VectorCopy(ed->v->origin,desc.anchor);
    rbeapi->AngleVectors(ed->v->angles,desc.axis,right,up);
    VectorCopy(up,desc.axis2);
    if(type==JOINTTYPE_HINGE2) {
        float length=sqrtf(DotProduct(ed->v->velocity,ed->v->velocity));int i;
        for(i=0;i<3;i++) desc.axis2[i]=length>0?ed->v->velocity[i]/length:0;
    }
    if (type==JOINTTYPE_HINGE || type==JOINTTYPE_UNIVERSAL || type==JOINTTYPE_HINGE2) {desc.lower=-M_PI;desc.upper=M_PI;}
    if (type==JOINTTYPE_SLIDER) {desc.lower=-1e20f;desc.upper=1e20f;}
    if (ed->v->movedir[1]<0) {
        desc.velocity=ed->v->movedir[0];desc.max_force=-ed->v->movedir[1];
        if (ed->v->movedir[2]>0) {desc.upper=ed->v->movedir[2];desc.lower=-desc.upper;}
    } else if (ed->v->movedir[0]>0 && ed->v->movedir[1]>0) {
        desc.stiffness=ed->v->movedir[0];desc.damping=2*ed->v->movedir[1]*sqrtf(desc.stiffness);
        desc.upper=fmaxf(0,ed->v->movedir[2]);desc.lower=-desc.upper;
    }
    if (type==JOINTTYPE_HINGE) {desc.lower=fmaxf(-M_PI,desc.lower);desc.upper=fminf(M_PI,desc.upper);}
    if(type==JOINTTYPE_UNIVERSAL || type==JOINTTYPE_HINGE2) {
        desc.lower=fmaxf(-M_PI,desc.lower);desc.upper=fminf(M_PI,desc.upper);
        desc.lower2=desc.lower;desc.upper2=desc.upper;desc.max_force2=desc.max_force;
        desc.velocity2=desc.velocity;desc.stiffness2=desc.stiffness;desc.damping2=desc.damping;
    }
    desc.suspension_stiffness=ed->xv->joint_suspension_stiffness;
    desc.suspension_damping=ed->xv->joint_suspension_damping;
    if (joint && enemy==joint->enemy && aiment==joint->aiment) {
        fte_jolt_joint_desc changed=joint->settings;
        if (!memcmp(&desc,&changed,sizeof(desc))) return;
        changed.suspension_stiffness=desc.suspension_stiffness;
        changed.suspension_damping=desc.suspension_damping;
        if(type==JOINTTYPE_HINGE2 && joint->handle && !memcmp(&desc,&changed,sizeof(desc))) {
            // Updating the spring must not redefine rest anchors around the
            // currently compressed body. Cache rejected requests to avoid spam.
            if(!fte_jolt_joint_suspension(ctx->simulation,joint->handle,desc.suspension_stiffness,desc.suspension_damping))
                Con_Printf("Jolt suspension entity %i rejected: %s\n",ed->entnum,fte_jolt_error(ctx->simulation));
            joint->settings=desc;return;
        }
    }
    remove_joint(world,ed);joint=(jolt_joint_t*)calloc(1,sizeof(*joint));if (!joint) {ctx->failed=qtrue;return;}
    ed->rbe.joint.joint=joint;ed->rbe.joint_type=type;
    joint->simulation=ctx->simulation;joint->settings=desc;joint->enemy=enemy;joint->aiment=aiment;
    joint->handle=fte_jolt_joint_add(ctx->simulation,&desc);
    if (!joint->handle) Con_Printf("Jolt joint entity %i rejected: %s\n",ed->entnum,fte_jolt_error(ctx->simulation));
}
static void identity_matrix(float *matrix) {memset(matrix,0,12*sizeof(*matrix));matrix[0]=matrix[5]=matrix[10]=1;}
static qboolean matrix_state(const float *matrix,fte_jolt_state *state) {
    vec3_t forward={matrix[0],matrix[4],matrix[8]},left={matrix[1],matrix[5],matrix[9]},up={matrix[2],matrix[6],matrix[10]};
    memset(state,0,sizeof(*state));state->position[0]=matrix[3];state->position[1]=matrix[7];state->position[2]=matrix[11];
    return fte_jolt_quat(forward,left,up,state->rotation)?qtrue:qfalse;
}
static void state_matrix(const fte_jolt_state *state,float *matrix) {
    vec3_t forward,left,up;int i;fte_jolt_axes(state->rotation,forward,left,up);
    for(i=0;i<3;i++) {matrix[4*i]=forward[i];matrix[4*i+1]=left[i];matrix[4*i+2]=up[i];matrix[4*i+3]=state->position[i];}
}
static qboolean QDECL rag_matrix_to_body(rbebody_t *slot,float *matrix) {
    jolt_rag_body_t *body=(jolt_rag_body_t*)slot->body;fte_jolt_state state;
    if (!body || !body->handle || !body->world->rbe || !matrix_state(matrix,&state)) return qfalse;
    return fte_jolt_set(context(body->world)->simulation,body->handle,&state)?qtrue:qfalse;
}
static qboolean QDECL rag_create_body(world_t *world,rbebody_t *slot,rbebodyinfo_t *info,float *matrix,wedict_t *entity) {
    jolt_context_t *ctx=context(world);jolt_rag_body_t *body;fte_jolt_desc desc;float radius;
    memset(&desc,0,sizeof(desc));desc.motion=FTE_JOLT_DYNAMIC;desc.mass=info->mass;
    desc.friction=1;desc.gravity_scale=1;
    desc.entity=entity?entity->entnum:0;desc.owner=entity?entity->v->owner:0;
    desc.collision_family=entity?(unsigned)entity->entnum+1:0;
    desc.solid_mask=entity?dimension_mask(entity->xv->dimension_solid):~0u;
    desc.hit_mask=entity?dimension_mask(entity->xv->dimension_hit):~0u;
    VectorCopy(info->dimensions,desc.size);
    switch(info->geomshape) {
    case GEOMTYPE_BOX:desc.shape=FTE_JOLT_BOX;break;
    case GEOMTYPE_SPHERE:
        desc.shape=FTE_JOLT_SPHERE;radius=(desc.size[0]+desc.size[1]+desc.size[2])/3;
        desc.size[0]=desc.size[1]=desc.size[2]=2*radius;break;
    case GEOMTYPE_CAPSULE:case GEOMTYPE_CYLINDER:
        radius=(desc.size[0]+desc.size[1])*.5f;desc.size[0]=desc.size[1]=2*radius;
        if(info->geomshape==GEOMTYPE_CAPSULE) {desc.shape=FTE_JOLT_CAPSULE_Z;desc.size[2]+=2*radius;}
        else desc.shape=FTE_JOLT_CYLINDER_Z;break;
    default:Con_Printf("Jolt ragdoll body %s: unsupported shape.\n",info->name);return qfalse;
    }
    if(!matrix_state(matrix,&desc.state)) {Con_Printf("Jolt ragdoll body %s: invalid rigid transform.\n",info->name);return qfalse;}
    body=(jolt_rag_body_t*)calloc(1,sizeof(*body));if(!body) return qfalse;
    body->handle=fte_jolt_add(ctx->simulation,&desc);
    if(!body->handle) {Con_Printf("Jolt ragdoll body %s rejected: %s\n",info->name,fte_jolt_error(ctx->simulation));free(body);return qfalse;}
    body->world=world;body->entity=entity;body->next=ctx->rag_bodies;ctx->rag_bodies=body;
    slot->body=body;slot->geom=NULL;world->rbe_hasphysicsents=qtrue;return qtrue;
}
static void QDECL rag_matrix_from_body(world_t *world,rbebody_t *slot,float *matrix) {
    jolt_rag_body_t *body=(jolt_rag_body_t*)slot->body;fte_jolt_state state;identity_matrix(matrix);
    if(body && body->handle && fte_jolt_get(context(world)->simulation,body->handle,&state)) state_matrix(&state,matrix);
}
static void QDECL rag_matrix_from_joint(rbejoint_t *slot,rbejointinfo_t *info,float *matrix) {
    jolt_joint_t *joint=(jolt_joint_t*)slot->joint;fte_jolt_state state;(void)info;identity_matrix(matrix);
    if(joint && joint->handle && fte_jolt_joint_frame(joint->simulation,joint->handle,&state)) state_matrix(&state,matrix);
}
static void QDECL rag_enable_joint(rbejoint_t *slot,qboolean enable) {
    jolt_joint_t *joint=(jolt_joint_t*)slot->joint;
    if(joint && joint->handle) fte_jolt_joint_enable(joint->simulation,joint->handle,enable);
}
static void QDECL rag_create_joint(world_t *world,rbejoint_t *slot,rbejointinfo_t *info,rbebody_t *a,rbebody_t *b,vec3_t axes[3]) {
    jolt_context_t *ctx=context(world);fte_jolt_joint_desc desc;jolt_joint_t *joint;
    memset(&desc,0,sizeof(desc));desc.type=info->type;
    if(a&&a->body)desc.body1=((jolt_rag_body_t*)a->body)->handle;
    if(b&&b->body)desc.body2=((jolt_rag_body_t*)b->body)->handle;
    VectorCopy(axes[0],desc.anchor);VectorCopy(axes[1],desc.axis);VectorCopy(axes[2],desc.axis2);
    desc.lower=info->LoStop;desc.upper=info->HiStop;desc.max_force=info->FMax;desc.velocity=info->Vel;
    joint_compliance(&desc,info->ERP,info->CFM);
    desc.lower2=info->LoStop2;desc.upper2=info->HiStop2;desc.max_force2=info->FMax2;desc.velocity2=info->Vel2;
    {fte_jolt_joint_desc secondary;memset(&secondary,0,sizeof(secondary));
     joint_compliance(&secondary,info->ERP2,info->CFM2);desc.stiffness2=secondary.stiffness;desc.damping2=secondary.damping;}
    desc.suspension_stiffness=info->suspension_stiffness;
    desc.suspension_damping=info->suspension_damping;
    joint=(jolt_joint_t*)calloc(1,sizeof(*joint));if(!joint) return;
    joint->simulation=ctx->simulation;joint->settings=desc;
    joint->handle=fte_jolt_joint_add(ctx->simulation,&desc);
    if(!joint->handle) {Con_Printf("Jolt ragdoll joint %s rejected: %s\n",info->name,fte_jolt_error(ctx->simulation));free(joint);return;}
    slot->joint=joint;
}
static void QDECL rag_destroy_body(world_t *world,rbebody_t *slot) {
    jolt_rag_body_t *body=(jolt_rag_body_t*)slot->body,**link;
    if(!body) return;
    link=&context(world)->rag_bodies;while(*link && *link!=body)link=&(*link)->next;
    if(*link)*link=body->next;
    if(body->handle)fte_jolt_remove(context(world)->simulation,body->handle);
    free(body);slot->body=slot->geom=NULL;
}
static void QDECL rag_destroy_joint(world_t *world,rbejoint_t *slot) {(void)world;joint_free(slot);}
static void QDECL push_command(world_t *world,rbecommandqueue_t *command) {
    jolt_context_t *ctx=context(world);jolt_command_t *copy;
    if (!command->edict || ED_ISFREE(command->edict)) return;
    if (ctx->pending_commands>=8192) {Con_Printf("Jolt: physics command queue capacity exceeded.\n");return;}
    copy=(jolt_command_t*)calloc(1,sizeof(*copy));if (!copy) {ctx->failed=qtrue;return;}
    copy->command=command->command;copy->edict=command->edict;
    if (command->command==RBECMD_FORCE) {VectorCopy(command->v1,copy->v1);VectorCopy(command->v2,copy->v2);}
    else if (command->command==RBECMD_TORQUE) VectorCopy(command->v1,copy->v1);
    ++ctx->pending_commands;
    if (ctx->tail) ctx->tail->next=copy;else ctx->commands=copy;
    ctx->tail=copy;world->rbe_hasphysicsents=qtrue;
}
static void run_commands(world_t *world) {
    jolt_context_t *ctx=context(world);
    while (ctx->commands) {
        jolt_command_t *cmd=ctx->commands;jolt_entity_t *body;
        ctx->commands=cmd->next;--ctx->pending_commands;if (!ctx->commands) ctx->tail=NULL;
        body=ED_ISFREE(cmd->edict)?NULL:(jolt_entity_t*)cmd->edict->rbe.body.body;
        if (body && body->handle) switch (cmd->command) {
        case RBECMD_ENABLE:fte_jolt_enable(ctx->simulation,body->handle,1);break;
        case RBECMD_DISABLE:fte_jolt_enable(ctx->simulation,body->handle,0);break;
        case RBECMD_FORCE:fte_jolt_force(ctx->simulation,body->handle,cmd->v1,cmd->v2);break;
        case RBECMD_TORQUE:fte_jolt_torque(ctx->simulation,body->handle,cmd->v1);break;
        }
        free(cmd);
    }
}
static void read_back(world_t *world,wedict_t *ed) {
    jolt_entity_t *body=(jolt_entity_t*)ed->rbe.body.body;fte_jolt_state state;
    vec3_t forward,left,up;int i;float sign;
    if (!body || !body->handle || body->motion!=FTE_JOLT_DYNAMIC || (int)ed->v->movetype!=MOVETYPE_PHYSICS) return;
    if (!fte_jolt_get(context(world)->simulation,body->handle,&state)) return;
    fte_jolt_axes(state.rotation,forward,left,up);sign=mesh_sign(world,ed);
    for (i=0;i<3;i++) ed->v->origin[i]=state.position[i]-forward[i]*body->center[0]-left[i]*body->center[1]-up[i]*body->center[2];
    VectorCopy(state.velocity,ed->v->velocity);rbeapi->VectorAngles(forward,up,ed->v->angles,qfalse);ed->v->angles[PITCH]*=sign;
    ed->v->avelocity[PITCH]=state.spin[0]*(180.0/M_PI)*sign;
    ed->v->avelocity[ROLL]=state.spin[1]*(180.0/M_PI);ed->v->avelocity[YAW]=state.spin[2]*(180.0/M_PI);
    remember(ed);rbeapi->LinkEdict(world,ed,qtrue);
}
static wedict_t *contact_entity(world_t *world,fte_jolt_body handle,unsigned number) {
    wedict_t *ed;jolt_entity_t *body;
    if (number>=world->num_edicts) return NULL;
    ed=WEDICT_NUM_PB(world->progs,number);
    body=ED_ISFREE(ed)?NULL:(jolt_entity_t*)ed->rbe.body.body;
    if(body && body->handle==handle) return ed;
    {
        jolt_rag_body_t *rag;
        for(rag=context(world)->rag_bodies;rag;rag=rag->next)
            if(rag->handle==handle && rag->entity==ed && !ED_ISFREE(ed))return ed;
    }
    return NULL;
}
static void QDECL prepare_frame(world_t *world,double dt,double gravity) {
    jolt_context_t *ctx=context(world);unsigned i;
    (void)dt;(void)gravity;
    if(ctx->failed || (!world->rbe_hasphysicsents && !ctx->rag_bodies)) return;
    // Capture initial mover poses before classic pushers advance or are blocked.
    for(i=0;i<world->num_edicts;i++) {
        wedict_t *ed=WEDICT_NUM_PB(world->progs,i);
        if(!ED_ISFREE(ed) && !ed->rbe.body.body) sync_entity(world,ed);
    }
}
static void QDECL run_frame(world_t *world,double dt,double gravity) {
    jolt_context_t *ctx=context(world);rigidbodyengine_t *original=world->rbe;
    unsigned i,spawncount=world->spawncount;size_t index;fte_jolt_stats stats;
    if (ctx->failed) return;
    if (!world->rbe_hasphysicsents && !ctx->rag_bodies) return;
    for (i=0;i<world->num_edicts;i++) {
        wedict_t *ed=WEDICT_NUM_PB(world->progs,i);
        if (ED_ISFREE(ed)) {if (ed->rbe.body.body) remove_body(world,ed);continue;}
        sync_entity(world,ed);
    }
    /* Bodies must exist before resolving any joint's entity references. */
    for(i=1;i<world->num_edicts;i++) {
        wedict_t *ed=WEDICT_NUM_PB(world->progs,i);
        if(!ED_ISFREE(ed))sync_joint(world,ed);
    }
    run_commands(world);
    if (!fte_jolt_step(ctx->simulation,dt,gravity)) {
        Con_Printf("Jolt simulation stopped: %s\n",fte_jolt_error(ctx->simulation));ctx->failed=qtrue;return;
    }
    fte_jolt_get_stats(ctx->simulation,&stats);
    if (stats.dropped_time>0 && !ctx->warned_dropped) {Con_DPrintf("Jolt: physics catch-up time capped at eight substeps per frame.\n");ctx->warned_dropped=qtrue;}
    for (i=1;i<world->num_edicts;i++) {
        wedict_t *ed=WEDICT_NUM_PB(world->progs,i);
        if (!ED_ISFREE(ed)) read_back(world,ed);
        if (world->rbe!=original || world->spawncount!=spawncount) return;
    }
    /* Never call QC while Jolt has bodies locked or while its solver is active. */
    for (index=0;index<fte_jolt_contacts(ctx->simulation);index++) {
        fte_jolt_contact contact;trace_t trace;wedict_t *a,*b;
        if (!fte_jolt_get_contact(ctx->simulation,index,&contact)) continue;
        a=contact_entity(world,contact.body1,contact.entity1);b=contact_entity(world,contact.body2,contact.entity2);
        if (!a || !b) continue;
        memset(&trace,0,sizeof(trace));trace.fraction=trace.truefraction=1;
        VectorCopy(contact.position,trace.endpos);VectorNegate(contact.normal,trace.plane.normal);trace.ent=b;
        if (a->v->touch && world->Event_Touch) world->Event_Touch(world,a,b,&trace);
        if (world->rbe!=original || world->spawncount!=spawncount) return;
        a=contact_entity(world,contact.body1,contact.entity1);b=contact_entity(world,contact.body2,contact.entity2);
        if (a && b && b->v->touch && world->Event_Touch) {VectorCopy(contact.normal,trace.plane.normal);trace.ent=a;world->Event_Touch(world,b,a,&trace);}
        if (world->rbe!=original || world->spawncount!=spawncount) return;
    }
}
static void QDECL start_world(world_t *world) {
    jolt_context_t *ctx;
    if (world->rbe || !enabled->ival) return;
    ctx=(jolt_context_t*)calloc(1,sizeof(*ctx));if (!ctx) return;
    ctx->simulation=fte_jolt_create(8192,120,8);
    if (!ctx->simulation) {Con_Printf("Jolt: unable to create physics world.\n");free(ctx);return;}
    ctx->pub.End=end_world;ctx->pub.RemoveJointFromEntity=remove_joint;ctx->pub.RemoveFromEntity=remove_body;
    ctx->pub.RagMatrixToBody=rag_matrix_to_body;ctx->pub.RagMatrixFromJoint=rag_matrix_from_joint;
    ctx->pub.RagMatrixFromBody=rag_matrix_from_body;ctx->pub.RagEnableJoint=rag_enable_joint;
    ctx->pub.RagCreateJoint=rag_create_joint;ctx->pub.RagDestroyBody=rag_destroy_body;ctx->pub.RagDestroyJoint=rag_destroy_joint;
    ctx->pub.RunFrame=prepare_frame;ctx->pub.RunFrameAfterEntities=run_frame;ctx->pub.PushCommand=push_command;ctx->pub.RagCreateBody=rag_create_body;
    world->rbe=&ctx->pub;
    Con_Printf("Jolt %s rigid-body backend initialized (120 Hz; experimental).\n",fte_jolt_version());
}
static void QDECL shutdown_plugin(void) {
    if (jolt_registered) {jolt_registered=qfalse;rbeapi->UnregisterPhysicsEngine("Jolt");}
}
qboolean Plug_Init(void) {
    rbeapi=plugfuncs->GetEngineInterface(plugrbefuncs_name,sizeof(*rbeapi));
    if (!RBE_Compatible(rbeapi)) {
        Con_Printf("Jolt: incompatible rigid-body plugin interface.\n");return qfalse;
    }
    enabled=cvarfuncs->GetNVFDG("physics_jolt_enabled","0",0,"Enable experimental Jolt rigid props before loading a map. Classic movement is unchanged.","Jolt Physics");
    mesh_pitch=cvarfuncs->GetNVFDG("r_meshpitch","1",0,NULL,NULL);
    if (!rbeapi->RegisterPhysicsEngine("Jolt",start_world)) {
        Con_Printf("Jolt: another physics plugin is active; unload it before loading Jolt.\n");return qfalse;
    }
    jolt_registered=qtrue;
    if (!plugfuncs->ExportFunction("Shutdown",shutdown_plugin)) {shutdown_plugin();return qfalse;}
    return qtrue;
}
#else
qboolean Plug_Init(void) {return qfalse;}
#endif
