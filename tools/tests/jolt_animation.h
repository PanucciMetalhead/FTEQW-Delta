// Production adapter animation checks, included after the shared test helpers.
static unsigned animation_motors(fte_jolt_world *w) {
    fte_jolt_stats stats{};
    fte_jolt_get_stats(w, &stats);
    return stats.animation_motors;
}
static float blended_response(float strength, float mass = 2, int frames = 12,
                              double dt = 1.0 / 120) {
    auto *w = world();
    auto d = box(256);
    d.gravity_scale = 0;
    d.mass = mass;
    auto body = add(w, d);
    require(fte_jolt_animate(w, body, strength) != 0, "set fractional animation strength");
    auto target = state(w, body);
    target.position[0] = 64;
    require(fte_jolt_move(w, body, &target) != 0, "submit fractional animation target");
    advance(w, frames, dt, 0);
    float x = state(w, body).position[0];
    fte_jolt_destroy(w);
    return x;
}
static void animation_blending() {
    float weak = blended_response(.1f), strong = blended_response(.8f);
    require(weak > 0 && weak < strong && strong < 64,
            "strength changes physical response without snapping to target");
    require(std::fabs(blended_response(.5f, 1) - blended_response(.5f, 20)) < .05f,
            "mass-scaled response is consistent");
    auto *w = world();
    auto d = box(256);
    d.gravity_scale = 0;
    auto body = add(w, d);
    require(fte_jolt_animate(w, body, .5f) != 0 && animation_motors(w) == 1,
            "one animation motor per driven body");
    auto target = state(w, body);
    target.position[0] = 64;
    target.rotation[2] = std::sin(.6f);
    target.rotation[3] = std::cos(.6f);
    require(fte_jolt_move(w, body, &target) != 0, "fractional rotation and translation target");
    advance(w, 1, .001, 0);
    require(state(w, body).position[0] == 0, "fractional targets wait for a fixed tick");
    advance(w, 240, 1.0 / 120, 0);
    auto settled = state(w, body);
    require(std::fabs(settled.position[0] - 64) < .1 &&
                std::fabs(settled.rotation[2] - target.rotation[2]) < .002,
            "dynamic motor holds and converges on translation and orientation");
    for (float invalid : {-1.f, 1.01f, std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity()})
        require(!fte_jolt_animate(w, body, invalid) && animation_motors(w) == 1,
                "invalid strength preserves the live motor");
    auto invalid_target = target;
    invalid_target.position[0] = std::numeric_limits<float>::max();
    require(!fte_jolt_move(w, body, &invalid_target),
            "unbounded animation targets rejected before solver");
    // Opposite quaternion signs represent the same target, with no long rotation.
    for (float &v : target.rotation)
        v = -v;
    require(fte_jolt_move(w, body, &target) != 0, "equivalent signed quaternion accepted");
    advance(w, 8, 1.0 / 120, 0);
    require(std::fabs(state(w, body).spin[2]) < .1,
            "quaternion target interpolation takes shortest path");
    auto teleported = state(w, body);
    teleported.position[0] = 16;
    teleported.velocity[0] = teleported.velocity[1] = teleported.velocity[2] = 0;
    teleported.spin[0] = teleported.spin[1] = teleported.spin[2] = 0;
    require(fte_jolt_move(w, body, &target) != 0 && fte_jolt_set(w, body, &teleported) != 0,
            "explicit teleport cancels pending fractional target");
    advance(w, 30, 1.0 / 120, 0);
    require(std::fabs(state(w, body).position[0] - 16) < .1 && animation_motors(w) == 1,
            "explicit teleport rebases held motor target without disabling it");
    target.position[0] += 32;
    require(fte_jolt_move(w, body, &target) != 0, "moving fractional target");
    advance(w, 6, 1.0 / 120, 0);
    auto moving = state(w, body);
    require(moving.velocity[0] > 1, "fractional motor generates actual dynamic velocity");
    require(fte_jolt_animate(w, body, 0) != 0 && animation_motors(w) == 0,
            "release removes only the animation motor");
    auto released = state(w, body);
    require(std::fabs(released.velocity[0] - moving.velocity[0]) < .001 &&
                std::fabs(released.spin[2] - moving.spin[2]) < .001,
            "fractional release retains actual linear/angular velocity");
    require(!fte_jolt_move(w, body, &target), "passive body rejects targets");
    for (int cycle = 0; cycle < 12; ++cycle) {
        require(fte_jolt_animate(w, body, .25f) != 0 && fte_jolt_animate(w, body, .75f) != 0 &&
                    animation_motors(w) == 1,
                "live fractional edits reuse a single motor");
        auto before = state(w, body);
        require(fte_jolt_animate(w, body, 1) != 0 && animation_motors(w) == 0,
                "exact control removes fractional motor");
        require(std::fabs(state(w, body).position[0] - before.position[0]) < .001,
                "control switch preserves current pose");
        require(fte_jolt_animate(w, body, .5f) != 0 && animation_motors(w) == 1,
                "exact control can return to soft animation");
        require(fte_jolt_motion(w, body, FTE_JOLT_DYNAMIC) != 0 && animation_motors(w) == 0,
                "explicit motion override removes motor");
    }
    auto fixed = add(w, box(0, FTE_JOLT_STATIC));
    require(!fte_jolt_animate(w, fixed, .5f), "static bodies reject animation");
    auto *other = world();
    require(!fte_jolt_animate(other, body, .5f), "cross-world animation handle rejected");
    fte_jolt_destroy(other);
    require(fte_jolt_animate(w, body, .5f) != 0 && fte_jolt_remove(w, body) != 0 &&
                !animation_motors(w) && !fte_jolt_animate(w, body, .5f),
            "removal destroys motor and invalidates handle");
    fte_jolt_destroy(w);
    // Dynamic contacts resist the target, and removing the blocker permits
    // recovery.
    w = world();
    d = box(256);
    d.gravity_scale = 0;
    body = add(w, d);
    auto wall = box(256, FTE_JOLT_STATIC);
    wall.state.position[0] = 64;
    wall.size[0] = 16;
    wall.size[1] = wall.size[2] = 256;
    auto obstacle = add(w, wall);
    require(fte_jolt_animate(w, body, .8f) != 0, "drive body into wall");
    target = state(w, body);
    target.position[0] = 128;
    require(fte_jolt_move(w, body, &target) != 0, "target beyond wall accepted");
    advance(w, 120, 1.0 / 120, 0);
    require(state(w, body).position[0] > 35 && state(w, body).position[0] < 41,
            "contacts block fractional animation instead of teleporting through "
            "wall");
    require(fte_jolt_remove(w, obstacle) != 0, "remove motor blocker");
    advance(w, 240, 1.0 / 120, 0);
    require(std::fabs(state(w, body).position[0] - 128) < .2,
            "unblocked body recovers toward held target");
    fte_jolt_destroy(w);
    // Intermediate strengths have physical compliance under gravity.
    w = world();
    auto low = add(w, box(256));
    d = box(256);
    d.state.position[0] = 128;
    auto high = add(w, d);
    require(fte_jolt_animate(w, low, .05f) != 0 && fte_jolt_animate(w, high, .8f) != 0,
            "gravity load strengths");
    advance(w, 120, 1.0 / 120);
    require(state(w, low).position[2] < 230 && state(w, high).position[2] > 250,
            "weak drive yields under load while strong drive supports it");
    require(fte_jolt_animate(w, high, 0) != 0, "release settled fractional body wakes it");
    advance(w, 60, 1.0 / 120);
    require(state(w, high).position[2] < 220, "settled motor release resumes gravity");
    for (float tiny : {std::numeric_limits<float>::denorm_min(), 1e-20f, 1e-6f}) {
        require(fte_jolt_animate(w, high, tiny) != 0, "tiny finite strength accepted");
        advance(w, 2, 1.0 / 120);
        require(std::isfinite(state(w, high).position[2]), "tiny strength remains finite");
    }
    fte_jolt_destroy(w);
    // Original constraints survive motor creation, strength edits and removal.
    w = world();
    d = box(256);
    d.gravity_scale = 0;
    auto a = add(w, d);
    d.state.position[2] = 224;
    auto b = add(w, d);
    fte_jolt_joint_desc joint{};
    joint.type = FTE_JOLT_FIXED;
    joint.body1 = a;
    joint.body2 = b;
    require(fte_jolt_joint_add(w, &joint) != 0 && fte_jolt_animate(w, a, .6f) != 0,
            "jointed fractional drive");
    target = state(w, a);
    target.position[0] = 64;
    require(fte_jolt_move(w, a, &target) != 0, "jointed target");
    advance(w, 240, 1.0 / 120, 0);
    require(std::fabs(state(w, a).position[0] - 64) < .2 &&
                std::fabs(state(w, b).position[0] - 64) < .3 &&
                std::fabs(state(w, a).position[2] - state(w, b).position[2] - 32) < .2,
            "passive child stays attached to soft driven parent");
    fte_jolt_stats stats{};
    fte_jolt_get_stats(w, &stats);
    require(stats.joints == 1 && stats.animation_motors == 1,
            "authored joints and internal motors counted separately");
    require(fte_jolt_remove(w, a) != 0, "remove live constrained motor");
    fte_jolt_get_stats(w, &stats);
    require(stats.joints == 0 && stats.animation_motors == 0,
            "dependent joints and animation motor cleaned together");
    fte_jolt_destroy(w);
    // A real nonzero shape COM must not offset the authored body origin.
    w = world();
    float vertices[] = {16, -8, -8, 32, -8, -8, 16, 8, -8, 32, 8, -8,
                        16, -8, 8,  32, -8, 8,  16, 8, 8,  32, 8, 8};
    d = box(256);
    d.gravity_scale = 0;
    d.shape = FTE_JOLT_CONVEX;
    d.vertices = vertices;
    d.vertex_count = 8;
    body = add(w, d);
    require(fte_jolt_animate(w, body, .6f) != 0, "offset hull animation");
    target = state(w, body);
    target.position[0] = 64;
    target.rotation[2] = std::sin(.4f);
    target.rotation[3] = std::cos(.4f);
    require(fte_jolt_move(w, body, &target) != 0, "offset hull target");
    advance(w, 240, 1.0 / 120, 0);
    require(std::fabs(state(w, body).position[0] - 64) < .2 &&
                std::fabs(state(w, body).position[1]) < .2 &&
                std::fabs(state(w, body).rotation[2] - target.rotation[2]) < .002,
            "COM conversion preserves target body origin and rotation");
    fte_jolt_destroy(w);
    for (double dt : {1.0 / 30, 1.0 / 60, 1.0 / 144})
        require(std::fabs(blended_response(.5f, 2, int(std::round(1 / dt)), dt) - 64) < .2,
                "fixed-step convergence across render frame rates");
    std::puts("JOLT_ANIMATION_BLEND_OK: strength/compliance, contacts, rotation/COM, "
              "handoffs, invalid input, fixed steps, joints and cleanup");
}
