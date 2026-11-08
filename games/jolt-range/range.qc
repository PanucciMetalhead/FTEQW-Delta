// Jolt Range: all forces and moving hazard/corpse transforms run in SSQC.
#define RANGE_ROCK "progs/range_boulder.md5mesh"
#define RANGE_CHIP "progs/range_chip.md5mesh"
#define RANGE_SOLDIER "progs/range_grunt.md5mesh"
#define RANGE_CONVEX 12
#define RANGE_COMPOUND 13
#define RANGE_BOX 1
#define RANGE_CORPSE_DIM 2

float range_active;
void(entity rag) range_ragdoll_pose;

void() range_init =
{
    if (mapname != "jolt_range")
        return;
    if (!cvar("physics_jolt_enabled") || !physics_supported())
        error("Jolt Range needs the Jolt plugin. Load it and set physics_jolt_enabled 1 before map jolt_range.");
    range_active = TRUE;
    world.dimension_hit |= RANGE_CORPSE_DIM;
    precache_model(RANGE_ROCK);
    precache_model(RANGE_CHIP);
    precache_model(RANGE_SOLDIER);
    precache_sound("doors/basetry.wav");
};

void(entity soldier) range_end_push =
{
    if (!soldier.range_push_until)
        return;
    soldier.movetype = MOVETYPE_STEP;
    soldier.solid = soldier.range_saved_solid;
    soldier.geomtype = 0;
    soldier.mass = soldier.friction = soldier.damp_linear = soldier.damp_angular = 0;
    soldier.angles = '0 1 0' * soldier.range_yaw;
    soldier.velocity = soldier.avelocity = '0 0 0';
    soldier.range_push_until = 0;
    soldier.range_force_left = 0;
    soldier.range_pellets = '0 0 0';
    setorigin(soldier, soldier.origin);
};

void() range_frame =
{
    local entity soldier, rock;
    local vector center;
    local float force_time;
    if (!range_active)
        return;
    // The contact callback sees post-solver velocity. Keep the incoming speed too.
    for (rock = find(world, classname, "range_rock"); rock; rock = find(rock, classname, "range_rock"))
        rock.range_impact = rock.velocity;

    // Keep AD's pending animation/AI function; let Jolt carry the hull during the hit.
    for (soldier = find(world, classname, "monster_army"); soldier; soldier = find(soldier, classname, "monster_army")) {
        if (soldier.range_push_until) {
            if (soldier.range_push_until <= time)
                range_end_push(soldier);
            else {
                if (soldier.nextthink > 0 && soldier.nextthink <= time + frametime)
                    soldier.nextthink = time + frametime + 0.001;
                if (soldier.range_force_left > 0 && frametime > 0) {
                    force_time = min(soldier.range_force_left, frametime);
                    center = soldier.origin + 0.5 * (soldier.mins + soldier.maxs);
                    physics_addforce(soldier, soldier.range_pellets * (soldier.mass * force_time / frametime), center);
                    soldier.range_force_left -= force_time;
                }
            }
        }
    }
};

float(entity source) range_is_blast =
{
    return source.classgroup == CG_PROJROCKETS || source.classgroup == CG_PROJGRENADES;
};

float(entity target, entity source, entity attacker, float amount) range_soldier_hit =
{
    local vector center, direction;
    local float speed;
    if (!range_active || target.classtype != CT_MONARMY || amount <= 0)
        return FALSE;

    center = target.origin + 0.5 * (target.mins + target.maxs);
    if (range_is_blast(source)) {
        direction = normalize(center - source.origin);
        direction_z = max(0.35, direction_z + 0.3);
        // Keep the normal AD damage/gib thresholds. A surviving grunt gets back up.
        target.range_launch = normalize(direction) * min(380, 90 + amount * 3);
        if (target.health >= 1)
            return range_knockdown(target);
        target.range_fatal_shot = TRUE;
        return FALSE;
    }
    if (source.classgroup != CG_PROJSHELLS)
        return FALSE;
    speed = vlen(source.range_impact);
    if (speed <= 0)
        return FALSE;
    direction = source.range_impact / speed;
    if (target.health < 1) {
        // Keep any motion from earlier pellets when handing the body to the rig.
        target.range_launch = target.velocity + direction * min(75, speed * 0.035);
        target.range_fatal_shot = TRUE;
        return FALSE;
    }
    if (!(attacker.flags & FL_CLIENT))
        return FALSE;
    if (!target.range_push_until) {
        if (target.movetype != MOVETYPE_STEP)
            return FALSE;
        target.range_yaw = target.angles_y;
        target.range_saved_solid = target.solid;
        target.movetype = MOVETYPE_PHYSICS;
        // The bridge initializes SOLID_SLIDEBOX at zero angles.
        target.solid = SOLID_PHYSICS_BOX;
        target.geomtype = RANGE_BOX;
        target.mass = 75;
        target.friction = 0.11;
        target.damp_linear = 3.5;
        target.damp_angular = 12;
        target.flags = target.flags - (target.flags & FL_ONGROUND);
    }
    target.range_push_until = time + 0.28;
    // Spread each impact across a short contact window so a low or uneven
    // server tick cannot swallow the shove. Jolt resolves friction and movement.
    if (target.range_force_left <= 0)
        target.range_pellets = '0 0 0';
    target.range_pellets += direction * 1000 * min(1.4, speed / 1500);
    if (vlen(target.range_pellets) > 3000)
        target.range_pellets = normalize(target.range_pellets) * 3000;
    target.range_force_left = 0.080;
    return FALSE;
};

entity(vector at, vector lower, vector upper, float bodymass, float shape) range_body =
{
    local entity e = spawn();
    e.movetype = MOVETYPE_PHYSICS;
    e.geomtype = shape;
    e.mass = bodymass;
    e.friction = 0.78;
    e.damp_angular = 0.12;
    setsize(e, lower, upper);
    setorigin(e, at);
    return e;
};

void() range_rock_touch =
{
    local float speed;
    if (!(other.flags & FL_CLIENT) || other.health <= 0 || self.pain_finished > time)
        return;
    speed = max(vlen(self.velocity), vlen(self.range_impact));
    if (speed < 100)
        return;
    self.pain_finished = time + 0.55;
    T_Damage(other, self, world, min(65, speed * 0.13), DAMARMOR);
};

void() range_rock_remove =
{
    if (self.range_visual) {
        skel_delete(self.range_visual.skeletonindex);
        remove(self.range_visual);
    }
    remove(self);
};

void() range_rock_break =
{
    local entity piece;
    local float i;
    local vector dir;
    SpawnExplosion(EXPLODE_SMALL, self.origin, SOUND_REXP3);
    for (i = 0; i < 5; i += 1) {
        dir = normalize([crandom(), crandom(), 0.3 + random()]);
        piece = range_body(self.origin + dir * 24, '-9 -9 -9', '9 9 9', 9, RANGE_CONVEX);
        piece.classname = "range_rock_shard";
        setmodel(piece, RANGE_CHIP);
        piece.angles = [random() * 180, random() * 360, random() * 180];
        piece.bouncefactor = 0.26;
        piece.velocity = self.velocity * 0.35 + dir * (110 + random() * 90);
        piece.think = SUB_Remove;
        piece.nextthink = time + 7;
    }
    dprint("JOLT_RANGE rock shattered\n");
    range_rock_remove();
};

float(entity target, entity source, float amount) range_hit =
{
    if (!range_active || target.classname != "range_rock")
        return FALSE;
    if (range_is_blast(source) && amount > 0) {
        target.health -= amount;
        if (target.health <= 0) {
            // AD radius damage walks .chain; defer removal until after its pass.
            target.takedamage = DAMAGE_NO;
            target.think = range_rock_break;
            target.nextthink = time + 0.01;
        }
        else {
            physics_addforce(target, normalize(target.origin - source.origin + '0 0 16') *
                (target.mass * amount / max(frametime, 0.008333)), target.origin);
        }
    }
    return TRUE;
};

void() range_release =
{
    local entity rock = range_body(self.origin, '-46 -46 -46', '46 46 46', 300, RANGE_COMPOUND);
    local entity visual = spawn();
    setmodel(visual, RANGE_ROCK);
    visual.basebone = -1;
    visual.skeletonindex = skel_create(visual.modelindex);
    if (!visual.skeletonindex) {
        remove(visual);
        remove(rock);
        error("Jolt Range could not allocate a boulder pose.");
        return;
    }
    rock.range_visual = visual;
    rock.classname = "range_rock";
    rock.physics_shape = "progs/range_boulder.jolt";
    rock.solid = SOLID_PHYSICS_BOX;
    rock.dimension_hit |= RANGE_CORPSE_DIM;
    rock.angles_y = self.angles_y;
    rock.bouncefactor = 0.44;
    rock.damp_angular = 0.04;
    rock.health = 50;
    rock.takedamage = DAMAGE_AIM;
    rock.touch = range_rock_touch;
    rock.think = range_rock_remove;
    rock.nextthink = time + 24;
    setorigin(rock, self.origin);
    dprint("JOLT_RANGE rock released\n");
};

void() EndFrame =
{
    local entity rock, rag;
    if (!range_active)
        return;
    for (rock = find(world, classname, "range_rock"); rock; rock = find(rock, classname, "range_rock")) {
        // Use the same world basis as the invisible hull. The client receives
        // the mesh rotation in its root bone, independent of alias pitch and
        // Euler angle interpolation at the vertical crossing.
        makevectors(rock.angles);
        skel_set_bone(rock.range_visual.skeletonindex, 1, '0 0 0');
        setorigin(rock.range_visual, rock.origin);
    }
    for (rag = find(world, classname, "range_ragdoll"); rag; rag = find(rag, classname, "range_ragdoll"))
        range_ragdoll_pose(rag);
};

void() range_boulder_use =
{
    if (self.nextthink > 0)
        return;
    sound(self, CHAN_VOICE, "doors/basetry.wav", 1, ATTN_NORM);
    if (activator.flags & FL_CLIENT)
        centerprint(activator, "ROCKFALL!\nMove across the slope.");
    self.think = range_release;
    self.nextthink = time + 0.32;
};

void() range_boulder_spawner =
{
    if (range_active)
        self.use = range_boulder_use;
};

vector(entity soldier, vector localpos) range_grunt_point =
{
    makevectors('0 1 0' * soldier.angles_y);
    return soldier.origin + v_forward * localpos_x - v_right * localpos_y + v_up * localpos_z;
};

entity(entity rag, entity soldier, float bone, vector offset, vector lower, vector upper, float bodymass) range_limb =
{
    local entity limb = range_body(range_grunt_point(soldier, offset), lower, upper, bodymass, RANGE_BOX);
    limb.classname = "range_limb";
    limb.angles = '0 1 0' * soldier.angles_y;
    // Fitted envelopes overlap at the joints. Keep them solid against the
    // world and boulders while letting the joints govern limb interaction.
    limb.dimension_solid = RANGE_CORPSE_DIM;
    limb.dimension_hit = 1;
    limb.range_next = rag.range_parts;
    limb.range_bone = bone;
    limb.damp_angular = 1.2;
    rag.range_parts = limb;
    return limb;
};

void(entity rag, entity soldier, entity parent, entity limb, vector pivot, float kind, float limit) range_joint =
{
    local entity j = spawn();
    j.classname = "range_joint";
    j.enemy = parent;
    j.aiment = limb;
    j.jointtype = kind;
    j.angles = '0 1 0' * (soldier.angles_y + 90);
    j.movedir = '0 -0.001 0' + '0 0 1' * limit;
    j.range_next = rag.range_joints;
    rag.range_joints = j;
    setorigin(j, range_grunt_point(soldier, pivot));
};

#include "range_grunt.qh"

void() range_ragdoll_remove =
{
    local entity e;
    while (self.range_joints) {
        e = self.range_joints;
        self.range_joints = e.range_next;
        remove(e);
    }
    while (self.range_parts) {
        e = self.range_parts;
        self.range_parts = e.range_next;
        remove(e);
    }
    skel_delete(self.skeletonindex);
    remove(self);
};

void(entity rag) range_ragdoll_pose =
{
    local entity limb;
    // Root follows the pelvis. The other bones are relative to this common root.
    setorigin(rag, rag.enemy.origin);
    for (limb = rag.range_parts; limb; limb = limb.range_next) {
        makevectors(limb.angles);
        skel_set_bone(rag.skeletonindex, limb.range_bone, limb.origin - rag.origin);
    }
};

void() range_ragdoll_think =
{
    local entity soldier;
    local vector at;
    self.nextthink = time + 0.10;
    if (self.range_recover > time)
        return;

    soldier = self.range_soldier;
    if (soldier) {
        // Stand on the nearest clear patch of ground under the ragdoll.
        at = self.origin;
        tracebox(at + '0 0 48', soldier.mins, soldier.maxs, at - '0 0 96', MOVE_NORMAL, soldier);
        if (trace_startsolid || trace_fraction == 1 || trace_plane_normal_z < 0.7) {
            at = soldier.origin;
            tracebox(at, soldier.mins, soldier.maxs, at, MOVE_NORMAL, soldier);
            if (trace_startsolid)
                return;
        } else {
            at = trace_endpos + '0 0 1';
        }
        setmodel(soldier, soldier.mdl);
        setsize(soldier, self.mins, self.maxs);
        soldier.movetype = MOVETYPE_STEP;
        soldier.solid = SOLID_SLIDEBOX;
        soldier.takedamage = DAMAGE_AIM;
        soldier.velocity = '0 0 0';
        soldier.think = soldier.range_saved_think;
        soldier.nextthink = time + 0.05;
        soldier.range_launch = '0 0 0';
        setorigin(soldier, at);
        dprint("JOLT_RANGE grunt recovered\n");
    }
    range_ragdoll_remove();
};

entity(entity soldier, float recovering) range_ragdoll =
{
    local entity rag, limb;
    local vector lower, upper, force, moment;
    lower = soldier.mins;
    upper = soldier.maxs;
    range_end_push(soldier);
    rag = spawn();
    rag.classname = "range_ragdoll";
    setmodel(rag, RANGE_SOLDIER);
    setsize(rag, lower, upper);
    rag.solid = SOLID_NOT;
    rag.skin = soldier.skin;
    rag.basebone = -1;
    rag.skeletonindex = skel_create(rag.modelindex);
    if (!rag.skeletonindex) {
        remove(rag);
        return world;
    }
    skel_build(rag.skeletonindex, rag, rag.modelindex, 0, 0, 0);
    range_build_parts(rag, soldier);
    range_ragdoll_pose(rag);
    moment = rag.enemy.origin + '0 0 10';
    for (limb = rag.range_parts; limb; limb = limb.range_next) {
        force = soldier.range_launch * (limb.mass / max(frametime, 0.008333));
        physics_addforce(limb, force, moment);
    }
    rag.range_recover = time + 45;
    if (recovering) {
        rag.range_soldier = soldier;
        rag.range_recover = time + 2.1;
        soldier.range_saved_think = soldier.think;
        soldier.think = SUB_Null;
        soldier.nextthink = -1;
    }
    else {
        soldier.think = SUB_Remove;
        soldier.nextthink = time + 0.05;
    }
    soldier.modelindex = 0;
    soldier.movetype = MOVETYPE_NONE;
    soldier.solid = SOLID_NOT;
    soldier.takedamage = DAMAGE_NO;
    setorigin(soldier, soldier.origin);
    rag.think = range_ragdoll_think;
    rag.nextthink = time + 0.01;
    dprint("JOLT_RANGE grunt ragdolled\n");
    return rag;
};

float(entity soldier) range_knockdown =
{
    if (range_ragdoll(soldier, TRUE))
        return TRUE;
    soldier.range_launch = '0 0 0';
    return FALSE;
};

float() range_ragdoll_death =
{
    if (!range_active || !self.range_fatal_shot || self.gibbed)
        return FALSE;
    if (!range_ragdoll(self, FALSE))
        return FALSE;
    sound(self, CHAN_VOICE, "soldier/death1.wav", 1, ATTN_NORM);
    DropBackpack();
    return TRUE;
};
