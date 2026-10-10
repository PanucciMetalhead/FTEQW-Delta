# Building the SDL3 GPU fork

Use CMake on Windows, Linux, and macOS. The old platform-specific OpenGL make
targets are removed. `engine/Makefile`, `build_setup.sh`, and `build_wip.sh` are
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
plugins. `FTE_WERROR` defaults to OFF because upstream contains compiler warnings.
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
