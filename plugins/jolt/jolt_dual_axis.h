/* FTEQW Delta, GPL-2.0-or-later. Internal Jolt two-axis joint adapter.
 * Include after Jolt/Jolt.h and SixDOFConstraint.h. No engine types cross here.
 */
#ifndef FTE_JOLT_DUAL_AXIS_H
#define FTE_JOLT_DUAL_AXIS_H
// A universal joint has R = Ry(angle1) * Rx(angle2): body1 owns Y, body2 X.
// Locking swing Z keeps those axes perpendicular. Jolt's independent angular
// motor axes use body2 space, so add motors/soft stops on the actual moving
// universal axes rather than silently treating them as fixed Euler axes.
class DualAxisConstraint final : public JPH::TwoBodyConstraint {
    JPH::Ref<JPH::SixDOFConstraint> core;
    JPH::AngleConstraintPart motor[2], stop[2];
    JPH::Vec3 axis[2] = {JPH::Vec3::sAxisY(), JPH::Vec3::sAxisX()};
    float lower[2], upper[2], force[2], velocity[2], stiffness[2], damping[2], angle[2] = {};

  public:
    JPH_OVERRIDE_NEW_DELETE
    DualAxisConstraint(JPH::SixDOFConstraint *c, const JPH::SixDOFConstraintSettings &settings,
                       const fte_jolt_joint_desc &d)
        : TwoBodyConstraint(*c->GetBody1(), *c->GetBody2(), settings), core(c) {
        lower[0] = d.lower;
        lower[1] = d.lower2;
        upper[0] = d.upper;
        upper[1] = d.upper2;
        force[0] = d.max_force * metres * metres;
        force[1] = d.max_force2 * metres * metres;
        velocity[0] = d.velocity;
        velocity[1] = d.velocity2;
        stiffness[0] = d.stiffness * metres * metres;
        stiffness[1] = d.stiffness2 * metres * metres;
        damping[0] = d.damping * metres * metres;
        damping[1] = d.damping2 * metres * metres;
    }
    JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User1; }
    JPH::Mat44 GetConstraintToBody1Matrix() const override {
        return core->GetConstraintToBody1Matrix();
    }
    JPH::Mat44 GetConstraintToBody2Matrix() const override {
        return core->GetConstraintToBody2Matrix();
    }
    void NotifyShapeChanged(const JPH::BodyID &id, JPH::Vec3Arg delta) override {
        core->NotifyShapeChanged(id, delta);
    }
    void SetSuspension(float stiffness, float damping) {
        core->SetLimitsSpringSettings(
            JPH::SixDOFConstraintSettings::TranslationY,
            JPH::SpringSettings(JPH::ESpringMode::StiffnessAndDamping, stiffness, damping));
        ResetWarmStart();
    }
    void ResetWarmStart() override {
        core->ResetWarmStart();
        for (int i = 0; i < 2; i++) {
            motor[i].Deactivate();
            stop[i].Deactivate();
        }
    }
    void SetupVelocityConstraint(float dt) override {
        core->SetupVelocityConstraint(dt);
        auto frame1 = mBody1->GetCenterOfMassTransform() * core->GetConstraintToBody1Matrix();
        auto frame2 = mBody2->GetCenterOfMassTransform() * core->GetConstraintToBody2Matrix();
        axis[0] = frame1.GetAxisY().Normalized();
        axis[1] = frame2.GetAxisX().Normalized();
        JPH::Quat swing, twist;
        core->GetRotationInConstraintSpace().GetSwingTwist(swing, twist);
        angle[0] = swing.GetRotationAngle(JPH::Vec3::sAxisY());
        angle[1] = twist.GetRotationAngle(JPH::Vec3::sAxisX());
        for (int i = 0; i < 2; i++) {
            if (force[i] > 0)
                motor[i].CalculateConstraintProperties(*mBody1, *mBody2, axis[i], -velocity[i]);
            else
                motor[i].Deactivate();
            if (stiffness[i] > 0 && (angle[i] <= lower[i] || angle[i] >= upper[i])) {
                float error = angle[i] - (angle[i] <= lower[i] ? lower[i] : upper[i]);
                stop[i].CalculateConstraintPropertiesWithStiffnessAndDamping(
                    dt, *mBody1, *mBody2, axis[i], 0, error, stiffness[i], damping[i]);
            } else
                stop[i].Deactivate();
        }
    }
    void WarmStartVelocityConstraint(float ratio) override {
        core->WarmStartVelocityConstraint(ratio);
        for (int i = 0; i < 2; i++) {
            motor[i].WarmStart(*mBody1, *mBody2, ratio);
            stop[i].WarmStart(*mBody1, *mBody2, ratio);
        }
    }
    bool SolveVelocityConstraint(float dt) override {
        bool changed = false;
        for (int i = 0; i < 2; i++) {
            if (motor[i].IsActive())
                changed |= motor[i].SolveVelocityConstraint(*mBody1, *mBody2, axis[i],
                                                            -dt * force[i], dt * force[i]);
            if (stop[i].IsActive()) {
                float lo = lower[i] == upper[i] || angle[i] <= lower[i] ? 0.0f : -FLT_MAX;
                float hi = lower[i] == upper[i] || angle[i] >= upper[i] ? 0.0f : FLT_MAX;
                if (lower[i] == upper[i]) {
                    lo = -FLT_MAX;
                    hi = FLT_MAX;
                }
                changed |= stop[i].SolveVelocityConstraint(*mBody1, *mBody2, axis[i], lo, hi);
            }
        }
        return core->SolveVelocityConstraint(dt) || changed;
    }
    bool SolvePositionConstraint(float dt, float baumgarte) override {
        // Soft stops act in the velocity solver. Hard stops are in the core.
        return core->SolvePositionConstraint(dt, baumgarte);
    }
    void SaveState(JPH::StateRecorder &stream) const override {
        JPH::TwoBodyConstraint::SaveState(stream);
        core->SaveState(stream);
        for (int i = 0; i < 2; i++) {
            motor[i].SaveState(stream);
            stop[i].SaveState(stream);
        }
    }
    void RestoreState(JPH::StateRecorder &stream) override {
        JPH::TwoBodyConstraint::RestoreState(stream);
        core->RestoreState(stream);
        for (int i = 0; i < 2; i++) {
            motor[i].RestoreState(stream);
            stop[i].RestoreState(stream);
        }
    }
    // Engine-level scene serialization is deliberately not implemented yet.
    JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override { return nullptr; }
#ifdef JPH_DEBUG_RENDERER
    void DrawConstraint(JPH::DebugRenderer *renderer) const override {
        core->DrawConstraint(renderer);
    }
#endif
};
#endif
