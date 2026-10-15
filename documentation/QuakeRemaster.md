# Quake Remaster content

FTEQW Delta can load the owned Quake rerelease data through its native SDL3 GPU
renderer. The existing MD5 loader, QuakeC compatibility builtins, indexed model
skins, localization catalogs, and skeletal rendering provide the foundation.
This milestone validates that content and improves portable-install detection.

## Launch

Use the directory containing `id1/pak0.pak` as the basedir:

```sh
./build/fteqw -basedir "/path/to/Quake Enhanced" -quake_rerel
```

The remaster profile uses UTF-8 text, its existing 60 Hz server defaults, and the
rerelease gamecode compatibility paths. It does not require the original Kex
executable. `QuakeEX.kpf` remains an identifying resource when it is present;
portable exports without it are now recognized from the combined presence of
`mapdb.json`, `localization/loc_english.txt`, and `progs/player.md5mesh` in
`id1/pak0.pak`. Detection reads a bounded, validated PAK directory, not the full
archive. Directory names and MD5 replacements alone do not identify a remaster.
An explicit `-quake` still selects the classic profile even for such an archive.

Choose an installed single-player expansion with `-game`:

```sh
./build/fteqw -basedir "/path/to/Quake Enhanced" -quake_rerel -game hipnotic
./build/fteqw -basedir "/path/to/Quake Enhanced" -quake_rerel -game rogue
./build/fteqw -basedir "/path/to/Quake Enhanced" -quake_rerel -game dopa
./build/fteqw -basedir "/path/to/Quake Enhanced" -quake_rerel -game mg1
```

These are Scourge of Armagon, Dissolution of Eternity, Dimension of the Past,
and Dimension of the Machine. Keep `id1` beside the expansion directories so
shared models, textures, sounds and localization remain available. On Windows,
use the executable produced by the selected build configuration, typically
`build/Release/fteqw.exe`.

## Enhanced and classic models

The normal `r_replacemodels` list already includes `md5mesh`. The engine can use
an enhanced model while the gamecode continues requesting its `.mdl` filename.
A matching `.md5anim` supplies the animation poses used by the gamecode's frame
numbers; `mod_md5_singleanimation` defaults to `1` for this behavior.

For controlled comparisons, set the preference before loading a map:

```text
r_replacemodels "md5mesh"
map e1m1
```

To use the embedded original models, clear the list before loading/reloading:

```text
r_replacemodels ""
map e1m1
```

The preference is archived. Loading another map or restarting the engine ensures
already cached models are replaced. MDL/MD3 support is retained; explicit MD5
model loads still work when automatic replacements are disabled. The remaster's
indexed `.lmp` skins and any original models remain in the supplied PAKs.

MD5 provides animation bones and weights. Ragdolls additionally need physics
bodies, masses and constraint limits. FTE's `.doll` format describes that rig;
the supplied base archive contains no such definitions. Jolt integration and
optional ragdolls are subsequent milestones; this change does not alter classic
player movement, prediction, collision rules or ordinary entity physics code.

## Repeatable owned-data checks

Original assets are only read. The test tool creates isolated configurations,
compiled client fixtures, screenshots, saves, logs and result JSON files under
`--out`. It links the original PAK/music files, or copies them where symlinks are
unavailable. Choose an empty output directory outside the owned installation.
Python 3.9+ and Pillow are required, along with the native engine and `fteqcc`.

```sh
cmake -S . -B build/all -DCMAKE_BUILD_TYPE=Release \
  -DFTE_ENGINE=ON -DFTE_ENGINE_SERVER_ONLY=ON \
  -DFTE_TOOL_QCC=ON -DFTE_TOOL_QCCGUI=OFF
cmake --build build/all --target fteqw fteqw-sv fteqcc --config Release --parallel
python3 tools/gpu_remaster.py prepare "/path/to/Quake Enhanced" \
  --qcc build/all/fteqcc --out build/remaster-tests
python3 tools/gpu_remaster.py run --out build/remaster-tests \
  --engine build/all/fteqw --server build/all/fteqw-sv
```

On Windows multi-configuration builds, pass `--qcc build/all/Release/fteqcc.exe`
to `prepare`, and `--engine build/all/Release/fteqw.exe` plus
`--server build/all/Release/fteqw-sv.exe` to `run`.

`--profile models` runs model comparisons; `--profile campaigns` runs the
single-player campaign checks. The default is `all`. `check` re-evaluates saved
outputs without starting the engine. The model capture sequence starts after
client initialization and uses a fixed camera, so connection time and physical
mouse input cannot change the comparison pose.

On the tested macOS/Metal installation, release checks passed for:

- All **59 MD5 meshes**, comparing their parsed bone counts with the source.
- All **58 MD5 animation files**, checking the final pose index and rejection
  of a nonexistent following pose.
- Textured player, grunt and dog renders; automatic replacements matched
  direct MD5 rendering, while disabling replacements selected original MDLs.
- Animation-pose and lighting changes, plus identical fixed-scene captures
  after video restart.
- Base-game and four expansion gamecode loads, start/first-map transitions,
  legacy save/load, restored client views, and dedicated-server map startup.

The enhanced/classic model comparisons also passed in the full ASan/UBSan
client with undefined behavior fatal, including all model/animation inspections,
lighting changes and video restart. To reproduce that run after preparing a
separate output directory:

```sh
python3 tools/gpu_remaster.py prepare "/path/to/Quake Enhanced" \
  --qcc build/all/fteqcc --out build/remaster-sanitized-tests
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  python3 tools/gpu_remaster.py run --out build/remaster-sanitized-tests \
  --profile models --engine build/safety/fteqw
```

Use the sanitizer build described in [SafetyChecks.md](SafetyChecks.md). This
instrumented result covers the fixed model scenes; campaign save/load checks
were run in release builds. Leak checking remains disabled for engine lifetime
caches, while invalid-access and undefined-behavior checks remain active.

The tests inspect all skeletal files, but the visual comparisons cover the
three named character models and the ordinary campaign views. They are smoke
and regression checks, not full playthroughs of every map or every animation.
Native Linux/Windows rendering of these owned assets remains untested locally.

CI has no proprietary assets. Its sanitizer fixtures check missing/incomplete
marker sets, every short directory, bad offsets/lengths and file cleanup. The
headless startup check mounts a tiny synthetic identifying archive to verify
automatic remaster mode and an explicit classic override.

Instrumented startup and rendering exposed legacy signed shifts in archive
checksums, ZIP/GZIP/SHA-1 decoding, palette/normal-map alpha, negative PCM audio
samples, texture origins, packet headers, native byte swaps and item-stat masks.
The fixes retain intended bit patterns, sample scaling and lightmap grid bounds.
Connection cookies use unsigned arithmetic and copy clock bytes without aliasing.
SHA-1 buffers have proper alignment, and long HMAC keys use the reduced digest
length after hashing. Remote pickup sounds now respect their non-local seat
instead of indexing a player view with `-1`.

Grouped allocations retain SIMD alignment in release and debug builds. Packed
brush arrays and original MDL single/group animation poses receive explicit
alignment padding. The worker QuakeC garbage collector aligns its native pointer
table separately from the copied VM word memory. These changes preserve model
positions, normals, animation indexing and copied VM/string contents.

Data-free regression fixtures check reference hashes, palette/normal-map alpha,
full-range signed PCM conversion, 300 resampling cases, 5,409 texture bounds,
all 65,536 signed entity-size heights, packet fields and allocation layouts.
They also verify item pickup flash times and local/spectator/remote sound
attribution. See [SafetyChecks.md](SafetyChecks.md) for coverage and limits.

## Remaining compatibility limits

The supplied gamecode imports optional helpers that FTE does not implement:
`ex_bot_movetopoint`, `ex_bot_followentity`, and the `ex_draw_*` developer drawing
functions. Their import warnings remain visible. The exercised single-player
paths do not call them; the checks permit only that explicit known set and fail
on other unknown builtins or execution errors. Bot behavior and developer debug
visualization are not claimed as supported by this milestone.

The original Kex menu, account/crossplay services, add-on downloader and id Vault
are separate from running the supplied campaign content in FTE's own interface.
CTF/bot matches, absent add-on campaigns, and original-engine multiplayer
interoperability were not validated here.
