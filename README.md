# FTEQW SDL3 GPU

Fork of [fte-team/fteqw](https://github.com/fte-team/fteqw), based on upstream commit
`f937b9d88` (revision 6729). The desktop client uses SDL3 for windowing,
input, controllers, audio, and GPU rendering. It explicitly selects **Vulkan
on Windows and Linux** and **Metal on macOS**.

The OpenGL renderer and its platform context implementations have been removed.
CMake builds one desktop GPU renderer, `sdlgpu`. The dedicated server and the
engine's game, network, map, model, particle, and QuakeC systems are retained.
Some shared rendering helpers keep their upstream `engine/gl` paths; these
helpers do not implement an OpenGL backend.

Build and run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target fteqw --config Release --parallel
./build/fteqw -basedir /path/to/Quake -quake
```

Requires SDL **3.4 or newer**, CMake, and a C/C++ compiler. See
[building](documentation/Building.md), [dependencies](documentation/Dependencies.md),
and [renderer controls and validation](documentation/SDL3-GPU.md).

Graphics controls remain available through the in-game Options menus and console.
Normal mapping, realtime lighting and shadows, specular and offset mapping, bloom,
HDR framebuffers, FXAA, and reflective/refractive water have independent controls. Game data and normal/specular
replacement textures are supplied by the player and are not included here.

This is an active renderer migration. The validation record and remaining feature
work are listed in [SDL3-GPU.md](documentation/SDL3-GPU.md); upstream-wide visual
compatibility is not yet established.
Upstream binary self-updates are disabled for this fork.

FTEQW and the new engine code retain the GNU GPL version 2 or later. See
[LICENSE](LICENSE) and [upstream credits](documentation/Credits.md).
