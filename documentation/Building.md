# Building the SDL3 GPU fork

Use CMake 3.21 or newer and a compiler with C17 support on Windows, Linux,
and macOS (Clang/GCC, or Visual Studio 2019 16.8 or newer). The old
platform-specific OpenGL make targets are removed. `engine/Makefile`, `build_setup.sh`, and `build_wip.sh` are
small wrappers around CMake.

Install [dependencies](Dependencies.md), then configure and build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

For just the client, append `--target fteqw` to the build command. Multi-configuration
Windows builds place executables in `build/Release`; single-configuration Ninja
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
Legacy console, web, mobile, and Flatpak deployment files are upstream history;
they are not supported build paths for this desktop fork.

## OpenArena and the Quake 3 plugin

OpenArena requires the Quake 3 compatibility plugin. Build both targets and
keep the generated plugin beside the engine executable:

```sh
cmake --build build --target fteqw plug_quake3 --config Release --parallel
./build/fteqw -basedir "/path/to/OpenArena" -quake3 -game baseoa +plug_load quake3
```

The basedir contains `baseoa/*.pk3`. On Windows the engine is in
`build/Release/fteqw.exe`; its plugin is `fteplug_quake3.dll`. On macOS/Linux the
plugin is `fteplug_quake3.so`. The default plugin policy does not automatically
load every loose plugin built by CMake. `+plug_load quake3` loads the required
module without enabling all the other optional plugins.

If a connection fails with “Q3 plugin not loaded”, enter `plug_load quake3` in
the console, then `reconnect`. `plug_load quake3` can also be placed in the
game's `autoexec.cfg` for subsequent starts.

The Quake 3 profile defaults to protocol 68. Standard OpenArena servers use
[legacy protocol 71](https://github.com/OpenArena/engine/blob/master/README.md);
use `com_protocolversion 71` before connecting to those servers, or pass
`+com_protocolversion 71` at launch. Servers using another engine may advertise
a different protocol, so match the server's version.


To check OpenArena rendering using your installed game data, build the targets
above and install Pillow for Python. This test creates a separate output folder;
the original PK3 files are only read. Choose a new output directory for each run.

```sh
python3 tools/gpu_openarena.py prepare "/path/to/OpenArena" --out build/openarenadata
python3 tools/gpu_openarena.py run --out build/openarenadata
```

Use `--engine build/Release/fteqw.exe` for a Windows multi-configuration build.
The local match checks camera turning, movement, the cgame weapon toggle and
video restart using captured frames. `check` revalidates an existing run.
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
