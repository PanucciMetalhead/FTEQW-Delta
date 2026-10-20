// Point-anchor compatibility tests use the production C interface only.
static void point_anchors() {
    // Compare the exact anchor geometry (not solver trajectories) to real ODE
    // when available. Cases cover two bodies and both world/body orderings.
#ifdef FTE_TEST_ODE
    require(dInitODE2(0) != 0, "initialize ODE anchor reference");
#endif
    for (int order = 0; order < 3; ++order) {
        auto *w = world();
        auto d = box(256);
        d.gravity_scale = 0;
        d.state.position[0] = -64;
        auto a = order == 1 ? 0 : add(w, d);
        d.state.position[0] = 64;
        d.state.rotation[2] = d.state.rotation[3] = std::sqrt(.5f);
        auto b = order == 2 ? 0 : add(w, d);
        fte_jolt_joint_desc j{};
        j.type = FTE_JOLT_POINT;
        j.body1 = a;
        j.body2 = b;
        j.anchor[0] = 10;
        j.anchor[1] = 20;
        j.anchor[2] = 260;
        j.anchor2[0] = 7;
        j.anchor2[1] = 11;
        j.anchor2[2] = 13;
        j.point_mode = FTE_JOLT_POINT_LEGACY;
        auto joint = fte_jolt_joint_add(w, &j);
        require(joint != 0, "legacy point anchor created");
        float p1[3], p2[3];
        require(fte_jolt_joint_anchors(w, joint, p1, p2) != 0, "read caller-ordered anchors");
        float expected1[] = {10, 20, 260}, expected2[] = {7, 11, 13};
        if (order == 0) {
            expected2[0] = 53;
            expected2[1] = 7;
            expected2[2] = 269;
        }
        if (order == 1) {
            for (int i = 0; i < 3; ++i) {
                expected1[i] = j.anchor2[i];
                expected2[i] = j.anchor[i];
            }
        }
        for (int i = 0; i < 3; ++i) {
            require(std::fabs(p1[i] - expected1[i]) < .001f &&
                        std::fabs(p2[i] - expected2[i]) < .001f,
                    "rotated COM/world anchor geometry");
        }
#ifdef FTE_TEST_ODE
        auto ow = dWorldCreate();
        auto oa = a ? dBodyCreate(ow) : nullptr, ob = b ? dBodyCreate(ow) : nullptr;
        if (oa)
            dBodySetPosition(oa, -64, 0, 256);
        if (ob) {
            dBodySetPosition(ob, 64, 0, 256);
            dQuaternion q = {std::sqrt(.5), 0, 0, std::sqrt(.5)};
            dBodySetQuaternion(ob, q);
        }
        auto oj = dJointCreateBall(ow, nullptr);
        dJointAttach(oj, oa, ob);
        dJointSetBallAnchor(oj, 10, 20, 260);
        dJointSetBallAnchor2(oj, 7, 11, 13);
        dVector3 op1, op2;
        dJointGetBallAnchor(oj, op1);
        dJointGetBallAnchor2(oj, op2);
        for (int i = 0; i < 3; ++i)
            require(std::fabs(p1[i] - op1[i]) < .001 && std::fabs(p2[i] - op2[i]) < .001,
                    "caller anchor geometry matches installed ODE");
        dWorldDestroy(ow);
#endif
        // Invalid replacements never remove or modify the live constraint.
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(), 20000000.f}) {
            auto bad = j;
            bad.anchor2[0] = invalid;
            require(!fte_jolt_joint_add(w, &bad), "reject invalid second anchor");
            require(fte_jolt_joint_anchors(w, joint, p1, p2) != 0, "existing joint remains live");
        }
        auto bad = j;
        bad.point_mode = 3;
        require(!fte_jolt_joint_add(w, &bad), "reject unknown point mode");
        bad = j;
        bad.type = FTE_JOLT_FIXED;
        require(!fte_jolt_joint_add(w, &bad), "reject point mode on another joint type");
        auto *other = world();
        require(!fte_jolt_joint_anchors(other, joint, p1, p2), "cross-world anchor read rejected");
        fte_jolt_destroy(other);
        require(!fte_jolt_joint_anchors(w, joint, nullptr, p2), "null anchor output rejected");
        require(fte_jolt_joint_remove(w, joint) != 0 && !fte_jolt_joint_anchors(w, joint, p1, p2),
                "removed joint invalidates anchor queries");
        fte_jolt_destroy(w);
    }
#ifdef FTE_TEST_ODE
    dCloseODE();
#endif
    for (int mode : {FTE_JOLT_POINT_SHARED, FTE_JOLT_POINT_WORLD, FTE_JOLT_POINT_LEGACY}) {
        auto *w = world();
        auto d = box(256);
        d.gravity_scale = 0;
        auto b = add(w, d);
        fte_jolt_joint_desc j{};
        j.type = FTE_JOLT_POINT;
        j.body1 = b;
        j.point_mode = mode;
        j.anchor[2] = 256;
        j.anchor2[0] = 48;
        j.anchor2[2] = 256;
        auto joint = fte_jolt_joint_add(w, &j);
        require(joint != 0, "point convergence joint");
        advance(w, 120, 1.0 / 120, 0);
        float p1[3], p2[3];
        require(fte_jolt_joint_anchors(w, joint, p1, p2) != 0, "query settled point joint");
        require(std::fabs(state(w, b).position[0] - (mode ? 48 : 0)) < .05f,
                "explicit anchors converge while zero-initialized shared mode remains unchanged");
        for (int i = 0; i < 3; ++i)
            require(std::fabs(p1[i] - p2[i]) < .05f, "solver joins both anchor points");
        require(fte_jolt_joint_enable(w, joint, 0) && fte_jolt_joint_enable(w, joint, 1),
                "explicit point supports disabling and re-enabling");
        require(fte_jolt_remove(w, b) && !fte_jolt_joint_anchors(w, joint, p1, p2),
                "dependent point joint removed with body");
        fte_jolt_destroy(w);
    }
    // Shape-local center of mass is included when translating legacy offsets.
    auto *w = world();
    auto d = box(256);
    d.gravity_scale = 0;
    auto a = add(w, d);
    const float vertices[] = {16, -8, -8, 32, -8, -8, 16, 8, -8, 32, 8, -8,
                              16, -8, 8,  32, -8, 8,  16, 8, 8,  32, 8, 8};
    d.shape = FTE_JOLT_CONVEX;
    d.vertices = vertices;
    d.vertex_count = 8;
    d.state.position[0] = 128;
    auto b = add(w, d);
    fte_jolt_joint_desc j{};
    j.type = FTE_JOLT_POINT;
    j.body1 = a;
    j.body2 = b;
    j.anchor[2] = 256;
    j.point_mode = FTE_JOLT_POINT_LEGACY;
    auto joint = fte_jolt_joint_add(w, &j);
    float p1[3], p2[3];
    require(joint && fte_jolt_joint_anchors(w, joint, p1, p2) && std::fabs(p2[0] - 152) < .001,
            "legacy local anchor is relative to shape COM, not body origin");
    // A bounded local offset may still overflow allowed world coordinates.
    auto pose = state(w, b);
    pose.position[0] = 9999990;
    require(fte_jolt_set(w, b, &pose), "move body near explicit anchor bounds");
    j.anchor2[0] = 64;
    require(!fte_jolt_joint_add(w, &j), "reject transformed anchor outside world bounds");
    fte_jolt_destroy(w);
    std::puts("JOLT_POINT_ANCHORS_OK: local/rotated/world/reversed/COM, defaults, invalid input "
              "and cleanup");
}
