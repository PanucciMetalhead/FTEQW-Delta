// Compiled only into the smoke-test progs. Exercise actual AD projectile touches.
float range_test_phase, range_test_at, range_test_speed, range_test_spin, range_test_hop;
entity range_test_player, range_test_grunt, range_test_rock;
vector range_test_origin, range_test_forward, range_test_right, range_test_up;

// World normal of the 887/1536 ramp. The fitted collision boxes enclose
// every contributing skin weight, so this also bounds the rendered mesh.
vector range_test_normal = '0 -0.500080 0.865979';

void(float ok, string why) range_expect =
{
    if (!ok) {
        bprint(strcat("RANGE_SMOKE_FAIL: ", why, "\n"));
        localcmd("quit force\n");
    }
};

void(entity grunt, vector at, float hp) range_test_place =
{
    grunt.health = hp;
    grunt.angles = '0 270 0';
    grunt.think = SUB_Null;
    grunt.nextthink = -1;
    setorigin(grunt, at);
};

void(vector from, vector towards) range_test_rocket =
{
    local entity previous = self;
    self = range_test_player;
    Launch_Missile(from, normalize(towards - from), '0 0 0', CT_PROJ_ROCKET, SPEED_RLPLAYER);
    self = previous;
};

void(vector at, float damage) range_test_grenade =
{
    local entity previous = self;
    self = range_test_player;
    Launch_Grenade(at, '0 0 0', '0 0 0', CT_PROJ_GL);
    newmis.dmg = damage;
    newmis.nextthink = time + 0.08;
    self = previous;
};

void(string name) range_test_trigger =
{
    local entity previous = self;
    self = find(world, targetname, name);
    activator = world;
    self.use();
    self = previous;
};

void(entity rag) range_test_clearance =
{
    local entity limb;
    local vector n, lower, upper, axes;
    local float clearance = 1000;
    n = range_test_normal;
    for (limb = rag.range_parts; limb; limb = limb.range_next) {
        makevectors(limb.angles);
        axes = [n * v_forward, n * -v_right, n * v_up];
        lower = limb.mins;
        upper = limb.maxs;
        clearance = min(clearance, n * limb.origin +
            min(axes_x * lower_x, axes_x * upper_x) +
            min(axes_y * lower_y, axes_y * upper_y) +
            min(axes_z * lower_z, axes_z * upper_z));
    }
    dprint(sprintf("RANGE_SMOKE slope corpse clearance=%g\n", clearance));
    range_expect(rag.origin_y > 35 && clearance > -2, "corpse remains above the sloped BSP");
};

void() range_test_roll =
{
    local entity rock = range_test_rock;
    skel_get_bonerel(rock.range_visual.skeletonindex, 1);
    // Rotation about +X carries a rock's top downhill (toward -Y).
    range_test_spin += 0.5 * (
        range_test_forward_y * v_forward_z - range_test_forward_z * v_forward_y +
        range_test_right_y * v_right_z - range_test_right_z * v_right_y +
        range_test_up_y * v_up_z - range_test_up_z * v_up_y);
    range_test_forward = v_forward;
    range_test_right = v_right;
    range_test_up = v_up;
    if (rock.origin_y < range_test_origin_y - 70)
        range_test_hop = max(range_test_hop, range_test_normal * rock.velocity);
};

void(entity grunt) range_test_climb =
{
    local entity previous = self;
    local float step, height, error = 0;
    for (self = find(world, classname, "monster_army"); self; self = find(self, classname, "monster_army"))
        range_expect(self.movespeed >= 0, "range grunt has mobile AD AI");

    // Use the ordinary monster hull and walkmove path across every trigger stripe.
    self = grunt;
    setorigin(grunt, '-420 40 100');
    range_expect(droptofloor(), "grunt finds the ramp floor");
    for (step = 0; step < 184; step += 1) {
        height = grunt.origin_z;
        if (!walkmove(90, 8))
            break;
        error = max(error, fabs(grunt.origin_z - height - 8 * 887 / 1536));
    }
    dprint(sprintf("RANGE_SMOKE climb steps=%g, max height error=%g\n", step, error));
    range_expect(step == 184 && error < 0.10, "grunt climbs the continuous 30-degree ramp");
    self = previous;
};

void() range_smoke_tick =
{
    local entity e, rag, previous;
    if (range_test_phase == 8 || range_test_phase == 9)
        range_test_roll();
    if (!range_active || time < 1 || range_test_at > time)
        return;

    if (range_test_phase == 0) {
        range_test_grunt = find(world, targetname, "grunt1");
        if (!range_test_grunt || range_test_grunt.movetype != MOVETYPE_STEP)
            return;
        range_test_player = spawn();
        range_test_player.classtype = CT_PLAYER;
        range_test_player.flags = FL_CLIENT;
        range_test_climb(range_test_grunt);
        range_test_origin = '0 -90 24';
        range_test_place(range_test_grunt, range_test_origin, 100);
        previous = self;
        self = range_test_player;
        Launch_Bullet('0 -155 28', '0 1 0', CT_PROJ_SG, 1000);
        Launch_Bullet('0 -162 28', '0 1 0', CT_PROJ_SG, 1000);
        Launch_Bullet('0 -167 28', '0 1 0', CT_PROJ_SG, 1000);
        self = previous;
        range_test_at = time + 0.24;
    }
    else if (range_test_phase == 1) {
        range_expect(range_test_grunt.health < 100 && range_test_grunt.health > 0, "pellets deal AD damage");
        dprint(sprintf("RANGE_SMOKE pellet hp=%g movetype=%g origin=%v velocity=%v timer=%g time=%g\n",
            range_test_grunt.health, range_test_grunt.movetype, range_test_grunt.origin,
            range_test_grunt.velocity, range_test_grunt.range_push_until, time));
        range_expect(range_test_grunt.origin_y > range_test_origin_y + 10 &&
                     range_test_grunt.movetype == MOVETYPE_PHYSICS, "pellets drive Jolt displacement");
        range_expect(range_test_grunt.origin_y < range_test_origin_y + 35, "pellet knockback is modest");
        range_expect(fabs(range_test_grunt.angles_y - 270) < 4, "Jolt handoff preserves grunt facing");
        range_test_at = time + 0.2;
    }
    else if (range_test_phase == 2) {
        range_expect(range_test_grunt.movetype == MOVETYPE_STEP, "grunt resumes classic movement");
        range_test_place(range_test_grunt, '0 -90 24', 140);
        range_test_grenade('0 -150 6', 100);
        range_test_at = time + 0.43;
    }
    else if (range_test_phase == 3) {
        rag = find(world, classname, "range_ragdoll");
        range_expect(rag && rag.range_soldier == range_test_grunt && range_test_grunt.health > 0,
                     "nonlethal grenade knocks down a living grunt");
        range_expect(rag.enemy.origin_y > -55 && rag.skeletonindex, "blast launches a visible articulated rig");
        range_expect(vlen(rag.range_parts.origin - rag.enemy.origin) < 48, "limbs stay attached during blast");
        range_test_at = time + 2.1;
    }
    else if (range_test_phase == 4) {
        range_expect(range_test_grunt.solid == SOLID_SLIDEBOX && range_test_grunt.modelindex,
                     "living grunt recovers");
        range_test_place(range_test_grunt, '0 265 188', 100);
        range_test_rocket('0 165 132', '0 265 192');
        range_test_at = time + 0.32;
    }
    else if (range_test_phase == 5) {
        rag = find(world, classname, "range_ragdoll");
        range_expect(rag && !rag.range_soldier && rag.origin_y > 280, "non-gib rocket kill leaves a launched corpse");
        e = find(world, targetname, "grunt2");
        range_test_place(e, '208 -112 24', 12);
        range_test_grunt = e;
        range_test_rocket('208 -200 28', '208 -112 28');
        range_test_at = time + 0.3;
    }
    else if (range_test_phase == 6) {
        rag = find(world, classname, "range_ragdoll");
        range_expect(range_test_grunt.gibbed && !find(rag, classname, "range_ragdoll"), "gib kill keeps AD gibs without a second rig");
        range_test_trigger("rockfall1");
        range_test_at = time + 0.4;
    }
    else if (range_test_phase == 7) {
        range_test_rock = find(world, classname, "range_rock");
        range_expect(range_test_rock && range_test_rock.range_visual.modelindex, "rockfall releases a modeled prop");
        range_test_origin = range_test_rock.origin;
        skel_get_bonerel(range_test_rock.range_visual.skeletonindex, 1);
        range_test_forward = v_forward;
        range_test_right = v_right;
        range_test_up = v_up;
        range_test_at = time + 0.55;
    }
    else if (range_test_phase == 8) {
        range_test_speed = -range_test_rock.velocity_y;
        range_expect(range_test_speed > 40, "rock starts rolling from rest");
        range_test_at = time + 0.68;
    }
    else if (range_test_phase == 9) {
        rag = find(world, classname, "range_ragdoll");
        range_test_clearance(rag);
        dprint(sprintf("RANGE_SMOKE rock speed %g -> %g, downhill spin=%g, bounce speed=%g\n",
            range_test_speed, -range_test_rock.velocity_y, range_test_spin, range_test_hop));
        range_expect(range_test_rock.origin_y < range_test_origin_y - 120 &&
                     -range_test_rock.velocity_y > range_test_speed + 80, "rock gains speed down the slope");
        range_expect(range_test_spin > 2, "rendered rock rotates in the downhill direction");
        range_expect(range_test_hop > 12, "rolling rock rebounds along the slope normal");
        e = range_test_rock;
        range_test_rocket(e.origin - '0 92 46', e.origin);
        range_test_at = time + 0.3;
    }
    else if (range_test_phase == 10) {
        range_expect(!find(world, classname, "range_rock") && find(world, classname, "range_rock_shard"),
                     "rocket shatters a rolling boulder");
        range_test_trigger("rockfall2");
        range_test_at = time + 0.42;
    }
    else if (range_test_phase == 11) {
        e = find(world, classname, "range_rock");
        range_expect(e != world, "second release spawns independently");
        range_test_grenade(e.origin + '48 0 -16', 120);
        range_test_at = time + 0.29;
    }
    else if (range_test_phase == 12) {
        range_expect(!find(world, classname, "range_rock"), "grenade splash shatters a boulder");
        range_test_trigger("rockfall3");
        range_test_at = time + 0.88;
    }
    else if (range_test_phase == 13) {
        e = find(world, classname, "range_rock");
        range_expect(e && e.velocity_y < -60, "third boulder is in motion");
        range_test_player.health = 100;
        range_test_player.takedamage = DAMAGE_AIM;
        range_test_player.solid = SOLID_SLIDEBOX;
        setsize(range_test_player, '-16 -16 -24', '16 16 32');
        setorigin(range_test_player, e.origin + normalize(e.velocity) * 116 - '0 0 18');
        range_test_at = time + 0.8;
    }
    else if (range_test_phase == 14) {
        range_expect(range_test_player.health < 100, "a rolling boulder hurts a player in its path");
        e = find(world, targetname, "grunt4");
        range_test_place(e, '-260 -250 24', 3);
        previous = self;
        self = range_test_player;
        Launch_Bullet('-260 -318 28', '0 1 0', CT_PROJ_SG, SPEED_PLAYERSG);
        self = previous;
        range_test_at = time + 0.25;
    }
    else {
        for (rag = find(world, classname, "range_ragdoll"); rag; rag = find(rag, classname, "range_ragdoll"))
            if (!rag.range_soldier && rag.origin_x < -220 && rag.origin_y < -190)
                break;
        range_expect(rag && rag.skeletonindex && rag.range_parts && rag.origin_y > -250,
                     "fatal shotgun pellet hands off to a moving ragdoll");
        bprint("RANGE_SMOKE_OK\n");
        localcmd("quit force\n");
    }
    range_test_phase += 1;
};
