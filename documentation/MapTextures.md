# Textures and custom-map packaging

This guide covers **Quake 1 BSP/BSP2 maps in FTEQW Delta**, using the native
SDL3 GPU renderer: Metal on macOS and Vulkan on Windows/Linux. It explains
the default map-texture path; a custom material can define different behavior.
Quake II, Quake III, and Half-Life have different material and animation rules.

For a map with true-color artwork and emissive details, keep indexed fallback
textures in the BSP and ship the replacement PNGs with it in one **PK3**.
The engine reads the PNGs directly from the archive. Players do not need to
extract a directory full of loose textures.

## What each file does

| File | Purpose |
| --- | --- |
| `.map` | Editable geometry and texture names. Normally distributed separately as source. |
| Texture `.wad` | Editor/compiler input. Quake WAD2 uses the game's 256-color palette; WAD3 supports per-texture 256-color palettes. Neither standard miptex format stores arbitrary RGB/RGBA images. |
| `.bsp` | Compiled map. A normal Quake compile embeds its indexed textures, including animation frames. BSP2 expands geometry limits; it does not make those embedded textures true color. |
| `.png` | External RGB/RGBA replacement image or companion map, such as `_luma`. |
| `.pk3` | ZIP archive with engine-relative paths. Packages the BSP, PNGs, and other required runtime assets together. |
| `.pak` | Traditional Quake archive. It can also contain PNGs for an engine that supports them, but putting PNGs in a PAK does not give vanilla Quake replacement-texture support. |

The WAD can retain animation names and indexed fullbright pixels, but it does
not replace the external true-color images. Players normally need the BSP and
its runtime assets, not your editor WAD. If your map deliberately references
external WAD textures instead of embedding them, that WAD is a runtime
dependency too; the examples below assume embedded fallbacks.

## Base images and filenames

For a BSP named `my_map.bsp` with a texture named `mypanel`, use:

```text
textures/my_map/mypanel.png
```

Delta supports this map-specific replacement directory. A shared replacement
can instead use `textures/mypanel.png`, but that name can affect other maps.
Map-specific directories help keep a custom release self-contained without
replacing another author's textures. Use the BSP filename without `.bsp` as
the directory name, and keep filename case consistent.

The image name must match the **compiled texture name**, not the original
image's name before import. Quake texture identifiers have a 15-character
limit. That limit applies to the identifier in the WAD/BSP, not to a companion
PNG's complete filename with `_luma.png` appended.

Start with RGB/RGBA PNGs at the same dimensions as the source texture. Higher
resolution replacements can retain the same tiling when they keep the same
aspect ratio; the BSP still defines the original texture coordinates and
texture size. Resizing a PNG does not resize the map geometry.

For external PNGs and map fullbrights, use:

```text
gl_load24bit 1
r_fb_bmodels 1
r_fullbright 0
```

The first two normally default to 1. They affect texture loading and are
renderer-latched: set them before testing and restart the engine after a
change to avoid cached images or pending settings. `r_fb_models` is the
separate control for model fullbrights. `r_fullbright 1` brightens the whole
scene for inspection; it is not how a mapper enables one glowing texture.

## Native texture animation

Quake 1 recognizes `+0` through `+9` at the beginning of a texture name.
Everything after the two-character prefix must match across the chain.
For a four-frame lava animation, create four distinct images:

```text
+0mylava.png
+1mylava.png
+2mylava.png
+3mylava.png
```

1. Import all four frames into the editor/compiler WAD under those names.
2. Apply `+0mylava` to the faces in the map.
3. Compile with all animation siblings included in the BSP. Check your
   compiler's animation handling; PNG filenames alone cannot add missing BSP
   animation entries.
4. Ship all four replacement PNGs in `textures/my_map/`.

Use matching dimensions and contiguous frames starting at `+0`. A single GIF
renamed to `+0mylava.png` is not this workflow. The engine advances the native
Quake 1 chain at **0.2 seconds per frame (5 FPS)**. A four-frame loop therefore
lasts 0.8 seconds. Half-Life's animation timing is different; a tool's GoldSrc
documentation may describe 0.1-second frames instead.

Letters `+A` through `+J` define an alternate chain. They are useful for
switchable brush entities, but they do not create switch logic on their own.
The renderer selects the alternate chain when the entity's frame is nonzero;
the mod's QuakeC/game logic decides when that happens. Both chains must use
the same name after their prefixes, and each must be contiguous.

## Fullbright layers: `_luma` and `_glow`

Add a companion image using the complete base texture name:

```text
textures/my_map/mypanel.png
textures/my_map/mypanel_luma.png
```

`mypanel_glow.png` is an alternative name for the **same emissive/fullbright
slot**. It is not an additional layer stacked on top of `_luma`. Ship one
naming convention consistently rather than two different versions; filesystem
search priority can affect which matching asset is selected.

The companion should match the base image's dimensions and layout. In the
current native renderer, its RGB is added to the lit base color. This makes
the painted details visible in darkness. It does not remove lighting from
the base image, so the final surface can still brighten under illumination.

Emission, bloom, and illumination are separate:

- The companion image supplies emission on the surface itself.
- Bloom is a renderer/post-processing effect controlled by the player.
- Lighting nearby walls requires baked surface lights, realtime lights, or
  game-supplied lights. A `_luma` file alone does not cast light.

### Alpha in emissive images

**Current SDL3 GPU behavior: the emissive sample's alpha does not mask its
RGB.** The scene shader adds `glow.rgb`, not `glow.rgb * glow.a`. This applies
to both `_luma` and `_glow` in the default wall/model lighting path.

| Companion pixel | Contribution to emission |
| --- | --- |
| Opaque black, RGB `(0, 0, 0)` | None. The base texture still receives normal lighting. |
| Opaque colored pixel | Adds that RGB color. |
| Transparent colored pixel | Can still add its hidden RGB color. Alpha zero does not suppress it. |
| Partially transparent colored pixel | Alpha alone does not reduce the emission strength. |

For predictable masks, paint an **opaque black background**, then paint only
the details that should glow. Reduce RGB intensity to weaken emission. An
image editor's transparent checkerboard does not tell you whether the hidden
RGB is black: erasing alpha can leave the original artwork underneath.

This is the current Delta implementation, not a promise that other Quake
engines interpret emissive alpha identically. Opaque black backgrounds avoid
depending on hidden RGB or a particular alpha interpretation.

### A switch with a glowing indicator

For a switch whose off and on texture names are `myswitch0` and `myswitch1`:

```text
textures/my_map/myswitch0.png       # off artwork
textures/my_map/myswitch1.png       # on artwork
textures/my_map/myswitch1_luma.png  # black except for the colored indicator
```

The off state does **not** require a `_luma` file. Without an external
emissive companion, the loader may generate one from reserved fullbright
pixels in the embedded Quake texture. A `_glow` companion or an overriding
asset from another mounted package can also supply emission. If none of
those sources applies, omitting the off-state companion gives normal lighting.

An optional, entirely opaque-black `myswitch0_luma.png` explicitly suppresses
those fallback fullbrights for that texture. It is useful when the indexed
fallback has unwanted fullbright pixels; it is not mandatory for every off
state. Put that override in the map-specific directory too.

Game logic must select the corresponding base texture/material state. For
native alternate-chain switching, the equivalent names could be `+0myswitch`
for off and `+Amyswitch` for on, with `+Amyswitch_luma.png` as the on-state
companion. Compile both entries into the BSP and use a brush entity/gamecode
that selects the alternate chain. The suffix itself does not make a switch
interactive or detect whether it is on.

### Animated fullbright lava

Give each frame its own companion:

```text
+0mylava.png       +0mylava_luma.png
+1mylava.png       +1mylava_luma.png
+2mylava.png       +2mylava_luma.png
+3mylava.png       +3mylava_luma.png
```

To make the whole frame emissive, the companion can be an opaque copy of
that frame. To make only hot veins glow, use black elsewhere. Keep these
companions as external PNG assets, including when they are inside a PK3;
do not import them as extra animation frames into the WAD.

WadMaker's `~name.fullbright.png` workflow processes indexed fullbright pixels
for its WAD3 texture workflow. It is different from Delta loading an external
`name_luma.png` replacement. Renaming the companion to `.fullbright.png` or
adding `~` to an ordinary BSP texture name does not enable this PNG workflow.

## Transparent base textures

Base-image transparency and emissive masking are different operations. For a
Quake fence/grate using the supported `{` cutout convention, an example is:

```text
BSP texture name:                 {mygrate
RGBA replacement:                textures/my_map/{mygrate.png
Optional emissive companion:      textures/my_map/{mygrate_luma.png
```

Prepare the editor/compiler texture with the appropriate cutout handling as
well; the conventional indexed fallback uses index 255 for transparent
pixels. The default cutout path rejects transparent base texels. It is not
smooth, partially transparent glass, and putting alpha in an otherwise opaque
wall PNG does not automatically turn it into a cutout material.

Visual transparency does not define brush collision. A grate can still block
movement or shots unless the map's brush/entity setup gives it the intended
collision. An emissive companion cannot make holes in an opaque base surface.

## Package one runtime PK3

Stage only the files that players need. A development directory can use the
`.pk3dir` suffix, which Delta can mount as an unpacked package. Compress its
**contents**, not the enclosing directory, when preparing the release:

```text
my_map.pk3dir/
  maps/
    my_map.bsp
    my_map.lit             # only if the compile produced/needs this sidecar
  textures/
    my_map/
      mypanel.png
      mypanel_luma.png
      +0mylava.png
      +0mylava_luma.png
      +1mylava.png
      +1mylava_luma.png
      +2mylava.png
      +2mylava_luma.png
      +3mylava.png
      +3mylava_luma.png
  my_map-readme.txt
```

Include any other required runtime assets, such as sounds, skyboxes, materials,
or lighting sidecars, at their expected engine-relative paths. Keep the MAP,
compiler WAD, build logs, and editable artwork in a separate source release.
An archive containing only another ZIP is not the layout above.

A PK3 is a conventional ZIP with a `.pk3` extension. This Python 3 example
can be saved as `package_map.py` beside `my_map.pk3dir` and run with
`python3 package_map.py`:

```python
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

root = Path("my_map.pk3dir")
if not root.is_dir():
    raise SystemExit("Missing my_map.pk3dir staging directory")

with ZipFile("my_map.pk3", "w", compression=ZIP_DEFLATED) as archive:
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        if path.is_file() and not any(p.startswith(".") for p in relative.parts):
            archive.write(path, relative.as_posix())

with ZipFile("my_map.pk3") as archive:
    assert archive.testzip() is None
    assert "maps/my_map.bsp" in archive.namelist()
```

This replaces a previous `my_map.pk3` in the working directory. The resulting
archive must start with `maps/` and `textures/`, not
`my_map.pk3dir/maps/` or `id1/maps/`. The file extension alone does not convert
a 7z/RAR archive into a ZIP. FTEQCC also provides package creation and listing
commands; see [FTEQCC package commands](FTEQCC.md#package-and-decompiler-commands).

### Install and launch

For a map that uses the base Quake gamecode, the simple installation is:

```text
Quake/
  id1/
    pak0.pak
    pak1.pak
    my_map.pk3
```

Restart Delta after installing the archive, then enter `map my_map` in the
console. Delta automatically mounts PK3s in the active game directory; it
does not automatically mount an ordinary `.zip` in the same way. The PNGs can
remain inside the PK3 while playing.

For an isolated custom game directory, put the archive in `Quake/my_maps/`
and start from a shell with:

```sh
fteqw -basedir "/path/to/Quake" -quake -game my_maps +map my_map
```

The underlying Quake installation supplies its own base assets. If the map
requires a particular mod's gamecode, document that dependency and launch
with that mod instead; a texture archive does not implement new gameplay.
Include asset credits and applicable license notices with the release.

Locally installed PK3s and package-manager downloads are separate workflows.
For signed catalogs and remote installs, see [Package trust](PackageTrust.md).
Do not disable package trust or overwrite the player's `pak0.pak`/`pak1.pak`
to install a map.

## Verify the release package

Test the **built archive**, not only the unpacked staging directory. Use a
clean test game directory without duplicate loose maps/textures, an older
PK3, or an active `.pk3dir` copy of the same package. Otherwise a missing
archive entry can be hidden by another mounted asset.

Useful checks in Delta's console:

```text
path
r_imagelist mylava
gpu_info
```

`path` shows mounted search paths. `r_imagelist` reports image identifiers,
loaded formats and source paths; it can reveal a stale package, missing PNG,
or an unexpected `_glow` replacement. `gpu_info` reports the active GPU driver,
lightmap loading and fallback draws. Image loading can be asynchronous, so
inspect the view first and allow loading to finish before reading the list.

For animated lava, check that all four base PNGs and their four companions
load and that the cycle repeats. Keep `r_fullbright 0` while evaluating the
emission; whole-scene fullbright can hide a missing emissive layer. Inspect
the switch off/on states, transparency edges, distant mip levels, and a fresh
engine launch. Compare emission enabled/disabled in separate runs if needed.

| Symptom | Check |
| --- | --- |
| Map not found | Archive is mounted, and `maps/my_map.bsp` is directly at its expected path. |
| Colors look palette-limited | `gl_load24bit`, PNG paths/names, loaded image sources, and a fresh engine start. Embedded fallbacks remain indexed. |
| Animation stays still | All sibling texture entries are in the BSP, names match after `+0`/`+1`, and the sequence has no gaps. PNGs alone do not create the chain. |
| A whole panel glows | Hidden RGB in transparent emissive pixels, an unexpected companion, or embedded palette fullbrights. Use black RGB where emission should be absent. |
| Off state glows | A `_glow`/`_luma` from another source or palette-generated fullbrights. An explicit black companion can suppress fallback emission. |
| Indicator looks dark | `r_fb_bmodels`, the companion's full name/path/RGB, the selected switch state, and pending renderer settings. |
| Transparent wall stays solid-looking | The base material's cutout/blend setup; alpha in a `_luma` cannot make the base transparent. |
| Local test works but release fails | Another loose/staging asset supplied a file missing from the PK3, or filename case differs. |

PK3 loading, PNG replacements and the current emissive rules are Delta
features, not a guarantee of compatibility with every Quake engine. State
the required engine and BSP format in the map's readme and test any additional
engines you advertise.

## Implementation references

- [Texture animation sequencing and alternate chains](../engine/gl/gl_model.c):
  `Mod_UpdateBatchShader_Q1` and the `ANIM_CYCLE` setup.
- [Texture/companion loading](../engine/gl/gl_shader.c):
  `R_BuildLegacyTexnums`, `_luma:_glow` lookup and indexed fallback generation.
- [Replacement search and image reporting](../engine/client/image.c):
  `Image_LocateHighResTexture` and `r_imagelist` support.
- [Native scene shader](../engine/sdlgpu/shaders/scene.frag) and
  [Metal counterpart](../engine/sdlgpu/shaders/scene.frag.metal): RGB-only
  emissive addition and base cutout handling.
- [Archive mounting](../engine/common/fs.c): registered `pk3`/`pk3dir` formats
  and game-directory search paths.

See also [renderer controls and limitations](SDL3-GPU.md),
[FTE tools](Tools.md), and [the WadMaker documentation](https://github.com/pwitvoet/wadmaker/blob/master/wadmaker.md)
for its separate image-to-WAD workflow. Shared helpers under `engine/gl` do
not imply that this fork has an OpenGL rendering backend.
