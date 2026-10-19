// FTEQW Delta, GPL-2.0-or-later. Uses Jolt Physics under its MIT license.
// Jolt requires its umbrella header before every other Jolt header.
#include <Jolt/Jolt.h>

#include "jolt_api.h"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <locale>
#include <map>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {
constexpr float metres = 1.0f / 32.0f;
constexpr size_t contact_limit = 4096, force_limit = 8192;
std::mutex runtime_mutex;
unsigned runtime_users = 0;
std::atomic<uint64_t> next_token{1};
bool acquire_runtime() {
    std::lock_guard<std::mutex> lock(runtime_mutex);
    if (!runtime_users) {
        if (JPH::Factory::sInstance)
            return false;
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory;
        JPH::RegisterTypes();
    }
    ++runtime_users;
    return true;
}
void release_runtime() {
    std::lock_guard<std::mutex> lock(runtime_mutex);
    if (!--runtime_users) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}
bool finite3(const float *v) {
    return v && std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}
bool suspension_ok(float stiffness, float damping) {
    return std::isfinite(stiffness) && stiffness >= 0 && std::isfinite(damping) && damping >= 0 &&
           (stiffness > 0 || damping == 0);
}
bool rotation(const float *v) {
    if (!finite3(v) || !std::isfinite(v[3]))
        return false;
    double length =
        double(v[0]) * v[0] + double(v[1]) * v[1] + double(v[2]) * v[2] + double(v[3]) * v[3];
    return length > .999 && length < 1.001;
}
bool state_ok(const fte_jolt_state *s) {
    return s && finite3(s->position) && rotation(s->rotation) && finite3(s->velocity) &&
           finite3(s->spin);
}
JPH::Vec3 vec(const float *v, float scale = 1) {
    return JPH::Vec3(v[0] * scale, v[1] * scale, v[2] * scale);
}
JPH::RVec3 pos(const float *v) {
    return JPH::RVec3(double(v[0]) * metres, double(v[1]) * metres, double(v[2]) * metres);
}
JPH::Quat quat(const float *v) { return JPH::Quat(v[0], v[1], v[2], v[3]).Normalized(); }
template <class V> void store(V v, float *out, float scale = 1) {
    out[0] = float(v.GetX()) * scale;
    out[1] = float(v.GetY()) * scale;
    out[2] = float(v.GetZ()) * scale;
}
void store_quat(JPH::Quat q, float *v) {
    v[0] = q.GetX();
    v[1] = q.GetY();
    v[2] = q.GetZ();
    v[3] = q.GetW();
}
class Broad final : public JPH::BroadPhaseLayerInterface {
  public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return JPH::BroadPhaseLayer(layer ? 1 : 0);
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer.GetValue() ? "moving" : "static";
    }
#endif
};
class PairFilter final : public JPH::ObjectLayerPairFilter {
  public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        return a == 2 || b == 2;
    }
};
class BroadFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
  public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::BroadPhaseLayer b) const override {
        return a == 2 || b.GetValue() == 1;
    }
};
struct Record {
    JPH::BodyID id;
    int motion;
    unsigned entity, owner, solid_mask, hit_mask, family;
    bool proxy;
    bool has_target = false;
    fte_jolt_state target{};
    float radius = 0, inner_radius = 0;
    bool can_be_dynamic = false;
    JPH::Vec3 driven_velocity = JPH::Vec3::sZero(), driven_spin = JPH::Vec3::sZero();
};
#include "jolt_dual_axis.h"

struct JointRecord {
    JPH::Ref<JPH::TwoBodyConstraint> constraint;
    fte_jolt_body body1, body2;
    int type;
};
struct Force {
    fte_jolt_body token;
    JPH::Vec3 force;
    JPH::RVec3 point;
    bool torque;
};
class Contacts final : public JPH::ContactListener {
    fte_jolt_world *world;
    void record(const JPH::Body &, const JPH::Body &, const JPH::ContactManifold &);

  public:
    explicit Contacts(fte_jolt_world *w) : world(w) {}
    JPH::ValidateResult OnContactValidate(const JPH::Body &, const JPH::Body &, JPH::RVec3Arg,
                                          const JPH::CollideShapeResult &) override;
    void OnContactAdded(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &m,
                        JPH::ContactSettings &) override {
        record(a, b, m);
    }
    void OnContactPersisted(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &m,
                            JPH::ContactSettings &) override {
        record(a, b, m);
    }
};
} // namespace
struct fte_jolt_world {
    Broad broad;
    PairFilter pair_filter;
    BroadFilter broad_filter;
    Contacts listener;
    JPH::TempAllocatorImplWithMallocFallback temp{16 * 1024 * 1024};
    JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
    JPH::PhysicsSystem physics;
    std::unordered_map<fte_jolt_body, Record> bodies;
    std::unordered_map<fte_jolt_joint, JointRecord> joints;
    std::map<std::pair<fte_jolt_body, fte_jolt_body>, unsigned> connected;
    std::vector<Force> forces;
    std::vector<fte_jolt_contact> contacts;
    unsigned max_bodies, max_steps;
    double tick, accumulator = 0;
    fte_jolt_stats stats{};
    char error[256]{};
    fte_jolt_world(unsigned count, unsigned hz, unsigned steps)
        : listener(this), max_bodies(count), max_steps(steps), tick(1.0 / hz) {
        physics.Init(count, 0, count * 8, count * 8, broad, broad_filter, pair_filter);
        physics.SetContactListener(&listener);
        bodies.reserve(count);
        contacts.reserve(contact_limit);
    }
    Record *find(fte_jolt_body token) {
        auto i = bodies.find(token);
        if (i == bodies.end()) {
            std::snprintf(error, sizeof(error), "Invalid or stale body handle");
            return nullptr;
        }
        return &i->second;
    }
    int fail(const char *message) {
        std::snprintf(error, sizeof(error), "%s", message);
        return 0;
    }
};
namespace {
JPH::ValidateResult Contacts::OnContactValidate(const JPH::Body &a, const JPH::Body &b,
                                                JPH::RVec3Arg, const JPH::CollideShapeResult &) {
    auto ai = world->bodies.find(a.GetUserData()), bi = world->bodies.find(b.GetUserData());
    if (ai == world->bodies.end() || bi == world->bodies.end())
        return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
    const Record &x = ai->second, &y = bi->second;
    if (world->connected.count(std::minmax(a.GetUserData(), b.GetUserData())))
        return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
    if (x.family && x.family == y.family && (x.proxy || y.proxy))
        return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
    bool owner = x.entity && y.entity && (x.owner == y.entity || y.owner == x.entity);
    bool dimensions = (x.solid_mask & y.hit_mask) && (y.solid_mask & x.hit_mask);
    return !owner && dimensions ? JPH::ValidateResult::AcceptAllContactsForThisBodyPair
                                : JPH::ValidateResult::RejectAllContactsForThisBodyPair;
}
void Contacts::record(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &m) {
    if (m.mRelativeContactPointsOn1.empty())
        return;
    fte_jolt_body ta = a.GetUserData(), tb = b.GetUserData();
    for (const auto &c : world->contacts)
        if (c.body1 == ta && c.body2 == tb)
            return;
    if (world->contacts.size() == contact_limit) {
        ++world->stats.contact_overflows;
        return;
    }
    fte_jolt_contact contact{};
    contact.body1 = ta;
    contact.body2 = tb;
    contact.entity1 = world->bodies.at(ta).entity;
    contact.entity2 = world->bodies.at(tb).entity;
    store(m.GetWorldSpaceContactPointOn1(0), contact.position, 1 / metres);
    store(m.mWorldSpaceNormal, contact.normal);
    world->contacts.push_back(contact);
}
// Authored collision pieces are deliberately bounded; no implicit decomposition
// of concave triangle soup and no silent replacement with primitive geometry.
constexpr size_t hull_points_limit = 4096, compound_parts_limit = 64;
constexpr size_t compound_bytes_limit = 256 * 1024;
JPH::RefConst<JPH::Shape> convex_shape(fte_jolt_world *w, const float *vertices, size_t count,
                                       float scale = 1) {
    if (!vertices || count < 4 || count > hull_points_limit || !std::isfinite(scale) ||
        scale <= 0) {
        w->fail("Convex hull requires 4..4096 points and a positive finite scale");
        return nullptr;
    }
    JPH::Array<JPH::Vec3> points;
    points.reserve(count);
    JPH::Vec3 low = JPH::Vec3::sReplicate(FLT_MAX), high = -low;
    for (size_t i = 0; i < count; ++i) {
        const float *v = vertices + 3 * i;
        if (!finite3(v) || std::fabs(double(v[0]) * scale) > 1000000 ||
            std::fabs(double(v[1]) * scale) > 1000000 ||
            std::fabs(double(v[2]) * scale) > 1000000) {
            w->fail("Convex coordinates must be finite and within 1000000 local Quake units");
            return nullptr;
        }
        auto point = vec(v, scale * metres);
        points.push_back(point);
        low = JPH::Vec3::sMin(low, point);
        high = JPH::Vec3::sMax(high, point);
    }
    float extent = (high - low).ReduceMin();
    if (extent < .25f * metres) {
        w->fail("Convex hull must span at least 0.25 Quake units on every local axis");
        return nullptr;
    }
    JPH::ConvexHullShapeSettings settings(points, std::min(.01f, extent * .05f));
    settings.mHullTolerance = .0001f;
    settings.mMaxErrorConvexRadius = .001f;
    auto result = settings.Create();
    if (result.HasError()) {
        w->fail(result.GetError().c_str());
        return nullptr;
    }
    auto shape = result.Get();
    float volume = shape->GetVolume();
    auto mass = shape->GetMassProperties();
    if (!std::isfinite(volume) || volume <= 1e-9f || !std::isfinite(mass.mMass) ||
        mass.mMass <= 0 || !std::isfinite(shape->GetInnerRadius()) ||
        shape->GetInnerRadius() <= 0) {
        w->fail("Convex hull requires nonzero three-dimensional volume and valid mass properties");
        return nullptr;
    }
    return shape;
}
class Recipe {
    std::string_view text;
    size_t cursor = 0;

  public:
    explicit Recipe(std::string_view input) : text(input) {}
    std::string_view next() {
        for (;;) {
            while (cursor < text.size() && (text[cursor] == ' ' || text[cursor] == '\t' ||
                                            text[cursor] == '\r' || text[cursor] == '\n'))
                ++cursor;
            if (cursor + 1 < text.size() && text[cursor] == '/' && text[cursor + 1] == '/') {
                while (cursor < text.size() && text[cursor] != '\n')
                    ++cursor;
            } else
                break;
        }
        if (cursor == text.size())
            return {};
        size_t start = cursor++;
        if (text[start] == '{' || text[start] == '}')
            return text.substr(start, 1);
        while (cursor < text.size() && text[cursor] != ' ' && text[cursor] != '\t' &&
               text[cursor] != '\r' && text[cursor] != '\n' && text[cursor] != '{' &&
               text[cursor] != '}')
            ++cursor;
        return text.substr(start, cursor - start);
    }
    static bool number(std::string_view token, float &value) {
        if (token.empty() || token.size() > 64)
            return false;
        // The classic locale makes recipes portable even in non-English hosts.
        std::istringstream stream{std::string(token)};
        stream.imbue(std::locale::classic());
        stream >> std::noskipws >> value;
        return stream && stream.eof() && std::isfinite(value);
    }
};
JPH::RefConst<JPH::Shape> compound_shape(fte_jolt_world *w, const fte_jolt_desc &d) {
    if (!d.compound_text || !d.compound_length || d.compound_length > compound_bytes_limit ||
        std::memchr(d.compound_text, 0, d.compound_length)) {
        w->fail("Compound recipe must contain 1..262144 bytes of text without embedded NULs");
        return nullptr;
    }
    Recipe recipe(std::string_view(d.compound_text, d.compound_length));
    if (recipe.next() != "ftejolt" || recipe.next() != "1") {
        w->fail("Compound recipe must start with ftejolt 1");
        return nullptr;
    }
    JPH::StaticCompoundShapeSettings settings;
    size_t parts = 0, total = 0;
    auto word = recipe.next();
    while (!word.empty()) {
        if (word != "hull" || ++parts > compound_parts_limit || recipe.next() != "{") {
            w->fail("Compound expects hull { x y z ... } blocks, at most 64 pieces");
            return nullptr;
        }
        std::vector<float> vertices;
        word = recipe.next();
        while (word != "}") {
            float v[3];
            if (++total > hull_points_limit || vertices.size() / 3 >= 256 ||
                !Recipe::number(word, v[0]) || !Recipe::number(recipe.next(), v[1]) ||
                !Recipe::number(recipe.next(), v[2])) {
                w->fail("Invalid compound vertex; limit is 256 points per piece, 4096 total");
                return nullptr;
            }
            vertices.insert(vertices.end(), v, v + 3);
            word = recipe.next();
        }
        auto hull = convex_shape(w, vertices.data(), vertices.size() / 3,
                                 d.shape_scale ? d.shape_scale : 1);
        if (!hull)
            return nullptr;
        settings.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), hull.GetPtr());
        word = recipe.next();
    }
    if (!parts) {
        w->fail("Compound recipe contains no hulls");
        return nullptr;
    }
    auto result = settings.Create();
    if (result.HasError()) {
        w->fail(result.GetError().c_str());
        return nullptr;
    }
    return result.Get();
}

JPH::RefConst<JPH::Shape> make_shape(fte_jolt_world *w, const fte_jolt_desc &d) {
    if (d.shape == FTE_JOLT_CONVEX)
        return convex_shape(w, d.vertices, d.vertex_count);
    if (d.shape == FTE_JOLT_COMPOUND)
        return compound_shape(w, d);
    if (d.shape == FTE_JOLT_MESH) {
        if (d.motion == FTE_JOLT_DYNAMIC || !d.vertices || !d.indices || d.vertex_count < 3 ||
            d.vertex_count > 2000000 || !d.triangle_count || d.triangle_count > 2000000) {
            w->fail("Triangle meshes require a static/kinematic body and bounded vertex/index "
                    "data");
            return nullptr;
        }
        JPH::VertexList vertices;
        JPH::IndexedTriangleList triangles;
        vertices.reserve(d.vertex_count);
        triangles.reserve(d.triangle_count);
        for (size_t i = 0; i < d.vertex_count; i++) {
            const float *v = d.vertices + 3 * i;
            if (!finite3(v)) {
                w->fail("Nonfinite mesh vertex");
                return nullptr;
            }
            vertices.emplace_back(v[0] * metres, v[1] * metres, v[2] * metres);
        }
        for (size_t i = 0; i < d.triangle_count; i++) {
            const int *p = d.indices + 3 * i;
            if (p[0] < 0 || p[1] < 0 || p[2] < 0 || size_t(p[0]) >= d.vertex_count ||
                size_t(p[1]) >= d.vertex_count || size_t(p[2]) >= d.vertex_count) {
                w->fail("Mesh triangle index out of bounds");
                return nullptr;
            }
            triangles.emplace_back(p[0], p[1], p[2]);
        }
        auto result = JPH::MeshShapeSettings(std::move(vertices), std::move(triangles)).Create();
        if (result.HasError()) {
            w->fail(result.GetError().c_str());
            return nullptr;
        }
        return result.Get();
    }
    if (!finite3(d.size) || d.size[0] < .25f || d.size[1] < .25f || d.size[2] < .25f) {
        w->fail("Primitive body dimensions must be finite and at least 0.25 Quake "
                "units");
        return nullptr;
    }
    JPH::Vec3 half = vec(d.size, .5f * metres);
    if (d.shape == FTE_JOLT_BOX) {
        auto result = JPH::BoxShapeSettings(half, std::min(.01f, half.ReduceMin() * .1f)).Create();
        if (result.HasError()) {
            w->fail(result.GetError().c_str());
            return nullptr;
        }
        return result.Get();
    }
    if (d.shape == FTE_JOLT_SPHERE)
        return new JPH::SphereShape(half.GetX());
    bool capsule = d.shape >= FTE_JOLT_CAPSULE_X && d.shape <= FTE_JOLT_CAPSULE_Z;
    bool cylinder = d.shape >= FTE_JOLT_CYLINDER_X && d.shape <= FTE_JOLT_CYLINDER_Z;
    if (!capsule && !cylinder) {
        w->fail("Unsupported shape");
        return nullptr;
    }
    int axis = d.shape - (capsule ? FTE_JOLT_CAPSULE_X : FTE_JOLT_CYLINDER_X);
    float extents[3] = {half.GetX(), half.GetY(), half.GetZ()};
    float radius = std::min(extents[(axis + 1) % 3], extents[(axis + 2) % 3]);
    JPH::RefConst<JPH::Shape> shape;
    if (capsule) {
        radius = std::min(radius, extents[axis]);
        float height = extents[axis] - radius;
        if (height <= 1e-6f)
            shape = new JPH::SphereShape(radius);
        else
            shape = new JPH::CapsuleShape(height, radius);
    } else
        shape =
            new JPH::CylinderShape(extents[axis], radius, std::min(radius, extents[axis]) * .1f);
    if (axis == 1)
        return shape;
    JPH::Quat turn = JPH::Quat::sRotation(axis == 0 ? JPH::Vec3::sAxisZ() : JPH::Vec3::sAxisX(),
                                          JPH::JPH_PI / 2);
    return new JPH::RotatedTranslatedShape(JPH::Vec3::sZero(), turn, shape);
}
} // namespace
extern "C" {
#define FTE_JOLT_STRING_(x) #x
#define FTE_JOLT_STRING(x) FTE_JOLT_STRING_(x)
const char *fte_jolt_version(void) {
    return FTE_JOLT_STRING(JPH_VERSION_MAJOR) "." FTE_JOLT_STRING(
        JPH_VERSION_MINOR) "." FTE_JOLT_STRING(JPH_VERSION_PATCH);
}
fte_jolt_world *fte_jolt_create(unsigned max_bodies, unsigned hz, unsigned max_steps) {
    if (!max_bodies || max_bodies > 8192 || !hz || hz > 1000 || !max_steps || max_steps > 64 ||
        !acquire_runtime())
        return nullptr;
    auto *world = new (std::nothrow) fte_jolt_world(max_bodies, hz, max_steps);
    if (!world)
        release_runtime();
    return world;
}
void fte_jolt_destroy(fte_jolt_world *w) {
    if (!w)
        return;
    while (!w->joints.empty())
        fte_jolt_joint_remove(w, w->joints.begin()->first);
    auto &api = w->physics.GetBodyInterface();
    for (const auto &p : w->bodies) {
        api.RemoveBody(p.second.id);
        api.DestroyBody(p.second.id);
    }
    delete w;
    release_runtime();
}
const char *fte_jolt_error(const fte_jolt_world *w) {
    return w ? w->error : "Jolt world is null or could not be created";
}
fte_jolt_body fte_jolt_add(fte_jolt_world *w, const fte_jolt_desc *d) {
    if (!w)
        return 0;
    if (!d || !state_ok(&d->state) || d->motion < 0 || d->motion > 2 || !std::isfinite(d->mass) ||
        (d->motion == FTE_JOLT_DYNAMIC && d->mass <= 0) || !std::isfinite(d->friction) ||
        d->friction < 0 || !std::isfinite(d->restitution) || d->restitution < 0 ||
        d->restitution > 1 || !std::isfinite(d->gravity_scale) ||
        !std::isfinite(d->linear_damping) || d->linear_damping < 0 ||
        !std::isfinite(d->angular_damping) || d->angular_damping < 0) {
        w->fail("Invalid body state/material/mass");
        return 0;
    }
    if (w->bodies.size() >= w->max_bodies) {
        w->fail("Jolt body capacity reached");
        return 0;
    }
    auto shape = make_shape(w, *d);
    if (!shape)
        return 0;
    JPH::EMotionType motion = d->motion == 0   ? JPH::EMotionType::Static
                              : d->motion == 1 ? JPH::EMotionType::Kinematic
                                               : JPH::EMotionType::Dynamic;
    JPH::BodyCreationSettings settings(shape, pos(d->state.position), quat(d->state.rotation),
                                       motion, JPH::ObjectLayer(d->motion));
    auto token = next_token.fetch_add(1);
    settings.mUserData = token;
    settings.mFriction = d->friction;
    settings.mRestitution = d->restitution;
    settings.mGravityFactor = d->gravity_scale;
    settings.mLinearVelocity = vec(d->state.velocity, metres);
    settings.mAngularVelocity = vec(d->state.spin);
    settings.mLinearDamping = d->linear_damping;
    settings.mAngularDamping = d->angular_damping;
    settings.mMaxLinearVelocity = 100000 * metres;
    settings.mMotionQuality = d->motion == FTE_JOLT_DYNAMIC ? JPH::EMotionQuality::LinearCast
                                                            : JPH::EMotionQuality::Discrete;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = d->mass > 0 ? d->mass : 1;
    if (d->motion == FTE_JOLT_KINEMATIC && d->shape == FTE_JOLT_MESH) {
        // Mesh shapes have no volumetric inertia. Kinematics have infinite
        // solver mass; provide a harmless explicit tensor instead of querying it.
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
        settings.mMassPropertiesOverride.mInertia = JPH::Mat44::sIdentity();
    }
    auto &api = w->physics.GetBodyInterface();
    auto *body = api.CreateBody(settings);
    if (!body) {
        w->fail("Jolt could not allocate body");
        return 0;
    }
    auto id = body->GetID();
    w->bodies.emplace(token, Record{id, d->motion, d->entity, d->owner, d->solid_mask, d->hit_mask,
                                    d->collision_family, d->is_proxy != 0});
    auto &record = w->bodies.at(token);
    record.radius = shape->GetLocalBounds().GetExtent().Length();
    record.inner_radius = shape->GetInnerRadius();
    record.can_be_dynamic = d->shape != FTE_JOLT_MESH;
    api.AddBody(id, d->motion ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    return token;
}
int fte_jolt_remove(fte_jolt_world *w, fte_jolt_body body) {
    if (!w)
        return 0;
    auto *p = w->find(body);
    if (!p)
        return 0;
    for (auto it = w->joints.begin(); it != w->joints.end();) {
        auto current = it++;
        if (current->second.body1 == body || current->second.body2 == body)
            fte_jolt_joint_remove(w, current->first);
    }
    auto &api = w->physics.GetBodyInterface();
    auto bounds = api.GetTransformedShape(p->id).GetWorldSpaceBounds();
    bounds.ExpandBy(JPH::Vec3::sReplicate(2 * metres));
    // Sleeping contacts are not necessarily revisited after their support is
    // removed; wake the local neighborhood before discarding that geometry.
    api.ActivateBodiesInAABox(bounds, JPH::BroadPhaseLayerFilter(), JPH::ObjectLayerFilter());
    api.RemoveBody(p->id);
    api.DestroyBody(p->id);
    w->bodies.erase(body);
    return 1;
}
int fte_jolt_get(fte_jolt_world *w, fte_jolt_body body, fte_jolt_state *out) {
    if (!w || !out)
        return 0;
    auto *p = w->find(body);
    if (!p)
        return 0;
    auto &api = w->physics.GetBodyInterface();
    JPH::RVec3 position;
    JPH::Quat orientation;
    api.GetPositionAndRotation(p->id, position, orientation);
    store(position, out->position, 1 / metres);
    store_quat(orientation, out->rotation);
    if (p->motion) {
        store(api.GetLinearVelocity(p->id), out->velocity, 1 / metres);
        store(api.GetAngularVelocity(p->id), out->spin);
    } else {
        std::fill(out->velocity, out->velocity + 3, 0);
        std::fill(out->spin, out->spin + 3, 0);
    }
    return 1;
}
int fte_jolt_set(fte_jolt_world *w, fte_jolt_body body, const fte_jolt_state *s) {
    if (!w)
        return 0;
    if (!state_ok(s))
        return w->fail("Invalid body state");
    auto *p = w->find(body);
    if (!p)
        return 0;
    p->has_target = false;
    p->driven_velocity = p->driven_spin = JPH::Vec3::sZero();
    auto &api = w->physics.GetBodyInterface();
    if (p->motion)
        api.SetPositionRotationAndVelocity(p->id, pos(s->position), quat(s->rotation),
                                           vec(s->velocity, metres), vec(s->spin));
    else
        api.SetPositionAndRotation(p->id, pos(s->position), quat(s->rotation),
                                   JPH::EActivation::DontActivate);
    return 1;
}
int fte_jolt_move(fte_jolt_world *w, fte_jolt_body token, const fte_jolt_state *target) {
    if (!w)
        return 0;
    auto *body = w->find(token);
    if (!body || body->motion != FTE_JOLT_KINEMATIC)
        return w->fail("Kinematic target requires a live kinematic body");
    if (!state_ok(target))
        return w->fail("Invalid kinematic target");
    body->target = *target;
    body->has_target = true;
    return 1;
}
int fte_jolt_motion(fte_jolt_world *w, fte_jolt_body token, int motion) {
    if (!w)
        return 0;
    auto *body = w->find(token);
    if (!body || body->motion == FTE_JOLT_STATIC ||
        (motion != FTE_JOLT_KINEMATIC && motion != FTE_JOLT_DYNAMIC) ||
        (motion == FTE_JOLT_DYNAMIC && !body->can_be_dynamic))
        return w->fail("Motion changes require a live movable convex body");
    if (body->motion == motion)
        return 1;
    auto &api = w->physics.GetBodyInterface();
    auto velocity = motion == FTE_JOLT_DYNAMIC ? body->driven_velocity : JPH::Vec3::sZero();
    auto spin = motion == FTE_JOLT_DYNAMIC ? body->driven_spin : JPH::Vec3::sZero();
    body->has_target = false;
    body->driven_velocity = body->driven_spin = JPH::Vec3::sZero();
    w->forces.erase(std::remove_if(w->forces.begin(), w->forces.end(),
                                   [token](const Force &force) { return force.token == token; }),
                    w->forces.end());
    api.SetMotionType(body->id,
                      motion == FTE_JOLT_DYNAMIC ? JPH::EMotionType::Dynamic
                                                 : JPH::EMotionType::Kinematic,
                      JPH::EActivation::Activate);
    body->motion = motion;
    api.SetObjectLayer(body->id, JPH::ObjectLayer(motion));
    api.SetMotionQuality(body->id, motion == FTE_JOLT_DYNAMIC ? JPH::EMotionQuality::LinearCast
                                                              : JPH::EMotionQuality::Discrete);
    api.SetLinearAndAngularVelocity(body->id, velocity, spin);
    api.InvalidateContactCache(body->id);
    // Preserve constraint anchors and body identity across animation handoffs.
    // Cached impulses from the previous infinite/finite mass must be discarded.
    for (const auto &entry : w->joints)
        if (entry.second.body1 == token || entry.second.body2 == token) {
            entry.second.constraint->ResetWarmStart();
            api.ActivateConstraint(entry.second.constraint);
        }
    return 1;
}
int fte_jolt_material(fte_jolt_world *w, fte_jolt_body body, float friction, float restitution,
                      float gravity) {
    if (!w)
        return 0;
    auto *p = w->find(body);
    if (!p)
        return 0;
    if (!std::isfinite(friction) || friction < 0 || !std::isfinite(restitution) ||
        restitution < 0 || restitution > 1 || !std::isfinite(gravity))
        return w->fail("Invalid material");
    auto &api = w->physics.GetBodyInterface();
    api.SetFriction(p->id, friction);
    api.SetRestitution(p->id, restitution);
    if (p->motion)
        api.SetGravityFactor(p->id, gravity);
    return 1;
}
int fte_jolt_filter(fte_jolt_world *w, fte_jolt_body body, unsigned owner, unsigned solid,
                    unsigned hit) {
    if (!w)
        return 0;
    auto *p = w->find(body);
    if (!p)
        return 0;
    if (p->owner != owner || p->solid_mask != solid || p->hit_mask != hit) {
        p->owner = owner;
        p->solid_mask = solid;
        p->hit_mask = hit;
        auto &api = w->physics.GetBodyInterface();
        api.InvalidateContactCache(p->id);
        // A sleeping body must reconsider contacts when its collision rules
        // change, including losing the floor it was resting on.
        if (p->motion == FTE_JOLT_DYNAMIC)
            api.ActivateBody(p->id);
    }
    return 1;
}
int fte_jolt_enable(fte_jolt_world *w, fte_jolt_body body, int enabled) {
    if (!w)
        return 0;
    auto *p = w->find(body);
    if (!p || p->motion != FTE_JOLT_DYNAMIC)
        return 0;
    auto &api = w->physics.GetBodyInterface();
    if (enabled)
        api.ActivateBody(p->id);
    else
        api.DeactivateBody(p->id);
    return 1;
}
int fte_jolt_force(fte_jolt_world *w, fte_jolt_body body, const float *force, const float *point) {
    if (!w)
        return 0;
    auto *p = w->find(body);
    if (!p || p->motion != FTE_JOLT_DYNAMIC)
        return 0;
    if (!finite3(force) || !finite3(point) || w->forces.size() >= force_limit)
        return w->fail("Invalid force or command capacity exceeded");
    w->forces.push_back(Force{body, vec(force, metres), pos(point), false});
    return 1;
}
int fte_jolt_torque(fte_jolt_world *w, fte_jolt_body body, const float *torque) {
    if (!w)
        return 0;
    auto *p = w->find(body);
    if (!p || p->motion != FTE_JOLT_DYNAMIC)
        return 0;
    if (!finite3(torque) || w->forces.size() >= force_limit)
        return w->fail("Invalid torque or command capacity exceeded");
    w->forces.push_back(Force{body, vec(torque, metres * metres), JPH::RVec3::sZero(), true});
    return 1;
}
int fte_jolt_impulse(fte_jolt_world *w, fte_jolt_body body, const float *impulse) {
    if (!w)
        return 0;
    auto *p = w->find(body);
    if (!p || p->motion != FTE_JOLT_DYNAMIC)
        return 0;
    if (!finite3(impulse))
        return w->fail("Invalid impulse");
    w->physics.GetBodyInterface().AddImpulse(p->id, vec(impulse, metres));
    return 1;
}
int fte_jolt_step(fte_jolt_world *w, double dt, double gravity) {
    if (!w)
        return 0;
    if (!std::isfinite(dt) || dt < 0 || !std::isfinite(gravity))
        return w->fail("Invalid physics step");
    w->stats.steps = 0;
    w->stats.collision_steps = 0;
    w->contacts.clear();
    double limit = w->tick * w->max_steps;
    if (dt > limit) {
        w->stats.dropped_time += dt - limit;
        dt = limit;
    }
    w->accumulator += dt;
    w->physics.SetGravity(JPH::Vec3(0, 0, float(-gravity * metres)));
    auto &api = w->physics.GetBodyInterface();
    unsigned ticks = std::min(w->max_steps, unsigned((w->accumulator + 1e-10) / w->tick));
    struct Move {
        Record *body;
        JPH::RVec3 start;
        JPH::Quat rotation;
    };
    std::vector<Move> moves;
    float sweep_distance = 4 * metres;
    if (ticks)
        for (auto &entry : w->bodies) {
            auto &body = entry.second;
            if (body.motion == FTE_JOLT_DYNAMIC && body.inner_radius > 0)
                sweep_distance =
                    std::min(sweep_distance, std::max(metres / 32, body.inner_radius * .5f));
            if (body.motion == FTE_JOLT_KINEMATIC && !body.has_target)
                body.driven_velocity = body.driven_spin = JPH::Vec3::sZero();
            if (body.has_target) {
                JPH::RVec3 start;
                JPH::Quat rotation;
                api.GetPositionAndRotation(body.id, start, rotation);
                moves.push_back({&body, start, rotation});
            }
        }
    while (w->accumulator + 1e-10 >= w->tick && w->stats.steps < w->max_steps) {
        unsigned collision_steps = 1;
        for (const auto &move : moves) {
            auto &body = *move.body;
            float fraction = float(w->stats.steps + 1) / ticks;
            auto target = move.start + (pos(body.target.position) - move.start) * fraction;
            auto orientation = move.rotation.SLERP(quat(body.target.rotation), fraction);
            JPH::RVec3 current;
            JPH::Quat rotation;
            api.GetPositionAndRotation(body.id, current, rotation);
            float travel = JPH::Vec3(target - current).Length() +
                           body.radius * 2 *
                               std::acos(std::min(
                                   1.0f, std::fabs((orientation * rotation.Conjugated()).GetW())));
            collision_steps =
                std::max(collision_steps,
                         unsigned(std::min(64.0, std::ceil(double(travel) / sweep_distance))));
            api.MoveKinematic(body.id, target, orientation, float(w->tick));
        }
        for (const auto &force : w->forces) {
            auto i = w->bodies.find(force.token);
            if (i == w->bodies.end())
                continue;
            if (force.torque)
                api.AddTorque(i->second.id, force.force);
            else
                api.AddForce(i->second.id, force.force, force.point);
        }
        auto error = w->physics.Update(float(w->tick), int(collision_steps), &w->temp, &w->jobs);
        if (error != JPH::EPhysicsUpdateError::None)
            return w->fail("Jolt simulation capacity exceeded");
        w->accumulator = std::max(0.0, w->accumulator - w->tick);
        ++w->stats.steps;
        w->stats.collision_steps += collision_steps;
        w->stats.simulated_time += w->tick;
    }
    if (w->stats.steps) {
        w->forces.clear();
        for (const auto &move : moves) {
            move.body->driven_velocity = api.GetLinearVelocity(move.body->id);
            move.body->driven_spin = api.GetAngularVelocity(move.body->id);
            move.body->has_target = false;
            api.SetLinearAndAngularVelocity(move.body->id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
        }
    }
    return 1;
}
void fte_jolt_get_stats(const fte_jolt_world *w, fte_jolt_stats *out) {
    if (!out)
        return;
    *out = {};
    if (!w)
        return;
    *out = w->stats;
    out->bodies = unsigned(w->bodies.size());
    out->joints = unsigned(w->joints.size());
    out->active = w->physics.GetNumActiveBodies(JPH::EBodyType::RigidBody);
}
size_t fte_jolt_contacts(const fte_jolt_world *w) { return w ? w->contacts.size() : 0; }
int fte_jolt_get_contact(const fte_jolt_world *w, size_t i, fte_jolt_contact *out) {
    if (!w || !out || i >= w->contacts.size())
        return 0;
    *out = w->contacts[i];
    return 1;
}
fte_jolt_joint fte_jolt_joint_add(fte_jolt_world *w, const fte_jolt_joint_desc *d) {
    if (!w || !d)
        return 0;
    if (w->joints.size() >= 16384 || (!d->body1 && !d->body2) || d->body1 == d->body2 ||
        !finite3(d->anchor) || !finite3(d->axis) || !std::isfinite(d->lower) ||
        !std::isfinite(d->upper) || d->lower > 0 || d->upper < 0 || !std::isfinite(d->max_force) ||
        d->max_force < 0 || !std::isfinite(d->velocity) || !std::isfinite(d->stiffness) ||
        d->stiffness < 0 || !std::isfinite(d->damping) || d->damping < 0) {
        w->fail("Invalid joint settings or joint capacity exceeded");
        return 0;
    }
    if (!suspension_ok(d->suspension_stiffness, d->suspension_damping) ||
        (d->type != FTE_JOLT_HINGE2 &&
         (d->suspension_stiffness != 0 || d->suspension_damping != 0))) {
        w->fail("Suspension requires hinge2, nonnegative finite stiffness/damping and positive "
                "stiffness for damping");
        return 0;
    }
    Record *a = d->body1 ? w->find(d->body1) : nullptr, *b = d->body2 ? w->find(d->body2) : nullptr;
    if ((d->body1 && !a) || (d->body2 && !b))
        return 0;
    if ((!a || a->motion != FTE_JOLT_DYNAMIC) && (!b || b->motion != FTE_JOLT_DYNAMIC)) {
        w->fail("A joint requires at least one dynamic body");
        return 0;
    }
    auto &api = w->physics.GetBodyInterface();
    JPH::BodyID id1 = a ? a->id : JPH::BodyID(), id2 = b ? b->id : JPH::BodyID();
    JPH::Ref<JPH::TwoBodyConstraint> constraint;
    JPH::Vec3 axis = vec(d->axis), normal = JPH::Vec3::sZero();
    if (d->type == FTE_JOLT_HINGE || d->type == FTE_JOLT_SLIDER || d->type == FTE_JOLT_UNIVERSAL ||
        d->type == FTE_JOLT_HINGE2) {
        if (std::fabs(axis.LengthSq() - 1) > .001) {
            w->fail("Joint axis must be normalized");
            return 0;
        }
        axis = axis.Normalized();
        normal = axis.GetNormalizedPerpendicular();
    }
    if (d->type == FTE_JOLT_POINT) {
        JPH::PointConstraintSettings settings;
        settings.mPoint1 = settings.mPoint2 = pos(d->anchor);
        constraint = api.CreateConstraint(&settings, id1, id2);
    } else if (d->type == FTE_JOLT_FIXED) {
        JPH::FixedConstraintSettings settings;
        settings.mAutoDetectPoint = true;
        constraint = api.CreateConstraint(&settings, id1, id2);
    } else if (d->type == FTE_JOLT_HINGE) {
        if (d->lower < -JPH::JPH_PI || d->upper > JPH::JPH_PI) {
            w->fail("Hinge limits must be within [-pi, pi]");
            return 0;
        }
        JPH::HingeConstraintSettings settings;
        settings.mPoint1 = settings.mPoint2 = pos(d->anchor);
        settings.mHingeAxis1 = settings.mHingeAxis2 = axis;
        settings.mNormalAxis1 = settings.mNormalAxis2 = normal;
        settings.mLimitsMin = d->lower;
        settings.mLimitsMax = d->upper;
        settings.mLimitsSpringSettings =
            JPH::SpringSettings(JPH::ESpringMode::StiffnessAndDamping,
                                d->stiffness * metres * metres, d->damping * metres * metres);
        settings.mMotorSettings.SetTorqueLimit(d->max_force * metres * metres);
        constraint = api.CreateConstraint(&settings, id1, id2);
        if (constraint && d->max_force > 0) {
            auto *hinge = static_cast<JPH::HingeConstraint *>(constraint.GetPtr());
            hinge->SetTargetAngularVelocity(d->velocity);
            hinge->SetMotorState(JPH::EMotorState::Velocity);
        }
    } else if (d->type == FTE_JOLT_SLIDER) {
        JPH::SliderConstraintSettings settings;
        settings.mAutoDetectPoint = true;
        settings.mSliderAxis1 = settings.mSliderAxis2 = axis;
        settings.mNormalAxis1 = settings.mNormalAxis2 = normal;
        settings.mLimitsMin = d->lower * metres;
        settings.mLimitsMax = d->upper * metres;
        settings.mLimitsSpringSettings =
            JPH::SpringSettings(JPH::ESpringMode::StiffnessAndDamping, d->stiffness, d->damping);
        settings.mMotorSettings.SetForceLimit(d->max_force * metres);
        constraint = api.CreateConstraint(&settings, id1, id2);
        if (constraint && d->max_force > 0) {
            auto *slider = static_cast<JPH::SliderConstraint *>(constraint.GetPtr());
            slider->SetTargetVelocity(d->velocity * metres);
            slider->SetMotorState(JPH::EMotorState::Velocity);
        }
    } else if (d->type == FTE_JOLT_UNIVERSAL || d->type == FTE_JOLT_HINGE2) {
        JPH::Vec3 second = vec(d->axis2);
        if (!finite3(d->axis2) || std::fabs(second.LengthSq() - 1) > .001 ||
            std::fabs(axis.Dot(second)) > .001 || !std::isfinite(d->lower2) ||
            !std::isfinite(d->upper2) || d->lower2 > 0 || d->upper2 < 0 ||
            d->lower < -JPH::JPH_PI || d->upper > JPH::JPH_PI || d->lower2 < -JPH::JPH_PI ||
            d->upper2 > JPH::JPH_PI || !std::isfinite(d->max_force2) || d->max_force2 < 0 ||
            !std::isfinite(d->velocity2) || !std::isfinite(d->stiffness2) || d->stiffness2 < 0 ||
            !std::isfinite(d->damping2) || d->damping2 < 0) {
            w->fail("Invalid two-axis joint: need orthogonal unit axes, angular "
                    "limits and valid suspension");
            return 0;
        }
        second = (second - axis * axis.Dot(second)).Normalized();
        JPH::SixDOFConstraintSettings settings;
        using Axis = JPH::SixDOFConstraintSettings;
        settings.mPosition1 = settings.mPosition2 = pos(d->anchor);
        // R = Ry(angle1) * Rx(angle2), with Y attached to body1, X to body2.
        settings.mAxisX1 = settings.mAxisX2 = second;
        settings.mAxisY1 = settings.mAxisY2 = axis;
        for (int i = 0; i < 3; i++)
            settings.MakeFixedAxis(static_cast<Axis::EAxis>(i));
        settings.MakeFixedAxis(Axis::RotationZ);
        settings.mSwingType = JPH::ESwingType::Pyramid;
        settings.SetLimitedAxis(Axis::RotationY, d->stiffness > 0 ? -JPH::JPH_PI : d->lower,
                                d->stiffness > 0 ? JPH::JPH_PI : d->upper);
        settings.SetLimitedAxis(Axis::RotationX, d->stiffness2 > 0 ? -JPH::JPH_PI : d->lower2,
                                d->stiffness2 > 0 ? JPH::JPH_PI : d->upper2);
        if (d->type == FTE_JOLT_HINGE2 && d->suspension_stiffness > 0) {
            settings.SetLimitedAxis(Axis::TranslationY, 0, 0);
            settings.mLimitsSpringSettings[Axis::TranslationY] =
                JPH::SpringSettings(JPH::ESpringMode::StiffnessAndDamping, d->suspension_stiffness,
                                    d->suspension_damping);
        }
        auto *core =
            static_cast<JPH::SixDOFConstraint *>(api.CreateConstraint(&settings, id1, id2));
        if (core)
            constraint = new DualAxisConstraint(core, settings, *d);
    } else {
        w->fail("Unsupported joint type");
        return 0;
    }
    if (!constraint) {
        w->fail("Unable to create Jolt constraint");
        return 0;
    }
    fte_jolt_joint token = next_token.fetch_add(1);
    w->joints.emplace(token, JointRecord{constraint, d->body1, d->body2, d->type});
    if (d->body1 && d->body2)
        ++w->connected[std::minmax(d->body1, d->body2)];
    w->physics.AddConstraint(constraint);
    if (a)
        api.InvalidateContactCache(a->id);
    if (b)
        api.InvalidateContactCache(b->id);
    api.ActivateConstraint(constraint);
    return token;
}
int fte_jolt_joint_suspension(fte_jolt_world *w, fte_jolt_joint token, float stiffness,
                              float damping) {
    if (!w)
        return 0;
    auto it = w->joints.find(token);
    if (it == w->joints.end())
        return w->fail("Invalid or stale joint handle");
    if (it->second.type != FTE_JOLT_HINGE2 || !suspension_ok(stiffness, damping))
        return w->fail("Invalid hinge2 suspension settings");
    static_cast<DualAxisConstraint *>(it->second.constraint.GetPtr())
        ->SetSuspension(stiffness, damping);
    w->physics.GetBodyInterface().ActivateConstraint(it->second.constraint);
    return 1;
}
int fte_jolt_joint_remove(fte_jolt_world *w, fte_jolt_joint token) {
    if (!w)
        return 0;
    auto it = w->joints.find(token);
    if (it == w->joints.end())
        return w->fail("Invalid or stale joint handle");
    auto &j = it->second;
    auto &api = w->physics.GetBodyInterface();
    api.ActivateConstraint(j.constraint);
    w->physics.RemoveConstraint(j.constraint);
    if (j.body1 && j.body2) {
        auto pair = w->connected.find(std::minmax(j.body1, j.body2));
        if (pair != w->connected.end() && !--pair->second)
            w->connected.erase(pair);
    }
    for (auto body : {j.body1, j.body2})
        if (body)
            api.InvalidateContactCache(w->bodies.at(body).id);
    w->joints.erase(it);
    return 1;
}
int fte_jolt_joint_enable(fte_jolt_world *w, fte_jolt_joint token, int enabled) {
    if (!w)
        return 0;
    auto it = w->joints.find(token);
    if (it == w->joints.end())
        return w->fail("Invalid or stale joint handle");
    it->second.constraint->SetEnabled(enabled != 0);
    w->physics.GetBodyInterface().ActivateConstraint(it->second.constraint);
    return 1;
}
int fte_jolt_joint_frame(fte_jolt_world *w, fte_jolt_joint token, fte_jolt_state *out) {
    if (!w || !out)
        return 0;
    auto it = w->joints.find(token);
    if (it == w->joints.end())
        return w->fail("Invalid or stale joint handle");
    auto *c = it->second.constraint.GetPtr();
    auto matrix = c->GetBody1()->GetCenterOfMassTransform() * c->GetConstraintToBody1Matrix();
    *out = {};
    store(matrix.GetTranslation(), out->position, 1 / metres);
    store_quat(matrix.GetQuaternion().Normalized(), out->rotation);
    return 1;
}
int fte_jolt_quat(const float *forward, const float *left, const float *up, float *out) {
    if (!out || !finite3(forward) || !finite3(left) || !finite3(up))
        return 0;
    auto f = vec(forward), l = vec(left), u = vec(up);
    if (std::fabs(f.LengthSq() - 1) > .001 || std::fabs(l.LengthSq() - 1) > .001 ||
        std::fabs(u.LengthSq() - 1) > .001 || f.Cross(l).Dot(u) < .999)
        return 0;
    auto q = JPH::Mat44(JPH::Vec4(f, 0), JPH::Vec4(l, 0), JPH::Vec4(u, 0), JPH::Vec4(0, 0, 0, 1))
                 .GetQuaternion()
                 .Normalized();
    store_quat(q, out);
    return 1;
}
void fte_jolt_axes(const float *q, float *forward, float *left, float *up) {
    auto matrix = JPH::Mat44::sRotation(quat(q));
    store(matrix.GetAxisX(), forward);
    store(matrix.GetAxisY(), left);
    store(matrix.GetAxisZ(), up);
}
}
