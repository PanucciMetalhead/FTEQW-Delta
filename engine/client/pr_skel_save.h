/* World-owned skeleton resources for bounded backend checkpoints. */
static qboolean skel_checkpoint_doll(doll_t *);
qboolean QDECL skel_checkpoint_get(world_t *world, int id, rbeskel_t *out) {
    skelobject_t *s = skel_get(world, id);
    if (!s || !world->progs || !out)
        return false;
    memset(out, 0, sizeof(*out));
    out->id = id;
    out->modelindex = s->modelindex;
    out->type = s->type;
    out->numbones = s->numbones;
    out->bonematrix = s->bonematrix;
    if (s->doll) {
        out->entity = s->entity ? s->entity->entnum : 0;
        out->animsource = s->animsource ? (int)(s->animsource - skelobjects) + 1 : 0;
        out->numbodies = s->numbodies;
        out->numjoints = s->numjoints;
        out->definition = skel_checkpoint_doll(s->doll) ? s->doll->definition : NULL;
        out->body = s->body;
        out->joint = s->joint;
        out->jointinfo = s->doll->joint;
    }
    return true;
}
static qboolean skel_checkpoint_matrices(const float *data, int count) {
    int i;
    if (!data || count < 0 || count > MAX_BONES)
        return false;
    for (i = 0; i < count * 12; i++)
        if (!isfinite(data[i]) || fabs(data[i]) > 10000000)
            return false;
    return true;
}
/* Bound the existing authored-text parser before it can allocate definitions. */
static qboolean skel_checkpoint_text(const rbeskel_t *v) {
    const char *text = v->definition;
    int bodies = 0, joints = 0, lines = 0;
    if (!text || !*text || strlen(text) > 1048576)
        return false;
    while (text && *text) {
        if (++lines > 131072)
            return false;
        text = Cmd_TokenizeString(text, false, false);
        if (!stricmp(Cmd_Argv(0), "body") && ++bodies > v->numbodies)
            return false;
        if (!stricmp(Cmd_Argv(0), "joint") && ++joints > v->numjoints)
            return false;
    }
    return bodies == v->numbodies && joints == v->numjoints;
}
static qboolean skel_checkpoint_doll(doll_t *d) {
    int i, j;
    if (!d || !isfinite(d->refanimtime) || fabs(d->refanimtime) > 10000000 || d->refanim < -1 ||
        d->refanim > 65535)
        return false;
    for (i = 0; i < d->numbodies; i++) {
        rbebodyinfo_t *b = &d->body[i];
        if (b->bone < 0 || b->bone >= d->numbones ||
            (b->orient && (b->orientpeer < 0 || b->orientpeer >= d->numbones)) ||
            !isfinite(b->animate) || b->animate < 0 || b->animate > 1 || !isfinite(b->mass) ||
            b->mass < .000001 || b->mass > 1000000000 ||
            !skel_checkpoint_matrices(b->relmatrix, 1) ||
            !skel_checkpoint_matrices(b->inverserelmatrix, 1))
            return false;
        for (j = 0; j < 3; j++)
            if (!isfinite(b->dimensions[j]) || b->dimensions[j] < 0 || b->dimensions[j] > 1000000)
                return false;
    }
    for (i = 0; i < d->numjoints; i++) {
        rbejointinfo_t *j = &d->joint[i];
        const float *values[] = {&j->FMax,
                                 &j->FMax2,
                                 &j->HiStop,
                                 &j->HiStop2,
                                 &j->LoStop,
                                 &j->LoStop2,
                                 &j->CFM,
                                 &j->CFM2,
                                 &j->ERP,
                                 &j->ERP2,
                                 &j->Vel,
                                 &j->Vel2,
                                 &j->suspension_stiffness,
                                 &j->suspension_damping};
        int k;
        if (j->body1 < -1 || j->body1 >= d->numbodies || j->body2 < -1 ||
            j->body2 >= d->numbodies || j->body1 == j->body2 || j->bonepivot < 0 ||
            j->bonepivot >= d->numbones)
            return false;
        for (k = 0; k < countof(values); k++)
            if (!isfinite(*values[k]) || fabs(*values[k]) > 1e21)
                return false;
        for (k = 0; k < 3; k++)
            if (!isfinite(j->offset[k]) || !isfinite(j->axis[k]) || !isfinite(j->axis2[k]) ||
                fabs(j->offset[k]) > 10000000 || fabs(j->axis[k]) > 10000000 ||
                fabs(j->axis2[k]) > 10000000)
                return false;
    }
    return true;
}
static model_t *skel_checkpoint_model(world_t *world, const rbeskel_t *v) {
    model_t *model;
    if (!world || !world->progs || !v || v->id < 1 || v->id > MAX_SKEL_OBJECTS || v->numbones < 1 ||
        v->numbones > MAX_BONES || (v->type != SKEL_RELATIVE && v->type != SKEL_ABSOLUTE) ||
        v->modelindex < 1 || v->modelindex >= MAX_PRECACHE_MODELS ||
        !skel_checkpoint_matrices(v->bonematrix, v->numbones))
        return NULL;
    model = world->Get_CModel(world, v->modelindex);
    if (!model || model->loadstate != MLS_LOADED ||
        Mod_GetNumBones(model, v->type != SKEL_RELATIVE) != v->numbones)
        return NULL;
    return model;
}
static qboolean skel_checkpoint_allocate(world_t *world, const rbeskel_t *v) {
    model_t *model = skel_checkpoint_model(world, v);
    skelobject_t *s;
    if (!model)
        return false;
    s = &skelobjects[v->id - 1];
    /* Handles are also plain QC floats in globals: retain the exact ID.
     * A foreign world's live or cached allocation must never be stolen. */
    if (s->inuse || (s->world && s->world != world))
        return false;
    if (s->ownedmatrix)
        BZ_Free(s->bonematrix);
    memset(s, 0, sizeof(*s));
    s->world = world;
    s->model = model;
    s->modelindex = v->modelindex;
    s->numbones = v->numbones;
    s->type = v->type;
    s->bonematrix =
        (float *)PR_AddString(world->progs, "", sizeof(float) * 12 * v->numbones, false);
    memcpy(s->bonematrix, v->bonematrix, sizeof(float) * 12 * v->numbones);
    s->inuse = 1;
    if (numskelobjectsused < v->id)
        numskelobjectsused = v->id;
    return true;
}
qboolean QDECL skel_checkpoint_restore(world_t *world, const rbeskel_t *v, qboolean rig) {
    skelobject_t *s, *source = NULL;
    model_t *model;
    wedict_t *entity;
    int i;
    doll_t *d;
    float emat[12];
    vec3_t angles, forward, right, up;
    if (!rig)
        return skel_checkpoint_allocate(world, v);
    model = skel_checkpoint_model(world, v);
    if (!model)
        return false;
    s = &skelobjects[v->id - 1];
    if (s->inuse != 1 || s->world != world || s->model != model || s->doll)
        return false;
    if (!v->numbodies)
        return !v->numjoints && !v->animsource;
    if (v->numbodies < 0 || v->numbodies > 8192 || v->numjoints < 0 || v->numjoints > 16384 ||
        v->type != SKEL_ABSOLUTE || !v->body || !v->joint || v->entity < 1 ||
        v->entity >= world->num_edicts || !world->rbe || !world->rbe->RagSetBodyAnimation ||
        !skel_checkpoint_text(v))
        return false;
    entity = WEDICT_NUM_PB(world->progs, v->entity);
    if (ED_ISFREE(entity) || entity->xv->skeletonindex != v->id)
        return false;
    if (v->animsource) {
        source = skel_get(world, v->animsource);
        if (!source || source == s || source->type != SKEL_RELATIVE ||
            source->numbones != v->numbones)
            return false;
    }
    for (i = 0; i < v->numbodies; i++)
        if (!isfinite(v->body[i].animstrength) || v->body[i].animstrength < 0 ||
            v->body[i].animstrength > 1 || !skel_checkpoint_matrices(v->body[i].animmatrix, 1))
            return false;
    d = rag_createdollfromstring(model, "<checkpoint>", v->numbones, v->definition);
    rag_cachedoll(d);
    if (!skel_checkpoint_doll(d) || d->numbodies != v->numbodies || d->numjoints != v->numjoints) {
        if (d)
            rag_freedoll(d);
        return false;
    }
    VectorCopy(entity->v->angles, angles);
    angles[0] *= r_meshpitch.value;
    angles[2] *= r_meshroll.value;
    AngleVectors(angles, forward, right, up);
    bonemat_fromqcvectors(emat, forward, right, up, entity->v->origin);
    if (!rag_instanciate(s, d, emat, entity)) {
        rag_freedoll(d);
        return false;
    }
    /* Restore joints while all bodies are dynamic; the backend restores the
     * saved passive/kinematic/fractional mask after the original joint frames. */
    if (!rag_setanimation(s, -1, 0)) {
        rag_uninstanciate(s);
        rag_freedoll(d);
        return false;
    }
    s->entity = entity;
    s->animsource = source;
    s->numanimated = 0;
    for (i = 0; i < v->numbodies; i++) {
        s->body[i].animstrength = v->body[i].animstrength;
        memcpy(s->body[i].animmatrix, v->body[i].animmatrix, sizeof(s->body[i].animmatrix));
        s->numanimated += s->body[i].animstrength != 0;
    }
    return true;
}
