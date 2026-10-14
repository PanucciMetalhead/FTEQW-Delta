# FTEQW Delta

[FTEQW Delta](https://github.com/PanucciMetalhead/FTEQW-Delta) is a fork of [fte-team/fteqw](https://github.com/fte-team/fteqw), based on upstream commit
`f937b9d88` (revision 6729). The desktop client uses SDL3 for windowing,
input, controllers, audio, and GPU rendering. It explicitly selects **Vulkan
on Windows and Linux** and **Metal on macOS**. Supported targets are native
desktop applications for these three platforms.

The OpenGL renderer and its platform context implementations have been removed.
CMake builds one desktop GPU renderer, `sdlgpu`. The dedicated server and the
engine's game, network, map, model, particle, and QuakeC systems are retained.
Some shared rendering helpers keep their upstream `engine/gl` paths; these
helpers do not implement an OpenGL backend. Native HTTP/HTTPS downloads,
WebSocket connections, and ICE networking remain available; see
[Networking.md](documentation/Networking.md) for transport settings and tests.

The project combines the native renderer with C17 modernization, network
robustness fixes, compiler improvements, and menu usability improvements.
The executable remains `fteqw` and the compiler remains `fteqcc`; existing
launch scripts, plugins, configuration files, and protocol identifiers keep
their established names.

Build and run:

```sh
git clone https://github.com/PanucciMetalhead/FTEQW-Delta.git
cd FTEQW-Delta
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target fteqw --config Release --parallel
./build/fteqw -basedir /path/to/Quake -quake
```

Requires SDL **3.4 or newer**, CMake, and a C/C++ compiler. See
[building](documentation/Building.md), [dependencies](documentation/Dependencies.md),
and [renderer controls and validation](documentation/SDL3-GPU.md).
Warning audits and expanded sanitizer checks are documented in
[SafetyChecks.md](documentation/SafetyChecks.md).

The [console command and cvar reference](documentation/console_commands.md) covers
Quake 1 and Quake 3/OpenArena, including shared settings, game-specific behavior,
and client/server availability.

For game and total-conversion development, see
[Writing Your progs.dat](documentation/writing_progs.md) and the
[FTEQCC compiler manual](documentation/FTEQCC.md).

Graphics controls remain available through the in-game Options menus and console.
Built-in graphics presets default to crisp pixel filtering. The engine menus offer
F1 setting help and a 15-second Keep/Revert confirmation for display changes;
see [renderer controls](documentation/SDL3-GPU.md#menu-usability).
Normal mapping, realtime lighting and shadows, specular and offset mapping, bloom,
HDR framebuffers, FXAA, and reflective/refractive water have independent controls. Game data and normal/specular
replacement textures are supplied by the player and are not included here.

This is an active renderer migration. The validation record and remaining feature
work are listed in [SDL3-GPU.md](documentation/SDL3-GPU.md); upstream-wide visual
compatibility is not yet established.
Upstream binary self-updates are disabled for this fork. Package downloads use
[signature-based trust](documentation/PackageTrust.md), with no automatic
mirror/TLS exception.

FTEQW and the new engine code retain the GNU GPL version 2 or later. See
[LICENSE](LICENSE) and [upstream credits](documentation/Credits.md).
