# MIDI music

The desktop client can synthesize Standard MIDI music with FluidSynth 2.2+.
It renders stereo PCM into the SDL3 mixer, using the existing `musicvolume`,
`mastervolume`, fading, stopping and looping controls. FluidSynth does not open
a separate audio device or MIDI port. The server does not link FluidSynth.

## Build and package

Install FluidSynth development headers/libraries: `brew install fluid-synth`
on macOS, or `libfluidsynth-dev` on Debian/Ubuntu. On Windows, use a matching
development package or vcpkg's `fluidsynth` port and its CMake toolchain/prefix.
Shared-library builds require FluidSynth and its runtime dependencies on the
target system, including the appropriate DLLs on Windows.

`FTE_DEP_FLUIDSYNTH=AUTO` enables MIDI when available. `ON` requires it and
`OFF` excludes it. The old incomplete `FTE_PLUG_TIMIDITY` option was removed;
MIDI is an optional built-in decoder, with no plugin to load.

The selected default is FluidR3 GM 3.1 by Frank Wen and contributors, a separate
MIT-licensed asset of 148,398,306 bytes (about 142 MiB). Original `COPYING` and
`README` files and a source/checksum notice are preserved in
[`dist/soundfonts/FluidR3-GM`](../dist/soundfonts/FluidR3-GM).
The SF2 itself stays outside Git history. Normal configuration/builds never
download it. Fetch it explicitly or stage an existing unmodified copy:

```sh
python3 tools/package_soundfont.py --download --out build/soundfonts
# Offline alternative:
python3 tools/package_soundfont.py --source /path/to/FluidR3_GM.sf2 --out build/soundfonts

cmake -S . -B build -DFTE_DEP_FLUIDSYNTH=ON \
  -DFTE_MIDI_SOUNDFONT="$PWD/build/soundfonts/FluidR3_GM.sf2"
cmake --build build --target fteqw midi-tests --config Release --parallel
```

The helper and CMake verify the pinned SHA-256:
`74594e8f4250680adf590507a306655a299935343583256f3b722c48a1bc1cb0`.
With `FTE_MIDI_SOUNDFONT` set, client builds stage the font and all notices
under `soundfonts/` beside the executable, including configuration-specific
directories on Windows. Include that entire directory in portable releases.

The CMake `MIDI` installation component installs the font and notices under
`${CMAKE_INSTALL_DATADIR}/fteqw/soundfonts`. The configured absolute install
path is also checked for the default font. Set `CMAKE_INSTALL_PREFIX` at
configure time for system installations; portable layouts use the directory
beside the executable. Install the asset component with:

```sh
cmake --install build --component MIDI --config Release
```

Retain the MIT copyright/permission notice in every SoundFont distribution.
FluidSynth has its own LGPL-2.1-or-later license. When bundling its binaries,
preserve its notices and provide corresponding source as required by that
license, along with any bundled dependencies' notices and obligations.
See [FluidSynth](https://github.com/FluidSynth/fluidsynth) and the
[pinned FluidR3 packaging source](https://github.com/pianobooster/fluid-soundfont/tree/24c5c8a82e2f2cec2497710c6a72e5ccfeca40cd).
MIDI compositions and commercial game soundtracks are not included.

## Playback

Open **Options → Audio → MIDI SoundFont** to choose instruments. Both the
native and QuakeC audio menus open the same picker. It lists loose `.sf2` and
`.sf3` files directly inside `soundfonts/` beside the executable, in the active
game directories, in the home/base directory, and in the configured installation
directory. Uppercase extensions are accepted. SF3 playback requires a FluidSynth
build with SF3 support. Subdirectories and fonts packed inside PAK/PK3 files are
not listed.

The active font is marked with `*`. Select a file with the mouse, Enter or the
controller confirm button to save the choice and restart audio automatically.
Looping music restarts with the chosen instruments. Use **Rescan Folder** after
adding files while the picker is open; reopening the picker also rescans.
Long lists scroll, and hovering a file shows its full path. **Back** or Escape
returns to the audio menu without changing the selected font. Console selection
still works for fonts outside these directories.

Place tracks in the game's `music/` directory. Named and numbered tracks
recognize `.mid` and `.midi`, including `music/track02.mid`. The `midi/`
directory is also searched for Hexen II's original music. Existing recorded
replacements retain precedence within the same game search depth.

```text
music castle
music music/castle.mid
music castle -
music 2
stopmusic
```

The first two commands loop, the third plays once, and the fourth uses numbered
track lookup. Hexen II's MIDI-name protocol message now starts named music;
an empty name or `none` stops it. Owned Hexen II data is still required.
Named map music from the fork's server uses the same music path.

`snd_midi_soundfont` defaults to `soundfonts/FluidR3_GM.sf2`. It accepts a loose
file relative to the executable, game directory, or engine home/base directory,
in that order, or an absolute path. SoundFonts inside PAK/PK3 files are not
supported. The default also checks the configured installation path. A chosen
missing font does not silently fall back to another bank. Servers cannot set
this cvar. After changing it, use `snd_restart` to reload audio.

Supported inputs are Standard MIDI format 0/1 with PPQN timing, up to 16 MiB,
128 tracks, one million events and one hour per file. Format 2, SMPTE timing,
RIFF-wrapped MIDI and hardware MIDI input/output are outside this implementation.
A two-second release/reverb tail follows the last event before the playlist
advances or loops. Seeking renders intermediate samples to preserve controller,
tempo and envelope state; large seeks cost more. PCM buffering stays bounded.

## Verification

```sh
ctest --test-dir build -C Release -R '^midi-' --output-on-failure
python3 tools/midi_integration.py --engine build/fteqw --out build/midi-integration
python3 tools/midi_menu.py --engine build/fteqw --font build/soundfonts/FluidR3_GM.sf2 --out build/midi-picker
```

`midi-parser` requires no SoundFont or game data. `midi-synthesis` is registered
when `FTE_MIDI_SOUNDFONT` is set. It checks real FluidR3 output at 22.05, 44.1
and 48 kHz, tempo changes, sustain, EOF, rewind, seeking, overlapping reads and
missing fonts. Both use the production decoder and also run in sanitizer builds.

The engine check uses SDL's dummy audio driver, the headless renderer and
original synthetic MIDI files. It checks named/numeric lookup, looping,
one-shot playback, invalid files, missing fonts, sound restart and a 750-second
seek in a long track without
game data or a GPU. Pass `--soundfont /path/to/font.sf2` when no default is
packaged. On macOS, headless SDL tray initialization still requires a normal
desktop process context. These checks verify samples and mixer integration;
listening checks on each release platform remain useful.

On macOS/Linux, the picker check uses a temporary input plugin to exercise
discovery, sorting, uppercase extensions, adding files while open, refresh,
selection, audio/music reload, saved settings, empty folders and literal
filenames containing spaces and semicolons. It copies the executable and uses
isolated font links/copies. Add `--menusys build/menusys.pk3` for QuakeC menu
integration, `--many` for a long list, or `--gpu --game /path/to/Quake` for native
screenshots with owned menu artwork. No original font/game files are changed.
GPU captures require Pillow and compare the picker header across refreshed,
scrolled and empty states to check that list clipping leaves it intact.

Local validation on macOS passed for the full client, client-only executable,
dedicated server and MIDI-disabled client builds. Parser and FluidR3 synthesis
checks, and the complete SDL mixer fixture including the long seek, passed in
both normal Debug and ASan/UBSan builds. Asset staging/install checks preserved
the expected checksum and notices; incorrect assets were rejected. Desktop CI
is configured for Windows, Linux and macOS; those remote runs and playback with
owned Hexen II content have not been performed as part of this local check.

The shared SoundFont picker also passed normal and ASan/UBSan input checks,
including QuakeC-menu integration, refresh, saved selection and music reload.
Native Metal captures with 44 listed fonts and an empty folder were inspected;
the header image comparison passed in both normal and sanitizer builds.
