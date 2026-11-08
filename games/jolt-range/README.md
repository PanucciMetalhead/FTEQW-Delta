# Jolt Range: Rockfall

Jolt Range is a single-player physics test for FTEQW-Delta, built on [Arcane
Dimensions 1.80 Patch 1](https://www.simonoc.com/pages/design/sp/ad.html) by
Simon O'Callaghan and the AD team. The map starts you with the two shotguns,
grenade launcher, rocket launcher, ammo, and armor. Try a shotgun on the grunt
on the starting apron, then head uphill. All eight grunts can pursue you.
Each yellow stripe marks a rockfall trigger on the continuous 30° ramp.
Strafe across the boulder's path or shatter it with an explosive.
The teal teleporter at the summit opens the completion screen. F8 restarts the
range.

Build the engine with `-DFTE_PLUG_JOLT=ON` and keep its matching plugin next to
the engine. See the [Jolt setup guide](../../documentation/JoltPhysics.md) for the
pinned dependency and build options. You also need Python 3.9 or later, FTEQCC,
ericw-tools, full Quake 1 data, and the original AD 1.80 Patch 1 files
(`pak0.pak`, `pak1.pak`, `pak2.pak`).
The builder makes a separate game directory with symlinks to the original
archives and generates the map, models, and QuakeC locally.

```sh
python3 games/jolt-range/build_range.py \
  --quake /path/to/Quake \
  --ad /path/to/ad_v1_80p1final \
  --qcc /path/to/fteqcc \
  --qbsp /path/to/qbsp --vis /path/to/vis --light /path/to/light \
  --engine /path/to/jolt-build/fteqw \
  --out build/jolt-range
```

Run `build/jolt-range/Play Jolt Range.command`, or run the engine from the
directory containing its Jolt plugin:

```sh
./fteqw -nohome -quake -basedir /absolute/path/to/build/jolt-range \
  -game ad -game jolt_range +set plug_loaddefault 0 +exec jolt_range.cfg
```

For the repeatable server check, add `--smoke-server /path/to/jolt-build/fteqw-sv`
when building into an empty directory. The check uses actual AD pellets, rockets
and grenades. It checks that a grunt can walk the full ramp and that a fatal
pellet leaves a ragdoll. It also checks grunt displacement and facing,
recovery from a nonlethal blast, corpse clearance, and the normal gib threshold.
It also checks downhill roll direction, acceleration and bounce, contact
damage, and shattering with either launcher. It writes `smoke.log` to the
staging folder.

The QuakeC hooks are active only on `jolt_range`. AD still handles projectile
flight, hits, damage, and its normal gib threshold. Pellet impacts apply a
short force along the pellet's incoming direction. The grunt keeps its facing
through the handoff to Jolt and then resumes ordinary AI movement. Fatal
pellets and explosive hits can both hand the grunt to a nine-body rig.
The collision boxes enclose
the skinned mesh and contact the ramp and boulders. The joints control the
limbs without limb-on-limb contacts. A surviving grunt gets up after about two
seconds, and a fatal non-gib hit leaves a corpse.

Boulders use an irregular convex hull built from the same vertices as the
visible mesh. The mesh follows the hull's rotation through a single bone.
The quartz seam stays fixed to the rock surface as it tumbles.
Gravity drives the roll, and the facets and contact restitution produce small
hops. Explosives break them into separate simulated chips. The editable map
and WAD are in `source/`.

The overlay QuakeC uses GPL v2 or later, like the AD source it extends. The
builder uses the local AD and Quake assets to make a personal test install; it
does not vendor the assets or modify either source installation.
