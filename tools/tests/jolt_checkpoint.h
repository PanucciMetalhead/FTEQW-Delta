// Portable continuation tests: recreate the same scene with fresh native IDs.
static void entity_checkpoints() {
    auto *w = world();
    auto d = box(128);
    d.gravity_scale = 0;
    auto body = add(w, d);
    fte_jolt_joint_desc settings{};
    settings.type = FTE_JOLT_SLIDER;
    settings.body2 = body;
    settings.axis[0] = 1;
    settings.lower = 0;
    settings.upper = 40;
    settings.velocity = 80;
    settings.max_force = 100000;
    auto joint = fte_jolt_joint_add(w, &settings);
    require(joint, "checkpoint slider");
    advance(w, 120);
    require(std::fabs(state(w, body).position[0] - 40) < .1, "slider reaches original limit");
    fte_jolt_runtime runtime{};
    fte_jolt_joint_checkpoint saved{};
    double rem, sim;
    require(fte_jolt_runtime_get(w, body, &runtime) &&
                fte_jolt_joint_checkpoint_get(w, joint, &saved) &&
                fte_jolt_clock_get(w, &rem, &sim),
            "capture entity continuation");
    fte_jolt_destroy(w);
    w = world();
    d.state = runtime.state;
    body = add(w, d);
    saved.settings.body2 = body;
    joint = fte_jolt_joint_checkpoint_add(w, &saved);
    require(joint && fte_jolt_runtime_set(w, body, &runtime) && fte_jolt_clock_set(w, rem, sim),
            "rebuild with fresh body IDs");
    auto before = state(w, body);
    advance(w, 60);
    require(std::fabs(state(w, body).position[0] - 40) < .1,
            "loaded slider keeps original zero/limit frame");
    auto invalid = runtime;
    invalid.state.rotation[3] = 0;
    require(!fte_jolt_runtime_set(w, body, &invalid) &&
                std::fabs(state(w, body).position[0] - 40) < .1,
            "invalid runtime preserves live state");
    invalid = runtime;
    invalid.active = 2;
    require(!fte_jolt_runtime_set(w, body, &invalid), "invalid activation flag rejected");
    auto bad = saved;
    bad.reference2.rotation[3] = 0;
    require(!fte_jolt_joint_checkpoint_add(w, &bad) &&
                std::fabs(state(w, body).position[0] - before.position[0]) < .1,
            "invalid references preserve current poses");
    require(!fte_jolt_clock_set(w, 1.0 / 120, 0) && !fte_jolt_clock_set(w, -1, 0) &&
                !fte_jolt_clock_set(w, 0, std::numeric_limits<double>::infinity()),
            "invalid clock rejected");
    fte_jolt_joint_checkpoint disabled{};
    require(fte_jolt_joint_enable(w, joint, 0) &&
                fte_jolt_joint_checkpoint_get(w, joint, &disabled) && !disabled.enabled,
            "disabled joint captured");
    auto *other = world();
    require(!fte_jolt_runtime_set(other, body, &runtime) &&
                !fte_jolt_joint_checkpoint_add(other, &saved),
            "cross-world checkpoint handles rejected");
    fte_jolt_destroy(other);
    fte_jolt_destroy(w);
    // Sleep state survives reconstructing an otherwise active body in midair.
    w = world();
    body = add(w, box(256));
    require(fte_jolt_enable(w, body, 0) && fte_jolt_runtime_get(w, body, &runtime) &&
                !runtime.active,
            "capture sleeping body");
    require(fte_jolt_enable(w, body, 1) && fte_jolt_runtime_set(w, body, &runtime),
            "restore sleep");
    advance(w, 60);
    require(state(w, body).position[2] == 256, "restored sleeping body does not resume falling");
    float force[] = {0, 0, 10}, at[] = {0, 0, 256};
    require(fte_jolt_force(w, body, force, at) && !fte_jolt_clock_get(w, &rem, &sim),
            "pending force prevents incomplete checkpoint");
    advance(w, 1);
    require(fte_jolt_clock_get(w, &rem, &sim), "fixed tick clears pending force restriction");
    fte_jolt_destroy(w);
    // Pending kinematic moves and fractional tick remainder resume together.
    w = world();
    d = box(128, FTE_JOLT_KINEMATIC);
    body = add(w, d);
    auto target = state(w, body);
    target.position[0] = 12;
    require(fte_jolt_move(w, body, &target), "queue saved move");
    advance(w, 1, .001, 0);
    require(fte_jolt_runtime_get(w, body, &runtime) && runtime.has_target &&
                fte_jolt_clock_get(w, &rem, &sim),
            "capture partial kinematic tick");
    fte_jolt_destroy(w);
    w = world();
    body = add(w, d);
    require(fte_jolt_runtime_set(w, body, &runtime) && fte_jolt_clock_set(w, rem, sim),
            "restore pending move and clock");
    advance(w, 1, 1.0 / 120 - .001, 0);
    require(std::fabs(state(w, body).position[0] - 12) < .001,
            "pending move advances at original tick boundary");
    fte_jolt_destroy(w);
    std::puts("JOLT_ENTITY_CHECKPOINT_OK: frames, motion/sleep, partial ticks, validation and "
              "fresh native IDs");
}
