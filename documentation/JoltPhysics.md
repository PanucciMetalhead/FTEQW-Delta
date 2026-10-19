# Experimental Jolt Physics

The `dev/jolt` milestone adds an **optional rigid-body plugin** for QuakeC mods.
It is off by default at build time and at runtime. It does not replace classic
Quake player movement, prediction, projectiles, or ordinary entity physics.
ODE remains available as a reference and fallback; the legacy Bullet backend
is retained during the transition. Quake 3's qagame physics is
not redirected to Jolt.

## Scope of this milestone

Supported through the existing SSQC/CSQC rigid-body interface:

- Dynamic boxes, spheres, capsules and cylinders, including explicit X/Y/Z axes.
- Static triangle collision meshes generated from world/brush models, with the
  existing mesh generator's sky/liquid exclusions.
- Mass, friction, restitution, gravity scale, linear/angular damping, position,
  orientation, linear/angular velocity, forces, torque and sleeping/waking.
- Owner and collision-dimension filtering; QuakeC contact callbacks deferred
  until simulation finishes, with entity removal/reuse checks.
- Point, fixed, hinge, slider, universal and hinge2 joints, including world anchors, limits,
  velocity motors, hinge2 linear suspension, enabling/disabling and connected-body collision exclusion.
- Experimental `.doll` rigs: primitive bodies, pose read/write, skeletal joints,
  passive simulation, and explicit cleanup on world/backend shutdown.
- World creation/destruction, backend restart, plugin unload/reload and map changes.

A mod must explicitly use `MOVETYPE_PHYSICS`. Loading the plugin does not convert
existing monsters, items, doors or players to rigid bodies. Simulation results
are copied back only to entities using that movement type. Other solid entities
provide static collision proxies, refreshed from their QuakeC poses each frame.
Those proxies do **not** yet implement swept kinematic pushers or velocity-based
interaction with moving platforms.

**Not implemented:** dynamic triangle meshes/convex conversion, swept kinematic pushers,
multithreaded simulation, or Jolt state serialization. Unsupported rig joints
reject the entire rig and roll back constructed bodies/joints. There is no
promise of ODE-identical solver response or multiplayer determinism. SSQC and
native Metal rendering are exercised; CSQC uses the shared interface but has
not had a separate end-to-end validation run. Save/load of an actively simulated
scene is not part of this milestone's validation.

## Build

The engine remains C17. Only Jolt and its isolated adapter use C++17. Use a
separate build directory so an experimental build does not replace your usual
executable. Supply a local Jolt source checkout; CMake does not download it or
build inside it.

The tested snapshot is `jrouwe/JoltPhysics` commit
`1b2e77e3c6c09709e5edc79dc208459b8ff32051` (headers report 5.6.1).
Desktop CI is configured to use that exact snapshot. Other revisions have not
been validated. The source directory must contain `Build/CMakeLists.txt` and
`Jolt/`.

```sh
cmake -S . -B build/jolt -DCMAKE_BUILD_TYPE=Debug \
  -DFTE_PLUG_JOLT=ON -DFTE_JOLT_SOURCE_DIR=/path/to/JoltPhysics \
  -DFTE_TOOL_QCCGUI=OFF -DFTE_TOOL_QCVM=ON -DBUILD_TESTING=ON
cmake --build build/jolt --target fteqw jolt-check-deps --config Debug --parallel
ctest --test-dir build/jolt -C Debug -R 'jolt-' --output-on-failure
```

For a headless test build, add `-DFTE_ENGINE=OFF
-DFTE_ENGINE_SERVER_ONLY=ON` and omit `fteqw` from the build targets.
Multi-configuration generators put binaries under `build/jolt/Debug`; the
plugin must remain beside the engine executable. Rigid-body plugin ABI 2 checks
entity and joint layout sizes; rebuild physics plugins together with this engine.
Older ABI 1 modules are rejected before their callbacks are used. The Jolt
module is `fteplug_jolt.so` on
macOS/Linux or `fteplug_jolt.dll` on Windows. Jolt is statically linked into the
plugin; no separate Jolt shared library is required. Retain the accompanying
`Jolt-LICENSE.txt` when distributing it.

The build uses double-precision internal positions, a portable CPU feature
baseline and single-threaded Jolt jobs. Jolt compiler options stay scoped to
its dependency target; they do not change the C17 engine's movement/math flags.
The existing SDL3 renderer still selects Metal/Vulkan independently of physics.

If ODE development headers/libraries are installed, CMake's existing
`FTE_PLUG_ODE` option can build `plug_ode`. The tests optionally compare against
that library and plugin. `FTE_JOLT_ODE_REFERENCE=OFF` disables those comparisons;
Jolt itself does not depend on ODE. Vendor ODE headers are system includes so
old declarations in those headers do not relax strict diagnostics for engine C.

## Enable for a mod

Before loading its map:

```text
plug_load jolt
physics_jolt_enabled 1
map mymap
```

Only one rigid-body plugin can register at a time. If ODE is already loaded,
unload it first with `plug_close ode`. Change `physics_jolt_enabled` before a
map load; it is a non-archived opt-in. Setting it to zero does not tear down an
already active world immediately. QuakeC can explicitly shut down/restart the
world with `physics_supported(0)` / `physics_supported(1)`. Restarting rebuilds
collision data and can hitch. Unloading the plugin also releases its worlds.

With the generated engine declarations from
[Writing Your progs.dat](writing_progs.md), an original spawn function could be:

```qc
void() physics_crate =
{
    if (!physics_supported())
    {
        remove(self); // The mod chooses its own fallback policy.
        return;
    }
    precache_model("progs/crate.md3");
    setmodel(self, "progs/crate.md3");
    setsize(self, '-16 -16 -16', '16 16 16');
    self.movetype = MOVETYPE_PHYSICS;
    self.solid = SOLID_BBOX;
    self.geomtype = GEOMTYPE_BOX;
    self.mass = 10;
    self.friction = 0.8;
    self.bouncefactor = 0.1;
    setorigin(self, self.origin);
};
```

Use your own model and bounds; visual models do not have to be skeletal. When
using `SOLID_BBOX`, `SOLID_SLIDEBOX` or `SOLID_CORPSE`, the proxy starts aligned
with the world axes, matching the existing backend convention. A dynamic body
can subsequently rotate. `geomtype` selects the primitive collision shape;
`GEOMTYPE_TRIMESH` is supported only for static entities in this milestone.

`physics_addforce(entity, force, point)` takes a **world-space point**, despite
the old declaration naming it `relative_ofs`. Force uses mass × Quake
units/second² and torque uses mass × Quake units²/second². A queued force lasts
across the fixed substeps of the next advancing engine frame. It is not an
instantaneous velocity impulse. `physics_enable(entity, 0)` puts a dynamic body
to sleep; contact or subsequent activation can wake it. It is not permanent
removal from the collision world.

Quake coordinates stay Z-up. The adapter converts at 32 Quake units per metre;
this conversion is internal and does not rescale assets. The solver runs at
120 Hz, with at most eight fixed substeps per engine frame. Excess catch-up time
is dropped and reported once through developer logging. Contact/command queues
are bounded; the current adapter API exposes contact-overflow statistics.

`.gravity = 0` retains the engine's default-gravity convention; nonzero values
scale world gravity. Nonpositive mass defaults to 1 in the engine bridge.
Positive `.friction` is used directly; zero retains the existing default
convention of 1. `.bouncefactor` is clamped to 0–1. These values are opt-in mod
properties, not global changes to the base game's physics.

## Repeatable tests

`jolt-check-deps` builds the adapter tests, C17 boundary executable, plugin,
dedicated server, compiler, and optional ODE plugin needed by CTest.
The `jolt-` tests are:

| Test | Coverage |
| --- | --- |
| `jolt-c-boundary` | A real C17 translation unit creates/steps/destroys the C++ backend and creates/toggles/removes a joint through its C ABI. |
| `jolt-backend` | All primitive shapes, static mesh/CCD, unit conversions, gravity, forces/torque/impulses, partial steps, sleeping/waking, owner/dimension filters, cached-contact changes, stacking, restitution/friction, invalid input, stale/cross-world handles, capacity limits, repeated worlds and fixed-step behavior at 60/144 frame rates. World/body constraints, angular/linear limits and motors, joint frames, connected collision exclusion, dependent-joint removal and repeated multi-body teardown. Suspension load/deflection, damping energy loss, arbitrary-axis forces, live spring edits preserving rest anchors, invalid edits, disabled joints and rigid-pivot restoration. Optional ODE free-fall comparison. |
| `jolt-humanoid` | Original 24-bone mesh/animation and 16-body rig, actual animation changes, activation without position/orientation jumps, all-bone relative render copying, floor/stairs/slope/wall/overlap, repeated spawn/removal/animation transitions and live backend restart. No owned assets required. |
| `jolt-engine` | Real compiled SSQC and an original synthetic BSP: absent/disabled/enabled plugin, map collision, QuakeC forces/touches, removal before/after body creation, sleeping, shape rebuilds, backend restart, plugin reload and three map loads per mode. An original MD5/.doll rig exercises bone simulation, relative render-pose copying, named joint toggling, rejected-body/joint rollback and live-rig restart/reload. QuakeC suspension updates and translated/rotated world-anchored rigs are checked before and after backend restart. Optional ODE collision/fixed-joint baseline. Ordinary toss position is compared across modes. |

These tests use no commercial game assets and contact no game servers. Each
engine test invocation creates a new child of `build/jolt/physics-tests/` and
keeps its logs and `results.json`. Engine startup may report absent optional
configs/native game libraries because the fixture deliberately has no game
installation. The script requires explicit success markers and rejects physics
failures and sanitizer diagnostics. Fixture deadlines accumulate actual physics
`frametime`, and map/plugin cycling waits for completed checks. Engine startup
and server catch-up timewarps therefore cannot advance a test's physics clock.
The ODE collision baseline has a separate bounded settling deadline because
its default damping differs from Jolt's; collision bounds remain identical.
Each server process has a separate 60-second wall-clock watchdog; the complete
CTest case allows 300 seconds for all modes on slower/shared runners.

For ASan/UBSan, configure another single-configuration Debug build with
`-DFTE_SANITIZERS=ON`, then build `jolt-check-deps` and run the same tests.
Jolt, the adapter and plugin are instrumented as well as the server. Existing
engine startup tests disable leak detection for process-lifetime engine caches;
invalid access and undefined-behavior checks still fail the run. This is not a
complete engine shutdown leak audit or a third-party ODE library audit.

Local validation on Apple Silicon/macOS passes native Debug, Release and
ASan/UBSan adapter, ABI and engine checks against the supplied Jolt checkout and Homebrew
ODE. The complete native Debug build passes 6/6 CTest checks; the instrumented
build passes 9/9, including 37 standalone source-based sanitizer fixtures. The native
Metal demo also passed client connection, joint release and map-reload checks;
its captured poses were visually inspected. Skeleton allocation without CSQC,
world isolation, cache reuse and native/VM ownership have dedicated sanitizer
coverage in `parser-sanitizers`.

Desktop CI runs Release checks on Windows, Linux (GCC/Clang) and macOS,
plus Debug sanitizer checks on Linux/macOS. Jolt inherits the engine's selected
MSVC runtime. Jolt and its adapter both enable C++ RTTI so UBSan's virtual-call
checks can resolve their type information. The integration fixture selects plain
console output for redirected Windows logs. Clang-instrumented plugins resolve sanitizer symbols through the
instrumented host. Sanitizer-enabled Jolt hosts use the C++ linker driver so
Clang also supplies its C++ UBSan runtime (vptr cache/handlers); engine sources
still compile as C17. Ordinary plugin builds retain strict symbol resolution.
Further work includes additional real-mod rigs, CSQC validation, mixed animated/kinematic behavior, save/load and performance
checks.

## QuakeC joints and skeletal rigs

A joint is a separate non-`MOVETYPE_PHYSICS` entity. `.enemy` and `.aiment` select
its bodies; a world reference anchors that side to the world. Resolve bodies
before joints each frame. The supported `.jointtype` values are `-1` (fixed),
`1` (point), `2` (hinge), `3` (slider), `4` (universal) and `5` (hinge2). The joint's `.origin` is its world
pivot and its `.angles` define the hinge/slider's forward axis. Fixed joints
preserve the bodies' relative pose. Removing a body removes its dependent
constraints; a new body after a shape rebuild receives newly resolved joints.

For hinge/slider/universal/hinge2 entity motors, `.movedir = 'velocity -maxforce stop'` selects
a velocity motor with symmetric limits when `stop > 0`. With positive stiffness
and damping entries, `.movedir` selects soft symmetric limits. Hinge values use
radians; slider distances use Quake units. `.doll` rigs use `LoStop`/`HiStop`,
`FMax` and `Vel`. `Vel2`, `FMax2`, `LoStop2`, `HiStop2`, `ERP2` and `CFM2` configure
the second angular axis of universal/hinge2 rigs. ODE ERP/CFM limit softness is converted using
`k = ERP / (h * CFM)` and `c = (1 - ERP) / CFM`, with `h = 1/120`. This does not
make the solvers identical. Point joints currently use one shared world pivot;
ODE's alternate second-anchor convention and joint groups are not implemented.

For a passive `.doll`, set `animate 0` in the body defaults. Body dimensions
retain the old format's conventions: boxes use full extents; spheres use the
mean dimension as radius; Z-aligned cylinders/capsules use the mean X/Y dimension
as radius and Z as cylinder length. Hinge/slider limits must include zero;
hinge limits lie within `[-pi, pi]`. Scaled/non-rigid bone transforms are rejected.
`offset x y z [pitch yaw roll]` sets a body's transform relative to its bone.
`orient peer_bone` aims local Z toward another bone in the reference pose;
offsets are then applied in that oriented frame. The same frozen transform and
its inverse are used during simulation and bone reconstruction. `pivot bone
[x y z]` chooses a joint's pivot bone and optional offset in that bone's local
coordinates. Joint `axis` and `axis2` remain reference **model-space** directions.
Invalid rig directives/names/types reject the whole definition. Failed `doll` or
`dollstring` loads return zero and keep an existing rig; `cleardoll` is explicit.

Universal/hinge2 `axis` is attached to body1 and `axis2` to body2; specify
perpendicular, normalized directions. The adapter keeps them perpendicular while
allowing independent rotations, motors and hard/soft limits. Internally Jolt's
six-degree-of-freedom frame locks the forbidden swing; a small constraint wrapper
adds motors and soft stops along the actual moving joint axes. This avoids using
body2's fixed motor axes for both rotations. Angular ranges include zero and
stay in `[-pi, pi]`; avoid a reference swing at exactly 180 degrees.

Entity universal joints use the forward/up directions from `.angles`. For
hinge2 entities the second direction comes from normalized `.velocity`, retaining
the existing entity-interface convention. Entity motors/limits apply the same
settings to both axes; `.doll` rigs expose independent settings.

### Hinge2 suspension

Hinge2 defaults to a rigid pivot. FTEQW Delta adds two **Jolt-only** entity fields:

```qc
.float joint_suspension_stiffness;
.float joint_suspension_damping;
// After creating a JOINTTYPE_HINGE2 joint and assigning its bodies/axes:
joint.joint_suspension_stiffness = 2000;
joint.joint_suspension_damping = 150;
```

Use the generated declarations instead of redeclaring these fields in a mod
that already includes them. Linear suspension acts along body1's first axis;
its rest position is the joint's original pivot. Stiffness uses mass/second²
(force per Quake unit); damping uses mass/second (force per Quake unit/second).
For example, mass 2 under gravity 800 with stiffness 2000 settles 0.8 Quake units
below its rest position. Higher stiffness reduces sag; damping reduces oscillation.
Angular stop softness remains separate from these linear spring controls.

Both values must be finite and nonnegative. Nonzero damping requires positive
stiffness. Setting both to zero restores the original rigid pivot. Editing just
these fields updates the existing constraint, preserves its rest anchors and
wakes its bodies; it does not re-enable a disabled joint. Invalid live edits log
an error and retain the last valid spring. Other joint/frame/body edits still
recreate the constraint through the existing entity interface.

The equivalent joint block in a `.doll` file is:

```text
joint suspension "" top
 type hinge2
 axis 0 0 1
 axis2 1 0 0
 SuspensionStiffness 2000
 SuspensionDamping 150
```

Here `top` must name an existing body; an empty first body anchors that side to
the world. Axes are reference model-space directions. The engine transforms the
reference bodies, pivot and axes through the entity pose before creating the
constraint, so translated/rotated world anchors stay in the correct place.
Nonzero suspension on another joint type, or invalid spring values, rejects the
whole rig and rolls back its temporary bodies/joints. ODE and Bullet reject rigs
using nonzero suspension directives; the new entity fields are Jolt-only. This
milestone does not claim ODE-identical suspension or joint-group compatibility.

`skel_ragupdate` derives an absolute simulated skeleton. To render a server-side
rig using existing FTE bone networking, retain that skeleton on the simulation
entity and create a second, relative skeleton for a visible entity:

```qc
// Setup after assigning your skeletal model to both entities:
sim.skeletonindex = skel_create(sim.modelindex);
skel_build(sim.skeletonindex, sim, sim.modelindex, 0, 0, 0);
skel_ragupdate(sim, "doll progs/myrig.doll", 0); // Check its return value.
visible.skeletonindex = skel_create(visible.modelindex);
visible.basebone = -1;
// Each update, with matching entity origins and angles:
skel_ragupdate(sim, "", 0);
skel_copybones(visible.skeletonindex, sim.skeletonindex, 0, 0);
```

Hide the simulation entity's visual model after setup if it would draw twice.
For a partly physical skeleton, retain a relative animation-source skeleton and
pass it to each `skel_ragupdate` call. Freeze that source on activation so bones
without rigid bodies retain their local animation pose while following their
simulated parent. Passing zero uses their bind-pose relations instead.
The output skeleton uses the existing relative-pose wire format; no protocol or
player-prediction rules are changed. The demo and engine regression fixture show
the complete original setup in `tools/tests/physics_fixture.qc`.

## Interactive local demo

The demo uses an original map, mesh, texture and three-bone rig. It links only
your owned Quake PAKs for the interface assets; it creates a separate directory
and does not edit the installation or its configs. Build `fteqw`, `fteqcc` and
`plug_jolt`, then from the repository root run:

```sh
python3 tools/physics_demo.py --quake-data "/path/to/Quake" \
  --out build/jolt-demo --run
```

Fly with WASD/mouse. F5 restarts the drop; F6 releases/re-enables the top joint.
Re-enabling a separated joint can pull its bodies back together. The terminal
prints the exact launch command and isolated directory. Add `--capture` to run
a short native renderer check with connected/settled/released/reloaded screenshots
and automatic exit. Capture timers start after the client spawns. The tool checks
client reconnection, backend recreation, screenshot files and sanitizer errors,
and writes `results.json`; inspecting the images remains a visual check. Use `--engine`, `--qcc` and `--plugin` to select other build
outputs, including Windows configuration subdirectories. Symlink creation must
be permitted on the host.


## Humanoid demo and owned remaster model

The humanoid tool uses a fresh isolated output directory. With no game-data
argument it creates an original mesh, eight animation poses, texture, rig, map
and gamecode, suitable for automated CI:

```sh
python3 tools/physics_humanoid.py --out build/humanoid-check --test
```

To try the remaster soldier locally, give the tool your owned installation.
The tool reads the skeleton to fit an original 16-body rig and links the PAKs;
it does not modify or redistribute those assets:

```sh
python3 tools/physics_humanoid.py --out build/humanoid-remaster \
  --remaster-data "/path/to/Quake Enhanced" --run
```

F6 toggles authored animation/passive ragdoll, F7 cycles floor/stairs/slope/wall/
overlap scenes, F5 resets to the floor scene and F8 reloads the map. WASD/mouse
flies the camera. Each transition back to animation deliberately restores the
authored pose; it is not a blended get-up animation or a physical animation
motor. Add `--capture` to record the transition and exit, or use `--test` instead
of `--run` to run the terrain/lifecycle assertions with the owned model.
An original mannequin can be rendered with `--quake-data "/path/to/Quake"`.
The test setup is a rigging example, not an automatic monster-death feature.

Use `--engine`, `--server`, `--qcc` and `--plugin` to choose other build outputs,
including sanitizer builds or Windows configuration subdirectories. In addition
to headless tests, a separate local GPU/network check is available:

```sh
python3 tools/physics_humanoid.py --out build/humanoid-network \
  --remaster-data "/path/to/Quake Enhanced" --network-test
```

It starts a private loopback dedicated server and two actual native clients.
Only the server loads Jolt. The check requires both clients to receive a
24-bone network skeleton, capture animated/falling/settled/restored poses and
finish without engine/GPU/sanitizer errors. Screenshots and logs are retained;
visual inspection complements the separate numeric pose encode/decode and
interpolation sanitizer fixture. This is not a wide-area loss/latency or
many-ragdoll bandwidth benchmark. Apple Silicon/Metal has been exercised with
the owned soldier; other desktop CI checks use the original headless assets.

## Bullet review and eventual backend removal

The existing Bullet implementation was reviewed for two-axis joint behavior,
shape conventions, collision exclusions, continuous collision detection and
kinematic-body costs. Useful compatibility details include attaching each
universal axis to a different body and excluding a ragdoll's regular entity
proxy. Jolt now exercises two-axis limits/motors explicitly; its collision-family
filter already handles the proxy exclusion. Jolt also already uses continuous
collision detection for dynamic primitives and avoids updating all ordinary
entities as kinematic bodies. The Bullet code has incomplete/commented motor,
softness and joint paths, so those fragments are not used as a correctness oracle.

Bullet and ODE will be removed, including their build options, plugins, source,
console registrations and documentation references, once Jolt passes the
replacement gates. Remaining gates include alternate joint-interface conventions, dynamic mesh/convex policy, moving-platform and
mixed animation interaction, independent CSQC physics, active-scene save/load,
and many-body CPU/bandwidth checks. ODE remains a useful comparison during that
work. No classic player-movement or Quake 3 qagame-physics conversion is implied.
