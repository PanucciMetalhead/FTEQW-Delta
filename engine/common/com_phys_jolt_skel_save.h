/* Numeric/hex skeletal extension to the entity checkpoint. No VM/native pointers. */
typedef struct {
    rbeskel_t view;
    float owner[4][3];
    float *pose;
    char *definition;
    rbeskelbody_t *metadata;
    fte_jolt_rig_runtime *runtime;
    fte_jolt_joint_checkpoint *joint;
    rbejoint_t *slots;
} jolt_saved_skel_t;
static qboolean js_pose(jolt_reader_t *r, float *pose, int bones) {
    int i, j;
    for (i = 0; i < bones; i++) {
        if (!js_key(r, "pose"))
            return qfalse;
        for (j = 0; j < 12; j++)
            if (!js_float(r, &pose[i * 12 + j]) || fabs(pose[i * 12 + j]) > 10000000)
                return qfalse;
    }
    return qtrue;
}
static qboolean js_save_skeletons(world_t *world, jolt_output_t *out, jolt_line_t *line) {
    jolt_context_t *ctx = context(world);
    rbeskel_t views[1024];
    int id, n = 0, i, j, k, totalbones = 0, totalbodies = 0, totaljoints = 0, actual = 0;
    size_t definitions = 0;
    jolt_rag_body_t *rag;
    if (rbeapi->SkelCheckpointGet)
        for (id = 1; id <= 1024; id++)
            if (rbeapi->SkelCheckpointGet(world, id, &views[n])) {
                rbeskel_t *v = &views[n++];
                if (v->id != id || v->modelindex < 1 || v->modelindex >= MAX_PRECACHE_MODELS ||
                    v->numbones < 1 || v->numbones > MAX_BONES || (v->type != 0 && v->type != 1) ||
                    !v->bonematrix || v->numbodies < 0 || v->numbodies > 8192 || v->numjoints < 0 ||
                    v->numjoints > 16384 || (totalbones += v->numbones) > 32768 ||
                    (totalbodies += v->numbodies) > 8192 || (totaljoints += v->numjoints) > 16384)
                    goto rejected;
                if (v->animsource) {
                    rbeskel_t source;
                    if (v->animsource == id ||
                        !rbeapi->SkelCheckpointGet(world, v->animsource, &source) ||
                        source.type != 0 || source.numbones != v->numbones)
                        goto rejected;
                }
                if (v->numbodies) {
                    wedict_t *ed;
                    if (v->type != 1 || v->entity < 1 || v->entity >= world->num_edicts ||
                        !v->definition || !*v->definition || strlen(v->definition) > 1048576 ||
                        (definitions += strlen(v->definition)) > 2097152 || !v->body || !v->joint)
                        goto rejected;
                    ed = WEDICT_NUM_PB(world->progs, v->entity);
                    if (ED_ISFREE(ed) || ed->xv->skeletonindex != id)
                        goto rejected;
                } else if (v->numjoints || v->entity || v->animsource)
                    goto rejected;
            }
    for (rag = ctx->rag_bodies; rag; rag = rag->next)
        if (rag->handle)
            ++actual;
    if (actual != totalbodies)
        goto rejected;
    js_add(line, "skeletons %d\n", n);
    if (!js_emit(out, line))
        return qfalse;
    for (i = 0; i < n; i++) {
        rbeskel_t *v = &views[i];
        js_add(line, "skeleton %d %d %d %d %d %d %d %d\n", v->id, v->modelindex, v->type,
               v->numbones, v->animsource, v->entity, v->numbodies, v->numjoints);
        if (!js_emit(out, line))
            return qfalse;
        for (j = 0; j < v->numbones; j++) {
            js_add(line, "pose");
            for (k = 0; k < 12; k++) {
                float value = v->bonematrix[j * 12 + k];
                if (!isfinite(value) || fabs(value) > 10000000)
                    goto rejected;
                js_add(line, " %.9g", (double)value);
            }
            js_add(line, "\n");
            if (!js_emit(out, line))
                return qfalse;
        }
        if (!v->numbodies)
            continue;
        {
            wedict_t *ed = WEDICT_NUM_PB(world->progs, v->entity);
            const float *parts[] = {ed->v->origin, ed->v->angles, ed->v->velocity,
                                    ed->v->avelocity};
            js_add(line, "owner");
            for (j = 0; j < 4; j++)
                for (k = 0; k < 3; k++) {
                    if (!isfinite(parts[j][k]) || fabs(parts[j][k]) > 10000000)
                        goto rejected;
                    js_add(line, " %.9g", (double)parts[j][k]);
                }
            js_add(line, "\n");
            if (!js_emit(out, line))
                return qfalse;
        }
        {
            size_t bytes = strlen(v->definition), offset;
            js_add(line, "definition %u\n", (unsigned)bytes);
            if (!js_emit(out, line))
                return qfalse;
            for (offset = 0; offset < bytes;) {
                size_t end = offset + 30;
                if (end > bytes)
                    end = bytes;
                js_add(line, "hex ");
                while (offset < end)
                    js_add(line, "%02x", (unsigned char)v->definition[offset++]);
                js_add(line, "\n");
                if (!js_emit(out, line))
                    return qfalse;
            }
        }
        for (j = 0; j < v->numbodies; j++) {
            jolt_rag_body_t *body = (jolt_rag_body_t *)v->body[j].odebody.body;
            fte_jolt_rig_runtime saved;
            fte_jolt_runtime *s = &saved.runtime;
            if (!body || !body->handle || body->world != world ||
                !fte_jolt_rig_runtime_get(ctx->simulation, body->handle, &saved) ||
                saved.strength != v->body[j].animstrength)
                goto rejected;
            js_add(line, "rigbody %d %.9g %d %d", j, (double)saved.strength, s->active,
                   s->has_target);
            js_state(line, &s->state);
            js_state(line, &s->target);
            for (k = 0; k < 3; k++)
                js_add(line, " %.9g", (double)s->driven_velocity[k]);
            for (k = 0; k < 3; k++)
                js_add(line, " %.9g", (double)s->driven_spin[k]);
            js_state(line, &saved.animation_pose);
            for (k = 0; k < 3; k++)
                js_add(line, " %.9g", (double)saved.motor_velocity[k]);
            for (k = 0; k < 3; k++)
                js_add(line, " %.9g", (double)saved.motor_spin[k]);
            for (k = 0; k < 12; k++) {
                float value = v->body[j].animmatrix[k];
                if (!isfinite(value) || fabs(value) > 10000000)
                    goto rejected;
                js_add(line, " %.9g", (double)value);
            }
            js_add(line, "\n");
            if (!js_emit(out, line))
                return qfalse;
        }
        for (j = 0; j < v->numjoints; j++) {
            jolt_joint_t *joint = (jolt_joint_t *)v->joint[j].joint;
            fte_jolt_joint_checkpoint saved;
            if (!joint || !joint->handle ||
                !fte_jolt_joint_checkpoint_get(ctx->simulation, joint->handle, &saved) ||
                !js_descriptor(&saved.settings))
                goto rejected;
            js_add(line, "rigjoint %d %d %d %d", j, saved.settings.type, saved.settings.point_mode,
                   saved.enabled);
            {
                const float *values[] = {JS_JOINT_FLOATS(&saved.settings)};
                for (k = 0; k < countof(values); k++)
                    js_add(line, " %.9g", (double)*values[k]);
            }
            js_state(line, &saved.reference1);
            js_state(line, &saved.reference2);
            js_add(line, "\n");
            if (!js_emit(out, line))
                return qfalse;
        }
    }
    return qtrue;
rejected:
    Con_Printf("Jolt: skeletal checkpoint rejected; use valid, bound, world-owned resources within "
               "save limits.\n");
    return qfalse;
}
static void js_free_skeletons(jolt_saved_skel_t *skels, int count) {
    int i;
    if (!skels)
        return;
    for (i = 0; i < count; i++) {
        free(skels[i].pose);
        free(skels[i].definition);
        free(skels[i].metadata);
        free(skels[i].runtime);
        free(skels[i].joint);
        free(skels[i].slots);
    }
    free(skels);
}
static int js_hex(int c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}
static qboolean js_read_skeletons(world_t *world, jolt_reader_t *r, jolt_saved_skel_t **out,
                                  int *count, int nb, int nj) {
    int n, i, j, k, bones = 0, bodies = nb, joints = nj;
    size_t definitions = 0;
    unsigned char ids[1025] = {0};
    jolt_saved_skel_t *skels;
    if (!js_key(r, "skeletons") || !js_int(r, &n, 0, 1024))
        return qfalse;
    *count = n;
    *out = skels = (jolt_saved_skel_t *)calloc(n ? n : 1, sizeof(*skels));
    if (!skels)
        return qfalse;
    for (i = 0; i < n; i++) {
        jolt_saved_skel_t *s = &skels[i];
        rbeskel_t *v = &s->view;
        if (!js_key(r, "skeleton") || !js_int(r, &v->id, 1, 1024) || ids[v->id]++ ||
            !js_int(r, &v->modelindex, 1, MAX_PRECACHE_MODELS - 1) || !js_int(r, &v->type, 0, 1) ||
            !js_int(r, &v->numbones, 1, MAX_BONES) || (bones += v->numbones) > 32768 ||
            !js_int(r, &v->animsource, 0, 1024) ||
            !js_int(r, &v->entity, 0, world->num_edicts - 1) ||
            !js_int(r, &v->numbodies, 0, 8192) || (bodies += v->numbodies) > 8192 ||
            !js_int(r, &v->numjoints, 0, 16384) || (joints += v->numjoints) > 16384)
            return qfalse;
        s->pose = (float *)calloc(v->numbones * 12, sizeof(float));
        v->bonematrix = s->pose;
        if (!s->pose || !js_pose(r, s->pose, v->numbones))
            return qfalse;
        if (!v->numbodies) {
            if (v->entity || v->numjoints || v->animsource)
                return qfalse;
            continue;
        }
        if (v->type != 1 || !v->entity || ED_ISFREE(WEDICT_NUM_PB(world->progs, v->entity)))
            return qfalse;
        if (!js_key(r, "owner"))
            return qfalse;
        for (j = 0; j < 4; j++)
            for (k = 0; k < 3; k++)
                if (!js_float(r, &s->owner[j][k]) || fabs(s->owner[j][k]) > 10000000)
                    return qfalse;
        {
            int length;
            size_t at = 0;
            if (!js_key(r, "definition") || !js_int(r, &length, 1, 1048576) ||
                (definitions += length) > 2097152)
                return qfalse;
            s->definition = (char *)calloc((size_t)length + 1, 1);
            v->definition = s->definition;
            if (!s->definition)
                return qfalse;
            while (at < (size_t)length) {
                char hex[64];
                size_t size, h;
                if (!js_key(r, "hex") || !js_word(r, hex) || !(size = strlen(hex)) || (size & 1) ||
                    size / 2 > (size_t)length - at)
                    return qfalse;
                for (h = 0; h < size; h += 2) {
                    int a = js_hex(hex[h]), b = js_hex(hex[h + 1]);
                    if (a < 0 || b < 0 || !(a || b))
                        return qfalse;
                    s->definition[at++] = (char)((a << 4) | b);
                }
            }
        }
        s->metadata = (rbeskelbody_t *)calloc(v->numbodies, sizeof(*s->metadata));
        v->body = s->metadata;
        s->runtime = (fte_jolt_rig_runtime *)calloc(v->numbodies, sizeof(*s->runtime));
        s->joint =
            (fte_jolt_joint_checkpoint *)calloc(v->numjoints ? v->numjoints : 1, sizeof(*s->joint));
        s->slots = (rbejoint_t *)calloc(v->numjoints ? v->numjoints : 1, sizeof(*s->slots));
        v->joint = s->slots;
        if (!s->metadata || !s->runtime || !s->joint || !s->slots)
            return qfalse;
        for (j = 0; j < v->numbodies; j++) {
            int index;
            fte_jolt_rig_runtime *saved = &s->runtime[j];
            fte_jolt_runtime *b = &saved->runtime;
            if (!js_key(r, "rigbody") || !js_int(r, &index, j, j) ||
                !js_float(r, &saved->strength) || saved->strength < 0 || saved->strength > 1 ||
                !js_int(r, &b->active, 0, 1) || !js_int(r, &b->has_target, 0, 1) ||
                (b->has_target && saved->strength == 0) || !js_readstate(r, &b->state) ||
                !js_readstate(r, &b->target))
                return qfalse;
            for (k = 0; k < 3; k++)
                if (!js_float(r, &b->driven_velocity[k]))
                    return qfalse;
            for (k = 0; k < 3; k++)
                if (!js_float(r, &b->driven_spin[k]))
                    return qfalse;
            if (!js_readstate(r, &saved->animation_pose))
                return qfalse;
            for (k = 0; k < 3; k++)
                if (!js_float(r, &saved->motor_velocity[k]))
                    return qfalse;
            for (k = 0; k < 3; k++)
                if (!js_float(r, &saved->motor_spin[k]))
                    return qfalse;
            s->metadata[j].animstrength = saved->strength;
            for (k = 0; k < 12; k++)
                if (!js_float(r, &s->metadata[j].animmatrix[k]) ||
                    fabs(s->metadata[j].animmatrix[k]) > 10000000)
                    return qfalse;
        }
        for (j = 0; j < v->numjoints; j++) {
            int index;
            fte_jolt_joint_checkpoint *c = &s->joint[j];
            if (!js_key(r, "rigjoint") || !js_int(r, &index, j, j) ||
                !js_int(r, &c->settings.type, -1, 5) || !js_int(r, &c->settings.point_mode, 0, 2) ||
                !js_int(r, &c->enabled, 0, 1))
                return qfalse;
            {
                float *values[] = {JS_JOINT_FLOATS(&c->settings)};
                for (k = 0; k < countof(values); k++)
                    if (!js_float(r, values[k]))
                        return qfalse;
            }
            if (!js_descriptor(&c->settings) || !js_readstate(r, &c->reference1) ||
                !js_readstate(r, &c->reference2))
                return qfalse;
        }
    }
    return qtrue;
}
static qboolean js_restore_skeletons(world_t *world, jolt_saved_skel_t *skels, int count) {
    jolt_context_t *ctx = context(world);
    int i, j;
    if (count && (!rbeapi->SkelCheckpointRestore || !rbeapi->SkelCheckpointGet ||
                  !rbeapi->SkelCheckpointClear))
        return qfalse;
    if (rbeapi->SkelCheckpointClear)
        rbeapi->SkelCheckpointClear(world);
    for (i = 0; i < count; i++)
        if (!rbeapi->SkelCheckpointRestore(world, &skels[i].view, qfalse))
            return qfalse;
    for (i = 0; i < count; i++) {
        jolt_saved_skel_t *s = &skels[i];
        rbeskel_t v;
        if (!rbeapi->SkelCheckpointRestore(world, &s->view, qtrue) ||
            !rbeapi->SkelCheckpointGet(world, s->view.id, &v))
            return qfalse;
        for (j = 0; j < v.numbodies; j++) {
            fte_jolt_rig_runtime stage = s->runtime[j];
            jolt_rag_body_t *body = (jolt_rag_body_t *)v.body[j].odebody.body;
            stage.strength = 0;
            stage.runtime.has_target = 0;
            if (!body || !fte_jolt_rig_runtime_set(ctx->simulation, body->handle, &stage))
                return qfalse;
        }
        for (j = 0; j < v.numjoints; j++) {
            fte_jolt_joint_checkpoint *c = &s->joint[j];
            const rbejointinfo_t *info = &v.jointinfo[j];
            jolt_joint_t *joint;
            if (c->settings.type != info->type || info->body1 < -1 || info->body1 >= v.numbodies ||
                info->body2 < -1 || info->body2 >= v.numbodies)
                return qfalse;
            c->settings.body1 =
                info->body1 < 0 ? 0 : ((jolt_rag_body_t *)v.body[info->body1].odebody.body)->handle;
            c->settings.body2 =
                info->body2 < 0 ? 0 : ((jolt_rag_body_t *)v.body[info->body2].odebody.body)->handle;
            joint_free(&v.joint[j]);
            joint = (jolt_joint_t *)calloc(1, sizeof(*joint));
            if (!joint)
                return qfalse;
            v.joint[j].joint = joint;
            joint->simulation = ctx->simulation;
            joint->settings = joint->requested = c->settings;
            joint->handle = fte_jolt_joint_checkpoint_add(ctx->simulation, c);
            if (!joint->handle)
                return qfalse;
        }
        for (j = 0; j < v.numbodies; j++) {
            jolt_rag_body_t *body = (jolt_rag_body_t *)v.body[j].odebody.body;
            if (!fte_jolt_rig_runtime_set(ctx->simulation, body->handle, &s->runtime[j]))
                return qfalse;
        }
    }
    /* A later kinematic handoff can wake an earlier connected body. Apply
     * activation only after every rig's motion types and motors are restored. */
    for (i = 0; i < count; i++) {
        rbeskel_t v;
        if (!rbeapi->SkelCheckpointGet(world, skels[i].view.id, &v))
            return qfalse;
        for (j = 0; j < v.numbodies; j++) {
            jolt_rag_body_t *body = (jolt_rag_body_t *)v.body[j].odebody.body;
            if (!fte_jolt_runtime_activation(ctx->simulation, body->handle,
                                             skels[i].runtime[j].runtime.active))
                return qfalse;
        }
    }
    if (count)
        Con_Printf("Jolt skeletal resources restored (%d skeletons).\n", count);
    return qtrue;
}
