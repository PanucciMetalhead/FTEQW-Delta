[![Jolt Physics in Quake](https://img.youtube.com/vi/aUfIBcb6XJM/maxresdefault.jpg)](https://www.youtube.com/watch?v=aUfIBcb6XJM)

# Rockfall tutorial

Rockfall is a single-player Jolt test level. Its console map name is
`jolt_range`. You'll climb a 30° ramp, fight grunts, and dodge or shatter
boulders on the way to the summit teleporter. The level uses [Arcane
Dimensions 1.80 Patch 1](https://www.simonoc.com/pages/design/sp/ad.html) by
Simon O'Callaghan and the AD team for its projectile shotguns and grunt AI.

## 1. Build the engine and stage the range

The Jolt integration is on `master`. Desktop CI builds it, and a local build
enables it with `-DFTE_PLUG_JOLT=ON`. Follow the [Jolt build
instructions](../../documentation/JoltPhysics.md#build) for the pinned
dependency. Build `fteqw`, `plug_jolt`, and `fteqcc` in the same build tree.
For the server smoke test, enable `-DFTE_ENGINE_SERVER_ONLY=ON` and build
`fteqw-sv` too. Keep the matching plugin beside each engine executable.

The range builder needs Python 3.9 or later, the ericw-tools `qbsp` and
`light` binaries, full Quake 1 data (`id1/pak0.pak` and `id1/pak1.pak`), and
AD 1.80 Patch 1 (`pak0.pak`, `pak1.pak`, and `pak2.pak`). The `vis` step
is optional. Run the builder from the repository root with an empty output
directory. It compiles the overlay and symlinks the original game archives
into the staging directory.

```sh
python3 games/jolt-range/build_range.py \
  --quake /path/to/Quake \
  --ad /path/to/ad_v1_80p1final \
  --qcc /path/to/fteqcc \
  --qbsp /path/to/qbsp --vis /path/to/vis --light /path/to/light \
  --engine /path/to/jolt-build/fteqw \
  --out build/jolt-range
```

On macOS or Linux, run the generated `build/jolt-range/Play Jolt Range.command`
from a shell, or run the engine from the directory containing its Jolt plugin.

```sh
./fteqw -nohome -quake -basedir /absolute/path/to/build/jolt-range \
  -game ad -game jolt_range +set plug_loaddefault 0 +exec jolt_range.cfg
```

`-game ad -game jolt_range` loads the overlay over AD.
[`jolt_range.cfg`](jolt_range.cfg) loads the plugin, sets
`physics_jolt_enabled 1`, selects `jolt_range.dat`, and then runs
`map jolt_range`. Jolt is opt-in at runtime as well as at build time.
`range_init` checks physics support on this map before the overlay creates
any bodies.

## 2. Play the range

1. Fire the shotgun at the grunt on the starting apron. A surviving grunt
   slides a short distance along the pellet's incoming direction, keeps its
   facing through the hit, and resumes walking. A fatal shotgun hit can leave
   a ragdoll.
2. Try a grenade or rocket near a grunt. A survivor can be thrown down and get
   up again. A lethal hit can leave a ragdoll or gib the grunt, depending on
   the damage.
3. Cross a yellow stripe to release a boulder from farther uphill. Watch the
   seam on the rock as it turns, picks up speed, and rebounds off the ramp.
   Strafe across its path or hit it with explosives to break it into
   simulated fragments.
4. Enter the teal teleporter at the summit to reach the completion screen.
   Press F8 to restart.

## 3. Trace the handoff from QuakeC to Jolt

Start in [`range.qc`](range.qc). The range hooks run in server-side QuakeC
(SSQC) on `jolt_range`. The game code decides what gets hurt, when to create
a rigid body, and when to return control to the grunt's AI. Jolt advances the
bodies and resolves gravity, contacts, friction, and joint constraints.
[`patch_source`](build_range.py) installs the hooks into a local copy of
AD's projectile, damage, soldier, and world code.

### Pellet hits

AD's projectile touch handler saves the pellet's incoming velocity before
removing the projectile. AD applies damage, then `range_soldier_hit` uses
the captured direction and speed to accumulate a push. For a surviving
grunt, the hook saves its facing, switches its collision box from
`MOVETYPE_STEP` to `MOVETYPE_PHYSICS`, and pauses its AI think. On each
`StartFrame`, `range_frame` applies part of the push with
`physics_addforce` at the box's world-space center. Applying force at the
center keeps torque low. The solver computes displacement against the ramp
and other solid surfaces. After roughly 0.28 seconds, `range_end_push`
restores ordinary movement and the saved facing. Pellet speed scales the
physical push, and AD supplies the projectile and damage rules.

### Ragdoll hits

For rocket and grenade hits, `range_soldier_hit` uses the direction away from
the blast to launch the grunt. AD's death hook calls `range_ragdoll_death`
for fatal pellet or explosive hits that leave an intact body. AD handles its
usual gibs. `range_ragdoll` also handles a living grunt knocked down by a
blast.

The rig uses nine fitted box bodies from `range_limb`, connected by
`range_joint`. The generated `source/qc/range_grunt.qh` pairs the bodies
with the bones in `range_grunt.md5mesh`. The builder generates this
ragdoll mesh from AD's grunt model. Live grunts use AD's original model.
Collision dimensions let the boxes contact the ramp and boulders while the
joints govern movement between limbs. `EndFrame` copies the simulated pose
into the visible skeleton. After about two seconds, a surviving grunt can
stand up on a clear spot and resume its AI.

### Rolling boulders

Each stripe targets an uphill `range_boulder_spawner`. After a short
warning, `range_release` creates a `MOVETYPE_PHYSICS` prop at rest.
The prop uses `GEOMTYPE_COMPOUND` with an authored shape file,
`progs/range_boulder.jolt`. That file contains a single irregular convex
hull made from the same points as `range_boulder.md5mesh`. The visible
mesh's root bone takes its orientation from the body in `EndFrame`.
Gravity accelerates the rock downhill, and the facets and restitution
produce small hops. The contact callback uses impact speed to determine
player damage. When explosive damage breaks the boulder, `range_rock_break`
replaces it with five convex fragments shaped from `range_chip.md5mesh`.

The map uses a continuous sloped BSP brush. Jolt uses the world geometry for
body contacts, while the player uses the engine's normal movement. The
incline camera fix in [`cl_pred.c`](../../engine/client/cl_pred.c) follows
the ground plane so walking uphill feels smooth.

## 4. Verify and experiment

Add `--smoke-server /path/to/jolt-build/fteqw-sv` to the build command and
use a fresh output directory. The dedicated server uses AD pellets, rockets,
and grenades to check grunt traversal, knockback, facing, ragdoll recovery,
corpse clearance, and gib handling. It also checks boulder roll direction,
acceleration, bounce, contact damage, and shattering with either launcher.
Look for `RANGE_SMOKE_OK` in `smoke.log` in the staging directory.

To see how restitution affects the rock, lower `rock.bouncefactor` in
`range_release`, rebuild into a fresh directory, and watch the first
boulder's contacts from the same spot. The smoke test's bounce check is
tuned to the original settings. The generated `source/jolt_range.map` and
`source/range.wad` let you inspect the ramp, trigger brushes, and spawn
positions in a map editor. The builder's `make_map` function defines the
layout for subsequent builds.

The overlay QuakeC uses GPL v2 or later, like the AD source it extends.
The staging directory is a personal test install built from local Quake
and AD assets. The original installations stay intact.
