/* Entity-scene persistence, included after the live bridge helpers. */
#include <stdarg.h>
#include <errno.h>
#include <float.h>

typedef struct {char data[4096];size_t used;qboolean ok;} jolt_line_t;
typedef struct {
    qboolean(QDECL *write)(void *,const char *,size_t);void *ctx;size_t total;
} jolt_output_t;
static void js_add(jolt_line_t *line,const char *fmt,...) {
    int count;va_list args;
    if(!line->ok) return;
    va_start(args,fmt);count=vsnprintf(line->data+line->used,sizeof(line->data)-line->used,fmt,args);va_end(args);
    if(count<0 || (size_t)count>=sizeof(line->data)-line->used) {line->ok=qfalse;return;}
    line->used+=count;
}
static qboolean js_emit(jolt_output_t *out,jolt_line_t *line) {
    if(!line->ok || out->total+line->used>0xf00000) return qfalse;
    out->total+=line->used;
    if(out->write && !out->write(out->ctx,line->data,line->used)) return qfalse;
    line->used=0;return qtrue;
}
#define JS_JOINT_FLOATS(d) \
    &(d)->anchor[0],&(d)->anchor[1],&(d)->anchor[2],&(d)->axis[0],&(d)->axis[1],&(d)->axis[2], \
    &(d)->lower,&(d)->upper,&(d)->max_force,&(d)->velocity,&(d)->stiffness,&(d)->damping, \
    &(d)->axis2[0],&(d)->axis2[1],&(d)->axis2[2],&(d)->lower2,&(d)->upper2,&(d)->max_force2, \
    &(d)->velocity2,&(d)->stiffness2,&(d)->damping2,&(d)->suspension_stiffness, \
    &(d)->suspension_damping,&(d)->anchor2[0],&(d)->anchor2[1],&(d)->anchor2[2]
#define JS_ENTITY_ATTRS(ed) &(ed)->xv->mass,&(ed)->xv->scale,&(ed)->xv->gravity, \
    &(ed)->xv->friction,&(ed)->xv->bouncefactor,&(ed)->xv->damp_linear, \
    &(ed)->xv->damp_angular,&(ed)->xv->dimension_solid,&(ed)->xv->dimension_hit
static void js_state(jolt_line_t *line,const fte_jolt_state *state) {
    const float *parts[]={state->position,state->rotation,state->velocity,state->spin};
    const int sizes[]={3,4,3,3};int i,j;
    for(i=0;i<4;i++)for(j=0;j<sizes[i];j++) js_add(line," %.9g",(double)parts[i][j]);
}
static qboolean js_entity_values(int shape,const float *const parts[6],const float *const attributes[9]) {
    int i,j;
    for(i=0;i<6;i++)for(j=0;j<3;j++)
        if(!isfinite(parts[i][j]) || fabs(parts[i][j])>(i>=4?1000000:10000000)) return qfalse;
    for(i=0;i<9;i++)
        if(!isfinite(*attributes[i]) || fabs(*attributes[i])>(i>=7?4294967296.0:1000000000.0)) return qfalse;
    if(*attributes[0]>0 && *attributes[0]<.000001) return qfalse;
    if(*attributes[1]!=0 && (fabs(*attributes[1])<.0001 || fabs(*attributes[1])>1000)) return qfalse;
    for(i=0;i<3;i++) {
        double size=(double)parts[5][i]-parts[4][i];
        if(size<0 || (shape!=GEOMTYPE_TRIMESH && shape!=GEOMTYPE_CONVEX && shape!=GEOMTYPE_COMPOUND && size<.0001)) return qfalse;
    }
    return true;
}
static qboolean js_springs(const fte_jolt_joint_desc *d) {
    const float values[]={d->stiffness,d->damping,d->stiffness2,d->damping2,d->suspension_stiffness,d->suspension_damping};
    int i;for(i=0;i<(int)countof(values);i++) if(values[i]>0 && values[i]<.000001) return qfalse;
    return qtrue;
}
static qboolean js_descriptor(const fte_jolt_joint_desc *d) {
    const float *values[]={JS_JOINT_FLOATS(d)};int j;
    for(j=0;j<(int)countof(values);j++)
        if(!isfinite(*values[j]) || fabs(*values[j])>((j==6 || j==7 || j==15 || j==16)?1e21:1000000000.0)) return qfalse;
    return js_springs(d);
}
static qboolean js_joint_values(const float *const parts[4],const float spring[2]) {
    int i,j;
    for(i=0;i<4;i++)for(j=0;j<3;j++)
        if(!isfinite(parts[i][j]) || fabs(parts[i][j])>(i==3?1000000000:10000000)) return qfalse;
    for(i=0;i<2;i++) if(!isfinite(spring[i]) || fabs(spring[i])>1000000000 || (spring[i]>0 && spring[i]<.000001)) return qfalse;
    if(parts[3][0]>0 && parts[3][1]>0 && (parts[3][0]<.000001 || parts[3][1]<.000001)) return qfalse;
    return qtrue;
}
static qboolean js_body_current(world_t *world,wedict_t *ed,jolt_entity_t *b) {
    int motion=(int)ed->v->movetype==MOVETYPE_PHYSICS?FTE_JOLT_DYNAMIC:
        ed->entnum && ((int)ed->v->movetype==MOVETYPE_PUSH || (int)ed->v->movetype==MOVETYPE_H2PUSHPULL)?FTE_JOLT_KINEMATIC:FTE_JOLT_STATIC;
    if(geometry(ed)!=b->shape || motion!=b->motion || (ed->xv->mass>0?ed->xv->mass:1)!=b->mass ||
       (ed->xv->scale?ed->xv->scale:1)!=b->scale || fmaxf(0,ed->xv->damp_linear)!=b->damping_linear ||
       fmaxf(0,ed->xv->damp_angular)!=b->damping_angular) return qfalse;
    if(((b->shape==GEOMTYPE_TRIMESH || b->shape==GEOMTYPE_CONVEX)?(int)ed->v->modelindex:0)!=b->modelindex ||
       strcmp(b->shape_file,b->shape==GEOMTYPE_COMPOUND?PR_GetString(world->progs,ed->xv->physics_shape):"")) return qfalse;
    if(b->shape!=GEOMTYPE_TRIMESH && b->shape!=GEOMTYPE_CONVEX &&
       (!equal3(b->mins,ed->v->mins) || !equal3(b->maxs,ed->v->maxs))) return qfalse;
    return qtrue;
}
static qboolean QDECL save_entities(world_t *world,qboolean(QDECL *write)(void *,const char *,size_t),void *user) {
    jolt_context_t *ctx=context(world);jolt_output_t out={write,user,0};jolt_line_t line={{0},0,qtrue};
    unsigned i,nb=0,nj=0;double remainder,simulated;
    if(world->num_edicts>1048576 || ctx->failed || ctx->rag_bodies || ctx->commands || !fte_jolt_clock_get(ctx->simulation,&remainder,&simulated)) {
        Con_Printf("Jolt: saving requires a healthy entity scene without live skeletal rigs or pending physics commands/forces.\n");return qfalse;
    }
    for(i=0;i<world->num_edicts;i++) {
        wedict_t *ed=WEDICT_NUM_PB(world->progs,i);jolt_entity_t *body; jolt_joint_t *joint;
        if(ED_ISFREE(ed)) continue;
        body=(jolt_entity_t*)ed->rbe.body.body;joint=(jolt_joint_t*)ed->rbe.joint.joint;
        if((!body || !body->handle) && geometry(ed)!=GEOMTYPE_NONE &&
           (geometry(ed)==GEOMTYPE_TRIMESH || geometry(ed)==GEOMTYPE_CONVEX || geometry(ed)==GEOMTYPE_COMPOUND ||
            (ed->v->maxs[0]>ed->v->mins[0] && ed->v->maxs[1]>ed->v->mins[1] && ed->v->maxs[2]>ed->v->mins[2]))) {
            Con_Printf("Jolt: save requires valid collision geometry synchronized at a physics tick.\n");return qfalse;
        }
        if(body && body->handle) ++nb;
        if(joint && joint->handle) ++nj;
    }
    js_add(&line,"jolt_entities 1 %d\nclock %.17g %.17g\ncounts %u %u\n",point_anchor2->value!=0,remainder,simulated,nb,nj);
    if(!js_emit(&out,&line)) return qfalse;
    for(i=0;i<world->num_edicts;i++) {
        wedict_t *ed=WEDICT_NUM_PB(world->progs,i);jolt_entity_t *body;fte_jolt_runtime state;int j;
        if(ED_ISFREE(ed)) continue;
        body=(jolt_entity_t*)ed->rbe.body.body;if(!body || !body->handle) continue;
        if(!js_body_current(world,ed,body)) {
            Con_Printf("Jolt: save after collision-shape edits reach a physics tick.\n");return qfalse;
        }
        {const float *parts[]={ed->v->origin,ed->v->angles,ed->v->velocity,ed->v->avelocity,ed->v->mins,ed->v->maxs};
         const float *attributes[]={JS_ENTITY_ATTRS(ed)};
         if(!js_entity_values(body->shape,parts,attributes)) {Con_Printf("Jolt: entity fields exceed save-state bounds.\n");return qfalse;}}
        if(!fte_jolt_runtime_get(ctx->simulation,body->handle,&state)) return qfalse;
        js_add(&line,"body %u %d %d %d %d %d",i,body->shape,body->motion,body->modelindex,state.has_target,state.active);
        js_state(&line,&state.state);js_state(&line,&state.target);
        {const float *parts[]={ed->rbe.origin,ed->rbe.angles,ed->rbe.velocity,ed->rbe.avelocity};int k;
         for(k=0;k<4;k++)for(j=0;j<3;j++) js_add(&line," %.9g",(double)parts[k][j]);}
        {const float *parts[]={ed->v->origin,ed->v->angles,ed->v->velocity,ed->v->avelocity,ed->v->mins,ed->v->maxs};int k;
         for(k=0;k<6;k++)for(j=0;j<3;j++) js_add(&line," %.9g",(double)parts[k][j]);}
        {const float *values[]={JS_ENTITY_ATTRS(ed)};
         for(j=0;j<(int)countof(values);j++) js_add(&line," %.9g",(double)*values[j]);}
        for(j=0;j<3;j++) js_add(&line," %.9g",(double)state.driven_velocity[j]);
        for(j=0;j<3;j++) js_add(&line," %.9g",(double)state.driven_spin[j]);
        js_add(&line,"\n");if(!js_emit(&out,&line)) return qfalse;
    }
    for(i=1;i<world->num_edicts;i++) {
        wedict_t *ed=WEDICT_NUM_PB(world->progs,i);jolt_joint_t *joint;fte_jolt_joint_checkpoint state;int j;
        if(ED_ISFREE(ed)) continue;
        joint=(jolt_joint_t*)ed->rbe.joint.joint;if(!joint || !joint->handle) continue;
        if(!fte_jolt_joint_checkpoint_get(ctx->simulation,joint->handle,&state) || !js_descriptor(&state.settings)) return qfalse;
        if((int)ed->xv->jointtype!=state.settings.type || ed->v->enemy!=joint->enemy || ed->v->aiment!=joint->aiment ||
           (state.settings.type==JOINTTYPE_POINT && state.settings.point_mode!=(point_anchor2->value!=0?FTE_JOLT_POINT_LEGACY:0))) {
            Con_Printf("Jolt: save after joint edits reach a physics tick.\n");return qfalse;
        }
        js_add(&line,"joint %u %d %d %d %d %d",i,joint->enemy,joint->aiment,state.settings.type,state.settings.point_mode,state.enabled);
        {const float *values[]={JS_JOINT_FLOATS(&state.settings)};
         for(j=0;j<(int)countof(values);j++) js_add(&line," %.9g",(double)*values[j]);}
        js_state(&line,&state.reference1);js_state(&line,&state.reference2);
        {const float *parts[]={ed->v->origin,ed->v->angles,ed->v->velocity,ed->v->movedir};int k;
         float spring[]={ed->xv->joint_suspension_stiffness,ed->xv->joint_suspension_damping};
         if(!js_joint_values(parts,spring)) return qfalse;
         for(k=0;k<4;k++)for(j=0;j<3;j++) js_add(&line," %.9g",(double)parts[k][j]);
         for(j=0;j<2;j++) js_add(&line," %.9g",(double)spring[j]);}
        js_add(&line,"\n");if(!js_emit(&out,&line)) return qfalse;
    }
    js_add(&line,"end\n");return js_emit(&out,&line);
}
typedef struct {const char *at,*end;} jolt_reader_t;
static qboolean js_word(jolt_reader_t *r,char word[64]) {
    size_t len=0;
    while(r->at<r->end && (unsigned char)*r->at<=32) ++r->at;
    while(r->at<r->end && (unsigned char)*r->at>32) {
        if(len>=63) return qfalse;
        word[len++]=*r->at++;
    }
    word[len]=0;return len>0;
}
static qboolean js_key(jolt_reader_t *r,const char *wanted) {char word[64];return js_word(r,word)&&!strcmp(word,wanted);}
static qboolean js_double(jolt_reader_t *r,double *value) {
    char word[64],*end;if(!js_word(r,word)) return qfalse;
    errno=0;*value=strtod(word,&end);return *word && !*end && isfinite(*value) && errno!=ERANGE;
}
static qboolean js_int(jolt_reader_t *r,int *value,int lower,int upper) {
    double n;if(!js_double(r,&n)||n<lower||n>upper||trunc(n)!=n) return qfalse;
    *value=(int)n;return qtrue;
}
static qboolean js_float(jolt_reader_t *r,float *value) {
    double n;if(!js_double(r,&n)||n>FLT_MAX||n<-FLT_MAX) return qfalse;
    *value=(float)n;return qtrue;
}
static qboolean js_readstate(jolt_reader_t *r,fte_jolt_state *s) {
    float *parts[]={s->position,s->rotation,s->velocity,s->spin};const int sizes[]={3,4,3,3};int i,j;
    for(i=0;i<4;i++)for(j=0;j<sizes[i];j++) if(!js_float(r,&parts[i][j])) return qfalse;
    return qtrue;
}
typedef struct {int entity,shape,motion,model;fte_jolt_runtime runtime;float remembered[4][3],qc[6][3],attributes[9];} jolt_saved_body_t;
typedef struct {int entity,enemy,aiment;fte_jolt_joint_checkpoint checkpoint;float qc[4][3],spring[2];} jolt_saved_joint_t;
static qboolean QDECL load_entities(world_t *world,const char *data,size_t length) {
    jolt_context_t *ctx=context(world);jolt_reader_t r;
    if(!data || !length || length>0x1000000) return qfalse;
    r.at=data;r.end=data+length;
    jolt_saved_body_t *bodies=NULL;jolt_saved_joint_t *joints=NULL;unsigned char *seen=NULL;
    int version,mode,nb,nj,i,j;double remainder,simulated;qboolean ok=qfalse;
    if(length>0x1000000 || world->num_edicts>1048576 || !world->num_edicts ||
       !js_key(&r,"jolt_entities") || !js_int(&r,&version,1,1) || !js_int(&r,&mode,0,1) || mode!=(point_anchor2->value!=0) || !js_key(&r,"clock") ||
       !js_double(&r,&remainder) || !js_double(&r,&simulated) || remainder<0 || remainder>=1.0/120 || simulated<0 ||
       !js_key(&r,"counts") || !js_int(&r,&nb,0,8192) || !js_int(&r,&nj,0,16384)) return qfalse;
    bodies=(jolt_saved_body_t*)calloc(nb?nb:1,sizeof(*bodies));
    joints=(jolt_saved_joint_t*)calloc(nj?nj:1,sizeof(*joints));seen=(unsigned char*)calloc(world->num_edicts,1);
    if(!bodies || !joints || !seen) goto done;
    for(i=0;i<nb;i++) {
        jolt_saved_body_t *b=&bodies[i];fte_jolt_runtime *s=&b->runtime;
        if(!js_key(&r,"body") || !js_int(&r,&b->entity,0,world->num_edicts-1) || seen[b->entity] ||
           ED_ISFREE(WEDICT_NUM_PB(world->progs,b->entity)) || !js_int(&r,&b->shape,0,GEOMTYPE_COMPOUND) ||
           !js_int(&r,&b->motion,0,2) || !js_int(&r,&b->model,0,MAX_PRECACHE_MODELS-1) ||
           !js_int(&r,&s->has_target,0,1) || !js_int(&r,&s->active,0,1) ||
           !js_readstate(&r,&s->state) || !js_readstate(&r,&s->target)) goto done;
        {int k;for(k=0;k<4;k++)for(j=0;j<3;j++) if(!js_float(&r,&b->remembered[k][j])) goto done;}
        {int k;for(k=0;k<6;k++)for(j=0;j<3;j++) if(!js_float(&r,&b->qc[k][j])) goto done;}
        for(j=0;j<9;j++)if(!js_float(&r,&b->attributes[j])) goto done;
        seen[b->entity]=1;
        for(j=0;j<3;j++) if(!js_float(&r,&s->driven_velocity[j])) goto done;
        for(j=0;j<3;j++) if(!js_float(&r,&s->driven_spin[j])) goto done;
    }
    for(i=0;i<nj;i++) {
        jolt_saved_joint_t *s=&joints[i];fte_jolt_joint_checkpoint *c=&s->checkpoint;
        if(!js_key(&r,"joint") || !js_int(&r,&s->entity,1,world->num_edicts-1) || seen[s->entity] ||
           ED_ISFREE(WEDICT_NUM_PB(world->progs,s->entity)) ||
           !js_int(&r,&s->enemy,0,world->num_edicts-1) || !js_int(&r,&s->aiment,0,world->num_edicts-1) ||
           !js_int(&r,&c->settings.type,-1,5) || !js_int(&r,&c->settings.point_mode,0,2) ||
           !js_int(&r,&c->enabled,0,1)) goto done;
        seen[s->entity]=2;
        {float *values[]={JS_JOINT_FLOATS(&c->settings)};
         for(j=0;j<(int)countof(values);j++) if(!js_float(&r,values[j])) goto done;}
        if(!js_descriptor(&c->settings)) goto done;
        if(!js_readstate(&r,&c->reference1) || !js_readstate(&r,&c->reference2)) goto done;
        {int k;const float *parts[4];
         for(k=0;k<4;k++) {parts[k]=s->qc[k];for(j=0;j<3;j++)if(!js_float(&r,&s->qc[k][j])) goto done;}
         for(j=0;j<2;j++)if(!js_float(&r,&s->spring[j])) goto done;
         if(!js_joint_values(parts,s->spring)) goto done;}
    }
    if(!js_key(&r,"end")) goto done;
    while(r.at<r.end && (unsigned char)*r.at<=32) ++r.at;
    if(r.at!=r.end) goto done;
    for(i=0;i<nb;i++) {
        const float *parts[6],*attributes[9];int k;
        for(k=0;k<6;k++)parts[k]=bodies[i].qc[k];
        for(k=0;k<9;k++)attributes[k]=&bodies[i].attributes[k];
        if(!js_entity_values(bodies[i].shape,parts,attributes)) goto done;
    }
    /* Preserve full float precision and distinguish pending QC edits from the
     * last synchronized transform. Ordinary QC entity text uses fewer digits. */
    for(i=0;i<nb;i++) {
        jolt_saved_body_t *b=&bodies[i];wedict_t *ed=WEDICT_NUM_PB(world->progs,b->entity);
        float *parts[]={ed->v->origin,ed->v->angles,ed->v->velocity,ed->v->avelocity,ed->v->mins,ed->v->maxs};
        float *attributes[]={JS_ENTITY_ATTRS(ed)};int k;
        for(k=0;k<6;k++)for(j=0;j<3;j++)parts[k][j]=b->qc[k][j];
        for(j=0;j<9;j++) *attributes[j]=b->attributes[j];
        VectorSubtract(ed->v->maxs,ed->v->mins,ed->v->size);
        rbeapi->LinkEdict(world,ed,qfalse);
    }
    /* Only now construct native objects. A rejected parse never reaches Jolt. */
    for(i=0;i<(int)world->num_edicts;i++) {
        wedict_t *ed=WEDICT_NUM_PB(world->progs,i);
        if(!ED_ISFREE(ed)) sync_entity(world,ed);
        if(ed->rbe.body.body && ((jolt_entity_t*)ed->rbe.body.body)->handle && seen[i]!=1) goto done;
    }
    for(i=0;i<nb;i++) {
        jolt_saved_body_t *s=&bodies[i];wedict_t *ed=WEDICT_NUM_PB(world->progs,s->entity);
        jolt_entity_t *body=(jolt_entity_t*)ed->rbe.body.body;
        if(!body || !body->handle || body->shape!=s->shape || body->motion!=s->motion || body->modelindex!=s->model ||
           !fte_jolt_runtime_set(ctx->simulation,body->handle,&s->runtime)) goto done;
        VectorCopy(s->remembered[0],ed->rbe.origin);VectorCopy(s->remembered[1],ed->rbe.angles);
        VectorCopy(s->remembered[2],ed->rbe.velocity);VectorCopy(s->remembered[3],ed->rbe.avelocity);
    }
    for(i=0;i<nj;i++) {
        jolt_saved_joint_t *s=&joints[i];wedict_t *ed=WEDICT_NUM_PB(world->progs,s->entity);jolt_joint_t *joint;
        {float *parts[]={ed->v->origin,ed->v->angles,ed->v->velocity,ed->v->movedir};int k;
         for(k=0;k<4;k++)for(j=0;j<3;j++)parts[k][j]=s->qc[k][j];
         ed->xv->joint_suspension_stiffness=s->spring[0];ed->xv->joint_suspension_damping=s->spring[1];
         rbeapi->LinkEdict(world,ed,qfalse);}
        if((int)ed->xv->jointtype!=s->checkpoint.settings.type || ed->v->enemy!=s->enemy || ed->v->aiment!=s->aiment ||
           !joint_body(world,s->enemy,&s->checkpoint.settings.body1) || !joint_body(world,s->aiment,&s->checkpoint.settings.body2)) goto done;
        joint=(jolt_joint_t*)calloc(1,sizeof(*joint));if(!joint) goto done;
        ed->rbe.joint.joint=joint;joint->simulation=ctx->simulation;
        joint->settings=joint->requested=s->checkpoint.settings;joint->enemy=s->enemy;joint->aiment=s->aiment;
        joint->handle=fte_jolt_joint_checkpoint_add(ctx->simulation,&s->checkpoint);
        if(!joint->handle) goto done;
        ed->rbe.joint_type=s->checkpoint.settings.type;
    }
    /* Constraint creation wakes bodies; restore explicit activation last. */
    for(i=0;i<nb;i++) {
        wedict_t *ed=WEDICT_NUM_PB(world->progs,bodies[i].entity);jolt_entity_t *body=(jolt_entity_t*)ed->rbe.body.body;
        if(!fte_jolt_runtime_set(ctx->simulation,body->handle,&bodies[i].runtime)) goto done;
    }
    if(!fte_jolt_clock_set(ctx->simulation,remainder,simulated)) goto done;
    world->rbe_hasphysicsents=qtrue;ok=qtrue;
    Con_Printf("Jolt entity scene restored (%d bodies, %d joints).\n",nb,nj);
done:
    free(bodies);free(joints);free(seen);return ok;
}
#undef JS_JOINT_FLOATS
#undef JS_ENTITY_ATTRS
