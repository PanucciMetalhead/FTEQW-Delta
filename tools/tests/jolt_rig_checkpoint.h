static void skeletal_checkpoints() {
    for (float strength : {0.f, .4f, 1.f}) {
        auto *original = world();
        auto d = box(512);
        auto body = add(original, d);
        require(fte_jolt_animate(original, body, strength), "checkpoint animation mask");
        auto target = state(original, body);
        target.position[0] = 48;
        target.position[2] = 530;
        target.rotation[2] = std::sin(.2f);
        target.rotation[3] = std::cos(.2f);
        if (strength > 0)
            require(fte_jolt_move(original, body, &target), "checkpoint moving rig");
        advance(original, 8, 1.0 / 120);
        target.position[0] = 80;
        if (strength > 0)
            require(fte_jolt_move(original, body, &target), "checkpoint pending rig target");
        advance(original, 1, .001);
        fte_jolt_rig_runtime snapshot{};
        double remainder, simulated;
        require(fte_jolt_rig_runtime_get(original, body, &snapshot) &&
                    snapshot.strength == strength &&
                    fte_jolt_clock_get(original, &remainder, &simulated),
                "capture all rig animation modes");
        auto *restored = world();
        auto copy = add(restored, d);
        require(fte_jolt_rig_runtime_set(restored, copy, &snapshot) &&
                    fte_jolt_clock_set(restored, remainder, simulated),
                "restore rig and partial tick");
        auto before = state(original, body), after = state(restored, copy);
        for (int axis = 0; axis < 3; axis++)
            require(std::fabs(before.position[axis] - after.position[axis]) < .0001 &&
                        std::fabs(before.velocity[axis] - after.velocity[axis]) < .0001,
                    "restore actual pose and momentum");
        for (int tick = 0; tick < 90; tick++) {
            advance(original, 1, 1.0 / 120);
            advance(restored, 1, 1.0 / 120);
        }
        before = state(original, body);
        after = state(restored, copy);
        for (int axis = 0; axis < 3; axis++)
            require(std::fabs(before.position[axis] - after.position[axis]) < .02 &&
                        std::fabs(before.velocity[axis] - after.velocity[axis]) < .03,
                    "restored passive/exact/fractional continuation");
        auto invalid = snapshot;
        invalid.strength = std::numeric_limits<float>::quiet_NaN();
        require(!fte_jolt_rig_runtime_set(restored, copy, &invalid),
                "invalid rig checkpoint mask rejected");
        invalid = snapshot;
        invalid.animation_pose.rotation[3] = 0;
        require(!fte_jolt_rig_runtime_set(restored, copy, &invalid),
                "invalid rig checkpoint orientation rejected");
        require(!fte_jolt_rig_runtime_set(restored, body, &snapshot),
                "foreign rig handles rejected");
        fte_jolt_destroy(original);
        fte_jolt_destroy(restored);
    }
    auto *w = world();
    auto body = add(w, box(256));
    fte_jolt_rig_runtime asleep{};
    require(fte_jolt_enable(w, body, 0) && fte_jolt_rig_runtime_get(w, body, &asleep) &&
                !asleep.runtime.active,
            "sleeping rig snapshot");
    fte_jolt_destroy(w);
    w = world();
    body = add(w, box(256));
    require(fte_jolt_rig_runtime_set(w, body, &asleep), "sleeping rig restore");
    advance(w, 30);
    require(state(w, body).position[2] == 256, "sleep retained in passive rig");
    fte_jolt_destroy(w);
    // Motion-type restoration wakes connected bodies. Restore activation last.
    w = world();
    auto first = add(w, box(400)), second = add(w, box(432));
    fte_jolt_joint_desc joint{};
    joint.type = FTE_JOLT_FIXED;
    joint.body1 = first;
    joint.body2 = second;
    require(fte_jolt_joint_add(w, &joint), "connected checkpoint sleep fixture");
    fte_jolt_rig_runtime a{}, b{};
    require(fte_jolt_rig_runtime_get(w, first, &a) && fte_jolt_rig_runtime_get(w, second, &b),
            "capture connected runtimes");
    a.runtime.active = b.runtime.active = 0;
    b.strength = 1;
    require(fte_jolt_rig_runtime_set(w, first, &a) && fte_jolt_rig_runtime_set(w, second, &b),
            "restore dynamic then exact connected bodies");
    fte_jolt_runtime awakened{};
    require(fte_jolt_runtime_get(w, first, &awakened) && awakened.active,
            "kinematic handoff actually wakes an already restored neighbor");
    require(fte_jolt_runtime_activation(w, first, 0) && fte_jolt_runtime_activation(w, second, 0),
            "apply final activation for dynamic and kinematic bodies");
    require(fte_jolt_rig_runtime_get(w, first, &a) && fte_jolt_rig_runtime_get(w, second, &b) &&
                !a.runtime.active && !b.runtime.active,
            "final pass retains both sleep flags");
    require(!fte_jolt_runtime_activation(w, first, 2), "invalid activation edit rejected");
    fte_jolt_destroy(w);
    // An extreme one-tick target can leave a large kinematic release velocity.
    // Refuse capture before writing a checkpoint the loader cannot accept.
    w = world();
    body = add(w, box(256));
    require(fte_jolt_animate(w, body, 1), "extreme checkpoint kinematic mask");
    auto target = state(w, body);
    target.position[0] = 1000000;
    require(fte_jolt_move(w, body, &target), "extreme checkpoint target");
    advance(w, 1, 1.0 / 120, 0);
    fte_jolt_runtime extreme{};
    require(fte_jolt_runtime_get(w, body, &extreme) &&
                std::fabs(extreme.driven_velocity[0]) > 10000000,
            "extreme fixture produces out-of-bounds release velocity");
    fte_jolt_rig_runtime bounded{};
    require(!fte_jolt_rig_runtime_get(w, body, &bounded),
            "uncapturable rig runtime rejected before serialization");
    advance(w, 1, 1.0 / 120, 0);
    require(fte_jolt_rig_runtime_get(w, body, &bounded) &&
                fte_jolt_rig_runtime_set(w, body, &bounded),
            "idle tick makes rig checkpoint round-trip safe again");
    fte_jolt_destroy(w);
    std::puts("JOLT_SKELETAL_CHECKPOINT_OK: masks, servo targets, partial ticks, momentum, sleep "
              "and fresh worlds");
}
