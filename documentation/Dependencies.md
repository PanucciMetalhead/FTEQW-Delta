# Dependencies

The desktop fork requires:

- SDL 3.4 or newer, including its CMake package.
- CMake 3.21 or newer and a C17 compiler (C++17 for optional Jolt).
- A Vulkan-capable driver on Windows/Linux, or Metal-capable hardware on macOS.

SDL 3.4 is required for selecting layers of depth render targets used by the
shadow cubemap array. The SDL2 compatibility library is not used. GPU and native
shader requirements are documented by [SDL](https://wiki.libsdl.org/SDL3/CategoryGPU).

Optional libraries retain upstream functionality: zlib/bzip2 for compressed
packages, libpng/libjpeg for images and screenshots, FreeType/fontconfig for
fonts, libvorbisfile for music, and GnuTLS for encrypted networking. Plugin
requirements are detected by CMake. Python 3 enables portable plugin metadata
packaging and the integration test tools; Pillow is needed for screenshot checks.

On macOS with Homebrew:

```sh
brew install cmake ninja sdl3 libpng jpeg-turbo freetype fontconfig libvorbis gnutls
```

GnuTLS support is enabled by `FTE_DEP_GNUTLS` when CMake finds its headers
and library. On macOS, the runtime loader uses the library path found by CMake,
then tries native `.dylib` names; rerun CMake if you move the installation.
`GNUTLS_STATIC=ON` selects direct linking (including shared libraries), despite
its historical option name. It is not required to load Homebrew GnuTLS.

On Linux, install your distribution's SDL3 development package if its version is
at least 3.4, together with the development packages for the optional libraries.
Otherwise build SDL from an official SDL 3.4 release and set `CMAKE_PREFIX_PATH`
to its installation prefix. SDL itself supplies window/audio platform backends;
the engine does not link an OpenGL context library.

On Windows, use the SDL3 development package for your compiler, or build SDL3
with CMake and point `SDL3_DIR` at the installed package. Put `SDL3.dll` beside
`fteqw.exe` when using shared SDL. Optional library DLLs must also be available.
The Vulkan loader and GPU driver are supplied by the system/driver installation.

Shader development additionally needs the Khronos glslang and SPIR-V tools:
`glslangValidator` and `spirv-val`. Ordinary engine builds use the embedded shaders.

The optional [Jolt plugin](JoltPhysics.md) requires an explicit external Jolt
Physics source checkout (`FTE_JOLT_SOURCE_DIR`) and a C++17 compiler. The engine
remains C17. Jolt is statically linked into its plugin and retains its MIT
license notice. ODE headers/libraries are optional comparison dependencies;
Homebrew supplies them with `brew install ode`. Default builds do not require
Jolt or fetch it from the network.
