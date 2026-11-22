# Building FTEQW Delta

This fork builds native desktop clients, dedicated servers, and development
tools. Use CMake 3.21 or newer and a compiler with C17 support on Windows, Linux,
and macOS (Clang/GCC, or Visual Studio 2019 16.8 or newer). The old
platform-specific OpenGL make targets are removed. `engine/Makefile`, `build_setup.sh`, and `build_wip.sh` are
small wrappers around CMake.

Install [dependencies](Dependencies.md), clone the fork, then configure and build:

```sh
git clone https://github.com/PanucciMetalhead/FTEQW-Delta.git
cd FTEQW-Delta
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

For just the client, append `--target fteqw` to the build command.
Windows builds copy the shared SDL3 runtime DLL beside SDL-linked clients and
image tools, so these build outputs run without adding SDL's installation prefix
to `PATH`. Keep that DLL beside the executable when copying it elsewhere.
Multi-configuration Windows builds place executables in `build/Release`; single-configuration Ninja
and Makefile builds place them directly in `build`.

```sh
./build/fteqw -basedir /path/to/Quake -quake
```

The basedir must contain `id1/pak0.pak` (and `pak1.pak` for the registered game).
Use `-window -width 1280 -height 720` for a window. `-gpu_debug` enables SDL GPU
validation. `gpu_info` reports the active native backend and frame statistics.

Debug builds:

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --target fteqw --config Debug --parallel
```

CMake options include `FTE_ENGINE_SERVER_ONLY`, `FTE_ENGINE_CLIENT_ONLY`,
`FTE_TOOL_IMAGE`, `FTE_TOOL_IQM`, `FTE_TOOL_QCC`, and the existing optional
plugins. The engine and C tools use ISO C17 mode, with GNU language mode
disabled. Serious declaration, prototype, pointer-type, return-value, and
format-security diagnostics are errors in both Release and Debug on GCC/Clang.
MSVC checks undeclared functions, excessive arguments, and missing returns.
`FTE_WERROR` makes all remaining warnings errors and defaults to OFF because
the inherited code still has warnings. `FTE_EXTRA_WARNINGS` enables additional
conversion, format, and extension audits. See [C17.md](C17.md) for the policy,
regression commands, and merge criteria.
For the warning inventory, parser fixtures and data-free sanitized server tests,
see [SafetyChecks.md](SafetyChecks.md).
Bundled menu/client-addon packages require both `fteqcc` and Python 3; CMake
skips these packages when either is unavailable. They no longer require shell
`echo` or a command-line ZIP utility on Windows.
Use `-DSDL3_DIR=/path/to/lib/cmake/SDL3` or `-DCMAKE_PREFIX_PATH=/prefix` when SDL
is installed outside normal search paths. Do not point these at SDL2.

The renderer's generated shader header is included with the source. Regular
builds do not need a shader compiler. After editing shaders:

```sh
python3 tools/build_gpu_shaders.py
```

This requires `glslangValidator` and `spirv-val`. Vulkan shaders are compiled and
validated as SPIR-V; native Metal source is embedded alongside them. Both shader
versions must be kept in sync and tested on their respective native backends.

See [SDL3-GPU.md](SDL3-GPU.md) for integration testing and the validation record.
Legacy console, mobile, and Flatpak deployment files are upstream history;
they are not supported build paths for this desktop fork.

## Profile-guided dedicated server builds

Native Clang Release builds on Linux and macOS can apply IR profile-guided
optimization to `fteqw-sv`. The client, tools, and plugins still use their
regular compile settings. Use a single-configuration generator and keep the
same compiler, source, and build options across the three builds.

```sh
cmake -S . -B build/pgo-train -G Ninja -DCMAKE_BUILD_TYPE=Release -DFTE_SERVER_PGO=generate
cmake --build build/pgo-train --target fteqw-sv fteqcc --parallel
mkdir -p build/pgo-train/profiles
LLVM_PROFILE_FILE="$PWD/build/pgo-train/profiles/trace-%p.profraw" \
  python3 tools/server_benchmark.py --server build/pgo-train/fteqw-sv \
  --qcc build/pgo-train/fteqcc --frames 240 --traces 0 256 1024
llvm-profdata merge -o build/pgo-train/server.profdata build/pgo-train/profiles/*.profraw
cmake -S . -B build/pgo-use -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFTE_SERVER_PGO=use -DFTE_SERVER_PROFILE=build/pgo-train/server.profdata
cmake --build build/pgo-use --target fteqw-sv --parallel
```

Use the `llvm-profdata` shipped with your compiler (`xcrun llvm-profdata` on
macOS). Collect profiles in a fresh directory when the source or toolchain
changes. Train on the maps, mods, and traffic you expect to serve. The
[loopback QuakeWorld benchmark](Networking.md#live-quakeworld-gameplay-under-loss)
can contribute another raw profile by running it with the instrumented
`--server` and the same `LLVM_PROFILE_FILE` directory before merging. The
trace-only example above is sufficient to test the build flow.

To train on multiplayer, run the instrumented server through the client
scaling benchmark with `LLVM_PROFILE_FILE` set before merging. Compare the
result on a different map and client count:

```sh
LLVM_PROFILE_FILE="$PWD/build/pgo-train/profiles/qw-%p.profraw" \
  python3 tools/network_scaling.py /path/to/Quake --server build/pgo-train/fteqw-sv \
  --map start --clients 8 24 --frames 600
# Merge the profiles and build with FTE_SERVER_PGO=use as shown above.
python3 tools/network_scaling.py /path/to/Quake --baseline build/pgo-baseline/fteqw-sv \
  --server build/pgo-use/fteqw-sv --map dm6 --clients 12 32 --frames 360 --repeats 3
```

On macOS arm64 with AppleClang 21 and ThinLTO, the multiplayer-only profile
reduced median mean frame work from 0.190 to 0.171 ms at 12 clients and from
0.256 to 0.243 ms at 32. Median p99 fell from 0.671 to 0.527 ms at 12 clients
and rose from 0.928 to 1.005 ms at 32. Each median comes from three short
runs per build on one host.

Compare the optimized server against Release on a workload omitted from
training. The benchmark alternates run order and saves each sample and
the median of each timing statistic.

```sh
cmake -S . -B build/pgo-baseline -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/pgo-baseline --target fteqw-sv --parallel
python3 tools/server_benchmark.py --baseline build/pgo-baseline/fteqw-sv \
  --server build/pgo-use/fteqw-sv --qcc build/pgo-train/fteqcc \
  --frames 240 --traces 512 2048 --repeats 3
```

On macOS arm64 with AppleClang 21 and ThinLTO, a profile trained on the three
synthetic cases above plus a local QuakeWorld session produced these medians
over three runs of 240 frames per build:

| Traces per tick | Release mean work | PGO mean work | Release p99 | PGO p99 |
| ---: | ---: | ---: | ---: | ---: |
| 512 | 0.401 ms | 0.344 ms | 0.693 ms | 0.595 ms |
| 2,048 | 1.345 ms | 1.049 ms | 2.415 ms | 2.034 ms |

Isolated spikes occurred in both builds. These results measure the synthetic
server workload on one machine. PGO remains opt-in because performance can
depend on the maps, mods, and traffic used for training.

## Original Quake II gamecode

Point `-basedir` at the installation directory containing `baseq2/pak0.pak`:

```sh
./build/fteqw -quake2 -basedir "/path/to/Quake II"
```

Quake II's game logic is a separate native module. The retail PAK files provide
maps, models, textures, and sounds, but cannot replace that module. A Windows
installation's `baseq2/gamex86.dll` is a 32-bit Windows binary; macOS, Linux, and
64-bit Windows engines need gamecode built for their own OS and CPU architecture.
The menu can work even when the game module is missing.

If starting a new game reports `Failed to load progs.dat`,
`Failed to load qwprogs.dat`, followed by `No gamecode available`, the Quake II
module did not load and FTE fell back to looking for QuakeC gamecode. Supplying
Quake 1's `progs.dat` does not fix this. FTE's normal CMake build does not include
the Quake II game module.

### Build the base-game module on Apple Silicon macOS

The following commands run from this fork's repository root and require Apple's
command-line compiler tools, Git, and Python 3. They build
[id Software's original GPL gamecode](https://github.com/id-Software/Quake-2),
pinned to commit `372afde46e7defc9dd2d719a1732b8ace1fa096e`. Use a fresh
`build/quake2-source` directory for the clone.

```sh
git clone https://github.com/id-Software/Quake-2.git build/quake2-source
git -C build/quake2-source checkout --detach 372afde46e7defc9dd2d719a1732b8ace1fa096e

python3 - <<'PY'
from pathlib import Path

path = Path("build/quake2-source/game/g_items.c")
source = path.read_text()
for name in ("jacket_armor_index", "combat_armor_index", "body_armor_index"):
    old = f"static int\t{name};"
    if source.count(old) != 1:
        raise SystemExit(f"Unexpected source declaration: {name}")
    source = source.replace(old, f"int\t{name};")
path.write_text(source)
PY

clang -arch arm64 -dynamiclib -O2 -std=gnu89 -DC_ONLY \
  -fno-strict-aliasing -Wno-deprecated-non-prototype \
  -Wno-pointer-to-int-cast build/quake2-source/game/*.c \
  -o build/q2gamearm64_baseq2.so

./build/fteqw -quake2 -basedir "/path/to/Quake II"
```

The source adjustment removes `static` from three armor index definitions to
match their existing `extern` declarations in `g_local.h`; modern Clang rejects
the conflicting declarations. `C_ONLY` selects the portable C implementation.
The legacy gamecode is compiled separately from this fork's C17 engine policy
and still emits inherited warnings.

Keep `q2gamearm64_baseq2.so` beside the native arm64 `fteqw` executable, including
when copying the engine elsewhere. The `.so` suffix is intentional on macOS:
this is a Mach-O library using FTE's expected module filename. FTE also accepts
`libgame_baseq2.so` beside the executable. These explicit filenames load without
enabling `com_gamedirnativecode`. A generic `game.so` inside `baseq2` requires
`com_gamedirnativecode 1` and is unnecessary for the layout above.

This recipe covers the original `baseq2` campaign. Mission packs and mods need
their own compatible game modules; renaming the base-game module does not add
their game logic. The original game data remains player-supplied. If distributing
the compiled module, follow the source release's GPL terms, including its source
availability requirements; see `build/quake2-source/gnu.txt`.

Validation covered loading this module with the arm64 engine in dedicated mode,
starting `base1`, running server frames, and shutting down cleanly. It does not
establish full campaign, savegame, mission-pack, or mod compatibility.

### Quake II Enhanced (2023 rerelease) limitation

Local Quake II Enhanced gameplay is currently unsupported. The gamecode recipe
above builds the original Quake II rules and does not enable the Enhanced
edition's gameplay changes, updated AI, or establish compatibility with its
Quake II 64 and Call of the Machine campaigns.

The fork retains upstream support code for the Enhanced network protocol
(version 2023), including `connectq2e`. Treat this as partial protocol
compatibility: Enhanced networking has not been validated in this fork, and
these code paths do not provide full Enhanced-edition support.

The local server still uses the original version 3 game API. The Enhanced
edition introduces different game interfaces, a client-game module, and 40 Hz
game logic; see [id Software's rerelease gamecode documentation](https://github.com/id-Software/quake2-rerelease-dll#readme).
Recompiling or renaming its game DLL for the engine's OS and CPU is insufficient.
Full support requires additional engine integration and compatibility testing.
Use the original Quake II data and matching classic gamecode for the setup above.

## OpenArena and the Quake 3 plugin

OpenArena requires the Quake 3 compatibility plugin. Build both targets and
keep the generated plugin beside the engine executable:

```sh
cmake --build build --target fteqw plug_quake3 --config Release --parallel
./build/fteqw -basedir "/path/to/OpenArena" -quake3 -basegame baseoa -game baseoa +plug_load quake3
```

The basedir contains `baseoa/*.pk3`. On Windows the engine is in
`build/Release/fteqw.exe`; its plugin is `fteplug_quake3.dll`. On macOS/Linux the
plugin is `fteplug_quake3.so`. The default plugin policy does not automatically
load every loose plugin built by CMake. `+plug_load quake3` loads the required
module without enabling all the other optional plugins.

If a connection fails with “Q3 plugin not loaded”, enter `plug_load quake3` in
the console, then `reconnect`. `plug_load quake3` can also be placed in the
game's `autoexec.cfg` for subsequent starts.

`-basegame baseoa` keeps OpenArena’s assets available when a remote server
requests the unmodified base game. Joining from the OpenArena browser dismisses
the menu so FTE can show connection/loading progress. The default `cl_sendguid`
value supplies the per-server player ID OpenArena requires; an explicit `0`
keeps GUID transmission disabled, and an explicitly supplied `cl_guid` is
preserved. Generated IDs are added only to the connection packet.

Data-free regression checks cover the browser handoff, IPv4/IPv6 master
response offsets, GUID defaults/opt-out, disconnect cleanup, packet timeout
refresh, download completion/error handling, cgame lifecycle, bounded browser
metadata, long systeminfo values, base-game/mod selection, and pure-package
name/checksum lists:

```sh
python3 tools/q3_ui_tests.py
```

The Quake 3 profile defaults to protocol 68. Standard OpenArena servers use
[legacy protocol 71](https://github.com/OpenArena/engine/blob/master/README.md);
use `com_protocolversion 71` before connecting to those servers, or pass
`+com_protocolversion 71` at launch. For OpenArena master-server discovery,
also set `com_protocolname Quake3Arena` and `net_q3master1 dpmaster.deathmask.net:27950`.
The master registry uses `Quake3Arena` for OpenArena; querying `OpenArena`
returns a different, normally empty game list. The browser waits for a fresh
master response and honours the selected master slot. The Q3 UI
registers `cl_maxPing` with the standard 800 ms default; an absent cvar would
make OpenArena silently clamp its filter to 100 ms.
Pure servers keep installed packages visible when their checksums are allowed,
including unreferenced player assets. Mods that advertise `sv_pure` without a
package list retain the usual OpenArena base-asset fallback. Servers running a
separate mod still require that mod's PK3s; `cl_downloads 1` (also available as
`cl_allowDownload 1`) permits the server to supply them. Quake 3 disconnects send
a reliable close command so retrying after a loading failure does not leave a
stale player connection behind. Incoming Q3 packets refresh the core connection
timeout during gameplay and downloads. A rejection from the pending Q3 server
stops the connection attempt and displays its reason.

Servers using another engine may advertise a different protocol, so match the
server's version.


To check OpenArena rendering using your installed game data, build the targets
above and install Pillow for Python. This test creates a separate output folder;
the original PK3 files are only read. Choose a new output directory for each run.

```sh
python3 tools/gpu_openarena.py prepare "/path/to/OpenArena" --out build/openarenadata
python3 tools/gpu_openarena.py run --out build/openarenadata
```

Use `--engine build/Release/fteqw.exe` for a Windows multi-configuration build.
The local match checks camera turning, movement, the cgame weapon toggle and
video restart using captured frames. It uses OpenArena 0.8.8's `pxlfan` map with
a fixed starting position and noclip so hazards cannot invalidate the camera
comparison. `check` revalidates an existing run.
Screenshots, the log and `openarena-results.json` remain in the output folder.
This checks local gameplay rendering; it does not test external servers.


The `materials` profile requires OpenArena 0.8.8 data, including `pxlfan`. It
checks the fan's baked vertex lighting, mixed-case pickup shader names, health
pickup effects and video restart. Use it with each action and a new output path:

```sh
python3 tools/gpu_openarena.py prepare "/path/to/OpenArena" --profile materials --out build/oa-materials
python3 tools/gpu_openarena.py run --profile materials --out build/oa-materials
```

The test enables cheats only in its isolated local server to place the camera;
normal gameplay rules are unaffected. Results are saved to
`openarena-material-results.json`.

## Quake rerelease content

No extra model library is required for the rerelease MD5 assets. Point the
engine at the folder containing `id1/pak0.pak`, using `-quake_rerel` for an
explicit profile. Portable PAK exports can also be auto-detected. See
[QuakeRemaster.md](QuakeRemaster.md) for expansion launches, model preferences,
owned-data checks and compatibility limits.
Dawn of the Machine uses `-game mg3`:

```sh
./build/fteqw -basedir "/path/to/Quake Enhanced" -quake_rerel -game mg3
```

## Jolt Physics for rigid bodies and ragdolls

`FTE_PLUG_JOLT=ON` and `FTE_JOLT_SOURCE_DIR=/path/to/JoltPhysics` build the
optional `plug_jolt` module from an external source checkout. It supports
rigid props, joints, and experimental skeletal ragdolls. Desktop CI enables
it. Local builds and games opt in separately.

See the [Jolt Physics guide](JoltPhysics.md) for the pinned dependency, build
and test commands, and runtime setup. The [Rockfall tutorial](../games/jolt-range/README.md)
shows how QuakeC hands rolling boulders and articulated grunt ragdolls to Jolt.
