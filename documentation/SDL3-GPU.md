# SDL3 GPU renderer

This page documents the native SDL3 GPU renderer in **FTEQW Delta**.

This local fork starts from FTEQW commit `f937b9d88` (revision 6729). The
supported desktop client is `sdlgpu`, using SDL 3.4 or later. It requests
`metal` on macOS and `vulkan` on Windows/Linux and checks the selected driver.
Device creation fails visibly if that backend is unavailable. There is no
OpenGL context or renderer fallback, and Windows does not fall back to D3D12.

The OpenGL backend, its platform context implementations, and the old SDL CD
driver have been removed. Shared map/model/material helpers retain their
upstream `engine/gl` filenames. Legacy `gl_*` console variables and material
blend tokens remain for configuration and shader-script compatibility.

## Menu usability

The engine Video menu offers display mode, resolution, refresh rate, interface
scaling, and shader-based display adjustments. VSync offers Off and On. FXAA
and floating-point scene storage are available under Rendering Options.

**Apply Settings** tests the display and interface-size changes for 15 seconds.
Choose **Keep** to accept them or **Revert** to restore the previous values.
Escape and an unanswered countdown also restore them. Enter initially selects
Revert. The timer uses real elapsed time and continues while rendering is idle.
`vid_revert` restores a pending test from the console; without a pending test it
does nothing. A failed mode change restores the previous settings automatically.
Console `vid_restart` keeps its existing behavior and does not open this prompt.
Use **Save all settings** to persist accepted changes.

Hover over a setting or press **F1** on the selected row for its description,
console name, default, and any pending value. Help identifies settings requiring
video Apply/restart. F1 toggles keyboard help; **Delete** resets sliders,
checkboxes, and cvar dropdowns. Delete in a text field edits its text.

Texture Options labels the default **Crisp Pixels**. World/model textures use
`gl_texturemode nll`: nearest magnification, smooth mip transitions, and linear
minification to reduce distant shimmer. HUD/menu images use
`gl_texturemode2d GL_NEAREST`. Every built-in graphics tier selects these modes;
users can still change either filter afterward. Existing saved configurations
and custom presets retain their explicit settings. Font smoothing and Panini's
resampling filter are separate controls.

## Graphics controls

Use **Options → Texture Options**, **Lighting**, and **Rendering Options**,
or the console. These controls can be changed while playing. `seta` saves a
console setting; a plain variable assignment changes it for the current run.
The lighting menu retains upstream's Apply/video restart behavior.

| Effect | Console controls | Notes |
| --- | --- | --- |
| View projection | `r_projection 0/6` | **Options → View Projection**: Regular or Panini. Panini reduces edge stretching at wide FOVs and keeps vertical lines straight. Applies immediately and saves in configs. |
| Field of view | `fov`, `cg_fov` | **Options → Field of View** offers 70–170. Both console names change the same saved setting, including in OpenArena. Wider console values work; regular perspective renders up to 179 degrees. OpenArena zoom and server FOV limits are preserved. |
| FOV convention | `scr_fov_mode 4` | The default menu label is **Classic 4:3 / Widescreen**; classic vertical coverage expands sideways on wider screens. |
| Map lighting | `r_fullbright 0/1` | Zero uses BSP lightmaps; one forces full brightness. |
| Normal maps | `r_normalmapping 0/1` | Normal textures are loaded independently of this switch so enabling it works immediately. |
| Specular highlights | `gl_specular 0/1`, `gl_specular_power` | Uses material gloss/specular maps and realtime lights; directional lightmaps also support static highlights. |
| Height/offset mapping | `r_glsl_offsetmapping 0/1`, `r_glsl_offsetmapping_scale` | Uses the alpha channel of normal maps for a single height offset. |
| Directional lightmaps | `r_deluxemapping 0/1` | Requires a map with directional lightmap data. |
| Realtime map lights | `r_shadow_realtime_world 0/1` | Uses `.rtlights` or optional imported map lights. |
| Realtime transient lights | `r_shadow_realtime_dlight 0/1` | Includes game and QuakeC dynamic lights. |
| Realtime shadows | `r_shadow_realtime_world_shadows 0/1`, `r_shadow_realtime_dlight_shadows 0/1` | Depth cubemap shadows; `r_shadow_shadowmapping 0` disables shadow mapping. |
| Classic dynamic lightmaps | `r_dynamic 0/1` | Retains the generic engine lightmap path. |
| Static lightmap contribution with RT lights | `r_shadow_realtime_world_lightmaps` | Zero removes the static contribution; one retains it fully. |
| Water surfaces | `r_waterstyle 0/1/2/3` | Flat, classic, refraction, reflection + refraction. Per-fluid overrides include `r_slimestyle`, `r_lavastyle`, and `r_telestyle`. |
| Water movement/transparency | `r_waterwarp`, `r_wateralpha` | Refractive surfaces retain their configured opacity too, including custom mod liquids. Alpha 1 keeps the texture opaque; lower alpha reveals refraction. Subject to server visibility rules and map overrides. |
| Slime/lava transparency | `r_slimealpha`, `r_lavaalpha` | Empty inherits `r_wateralpha`. These overrides use the same opacity rules as other liquids, including refractive styles; lower alpha allows refraction to show through. |
| Bloom | `r_bloom 0/1` | Nonzero values set its strength; `gl_bloom` is an alias. |
| Floating-point scene buffer | `r_hdr_framebuffer 0/1` | Selects RGBA16F when supported; SDR display output. |
| Exposure adaptation | `r_hdr_irisadaptation 0/1` | Retains the engine's exposure calculation. |
| Antialiasing | `r_fxaa 0/1` | Post-process FXAA. |
| Texture filtering | `gl_texturemode`, `gl_texturemode2d`, `gl_texture_anisotropic_filtering` | Separate world/UI filtering, mip limits and anisotropy. |
| Display adjustments | `gamma`, `contrast`, `brightness`, `v_contrastboost` | Applied in the GPU presentation shader. |
| Scene-only display adjustments | `vid_sceneonlygamma 0/1` | **Options → Video → Scene-Only Gamma**. Off adjusts the full image; on adjusts the 3D view before drawing HUD, menu, console, and other screen overlays. Takes effect immediately and is saved in configs. |

The old Gamma Mode selector has been replaced by Scene-Only Gamma. SDL3 GPU
always uses shader correction; legacy `vid_hardwaregamma` values do not select
a display method or bake gamma into newly loaded textures in this renderer.
When scene-only correction is enabled, bloom and FXAA also run before screen
overlays, keeping interface colors and text out of those effects. Offscreen
CSQC 2D textures retain their normal render order and are corrected with the
scene or overlay that displays them.

The renderer also handles fullbright textures, player-color overlays, animated
textures and model interpolation, CPU skeletal animation, classic layered skies
and skyboxes, fog, particles, decals, HUD/menus, screenshots, and CSQC 2D render
targets. Availability of an effect still depends on the supplied game assets.

`gpu_info` reports the native driver, physical framebuffer size, frame geometry,
material counts, maximum lights per draw, shadow caster draws, loaded lightmaps,
fallback lightmap draws, and water views. Material counts are unlit, lightmapped,
model-lit, classic liquid, layered sky, enhanced liquid, and cube sky, in that order.
`-gpu_debug` enables SDL GPU debugging.

## Current implementation limits

This is a working desktop renderer migration, **not full upstream visual parity**.
The following work remains before that claim is appropriate:

- Custom GLSL/HLSL shader programs need native SDL GPU ports. Supported builtin
  programs use the new scene shader; unsupported programs use available material
  passes. A successful material fallback does not reproduce arbitrary shader code.
- Material permutations are incomplete: PBR, reflection cubemaps, projected and
  spot lights, light cubemap filters, soft particles, skeletal-specific effects,
  tessellation, and some specialized texture-coordinate/deformation generators
  need implementation and asset coverage. Height mapping is a simple offset,
  rather than the upstream relief-mapping path.
- Q3/CSQC portal cameras, skyrooms, recursive mirrors, VR/stereo, and nonstandard
  projections other than Panini have not been ported. Water subviews exclude other reflective or
  refractive surfaces to prevent recursion. Particle-driven ripple maps are not
  implemented; water styles 4/5 currently use style 3.
- At most 32 intersecting lights are selected per draw, ranked by contribution;
  eight lights can have 512×512 cubemap shadows per frame. This is not clustered
  lighting. Shadow and CPU geometry performance still need profiling on large maps.
- Up to four water planes receive separate views per frame. Subview visibility
  is conservative because legacy opaque-water VIS can omit required geometry;
  frustum, backface, clip-plane and depth culling still apply. The default view
  size is half resolution, controlled by `r_refractreflect_scale`.
- The window presents SDR. HDR framebuffer storage is separate from HDR display
  output, which is not implemented. FXAA is available for antialiasing.
- Windows/Linux Vulkan builds and runtime behavior require testing on those
  systems. The manual desktop CI matrix is prepared locally but has not run.
  Controllers, microphone capture, IME, multi-monitor fullscreen, and the full
  collection of supported games/mods need additional hardware/content coverage.

Only the desktop CMake build is supported by this fork. Old mobile/console
and distribution files are retained as upstream history, not supported recipes.
The obsolete automated Forgejo publication workflow has been removed; GitHub's
replacement workflow is manual and has no upload/publish step. Upstream binary
self-updates are disabled so they cannot replace the fork with an upstream build.

## Desktop validation

On macOS/Metal, the client, dedicated server, compiler, QuakeC VM, QTV, and
configured plugins built successfully. The compiler/VM regressions and
sanitizer networking tests passed, along with native HTTP file transfers and
a WebSocket game connection delivering reliable chat. The Quake 1 graphics
integration checks passed, including lighting, graphics switches, offscreen
rendering, video restart, and Ogg decoding. OpenArena checks passed for camera
turning, movement, weapon visibility, and video restart. Native Windows/Linux
runtime coverage remains pending.

## Reproduce the menu checks

On macOS/Linux, the native menu regression requires a C compiler, `pkg-config`,
SDL3 development files, and Pillow. Build `fteqw`, then run:

```sh
python3 tools/gpu_menu.py /path/to/Quake --out build/menu-qol
```

Choose a new output folder each time. The test reads supplied PAK files and
builds a temporary input plugin in that folder. It checks the 15-second timeout
while minimized, Keep, Escape, default Revert, console rollback, window-size
restoration, filtering in all seven built-in tiers, manual filtering overrides,
F1 help visibility and Delete reset. Logs, screenshots, and `results.json` stay
in the output folder. Inspect the confirmation and pending-value captures too.
This validation passed on macOS/Metal; Windows/Linux runtime checks are pending.

## Reproduce the integration test

The test requires legally supplied Quake data, Pillow, PNG support, and `fteqcc`.
It creates an isolated directory with links to the supplied pak files, synthetic
normal/gloss textures, one `.rtlights` fixture, a small client QuakeC program,
and an offscreen-target material. It does not modify the original game data.

```sh
cmake --build build --target fteqw fteqcc --config Release --parallel
python3 tools/gpu_integration.py prepare /path/to/Quake --qcc build/fteqcc
python3 tools/gpu_integration.py run build/fteqw
```

For multi-configuration builds, use `build/Release/fteqw.exe` and the corresponding
compiler executable. `--out` and `--log` select isolated output paths. `check`
can revalidate an existing completed run. Do not run two tests against the same
output directory concurrently. The engine run takes about 85 seconds.

The script pauses the simulation and compares screenshot pairs after changing
one control. It checks distinct images for normal/specular/offset mapping,
world/dynamic lighting, shadows, bloom, HDR, FXAA, and water reflection/refraction.
It then changes maps, checks a red/blue CSQC offscreen texture, restarts video,
and checks it again. It verifies static map lighting against `r_fullbright 1`,
restores lighting after the toggle and another restart, and opens Video Options
to check resolution selection. If `id1/music/track02.ogg` is supplied, it links
that file into the test directory and verifies successful stereo PCM decoding.
It rejects engine/GPU errors and writes `results.json`.
Image differences verify that controls affect output; they are not a proof of
pixel equivalence with upstream or broad mod compatibility.

The E1M1 slime regression uses `--profile slime` with each of the `prepare`,
`run`, and `check` commands. It switches through normal, nice, and realtime
presets at a fixed pool camera; checks surface colour and blackened room
geometry; then exercises explicit transparency, reflection, underwater views,
animated surface textures, and video restart. Results are in `slime-results.json`.

The Arcane Dimensions hub regression uses `--profile ad` with the same
commands. Supply a game folder containing both `id1` and `ad_v1_80p1final`;
only its PAK files are linked/copied into the isolated output. Fixed cameras
check the blood surface under normal/nice/realtime, reflective liquids, the
custom cube sky versus fast sky, a user sky override and restoration, surface
animation, preset restoration, and video restart. Results are in
`ad-results.json`. Screenshots are in the isolated mod directory.

The `--profile tears` regression loads Arcane Dimensions `ad_tears` using the
same isolated PAK links. Entrance lettering/vines and a foliage alcove are
captured across normal/nice/realtime and video restart. A fixture light in the
alcove makes the plants and their shadows visible. `gpu_info` reports cutout
draws, missing base-texture bindings, and cutout shadow caster batches; the
checker requires valid bindings and complete lightmaps across the run. Results
are in `tears-results.json`. Inspect the captures for transparent gaps; the
binding checks alone cannot verify the silhouette of every texture.

## Renderer performance

Adjacent draws with identical pipeline state, viewport/scissor, textures and
uniforms are combined without sorting or changing triangle/blend order. Index
rebasing respects 16-bit buffer limits. For non-RT material passes, state is
prepared once and subsequent meshes append directly to the draw; RT lighting
still selects lights for each mesh. Each render pass reuses unchanged uniforms,
samplers and pipeline bindings, and frames without shadow lights skip repeated
clears after the fallback shadow resource has been initialized.

`gpu_benchmark 10 label` measures frame throughput, median/p95 intervals and
CPU acquisition, scene construction and command encoding time. Warm up first;
use a Release executable without `-gpu_debug`, `vid_vsync 0`, a high `cl_maxfps`,
and `cl_idlefps 0` for throughput measurements. Swapchain acquisition includes
GPU/presentation backpressure; these are CPU stage timings, not GPU timestamps.
`gpu_batching 0/1` provides reference/optimized submission for image comparisons.
It defaults to one, resets on video restart, and does not change graphics cvars.

The controlled performance test uses the same owned game data as the AD tests:

```sh
python3 tools/gpu_performance.py prepare /path/to/Quake --qcc build/all/fteqcc
./build/fteqw -nohome -basedir build/perfdata -quake -window -width 1470 -height 923 +vid_renderer sdlgpu +exec gpu-test.cfg > build/performance.log 2>&1
python3 tools/gpu_performance.py check --target-fps 60
```

The config selects fullscreen, pauses the entrance scene for reference/batched
captures and benchmarks, then unpauses and rotates the camera through a full
sweep. The checker compares map pixels and geometry counts, checks lightmaps,
and optionally enforces average and p95 frame time against a target. Results
are in `performance-results.json`. Performance varies with scene, resolution,
build type and machine load; avoid compiling or running other engines during
measurements. This is a renderer test at the entrance, not a traversal of the
entire map or a direct benchmark against another engine.

## Local validation record

On 2026-10-09, on macOS arm64 with AppleClang and SDL 3.4.18:

- Debug desktop client and image utility built successfully.
- The default Release build, including dedicated server, tools, and available
  optional plugins, built successfully. Optional dependencies are reported by CMake.
- Native Metal shaders compiled at runtime with `-gpu_debug`; the integration
  run completed without reported GPU or engine errors.
- Quake `start` and `e1m1` rendered using isolated game data. All screenshot
  switch checks and CSQC offscreen/map-change/video-restart checks passed.
- Restored BSP lightmap allocation by declaring the wall program's lightmap
  samplers, and restored the overbright scale removed during CPU packing.
  All lightmaps loaded with zero visible wall draws using a white fallback.
  Static lighting recovered after toggling `r_fullbright` and restarting video.
- The client now links the detected libvorbisfile and its headers. The supplied
  `track02.ogg` decoded to 16-bit stereo PCM without library-loading errors.
- Resolution menus clamp oversized modes to the last available size and use
  virtual dimensions for 2D choices. Opening Video Options at a Retina
  1600x1200 framebuffer produced no `Fixed initialvalue for Size` warning.
- Embedded SPIR-V shaders passed `spirv-val` after compilation with glslang.
  Native Vulkan execution has not been verified on this Mac.
- The client linked SDL3 and contained no OpenGL framework dependency.
- The native in-game texture, lighting, and rendering menus were captured and
  inspected with the new controls visible. Both portable QuakeC packages passed
  ZIP integrity checks.

Generated screenshots, logs, and test results remain under ignored `build/`.
No game data or test artifacts are included in the source, and nothing has been
pushed or published.

On 2026-10-10, the E1M1 slime regression passed on the same Mac. Refractive
slime/lava now retain their surface according to the fluid alpha setting,
and advanced liquids animate their surface texture as well as the refracted
view. Refraction uses tint and grazing-angle blending instead of a fixed 85%
replacement of the liquid surface. Tangent-space directional lightmaps are
transformed into world space before lighting, fixing black
patches when the nice preset generates `.lux` data. Normal/nice/realtime
visibility, explicit transparency, reflection, underwater views, surface
animation, and video restart were checked. Debug and Release builds succeeded;
the embedded Vulkan shaders were regenerated and validated with `spirv-val`.
The broader graphics suite also passed again, including ordinary water,
lighting/shadows, normal/specular/offset mapping, post-processing, offscreen
targets, and the earlier lightmap/Vorbis/resolution-menu regressions.

The Arcane Dimensions `start` regression also passed on 2026-10-10. Programs
attached to individual material passes now select their native scene mode.
`defaultskybox` uses a dedicated cube sampler in both Metal and SPIR-V, with
properly typed fallback bindings and the existing geometry/depth mask. The
hub's `moonrise_` sky rendered across normal/nice/realtime; fast sky, sky
replacement/restoration, and video restart worked. The resolved liquid alpha
is carried from the material generator to refraction for all liquid names,
including `*waterblood`, map overrides, and per-fluid alpha settings. Blood
remained red across the presets, while reflections and surface animation
remained visible. The E1M1 slime suite passed again, including transparency
and underwater views. Debug and default Release builds succeeded.
The full graphics suite passed again after these changes, including an explicit
opaque/transparent ordinary-water comparison, lighting/shadows, normal/specular/
offset mapping, post-processing, offscreen targets, static lighting recovery,
Vorbis music decoding, and the resolution menu. Embedded Vulkan shaders passed
`spirv-val`; execution was verified using native Metal on this Mac.

The `ad_tears` cutout regression passed on 2026-10-10. Wall and skin programs
now declare their named diffuse sampler instead of reserving an empty explicit
texture stage. This restores embedded fenced textures such as `{vinehang1`,
`{vine3`, `{plant_03`, and the map's title lettering. Program `MASK`/`MASKLT`
cutoffs are applied by the scene and shadow shaders in both Metal and SPIR-V;
shadow batch merging retains distinct cutoff values. Entrance lettering/vines
and plant silhouettes were visually inspected. All cutout draws used real
base textures across normal/nice/realtime and restart, with complete lightmaps
and no reported engine/GPU errors. The lit foliage fixture also exercised
cutout shadow caster batches. Debug and default Release builds succeeded;
SPIR-V validation and the full graphics suite passed again.

On 2026-10-10, the optimized primary Release executable passed the performance
test on native Metal at 2940x1846. Faithful reference submission averaged
84.07 FPS with 11,353 draws; batching averaged 136.19 FPS with 186 draws and
the same 59,184 vertices. Command encoding fell from 3.971 to 0.313 ms. Batched
p95 frame time was 8.118 ms; the unpaused camera sweep had a 7.167 ms p95.
Reference/batched map pixels in the checked region matched exactly, with all
760 lightmaps loaded. Both phases met the 60 FPS target. These reference numbers
compare submission modes in the final renderer; they are not the original
renderer's frame rate in another camera/build configuration.

The primary `build/fteqw` is now Release rather than Debug. CMake also defaults
new single-configuration builds to Release when no build type is selected;
explicit Debug builds remain available. SDL worker threads requested with small
stacks now receive at least 1 MB: the optimized model loader's stack exceeded
the old 256 KB hint when SDL3 began honoring it. This fixes the Release startup
crash observed while validating `ad_tears` with loader workers enabled.
The full graphics suite passed on the rebuilt primary Release client with GPU
debug validation, including lighting/shadows, normal/specular/offset maps,
post-processing, liquids, offscreen targets, static lighting recovery, music
decoding and resolution menus. The `ad_tears` cutout regression also passed
across normal/nice/realtime and video restart with no missing base textures or
lightmap fallbacks. The optimized Debug client was compiled separately and is
retained locally as `build/fteqw-debug-optimized`; performance figures above
refer to Release.

### Explosion sprite regression

`defaultsprite#MASK` rejects transparent texture pixels before vertex alpha is
applied. Its mask differs from the inverse opaque pass used by wall/skin programs.
The SDL GPU sprite mask now follows that rule without forcing the surviving
pixels opaque, preserving fades. Metal and Vulkan use the same comparison.

The fixture renders Quake's actual explosion sprite frames 0, 3, and 5 over a
uniform backdrop. It checks transparent corners, quarter-alpha fades, premultiplied
and additive sprite modes, and restoration after `vid_restart`:

```sh
python3 tools/gpu_sprites.py prepare /path/to/Quake --qcc build/all/fteqcc
./build/fteqw -nohome -basedir build/spritedata -quake -window -width 800 -height 600 -gpu_debug +exec gpu-test.cfg > build/sprite-after.log 2>&1
python3 tools/gpu_sprites.py check
```

macOS Metal validation reproduced the inverted explosion mask before the fix
and passed all fixture cases after the fix. The existing Arcane Dimensions
foliage test also passed. Vulkan shader binaries passed SPIR-V validation;
Vulkan runtime testing remains pending.

## Model replacement textures

With `gl_load24bit 1`, Quake model skins can be replaced in the active game
folder's `textures/models` directory. For example, `qw/textures/models/g_rock2_0.tga`
replaces skin 0 of `progs/g_rock2.mdl`, and
`qw/textures/models/quaddama_0.png` replaces skin 0 of `progs/quaddama.mdl`.
PNG and TGA are supported; use `_1`, `_2`, etc. for other skin indices.
Existing explicit FTE paths such as `qw/progs/g_rock2.mdl_0.tga` retain priority.
Multiplayer rulesets can restrict replacements of sensitive models.
Restart the renderer with `vid_restart` after adding or changing files.

The model texture regression renders actual Quake models with distinct PNG/TGA
skins, checks explicit-path priority, tests `gl_load24bit 0` and missing-image
fallbacks, and compares captures before and after a video restart:

```sh
python3 tools/gpu_model_textures.py prepare /path/to/Quake
for case in compat direct disabled missing; do
    ./build/fteqw -nohome -basedir "build/modeldata/$case" -quake -game qw \
        -window -width 800 -height 600 -gpu_debug +vid_renderer sdlgpu \
        +exec gpu-test.cfg > "build/modeldata/$case.log" 2>&1
done
python3 tools/gpu_model_textures.py check
```

Game data and generated images stay in the ignored build directory. Preparation
requires `build/all/fteqcc` (or `--qcc`), and image checks require Pillow.

macOS Metal validation passed all four cases. Copies of the reported rocket
launcher TGA and quad-damage PNG also rendered correctly after this lookup fix.
Vulkan runtime validation remains pending.

## Scene-only gamma regression

`tools/gpu_gamma.py` prepares isolated owned-game data and runs pixel checks
for whole-image versus scene-only correction, opaque and translucent overlays,
two successive paints of the same CSQC offscreen texture, SDR and HDR scene
buffers, live toggling, video restart, menu text, console-only rendering,
legacy gamma configuration, and saving the new setting. The fixture and outputs
are separate from the original Quake installation.

```sh
python3 tools/gpu_gamma.py prepare /path/to/Quake --out /tmp/fte-gamma-test
python3 tools/gpu_gamma.py run --out /tmp/fte-gamma-test
```

Use a new output directory for preparation. The run enables SDL GPU validation
and writes screenshots, `gamma.log`, and `gamma-results.json` there. Metal was
tested locally; Vulkan runtime validation still requires Windows/Linux hardware.

## Panini projection

Select **Options → View Projection → Panini**, or enter `r_projection 6`.
`r_projection 0` restores Regular. Only those two choices appear in the menu;
the older projection enums, shaders, and renderer implementations remain in
the source tree with their original numeric values for reference.

The SDL3 GPU implementation uses standard Panini with projection distance D=1,
following the [Pannini projection equations](https://www.mdpi.com/1424-8220/22/1/197).
Each 3D view is rendered once into a cached texture, then an inverse projection
pass maps it to the screen before drawing the HUD and menus. No-world 3D
model previews keep their normal perspective projection. Split views and
CSQC offscreen views have separate source textures, including HDR formats.
Gamma, bloom, and FXAA still run through the usual presentation path.

The implementation follows the normal FOV setting and FOV mode, capped at 170°
for the perspective source. It preserves horizontal coverage and fits the
projection within that source: the center is magnified and some vertical
coverage is cropped. Objects near the edges stretch less horizontally, while
horizontal lines away from the horizon can curve. It is most apparent at wide
FOVs. It does not provide a 180°/360° panorama or use the old cubemap renderer.
Regular does not allocate new projection targets or run a projection pass.

Panini quality controls are directly below **View Projection** in the native
Options menu (`menu_options` in either Quake 1 or Quake 3/OpenArena):

| Menu setting | Cvar | Default | Behavior |
| --- | --- | --- | --- |
| Panini Resolution | `r_panini_scale` | `1.25` | Source width/height multiplier, clamped to 1–2. A value of 2 requests four times the native pixel count. |
| Panini Filter | `r_panini_filter` | `1` | 0: original bilinear reconstruction; 1: bicubic reconstruction in magnified areas and anisotropic mip filtering in reduced areas. |
| Panini Sharpening | `r_panini_sharpen` | `0.2` | Strength from 0–1; zero disables sharpening. Local overshoot is limited, and sharpening fades out in reduced areas. |

Settings apply immediately, are archived with `cfg_save`, and affect only the
Panini 3D view. HUD/menu rendering and Regular projection are unaffected. Use
resolution 1, Bilinear, sharpening 0 to reproduce the original Panini path.
For a less expensive intermediate setting, use resolution 1 with Bicubic.

Source level-zero allocation is limited to 16 million pixels and 8192 pixels
per axis, keeping its aspect ratio within rounding to whole pixels. Native
views already above those limits are not reduced. Cached color/depth targets
are reused between frames; Panini adds no extra world views when changing quality or FOV. Mip levels require additional color memory and are generated
only for Bicubic mode. If a larger source cannot be allocated, the renderer
reports it and falls back to native size, without retrying every frame at the
same requested size. `gpu_info` reports actual source/output dimensions,
filter and sharpening, including a capped or fallback resolution.

The bicubic kernel groups its middle weights to use nine bilinear reads instead
of sixteen point reads. Regions reduced by the warp use mip filtering instead
of that kernel, and explicit gradients keep mip selection stable. Sharpening
preserves HDR highlights. Higher source resolutions still increase GPU work:
1.25 requests 56% more source pixels, while 2 requests 300% more, before caps.

At actual horizontal FOVs of 150°, 160° and 170°, the current projection enlarges
source pixels at the center by roughly 2.4, 3.4 and 6.2 per axis. These controls
improve detail, but cannot fully undo the sampling loss at extreme FOVs within
a modest pixel budget. FOV mode/aspect ratio can make the actual horizontal
angle differ from the console's `fov` number.


To test with owned Quake data, use a new output directory:

```sh
python3 tools/gpu_panini.py prepare /path/to/Quake --out /tmp/fte-panini-test
python3 tools/gpu_panini.py run --out /tmp/fte-panini-test
```

The runner checks rendered pixels against independently constructed Panini
rays at 90°, 140°, and 170° horizontal FOV, SDR/HDR, split views, offscreen
rendering, reflective/refractive liquids, 3D model previews, HUD colors,
scene-only gamma, video restart, saved quality settings, and scaled split/offscreen/water views.
Screenshots and results remain separate from the original game installation.
Allocation limits (including invalid cvar values) and mip-count boundaries are
checked under ASan/UBSan:

```sh
python3 tools/panini_tests.py
```

To compare center detail with an independently projected, higher-resolution
perspective reference at 150° and 170°, and record native-backend frame timings:

```sh
python3 tools/gpu_panini_quality.py prepare /path/to/Quake --out /tmp/fte-panini-quality
python3 tools/gpu_panini_quality.py run --out /tmp/fte-panini-quality
```

The quality runner uses small windows so that its reference fits the display,
then switches to desktop fullscreen for timing. It compares restored settings
as well as filtering, source resolution, default settings and maximum quality,
and saves `quality-results.json`. A display/presentation cap can conceal GPU
cost; interpret frame timings together with the acquisition timings in the log
and profile demanding maps at your playing resolution. This is not a promise
of a particular gameplay frame rate.

Metal runtime validation passed; Vulkan shaders are compiled and checked
with `spirv-val`, but Vulkan runtime validation requires Windows/Linux hardware.


## OpenArena lighting, FOV and shotgun smoke checks

The OpenArena `controls` profile checks real map lighting versus `r_fullbright 1`,
restoration, console FOV changes through 175, zoom, menu rendering and saved FOV.
Use owned OpenArena 0.8.8 data and a new isolated output directory:

```sh
python3 tools/gpu_openarena.py prepare /path/to/OpenArena --profile controls --out build/oa-controls
python3 tools/gpu_openarena.py run --profile controls --out build/oa-controls
python3 tools/q3_fov_tests.py
```

The focused FOV test compiles the actual plugin adjustment with Clang and
ASan/UBSan, covering zoom interpolation, aspect ratio, intermissions, UI scenes
and server restrictions. The compatibility adjustment extends the traditional
[cgame FOV clamp](https://github.com/OpenArena/gamecode/blob/main/code/cgame/cg_view.c)
without changing the game QVM or player movement.

The sprite test can also load the installed OpenArena shotgun smoke material.
It compares full, half and quarter alpha, then checks for an empty background
after the smoke disappears. Both the shader and texture come from the supplied
PK3s; original game files are only read:

```sh
python3 tools/gpu_sprites.py prepare /path/to/Quake --openarena /path/to/OpenArena --out build/oa-smoke
./build/fteqw -nohome -basedir build/oa-smoke -quake +exec gpu-test.cfg
python3 tools/gpu_sprites.py check --out build/oa-smoke --log /path/to/engine.log
```

Capture engine stdout to the log path when running the sprite fixture. For a
Windows multi-configuration build, use `build/Release/fteqw.exe`. Sprite results
include `shotgun_smoke_contrast_ratios`; no OpenArena assets are included in the
repository.

## Quake 3 / OpenArena skies

Scripted Quake 3 clouds use their `skyParms` cloud height and the Quake 3
angular projection onto a curved cloud layer. The camera-centered mesh uses
eight subdivisions per cube face, with cached coordinates for its height.
Each material stage keeps its own texture, blending, color generation and
`tcMod` animation; rotating stages use the parser's turns-per-second units.
Static six-face skyboxes remain static unless their material specifies animation.

Sky visibility masks write BSP depth without painting brush textures over the
projected sky. They use the original BSP entity transform, while generated sky
meshes render from inside the sky volume. `r_fastsky` and `r_skybox` retain their
existing controls. Quake 1's layered skies and native cube-sky path are preserved.

The cloud projection follows [Quake 3's sky coordinate calculation](https://github.com/ioquake/ioq3/blob/master/code/renderergl1/tr_sky.c)
and [skyParms height parsing](https://github.com/ioquake/ioq3/blob/master/code/renderergl1/tr_shader.c).

```sh
python3 tools/q3_sky_tests.py
python3 tools/gpu_openarena.py prepare /path/to/OpenArena --profile skies --out build/oa-skies
python3 tools/gpu_openarena.py run --profile skies --out build/oa-skies
```

The source-helper test uses Clang with ASan/UBSan to check projection geometry,
cloud heights/cache changes, clipped mesh bounds, scrolling order and rotation
speed. The GPU profile uses owned OpenArena data plus isolated diagnostic BSP
material substitutions and a generated coordinate texture. It checks the
projected texture coordinate at the zenith, changes cloud height and restores
it, observes stock cloud animation, switches fast sky, and checks a static
skybox and video restarts. It never changes the original game data.

## Quake rerelease MD5 checks

The existing SDL3 GPU skeletal path renders rerelease MD5 models and their
indexed skins. The owned-data harness checks every skeleton/animation header,
representative textured poses, lighting, original-model fallback, video
restart, and campaign save/load. See [QuakeRemaster.md](QuakeRemaster.md) for
commands, the Mac/Metal validation record, and remaining optional builtins.
