# Writing Your progs.dat

This guide takes you from an empty source folder to your own server game code
for **FTEQW Delta**, including a small playable test game. You do not need another
game's QuakeC source or a decompiled `progs.dat`. The engine supplies movement,
collision, networking, asset loading and rendering. You supply the rules:
spawning players, weapons, damage, enemies, objectives and interactions.

The tutorial targets FTEQCC's **FTE bytecode target** and the **NetQuake server
interface**. FTE can use other interfaces; choosing a server interface is separate
from choosing the network protocol. The reference below also covers QuakeWorld,
client-side and menu interfaces, with their scopes explicitly marked.

## Contents

- [What you are building](#what-you-are-building)
- [Anatomy of a QuakeC file](#anatomy-of-a-quakec-file)
- [Build your first game](#build-your-first-game)
- [Engine and game responsibilities](#engine-and-game-responsibilities)
- [Entities, interactions and damage](#entities-interactions-and-damage)
- [Sounds and music](#sounds-and-music)
- [Models, sprites and textures](#models-sprites-and-textures)
- [DEF and FGD editor definitions](#def-and-fgd-editor-definitions)
- [Extensions, clients and multiplayer](#extensions-clients-and-multiplayer)
- [Debugging, testing and shipping](#debugging-testing-and-shipping)
- [Complete engine API reference](#complete-engine-api-reference)

## What you are building

| File | Purpose | Loaded by |
| --- | --- | --- |
| `.qc` | Human-readable QuakeC source | FTEQCC |
| `progs.src` | Source list or include-based build entry point | FTEQCC |
| `progs.dat` | Compiled server-side QuakeC (SSQC); authoritative game rules | Server, including a local single-player server |
| `qwprogs.dat` | Conventional name for code using the QuakeWorld server interface | Server configured to load that interface |
| `csprogs.dat` | Optional client-side QuakeC (CSQC); custom HUD, view, effects and prediction | Each client |
| `menu.dat` | Optional custom menu QuakeC | Client menu VM |
| `.map` → `.bsp` | Editor map source → compiled world geometry and entity data | Map tools → engine |
| `.def` / `.fgd` | Entity descriptions, editable properties and previews | Map editor |

SSQC, CSQC and MenuQC run in **separate virtual machines**. An SSQC global is not
a CSQC global. A builtin available only in CSQC cannot be called from `progs.dat`.
Changes to `.qc` take effect after compilation and loading the new bytecode;
editing `.def` or `.fgd` does not change the running game.

Start with a small room and one player. Add one interaction, one weapon, one
enemy, then objectives. A total conversion also needs its own maps, art, sounds,
menu/HUD decisions and packaging. An engine API gives you tools to implement
these systems; it does not automatically supply Quake's game rules.

## Anatomy of a QuakeC file

QuakeC is case-sensitive. Declarations and statements end with `;`. Function
bodies use `{ ... }`, followed by `;` in traditional QuakeC syntax. `//` comments
end at the newline; `/* ... */` comments can span lines and do not nest.

```qc
// Fields belong to every entity; globals belong to this VM.
.float pulse_count;
var float pulses_total = 0;
const float PULSE_INTERVAL = 0.5;

// A declaration gives the compiler a signature before its use.
void() Pulse;

// Return type, argument list, function name, then the implementation.
void() Pulse =
{
    local float next;
    next = self.pulse_count + 1;
    self.pulse_count = next;
    pulses_total = pulses_total + 1;
    self.think = Pulse;
    self.nextthink = time + PULSE_INTERVAL;
};
```

An initialized global can be treated as a constant by FTEQCC. Use `var` for an
initialized value that will change, and `const` for one that must not change.
This guide uses explicit qualifiers to make the distinction clear. These are
compiler language features, not engine builtins.

| Type | Example | Meaning |
| --- | --- | --- |
| `void` | `void() Tick` | No return value |
| `float` | `local float damage;` | A 32-bit floating-point number; classic QC also uses it for counts and flags |
| `vector` | `local vector aim; aim = '1 0 0';` | Three floats, accessed as `aim_x`, `aim_y`, `aim_z` |
| `string` | `local string text; text = "Hello\n";` | VM string reference, not a C character array |
| `entity` | `local entity actor; actor = spawn();` | Reference to an entity in this VM; `world` is entity zero |
| `.float`, `.vector`, etc. | `.float shield; actor.shield = 25;` | Field identifier; the value is stored on each entity |
| Function type | `.void() think;` | A function reference; assigning it does not call it |
| `int`, pointers, structs, arrays | `local int index;` | FTEQCC/extended-bytecode features; check your target before using them |

Use `local` for function variables and initialize them before reading them.
Vectors in code use single quotes; strings use double quotes. A vector's
`*` operator with another vector is a **dot product**. Multiplying a vector by
a scalar scales it. To create a vector from runtime values, assign its three
components. `makevectors(angles)` writes the global `v_forward`, `v_right` and
`v_up`; another call overwrites them.

```qc
float(float a, float b) Larger =
{
    if (a > b)
        return a;
    return b;
};
```

Conditions use `if`, `else`, `while` and `for`. `==` compares and `=` assigns.
`&&`, `||` and `!` are logical operators. `&` and `|` operate on flag bits.
Classic QuakeC stores flags in floats; avoid inventing masks above the exact
integer range of that type. Use the declared integer types for APIs that
specifically require them. Function arguments are passed by value unless an
extended signature explicitly says otherwise. A function's `return` must match
its declared return type.

Builtins look like function declarations with engine bindings:

```qc
void(entity e, vector position) setorigin = #2;
```

`#2` means the engine implements the function. It is not QuakeC source you must
write. Extended `#0:name` bindings resolve by name. Copying a random builtin
number from another engine can call a completely different operation. Use the
generated declarations described next; do not invent numbers.

Preprocessor directives such as `#include`, `#define` and `#ifdef` act while
compiling. They do not check a connecting client's capabilities at runtime.
Put shared declarations before implementations. Declare functions before calls
that precede their definitions. Split larger projects into files such as
`player.qc`, `damage.qc`, `weapons.qc`, `monsters.qc` and `triggers.qc`.

## Build your first game

### 1. Create a game directory

For initial development alongside your legally installed Quake data:

```text
Quake/
  id1/                 Existing base data, kept separate
  mytc/
    src/
      engine_api.qc    Generated engine declarations
      progs.src
      main.qc
    maps/
      tc_intro.bsp     Your compiled test map
    progs.dat          Compiler output
    sound/
    music/
    progs/             Models and sprites; distinct from src/
    textures/
    scripts/
```

Loose files are easiest while developing. Lowercase names and forward slashes
make asset paths portable to case-sensitive filesystems. Paths in game code are
relative to the mounted game, never your machine's `/Users/...` directory.
Quake's `id1` can supply temporary UI/palette assets while you develop. This is
not yet a self-contained distributable total conversion.

### 2. Generate your declarations from this engine

Launch the client with your game directory selected:

```sh
fteqw -basedir "/path/to/Quake" -quake -game mytc
```

In its console:

```text
pr_sourcedir src
pr_dumpplatform -Ffte -Tnq -O engine_api
```

Read the `Written ...` message. The engine's active writable game directory can
depend on its manifest and home-directory settings; copy the resulting file
into `mytc/src/engine_api.qc` if it was written elsewhere. `-O` names the file
without its `.qc` extension. Generate on the **client executable**, since this
command is unavailable in the dedicated server build.

This file supplies the full system global/field layout, callback declarations,
constants, builtin signatures and descriptions. Do not reorder or selectively
delete its system declarations. FTEQCC checks their layout/CRC. Do not redeclare
the same fields elsewhere. Regenerate it when moving to a substantially changed
engine API, then rebuild and test your game.

For the QuakeWorld server interface use `-Tqw` instead of `-Tnq`; for a separate
client module use `-Tcs`, and for menu code use `-Tmenu`. Do not mix these layouts
in one compiled module. Omitting `-Ffte` does not make all exported extensions
portable to classic Quake; portability requires an appropriate target and a
deliberate subset of the API.

### 3. Write progs.src

Save this as `mytc/src/progs.src`:

```qc
#output "../progs.dat"
#include "engine_api.qc"
#include "main.qc"
```

Here `..` is relative to this source project. Compiler options can override the
output path. The older format also works: output filename on the first line,
then one source filename per line. Do not mix the two formats in one file.

### 4. Write main.qc

The following is a complete small SSQC implementation when included after the
generated NQ declarations. It needs **no models, sounds or borrowed game code**.
It lets a player walk around a map using engine movement. It intentionally has
no jump, weapons, enemies, fall damage, drowning, inventory or objectives yet.
Those are game rules to add, not compilation dependencies.

```qc
// main.qc -- an original, asset-free starting point.
// Editor/compiler-only map metadata, retained to avoid unknown-field messages.
.string wad;

/*QUAKED worldspawn (0.5 0.5 0.5) ?
The map world. "sounds" selects a music track; "wad" is the texture WAD list.
*/
/*FGD
@SolidClass = worldspawn : "Map world"
[
    message(string) : "Map title" : "My first game"
    sounds(integer) : "Music track" : 0
    wad(string) : "Texture WAD list" : ""
]
*/
void() worldspawn =
{
    local float i;
    // 'm' is ordinary brightness; initialize all style slots for this test game.
    for (i = 0; i < 64; i = i + 1)
        lightstyle(i, "m");
};

// Point entities retained so the game can find player starts.
/*QUAKED info_player_start (0 1 0) (-16 -16 -24) (16 16 32)
Player start. Place the origin 24 units above the floor. "angle" is facing yaw.
*/
/*FGD
@PointClass color(0 255 0) size(-16 -16 -24, 16 16 32)
= info_player_start : "Player start"
[
    angle(integer) : "Yaw" : 0
]
*/
void() info_player_start = {};
/*QUAKED info_player_deathmatch (1 0 0) (-16 -16 -24) (16 16 32)
Multiplayer start, reserved for a later spawn-selection policy.
*/
/*FGD
@PointClass color(255 0 0) size(-16 -16 -24, 16 16 32)
= info_player_deathmatch : "Multiplayer start"
[
    angle(integer) : "Yaw" : 0
]
*/
void() info_player_deathmatch = {};
// Static light entities have already been processed by the map's light tool.
/*QUAKED light (1 1 0.5) (-8 -8 -8) (8 8 8)
Static map light. "light" is the brightness consumed by the map light compiler.
*/
/*FGD
@PointClass color(255 255 128) size(-8 -8 -8, 8 8 8)
= light : "Static light"
[
    light(integer) : "Brightness" : 300
]
*/
void() light = {};

void() SetNewParms =
{
    parm1 = 100;
};

void() SetChangeParms =
{
    parm1 = self.health;
};

void() ClientConnect =
{
    sprint(self, "Welcome to your first game!\n");
};

void() PutClientInServer =
{
    local entity spot;
    spot = find(world, classname, "info_player_start");
    if (spot == world)
        error("Map needs an info_player_start\n");

    self.classname = "player";
    self.movetype = MOVETYPE_WALK;
    self.solid = SOLID_SLIDEBOX;
    self.flags = FL_CLIENT;
    self.health = 100;
    self.max_health = 100;
    self.takedamage = DAMAGE_YES;
    self.deadflag = 0;
    self.velocity = '0 0 0';
    self.view_ofs = '0 0 22';
    self.angles = spot.angles;
    self.fixangle = 1;
    setsize(self, '-16 -16 -24', '16 16 32');
    setorigin(self, spot.origin);
};

void() ClientDisconnect = {};
void() StartFrame = {};
void() PlayerPreThink = {};
void() PlayerPostThink = {};

void() ClientKill =
{
    // For this starter, the kill command simply restarts the player.
    PutClientInServer();
};

void() main = {};
```

`main` is a compatibility declaration; this engine does not use it as a C-style
program entry point. The engine calls the named callbacks. Empty callbacks are
deliberate here. `takedamage` does not implement a damage system by itself.
Likewise setting `health` does not automatically implement death or respawning.

`parm1` demonstrates map-transfer storage. To restore it later, call
`setspawnparms(self)` in `PutClientInServer`, validate `parm1`, then assign it to
`self.health`. The starter always resets health to 100. Do not read spawn
parameters as arbitrary persistent per-player globals outside that workflow.

### 5. Compile

From `mytc/src`:

```sh
fteqcc -srcfile progs.src -Tfte -O0 -Wall -Werror
```

In this repository, the compiler built with the complete CMake configuration is
`build/all/fteqcc` (`fteqcc.exe` on Windows). Use its absolute path if it is not on
your PATH. A successful compile produces `mytc/progs.dat`. Use `-O0` during
initial debugging; compare results before switching to `-O2`. See
[the FTEQCC manual](FTEQCC.md) for diagnostics, targets and optimization settings.

### 6. Make and run a tiny map

In a Quake-compatible editor, create a sealed room using six solid brushes: a
floor, ceiling and four walls. Give its interior plenty of clearance, for
example 1024 × 1024 units with the ceiling at Z=256 and floor top at Z=0.
Use textures from a WAD you have permission to use. Place `info_player_start` at
`0 0 24` with `angle` set to `0`, and a `light` near the ceiling. Save
`tc_intro.map`. The player box extends 24 units below its origin, so a start
inside the floor or outside the room will get stuck.

Build the map with a compatible QBSP, light and VIS toolchain, such as the
[ericw-tools map compilers](https://ericwa.github.io/ericw-tools/):

```sh
qbsp tc_intro.map
light tc_intro.bsp
vis tc_intro.bsp
```

Configure your WAD paths in the editor/toolchain. Fix leaks before VIS. Copy the
compiled BSP to `mytc/maps/tc_intro.bsp`, then run:

```sh
fteqw -basedir "/path/to/Quake" -quake -game mytc +map tc_intro
```

Use `developer 1` to see missing fields/spawn functions. A stock Quake map
contains monsters, doors and other classes absent from this starter; use your
small map first. The compiler builds bytecode, QBSP builds geometry, light
builds lighting, and VIS builds visibility. None substitutes for another.

## Engine and game responsibilities

| Engine provides | Your QuakeC implements |
| --- | --- |
| Entity storage and `.classname` spawn dispatch | A spawn function for each gameplay entity class |
| Collision and movement selected by `movetype`/`solid` | Jump/weapon decisions, AI goals and intended game mechanics |
| `think`, `touch`, `blocked` scheduling | What those callbacks do; when to reschedule them |
| Sound/model precaches and network messages | Which resources exist and when to play/show them |
| Player input fields and network transport | Damage, death, respawn, pickups, inventory, score and objectives |
| Renderer and optional CSQC APIs | Custom HUD/view/effects when you choose to implement CSQC |
| Save/load facilities | Save-compatible custom state and resource reconstruction |

The usual lifecycle is:

1. The engine loads `progs.dat` and parses the map entity text.
2. `worldspawn` runs for the world. Each map entity's `classname` selects a spawn
   function, such as `trigger_message`, with that entity in `self`.
3. Resource precaches are established during map initialization.
4. A joining player passes through `SetNewParms`, `ClientConnect` and
   `PutClientInServer`; do not assume `self` is valid in `SetNewParms`.
5. During simulation, `StartFrame`, player callbacks and entity callbacks run
   according to the engine's scheduling. Input processing/prediction can make
   player calls occur out of sequence with ordinary entity thinks.
6. Disconnect and map-transfer callbacks clean up or preserve relevant state.

`self` is context, not a permanent global "player" variable. In a `touch`
callback, `other` is the entity being touched. Store any context you need before
calling code that temporarily changes it, then restore it. The world entity is
the null/sentinel entity for many APIs: `find` returns `world` when nothing
matches. `remove` invalidates your logical ownership of an entity; do not keep
using that reference as though it were alive. Entity slots can be reused.

Change position with `setorigin`, size with `setsize`, model with `setmodel`.
These update collision links as well as fields. Assigning `.origin`, `.mins`
or `.solid` alone can leave stale collision information; relink after a solid
change. Set your intended bounding box **after** setting a non-BSP model.
Render mesh size is not automatically your gameplay collision shape.

## Entities, interactions and damage

A map's property values are strings. The engine converts recognized declared
fields according to their QC types. The special map key `angle` becomes yaw
in `.angles`; the map key `light` is mapped to `.light_lev`, allowing a spawn
function named `light` without a conflicting field declaration. Use `.vector`
for an `angles` key and `.string` for text/path keys.
Undeclared keys can be ignored or reported; they do not create fields for you.
Map-compiler keys beginning with `_` often affect compilation rather than QC.

Here is a brush trigger to add below `main.qc`. Its mapper-supplied inline model
(`*1`, `*2`, etc.) is already part of the loaded BSP.

```qc
.float message_after;

void() MessageTouch =
{
    if (!(other.flags & FL_CLIENT) || time < self.message_after)
        return;
    centerprint(other, self.message);
    self.message_after = time + 2;
};

/*QUAKED trigger_message (0.2 0.8 0.2) ?
Shows message to a touching player. Shared cooldown is two seconds.
"message" Text to display.
*/
/*FGD
@SolidClass color(51 204 51) = trigger_message : "Display a message"
[
    message(string) : "Text" : "Hello from your game!"
]
*/
void() trigger_message =
{
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_TRIGGER;
    setmodel(self, self.model);
    self.model = ""; // Keep collision modelindex, hide its rendering.
    setorigin(self, self.origin);
    self.touch = MessageTouch;
};
```

To use it, make a brush entity named `trigger_message` and give it a `message`.
The example shares one cooldown among all touching players. For independent
cooldowns, store per-player state instead. `use` is a function field for your
own interaction system: the engine does not automatically implement all
`target`/`targetname` gameplay wiring. Search matching targets and call their
`.use` yourself, with a documented activator convention.

An entity's `.think` runs when `.nextthink` becomes due. A repeating think must
set a new future `.nextthink`; assigning `.think` alone does not schedule it.
Use `time` for ordinary entities; push movers have a separate `.ltime` clock.
Multiply time-dependent rates by a suitable delta time, rather than assuming a
fixed number of frames per second.

For a simple damage policy, add these functions and a test target. This is an
original rules example, not the full Quake armor/team/death system:

```qc
void(entity victim, float amount) ApplyDamage =
{
    if (victim == world || victim.takedamage == DAMAGE_NO || amount <= 0)
        return;
    victim.health = victim.health - amount;
    if (victim.health > 0)
        return;
    if (victim.flags & FL_CLIENT)
    {
        // Temporary test policy: restart the dead player immediately.
        local entity saved_self;
        saved_self = self;
        self = victim;
        PutClientInServer();
        self = saved_self;
    }
    else
        remove(victim);
};

void() FireTestRay =
{
    local entity hit;
    local vector start;
    makevectors(self.v_angle);
    start = self.origin + self.view_ofs;
    traceline(start, start + v_forward * 2048, MOVE_NORMAL, self);
    hit = trace_ent; // Save results before another trace overwrites them.
    ApplyDamage(hit, 25);
};
```

Call `FireTestRay` from `PlayerPostThink` when `self.button0` is pressed, using a
custom `.float attack_after` field to limit it to, for example, one shot every
0.25 seconds. Trace results are shared globals; copy the ones you need before
calling another trace. For projectiles, spawn a moving entity, assign `.owner`,
`.touch` and a timed expiry; ignore its owner on impact. A server should decide
hits and damage. A visual client effect should not decide the outcome.

For AI, start with idle/chase/attack states scheduled through think callbacks.
The engine's trace and movement builtins help with collision, but choosing
targets, visibility tests, attack cooldowns, animation and health behavior
belongs to your game. Iterate entities with `find`/`nextent`; do not perform
unbounded whole-world searches every frame.

## Sounds and music

### Sound effects

Put a sound at `mytc/sound/tc/activate.wav`. Its QC path is **`tc/activate.wav`**,
without the `sound/` prefix. Precache it during `worldspawn` or an entity spawn
function before using it:

```qc
// Add during worldspawn:
precache_sound("tc/activate.wav");

// Call during an interaction with a valid emitting entity in self:
sound(self, CHAN_ITEM, "tc/activate.wav", 1, ATTN_NORM);
```

Volume is normally 0–1. Attenuation determines how quickly sound falls off with
distance. `ATTN_NONE` removes distance attenuation; it is not a mute setting.
Channel zero (`CHAN_AUTO`) allows overlapping instances. Nonzero channels on
the same entity replace the previous sample on that channel. Reserve channels
consistently for weapons, voice, items and body sounds. Do not start the same
loop every frame.

```qc
// Add during map initialization for a looping ambient source:
precache_sound("tc/machine.wav");
ambientsound(self.origin, "tc/machine.wav", 0.5, ATTN_STATIC);
```

Use a looping WAV exported with appropriate loop metadata, or another supported
looping setup; `ambientsound` establishes a static ambient event, not a moving
entity sound. FTE's extended `sound` parameters add pitch, flags and time offset;
they are optional and not part of the original five-argument Quake interface.
`ex_localsound` can send a sound to one player. The `localsound` builtin in SSQC
is deprecated because it bypasses networking: it may only reach the local
listen-server client. Use the network-aware operation for multiplayer.

| Format | Support in this fork's normal native build | Use |
| --- | --- | --- |
| `.wav` | Core loader | PCM WAV is the safest choice for short effects and loop markers; use ordinary integer PCM rather than assuming every WAV codec is supported |
| `.ogg` | Ogg **Vorbis**, through libvorbisfile | Compressed effects and music; an `.ogg` container carrying a different codec is not automatically Vorbis |
| `.mp3` | Conditional Windows ACM path / optional decoder plugin | Not a portable built-in decoder on macOS/Linux |
| `.flac`, `.opus`, other audio | Requires a suitable optional decoder/plugin or a differently configured build | A filename appearing in music search extensions does not guarantee a decoder is installed |

For a portable conversion in this fork, ship WAV effects and Ogg Vorbis music.
Verify the installed engine can open your actual files. Do not promise formats
solely because a platform media player can play them. The core sound input
registry is in `engine/client/snd_mem.c`; music searching is in
`engine/client/m_mp3.c`.

### Music

For conventional map music put `track02.ogg` in `mytc/music/`, and set the
worldspawn `sounds` map property to `2`. The engine uses worldspawn's `.sounds`
as the music track number. It searches both `track002` and `track02` forms;
`music/track02.ogg` is a straightforward convention. This is separate from
precache_sound: do not pass a music track to the spatial sound API just to get
background music.

For a named track, the FTE client console command is, for example:

```text
music music/tc_theme.ogg
```

With this server-side initialization policy you can request it for a joining
player:

```qc
// In PutClientInServer, after setting up the player:
stuffcmd(self, "music music/tc_theme.ogg\n");
```

This relies on the client's command support and server-command policy. A map
track is simpler for a broadly compatible server. Never construct `stuffcmd`
or `localcmd` commands by concatenating untrusted player/map text; quotes,
semicolons and newlines can change what is executed. Leave music volume under
the player's control (`bgmvolume`); do not repeatedly overwrite preferences.

## Models, sprites and textures

### Put a custom model in the world

Put `prop.mdl` at `mytc/progs/tc/prop.mdl`, then add this class:

```qc
/*QUAKED misc_tc_prop (0.5 0.5 1) (-16 -16 0) (16 16 32)
An immobile prop. Its gameplay collision box is 32 by 32 by 32 units.
*/
/*FGD
@PointClass color(128 128 255) size(-16 -16 0, 16 16 32)
= misc_tc_prop : "TC prop"
[
    angle(integer) : "Yaw" : 0
]
*/
void() misc_tc_prop =
{
    precache_model("progs/tc/prop.mdl");
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_BBOX;
    setmodel(self, "progs/tc/prop.mdl");
    setsize(self, '-16 -16 0', '16 16 32');
    setorigin(self, self.origin);
};
```

Use `SOLID_NOT` for a decorative object, `SOLID_TRIGGER` for an overlap-only
pickup, `SOLID_BBOX` for a blocking prop, and `SOLID_BSP` with `MOVETYPE_PUSH`
for appropriate brush movers. Visual triangles are not automatically solid.

The default configuration registers these model loaders. A loader's existence
does not mean this fork reproduces every material/shader feature of the game
that originally used that format. Test an exported asset and its animation on
the SDL GPU renderer before choosing your production art pipeline.

| Category | Formats | Notes |
| --- | --- | --- |
| Quake frame-animated mesh | `.mdl` | Straightforward classic workflow; embedded indexed skins and frame/framegroup animation |
| Other frame-animated meshes | `.md2`, `.md3`, Kingpin `.mdx` | Quake II, Quake III and Kingpin loaders; conventions and companion textures differ |
| Skeletal mesh | `.iqm` | Inter-Quake Model; a useful modern skeletal workflow with mesh and animations in one file |
| Other skeletal formats | `.md5mesh` with `.md5anim`, `.zym`, `.dpm`, `.psk` with animation data, Half-Life `.mdl` | Legacy/interchange loaders; format-specific animation and dependency requirements |
| Static mesh | `.obj` | Wavefront mesh; does not provide a character animation system |
| Scene/mesh interchange | `.gltf`, `.glb` | glTF 2 loader; package referenced buffers/textures and check supported material/compression features |
| Brush model | `.bsp`, map inline `*n` models | World/brush geometry; use map compilers, not a character-model exporter |
| Sprite model | `.spr`, `.sp2` | Quake sprite, including supported 32-bit variants, and Quake II sprite |

FTE also has composite animation and plugin/other-game loaders. Use the registry
in `engine/common/com_mesh.c`, `engine/gl/gl_model.c` and the glTF loader for the
precise format variants in your build. Those directory names contain shared
legacy infrastructure; this fork renders through SDL3 GPU.

### Remaster MD5 assets and skeletal physics

The Quake rerelease's `.md5mesh`/`.md5anim` pairs and indexed `.lmp` skins work
with the native renderer. With `mod_md5_singleanimation 1`, the matching animation
is expanded into poses that existing `.frame`/`.weaponframe` numbers can select.
Automatic replacements can keep the gamecode's `.mdl` paths; `r_replacemodels`
controls that preference. Load or reload a map after changing the preference.
For a mod that manages separate animations with skeleton APIs, the existing
`mod_md5_singleanimation 0` workflow remains available.

Animation bones and weights do not define ragdoll masses, bodies or constraints.
Those require a separate physics rig. The opt-in [Jolt plugin](JoltPhysics.md)
supports rigid props through `MOVETYPE_PHYSICS`, point/fixed/hinge/slider/
universal/hinge2 joints, and experimental `.doll` skeletal rigs through `skel_ragupdate`. Loading
a skeletal model does not create a physics rig. For server-side rigs, copy the
absolute simulated pose into a separate relative render skeleton with
`skel_copybones`; set the rendered entity's `.basebone = -1` to use the existing
bone networking. Local body offsets/rotations, orientation targets and joint pivot
offsets are supported. `cleardoll` releases a rig to resume authored animation; a
rejected `doll`/`dollstring` returns zero and retains the previous rig. See the Jolt
guide for the humanoid demo, `.doll` conventions and current limits. Hinge2
linear suspension uses the fork-only `joint_suspension_stiffness` and
`joint_suspension_damping` fields or `.doll` `SuspensionStiffness` /
`SuspensionDamping` directives. They require Jolt and do not affect ordinary
player physics. Engine-controlled pushers also carry/push Jolt props and skeletal
bodies; they retain engine movement and blocking rules for ordinary entities.
Jolt props do not trigger automatic door rollback or crush damage. See
[Quake Remaster content](QuakeRemaster.md) for launch
examples, tested assets, original-model fallback and compatibility limits.

### Frames, animation and the weapon viewmodel

For simple numbered-frame animation, set `.frame`, advance it from a think
function, and schedule the next think. Do not assume all formats number
animations like Quake's MDL. Framegroups can autoanimate in SSQC; CSQC has
different interpolation responsibilities. The extended APIs include named
frame lookup, animation durations and skeleton operations. Check the model's
actual exported names and indices before referencing them in code.

For a first-person model, precache it during initialization, then assign the
player's `.weaponmodel` to its path and `.weaponframe` to a valid frame:

```qc
// During worldspawn:
precache_model("progs/tc/v_tool.mdl");
// During player setup:
self.weaponmodel = "progs/tc/v_tool.mdl";
self.weaponframe = 0;
```

The player's third-person `.model` is separate. A conversion needs a model for
other players to see even if it already has a first-person weapon. Assign both
deliberately and preserve your collision box after setmodel.

### Sprites are supported

A sprite is a model containing flat frames that usually face the viewer.
Create/export a real `.spr` asset, put it in `mytc/progs/tc/glow.spr`, and use it
with `precache_model` and `setmodel` just like a mesh. For a decorative glow:

```qc
// In the sprite entity's spawn function:
precache_model("progs/tc/glow.spr");
self.movetype = MOVETYPE_NONE;
self.solid = SOLID_NOT;
setmodel(self, "progs/tc/glow.spr");
setsize(self, '0 0 0', '0 0 0');
setorigin(self, self.origin);
self.frame = 0;
```

A loose PNG is not a Quake `.spr` model. A `.sp2` references separate image
frames that must also be shipped. Sprite orientation, palette transparency or
RGBA alpha come from its format/export settings; they are not interchangeable.
Test edges and fading over both light and dark geometry. For HUD images, use
CSQC image-drawing builtins instead of spawning world sprites.

### Texture and material files

PNG and TGA are convenient lossless diffuse/alpha formats; JPEG is appropriate
for opaque images when the linked JPEG decoder is present. Classic indexed
`.lmp`/`.pcx` and BSP/WAD textures are also supported. The configured image
loaders additionally cover DDS, KTX, PKM, ASTC, PVR, BMP/ICO, PSD, XCF, the
PBM/PGM/PPM/PFM family, Radiance HDR and EXR (EXR needs its library). These have
variant, compression and GPU-format restrictions; converting to PNG/TGA is the
least surprising starting point. Listing a loader does not promise every codec
or container variant. A WAD is a texture archive, not a direct replacement image.

For this fork's model texture replacement convention, an MDL such as
`progs/tc/prop.mdl` can use `textures/models/prop_0.png` or `.tga` for skin zero.
The direct path `progs/tc/prop.mdl_0.tga` is also supported and has priority.
Use unique basenames to avoid collisions. Enable `gl_load24bit 1` and reload
assets/restart video when testing changes. Despite its inherited name, this
setting also controls external texture loading on SDL3 GPU. Other formats can
name their materials directly, so consult the model export instead of assuming
the MDL skin-zero convention applies to all formats.

Normal/specular/offset maps and shader scripts are optional presentation
features. Provide a usable diffuse fallback so gameplay remains readable on
the faithful preset and when normal mapping or real-time lights are disabled.
Do not base hit detection, puzzle solutions or visibility-critical warnings
solely on a renderer enhancement that players can turn off.

## DEF and FGD editor definitions

DEF and FGD describe entities to a **map editor**, including their names,
properties, display colors, boxes and spawnflags. They do not contain compiled
gameplay, spawn functions or runtime collision instructions. They are also
different from QC engine declarations such as `engine_api.qc` or the compiler's
C `progdefs.h` output. DEF means an entity definition file; FGD is the Valve
Forge Game Data format. [TrenchBroom supports both](https://trenchbroom.github.io/manual/latest/index.html#entity_definitions).

| DEF (QUAKED format) | FGD |
| --- | --- |
| Traditional Quake/Radiant text blocks | Structured class/property declarations |
| `/*QUAKED classname (r g b) (mins) (maxs) FLAGS ... */` for a point class | `@PointClass` with `size(...)`, properties and descriptions |
| `?` replaces the two boxes for a brush class | `@SolidClass` for a brush class |
| Named spawnflag positions and free-form property documentation | Explicit types, defaults, choices, flags and shared base classes |
| Colors conventionally use 0–1 components | `color(...)` conventionally uses 0–255 components |

In a QUAKED header, each flag position is a bit: first=1, second=2, third=4, etc.
Use placeholders such as `-` for unused positions so later flags keep their
bit numbers. Quake's map spawn-filter bits 256/512/1024/2048 are reserved for
skill/deathmatch filtering; avoid casually reusing them as custom entity flags.
FGD's `spawnflags(flags)` lists the actual bit values explicitly. The QC code
must test the same values (`self.spawnflags & 1`, for example). An editor
default is usually written to a new entity; it does not initialize every entity
at runtime. Supply sensible QC defaults when a mapper omits a key.

### Keep editor descriptions beside entity code

The trigger and prop above have both `/*QUAKED ... */` and `/*FGD ... */`
comments. The first is an actual DEF block. The second is a convention used here
to contain an actual FGD declaration; it is **not a special compiler directive**.
Since comments are ignored by QC compilation, the same file can hold gameplay
and its editor metadata. This also avoids a lossy automatic DEF→FGD conversion:
free-form DEF prose cannot reliably supply typed FGD defaults or choices.

Here is a small extractor. Save as `make_entitydefs.py` in your game source
directory, then run `python3 make_entitydefs.py`. It reads all `.qc` files under
that directory recursively and writes `../mytc.def` and `../mytc.fgd`.

```python
from pathlib import Path
import re

source = Path(__file__).resolve().parent
defs, fgds = [], []
for path in sorted(source.rglob("*.qc")):
    text = path.read_text(encoding="utf-8")
    for match in re.finditer(r"/\*QUAKED\b.*?\*/", text, re.S):
        defs.append(match.group(0))
    for match in re.finditer(r"/\*FGD\b(.*?)\*/", text, re.S):
        fgds.append(match.group(1).strip())
if not defs or not fgds:
    raise SystemExit("No QUAKED/FGD definitions found; check your source comments")
(source.parent / "mytc.def").write_text("\n\n".join(defs) + "\n", encoding="utf-8")
(source.parent / "mytc.fgd").write_text("\n\n".join(fgds) + "\n", encoding="utf-8")
print(f"Wrote {len(defs)} DEF blocks and {len(fgds)} FGD blocks")
```

This is an intentionally simple **comment extractor**, not a QC parser or FGD
validator. It includes comments in conditional code too, does not resolve
includes, and assumes these marker strings occur only in real comments. Keep
editor-facing definitions unique and use separate source roots for mutually
exclusive entity sets. Check the output in your editor; a successful QC compile
cannot catch malformed FGD syntax.

The starter already includes definitions for `worldspawn`, `info_player_start`,
`info_player_deathmatch` and `light`; the two additional examples bring this to
six classes. For a conversion, document every entity you ship. You can put shared FGD
`@BaseClass` definitions in an `/*FGD ... */` block and inherit them with
`base(...)`. An editor preview model path does not precache or set a runtime
model: your spawn function still has to do that.

Select the resulting definition file in TrenchBroom's map/entity settings and
add `mytc` as a mod asset directory. Other editors have their own file-selection
workflow. Compare the editor's property names with declared QC fields. Then
place the entity, compile the map and test the actual interaction in the engine.
Treat editor descriptions, spawnflag values and gameplay as one maintained
contract. See the [official editor manual](https://trenchbroom.github.io/manual/latest/index.html)
for its current definition syntax and UI.

## Extensions, clients and multiplayer

FTE's generated header advertises compile-time feature macros, but a
`#define FTE_SOMETHING` in your source does not enable that capability on another
engine. Query runtime extension support with `checkextension`, and individual
name-bound operations with `checkbuiltin` when that checking API is available:

```qc
if (checkbuiltin(ex_localsound))
    ex_localsound(self, "tc/activate.wav");
else
    sound(self, CHAN_ITEM, "tc/activate.wav", 1, ATTN_NORM);
```

This example targets FTE, where `checkbuiltin` exists. For an engine without
that builtin, calling it is itself unsupported; use a separate portable build
or an established bootstrap check. For `checkextension`, older compatible
engines use `cvar("pr_checkextension")` to indicate the checking API is present.
Check the server capability separately from a remote client's rendering or
protocol support.

**Compatibility labels in the reference:**

- **FTE extension** identifies an interface in an FTE-named extension group or
  an explicitly FTE-specific facility. These APIs go beyond original Quake;
  some have been adopted by other engines. It does not mean no other engine
  can ever implement that interface. Detect the feature, not the engine name.
- **Shared extension** identifies named DP, ZQ, EXT, FRIK and other extension
  interfaces exported here. A DP prefix alone is not a promise of identical
  behavior on DarkPlaces; read the operation's notes.
- **Unnumbered/FTE binding** means the declaration uses `#0:name` binding and
  requires a supporting compiler/VM. It is not proof of global exclusivity.
- **Engine interface** means a field/global/callback/constant or builtin with
  no extension attribution in the exporter. Some are classic and some are
  later additions; this label is not a vanilla compatibility guarantee.
- **Fork-only (FTEQW Delta)** identifies symbols introduced by this fork:
  `joint_suspension_stiffness` and `joint_suspension_damping` currently control
  Jolt hinge2 suspension. Their declarations do not imply that a physics plugin
  is loaded; use `physics_supported()` and explicitly select the Jolt backend.

Original Quake's useful core includes `self`, `other`, `world`, `time`,
`frametime`, entity position/size/model fields, the standard player callbacks,
`makevectors`, `setorigin`, `setmodel`, `setsize`, `spawn`, `remove`, `find`,
`traceline`, `precache_sound`, `precache_model`, `sound`, `lightstyle`,
`cvar` and print/network-writing builtins. Modern optional parameters, extended
signatures and high-number/name-bound operations still need capability checks.
An FTE-specific bytecode target is a separate portability constraint even when
your source uses only classic operations.

For a full conversion, add CSQC when you need a new HUD, custom camera or rich
client effects. Compile it with its own generated `-Tcs` header and output
`csprogs.dat`. SSQC communicates through custom stats, entity send callbacks or
explicit messages; CSQC must read exactly the same layout that SSQC writes.
Keep field order, integer widths and versions explicit. Do not use a server
entity reference directly as a client entity reference. A listen server must
behave correctly with two remote players too.

Keep the server authoritative for inventory, scoring and damage. Validate client
commands, indices, counts and ownership before acting. Rate-limit expensive
requests and bound loops/allocations. Prediction needs consistent client/server
movement rules: do not adopt `SV_PlayerPhysics` as an easy movement override,
because the engine explicitly warns it breaks prediction. The advanced
`SV_RunClientCommand`/`runstandardplayerphysics` path is a separate project to
undertake after the basic game works.

## Debugging, testing and shipping

Start with `developer 1`, meaningful `dprint` messages and FTEQCC warnings.
Use `error` for an unrecoverable game error and `objerror` for an invalid map
entity (the latter removes `self`). A compile warning about a misspelled or
uninitialized variable is often easier to fix than a later runtime symptom.
See the compiler's `--help=warnings`, `--help=targets` and `--manual` catalogs.

Temporary strings from builtins have lifetime rules. Use `strzone`/`strunzone`
or the documented persistent storage API when retaining a returned string
beyond its guaranteed lifetime. Close file/search/string-buffer/skeleton/JSON
handles using their matching cleanup operation. Keep enough metadata to rebuild
resources on map reload and save restore. Do not serialize raw process pointers.

Test in small stages:

1. Load a fresh map, walk, reconnect, restart it and change maps.
2. Test missing assets/keys, two interacting entities and blocked movers.
3. Test save/load and transferred player state before committing to a save format.
4. Test a dedicated server and at least two clients; check late joins and disconnects.
5. Test the faithful preset and optional graphics switches as well as enhanced presets.
6. Test the actual release build on each intended platform with your final assets.

Savegames can refer to fields and functions by name. Aggressive symbol stripping,
field changes and reflected/addon APIs need care when you change optimization
settings or ship updates. Give network/save schemas explicit versions when you
introduce custom data. Compiler optimization does not fix a per-frame whole-map
search or an endlessly growing entity list.

An independently distributed conversion must package its own necessary game/UI
assets, palette choices, maps and startup/game configuration. Check it from an
empty installation that has no `id1` or other mod assets to accidentally satisfy
dependencies. Include your entity definitions for mappers, credits, source/build
instructions and the licenses applicable to your code and assets. Do not bundle
Quake's commercial PAKs. PAK/PK3 are packaging options; keep source and generated
bytecode ownership clear and test the archive's internal paths.

## Complete engine API reference

This is a snapshot of the API advertised by this fork's native client build,
generated on 2026-10-19 from `pr_dumpplatform`. It enumerates active, exported
builtins, callbacks, globals, entity fields, constants, type aliases, convenience
macros and extension labels for **NQ SSQC**, **QW SSQC**, **CSQC** and **MenuQC**.
Obsolete entries that the engine deliberately omits are not supported API here.
Hexen II-specific alternate system layouts and optional plugin-added APIs are
outside this Quake `progs.dat` reference. Disabled build features will not appear
or may fail runtime capability checks. The API is broader than SDL GPU renderer
features: an advertised builtin can require an optional service or plugin.

Each item includes its exact declaration, where it is available and an
explanation. Different signatures in different VMs get separate entries. For
builtins, `#n` is the binding number, not a parameter. `optional` arguments may
be omitted; `...` is a variable argument list. Fields are declarations such as
`.float alpha`, accessed as `entity.alpha`. Callbacks have no `#` binding because
you provide their implementation. Constant declarations show their value.
Macros and types are source conveniences, not mutable engine state.

Game constants such as weapon inventory bits, death-state values and custom
spawnflag masks are yours to define; their names and values are not universally
provided by the engine. Similarly, console variables are accessed through the
cvar APIs, not automatically imported as every possible QC global. Compiler
keywords/intrinsics are a language reference topic rather than engine builtins.

Descriptions come from this engine's exporter where available, supplemented
with explanations for otherwise undocumented basic entries. Source authority:
`engine/server/pr_cmds.c` (`BuiltinList`, `QSG_Extensions`, `PR_DumpPlatform_f`),
`engine/server/progdefs.h` (shared fields), plus client/menu builtin tables.
Inherited compatibility warnings in those descriptions matter even in FTE.

To refresh the reference, export four headers into the same folder:

```text
pr_sourcedir src
pr_dumpplatform -Ffte -Tnq -O nq_api
pr_dumpplatform -Ffte -Tqw -O qw_api
pr_dumpplatform -Ffte -Tcs -O cs_api
pr_dumpplatform -Ffte -Tmenu -O menu_api
```

From the repository root, run:

```sh
python3 tools/build_progs_reference.py --api-dir /path/to/exported/src
python3 tools/build_progs_reference.py --api-dir /path/to/exported/src --check
```

This refreshes only the marked reference section, preserving the tutorial.
Review changes against the engine implementation; a generated signature cannot
prove that an optional renderer, decoder or service is available. Search this
page for a symbol's exact name instead of guessing a builtin number.

<!-- BEGIN GENERATED QUAKEC API -->

This snapshot lists **2014 distinct names** in **2128 declarations** across the four VM interfaces.

<!-- Export SHA-256 hashes for reproducibility:
nq_api.qc: 74054552df28908f3e6ffcd355165af0c68993e73d245c9a5ac342fa047fe669
qw_api.qc: 68de434b1b869b3415755faae2fb352343b3a7ce468192674247fe6b918f189e
cs_api.qc: a11e87fc3ac07a64293e1f8661a26227aa7c65c8d843f1878ade6c15b341f910
menu_api.qc: 2fd5688af04df36592db21dabe940cbbe89e2094463fdc32d8350d8b9dce7dea
-->

### Engine functions (builtins)

710 declarations.

#### `abort`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(optional __variant ret) abort = #211;
```

Part of FTE_MULTITHREADED QC execution is aborted. Parent QC functions on the stack will be skipped, effectively this forces all QC functions to 'return ret' until execution returns to the engine. If ret is ommited, it is assumed to be 0.

#### `acos`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(float c) acos = #472;
```

Part of DP_QC_ASINACOSATANATAN2TAN. Computes an inverse trigonometric function, returning radians; respect its input domain.

#### `adddecal`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(string shadername, vector origin, vector up, vector side, vector rgb, float alpha) adddecal = #375;
```

Adds a temporary clipped decal shader to the scene, centered at the given point with given orientation. Will be drawn by the next renderscene call, and freed by the next clearscene call.

#### `addentities`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float mask) addentities = #301;
```

Walks through all entities effectively doing this: if (ent.drawmask&mask){ if (!ent.predaw()) addentity(ent); } If mask&MASK_DELTA, non-csqc entities, particles, and related effects will also be added to the rentity list. If mask&MASK_STDVIEWMODEL then the default view model will also be added.

#### `addentity`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(entity ent) addentity = #302;
```

Copies the entity fields into a new rentity for later rendering via addscene.

#### `addentity_lighting`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(entity ent, vector lightdir, vector lightavg, vector lightrange, int reserved1=0,void*reserved2=0) addentity_lighting = #0:addentity_lighting;
```

Copies the entity fields into a new rentity for later rendering via addscene, but with explicit lighting info.

#### `addprogs`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(string progsname) addprogs = #202;
```

Part of FTE_MULTIPROGS Loads an additional .dat file into the current qcvm. The returned handle can be used with any of the externcall/externset/externvalue builtins. There are cvars that allow progs to be loaded automatically.

#### `addtrisoup_simple`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(string texturename, int flags, struct trisoup_simple_vert_s *verts, int *indexes, int numindexes) addtrisoup_simple = #0:addtrisoup_simple;
```

Adds the specified trisoup into the scene as additional geometry. This permits caching geometry to reduce builtin spam. Indexes are a triangle list (so eg quads will need 6 indicies to form two triangles). NOTE: this is not going to be a speedup over polygons if you're still generating lots of new data every frame.

#### `addwantedhostcachekey`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(string key) addwantedhostcachekey = #623;
```

Part of FTE_CSQC_SERVERBROWSER Asks the engine to track a custom serverinfo key.

#### `aim`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
vector(entity player, float missilespeed) aim = #44;
```

Returns a tweaked copy of the v_forward vector (must be set! ie: makevectors(player.v_angle) ). This is important for keyboard users (that don't want to have to look up/down the whole time), as well as joystick users (who's aim is otherwise annoyingly imprecise). Only the upwards component of the result will differ from the value of v_forward. The builtin will select the enemy closest to the crosshair within the angle of acos(sv_aim).

#### `altstr_count`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
DEP float(string str) altstr_count = #82;
```

Reports how many single-quotes there were in the string, divided by 2.

#### `altstr_get`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
DEP string(string str, float num) altstr_get = #84;
```

Gets the Nth single-quoted token in the input.

#### `altstr_prepare`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
DEP string(string str) altstr_prepare = #83;
```

Adds markup to escape only single-quotes. Does not add any.

#### `altstr_set`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
DEP string(string str, float num, string setval) altstr_set = #85;
```

Changes the Nth single-quoted token. The setval argument must not contain any single-quotes (use altstr_prepare to ensure this).

#### `ambientsound`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void (vector pos, string samp, float vol, float atten) ambientsound = #74;
```

Plays a sound at the specified position when clients first connect. FTE will force the sound to loop if it lacks wav cue stuff (unlike vanilla quake). These sounds cannot be stopped/replaced later.

#### `anglemod`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(float value) anglemod = #102;
```

Wraps an angle into the engine's 0-to-360-degree representation.

#### `anglesub`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(float newangle, float oldangle) anglesub = #0:anglesub;
```

Returns newangle-oldangle, except returning the shortest route around a circle so yields a result between -180 and +180.

#### `applycustomskin`

**Scope:** CSQC. **Compatibility:** FTE extension.

```qc
void(entity e, float skinobj) applycustomskin = #378;
```

Part of FTE_QC_CUSTOMSKINS Updates the entity's custom skin (refcounted).

#### `argescape`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
string(string s) argescape = #295;
```

Marks up a string so that it can be reliably tokenized as a single argument later.

#### `argv`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
string(float n) argv = #442;
```

Part of KRIMZON_SV_PARSECLIENTCOMMAND. Part of KRIMZON_SV_PARSECLIENTCOMMAND Returns one of the tokens found via tokenize (and equivelent builtins).

#### `argv`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
string(float) argv = #59;
```

Part of KRIMZON_SV_PARSECLIENTCOMMAND Returns one of the tokens found via tokenize (and equivelent builtins).

#### `argv_end_index`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(float idx) argv_end_index = #516;
```

Returns the character index that the tokenized arg stopped at.

#### `argv_start_index`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(float idx) argv_start_index = #515;
```

Returns the character index that the tokenized arg started at.

#### `asin`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(float s) asin = #471;
```

Part of DP_QC_ASINACOSATANATAN2TAN. Computes an inverse trigonometric function, returning radians; respect its input domain.

#### `atan`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(float t) atan = #473;
```

Part of DP_QC_ASINACOSATANATAN2TAN. Computes an inverse trigonometric function, returning radians; respect its input domain.

#### `atan2`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(float c, float s) atan2 = #474;
```

Part of DP_QC_ASINACOSATANATAN2TAN. Computes the quadrant-aware arctangent of a pair of components, returning radians; follow the exported argument order.

#### `base64decode`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
__variant*(string base64str, __out int bytes) base64decode = #0:base64decode;
```

Decodes a base64, returning a new block of memory that can must be freed with memfree.

#### `base64encode`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(__variant *ptr, int bytes, optional int offset) base64encode = #0:base64encode;
```

Returns a copy of a binary blob encoded as a base64 temp-string (uses + and /, so be sure to include quotes).

#### `bitshift`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(float number, float quantity) bitshift = #218;
```

Part of EXT_BITSHIFT. Shifts bits left for positive quantity and right for negative quantity; uses the extension's integer conversion rules.

#### `bound`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(float min,float value,float max) bound = #45;
```

Part of DP_QC_MINMAXBOUND. Clamps value between min and max.

#### `bound`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(float minimum, float val, float maximum) bound = #96;
```

Part of DP_QC_MINMAXBOUND Returns val, unless minimum is higher, or maximum is less.

#### `boxparticles`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float effectindex, entity own, vector org_from, vector org_to, vector dir_from, vector dir_to, float countmultiplier, optional float flags) boxparticles = #502;
```

Emits the selected particle effect over the supplied origin/direction box; flags and countmultiplier control the effect.

#### `bprint`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
DEP void(string text,...) bprint = #5;
```

Prints to all players. NQ and QW use different signatures; QW adds a priority argument. Menu version is deprecated.

#### `bprint`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
void(float msglvl, string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7) bprint = #23;
```

QW: Concatenates all string arguments, and prints the messsage on the console of only all clients who's 'msg' infokey is set lower or equal to the supplied 'msglvl' argument.

#### `bprint`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
void(string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7, optional string s8) bprint = #23;
```

NQ: Concatenates all arguments, and prints the messsage on the console of all connected clients.

#### `breakpoint`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void() breakpoint = #6;
```

Trigger a debugging event. FTE will break into the qc debugger. Other engines may crash with a debug execption.

#### `brush_calcfacepoints`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
int(int faceid, brushface_t *in_faces, int numfaces, vector *points, int maxpoints) brush_calcfacepoints = #0:brush_calcfacepoints;
```

Part of FTE_RAW_MAP Determines the points of the specified face, if the specified brush were to actually be created.

#### `brush_create`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
int(float modelidx, brushface_t *in_faces, int numfaces, int contents, optional int brushid) brush_create = #0:brush_create;
```

Part of FTE_RAW_MAP Inserts a new brush into the model. Return value is the new brush's id.

#### `brush_delete`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(float modelidx, int brushid) brush_delete = #0:brush_delete;
```

Part of FTE_RAW_MAP Destroys the specified brush.

#### `brush_findinvolume`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
int(float modelid, vector *planes, float *dists, int numplanes, int *out_brushes, int *out_faces, int maxresults) brush_findinvolume = #0:brush_findinvolume;
```

Part of FTE_RAW_MAP Allows you to easily obtain a list of brushes+faces within the given bounding region. If out_faces is not null, the same brush might be listed twice.

#### `brush_get`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
int(float modelidx, int brushid, brushface_t *out_faces, int maxfaces, int *out_contents) brush_get = #0:brush_get;
```

Part of FTE_RAW_MAP Queries a brush's information. You must pre-allocate the face array for the builtin to write to. Return value is the number of faces retrieved, 0 on error.

#### `brush_getfacepoints`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
int(float modelid, int brushid, int faceid, vector *points, int maxpoints) brush_getfacepoints = #0:brush_getfacepoints;
```

Part of FTE_RAW_MAP Returns the list of verticies surrounding the given face. If face is 0, returns the center of the brush (if space for 1 point) or the mins+maxs (if space for 2 points).

#### `brush_selected`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
float(float modelid, int brushid, int faceid, float selectedstate) brush_selected = #0:brush_selected;
```

Part of FTE_RAW_MAP Allows you to easily set transient visual properties of a brush. returns old value. selectedstate=-1 changes nothing (called for its return value).

#### `buf_copy`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
void(strbuf bufhandle_from, float bufhandle_to) buf_copy = #443;
```

Part of DP_QC_STRINGBUFFERS. Copies one string buffer's contents to another.

#### `buf_copy`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(strbuf bufhandle_from, strbuf bufhandle_to) buf_copy = #463;
```

Part of DP_QC_STRINGBUFFERS. Copies one string buffer's contents to another.

#### `buf_create`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
strbuf() buf_create = #440;
```

Part of DP_QC_STRINGBUFFERS. Allocates a string-buffer handle.

#### `buf_create`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
strbuf() buf_create = #460;
```

Part of DP_QC_STRINGBUFFERS. Allocates a string-buffer handle.

#### `buf_cvarlist`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(strbuf strbuf, string pattern, string antipattern) buf_cvarlist = #517;
```

Fills a string buffer with console-variable names matching pattern and excluding antipattern.

#### `buf_del`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
void(strbuf bufhandle) buf_del = #441;
```

Part of DP_QC_STRINGBUFFERS. Frees a string buffer and its contents.

#### `buf_del`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(strbuf bufhandle) buf_del = #461;
```

Part of DP_QC_STRINGBUFFERS. Frees a string buffer and its contents.

#### `buf_getsize`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(strbuf bufhandle) buf_getsize = #442;
```

Part of DP_QC_STRINGBUFFERS. Returns the string buffer's index extent/size; sparse indices may include empty entries.

#### `buf_getsize`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(strbuf bufhandle) buf_getsize = #462;
```

Part of DP_QC_STRINGBUFFERS. Returns the string buffer's index extent/size; sparse indices may include empty entries.

#### `buf_implode`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
string(strbuf bufhandle, string glue) buf_implode = #445;
```

Part of DP_QC_STRINGBUFFERS. Joins the buffer's strings using glue and returns the resulting string.

#### `buf_implode`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
string(strbuf bufhandle, string glue) buf_implode = #465;
```

Part of DP_QC_STRINGBUFFERS. Joins the buffer's strings using glue and returns the resulting string.

#### `buf_loadfile`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(string filename, strbuf bufhandle) buf_loadfile = #535;
```

Appends the named file into a string buffer (which must have been created in advance). The return value merely says whether the file was readable.

#### `buf_sort`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
void(strbuf bufhandle, float sortprefixlen, float backward) buf_sort = #444;
```

Part of DP_QC_STRINGBUFFERS. Sorts buffer strings, with prefix length and reverse-order controls.

#### `buf_sort`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(strbuf bufhandle, float sortprefixlen, float backward) buf_sort = #464;
```

Part of DP_QC_STRINGBUFFERS. Sorts buffer strings, with prefix length and reverse-order controls.

#### `buf_writefile`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(filestream filehandle, strbuf bufhandle, optional float startpos, optional float numstrings) buf_writefile = #536;
```

Writes the contents of a string buffer onto the end of the supplied filehandle (you must have already used fopen). Additional optional arguments permit you to constrain the writes to a subsection of the stringbuffer.

#### `bufstr_add`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(strbuf bufhandle, string str, float ordered) bufstr_add = #448;
```

Part of DP_QC_STRINGBUFFERS. Adds a string to a buffer, with ordered placement control; returns its index.

#### `bufstr_add`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(strbuf bufhandle, string str, float ordered) bufstr_add = #468;
```

Part of DP_QC_STRINGBUFFERS. Adds a string to a buffer, with ordered placement control; returns its index.

#### `bufstr_find`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(float bufhandle, string match, float matchrule, float startpos, float step) bufstr_find = #537;
```

Looks for the first occurence of the specified string in the buffer, returning its index or -1 on failure.

#### `bufstr_free`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
void(strbuf bufhandle, float string_index) bufstr_free = #449;
```

Part of DP_QC_STRINGBUFFERS. Releases the string at the specified buffer index.

#### `bufstr_free`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(strbuf bufhandle, float string_index) bufstr_free = #469;
```

Part of DP_QC_STRINGBUFFERS. Releases the string at the specified buffer index.

#### `bufstr_get`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
string(strbuf bufhandle, float string_index) bufstr_get = #446;
```

Part of DP_QC_STRINGBUFFERS. Retrieves a string at a buffer index.

#### `bufstr_get`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
string(strbuf bufhandle, float string_index) bufstr_get = #466;
```

Part of DP_QC_STRINGBUFFERS. Retrieves a string at a buffer index.

#### `bufstr_set`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
void(strbuf bufhandle, float string_index, string str) bufstr_set = #447;
```

Part of DP_QC_STRINGBUFFERS. Stores a string at a buffer index.

#### `bufstr_set`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(strbuf bufhandle, float string_index, string str) bufstr_set = #467;
```

Part of DP_QC_STRINGBUFFERS. Stores a string at a buffer index.

#### `builtin_find`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float(string builtinname) builtin_find = #100;
```

Looks to see if the named builtin is valid, and returns the builtin number it exists at.

#### `callfunction`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(... ) callfunction = #605;
```

Invokes the named function. The function name is always passed as the last parameter and must always be present. The others are passed to the named function as-is

#### `calltimeofday`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
__deprecated("Use strftime.") void() calltimeofday = #231;
```

Part of FTE_CALLTIMEOFDAY Asks the engine to instantly call the qc's 'timeofday' function, before returning. For compatibility with mvdsv. timeofday should have the prototype: void(float secs, float mins, float hour, float day, float mon, float year, string strvalue) The strftime builtin is more versatile and less weird.

#### `ceil`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(float) ceil = #36;
```

Rounds upward toward positive infinity.

#### `ceil`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(float) ceil = #38;
```

Rounds the given float upwards, even when negative.

#### `centerprint`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(entity ent, string text, optional string text2, optional string text3, optional string text4, optional string text5, optional string text6, optional string text7) centerprint = #73;
```

Sends centered text to the specified player's view.

#### `changelevel`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string map) changelevel = #64;
```

Not really any different from a localcmd, but with proper string escapes.

#### `changelevel`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(string mapname, optional string newmapstartspot) changelevel = #70;
```

Attempts to change the map to the named map. If 'newmapstartspot' is specified, the state of the current map will be preserved, and the argument will be passed to the next map in the 'startspot' global, and the next map will be loaded from archived state if it was previously visited. If not specified, all archived map states will be purged.

#### `changepic`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
DEP_CSQC void(string slot, string picname, optional entity player) changepic = #107;
```

Part of TEI_SHOWLMP2. Changes the image used by a legacy screen-picture slot.

#### `changepitch`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(entity ent) changepitch = #63;
```

Part of DP_QC_CHANGEPITCH. Moves an entity's pitch toward idealpitch at its pitch_speed limit.

#### `changeyaw`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void() changeyaw = #49;
```

Changes the self.angles_y field towards self.ideal_yaw by up to self.yaw_speed.

#### `checkbottom`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(entity ent) checkbottom = #40;
```

Expensive checks to ensure that the entity is actually sitting on something solid, returns true if it is.

#### `checkbuiltin`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(__variant funcref) checkbuiltin = #0:checkbuiltin;
```

Checks to see if the specified builtin is supported/mapped. This is intended as a way to check for #0 functions, allowing for simple single-builtin functions. Warning, if two different engines map different builtins to the same number, then this function will not tell you which will be called, only that it won't crash (the exception being #0, which are remapped as available).

#### `checkclient`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
entity() checkclient = #17;
```

Returns one of the player entities. The returned player will change periodically.

#### `checkcommand`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(string name) checkcommand = #294;
```

Part of FTE_QC_CHECKCOMMAND Checks to see if the supplied name is a valid command, cvar, or alias. Returns 0 if it does not exist.

#### `checkextension`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(string ext) checkextension = #1;
```

Checks if the named extension is supported by the running engine.

#### `checkextension`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(string extname) checkextension = #99;
```

Checks for an extension by its name (eg: checkextension("FRIK_FILE") says that its okay to go ahead and use strcat). Use cvar("pr_checkextension") to see if this builtin exists.

#### `checkpvs`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
float(vector viewpos, entity entity) checkpvs = #240;
```

Part of FTE_QC_CHECKPVS. Tests whether an entity is potentially visible from the supplied view position; PVS is not a line-of-sight collision trace.

#### `chr2str`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
string(float chr, ...) chr2str = #223;
```

Part of FTE_STRINGS The input floats are considered character values, and are concatenated.

#### `cin_close`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(string id) cin_close = #0:cin_close;
```

Closes the cinematic resource identified by id.

#### `cin_getstate`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(string id) cin_getstate = #0:cin_getstate;
```

Returns a cinematic's playback state.

#### `cin_open`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(string file, string id) cin_open = #0:cin_open;
```

Opens a cinematic/video resource identified by id; actual decoding requires a supported decoder.

#### `cin_restart`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(string file) cin_restart = #0:cin_restart;
```

Restarts playback of the named cinematic resource.

#### `cin_setstate`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(string id, float newstate) cin_setstate = #0:cin_setstate;
```

Changes a cinematic's playback state to the supplied state value.

#### `clearscene`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void() clearscene = #300;
```

Forgets all rentities, polygons, and temporary dlights. Resets all view properties to their default values.

#### `clientcommand`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
void(entity e, string s) clientcommand = #440;
```

Part of KRIMZON_SV_PARSECLIENTCOMMAND. Passes a player's command to the engine's ordinary command handler; useful for forwarding unhandled SV_ParseClientCommand commands.

#### `clientcount`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float() clientcount = #61;
```

Returns the maximum number of players on the server. Useless if its a remote server, so its a kinda useless builtin really.

#### `clientstat`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
void(float num, float type, .__variant fld) clientstat = #232;
```

Part of EXT_CSQC Specifies what data to use in order to send various stats, in a client-specific way. 'num' should be a value between 32 and 127, other values are reserved. 'type' must be set to one of the EV_* constants, one of EV_FLOAT, EV_STRING, EV_INTEGER, EV_ENTITY. fld must be a reference to the field used, each player will be sent only their own copy of these fields.

#### `clientstate`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float() clientstate = #62;
```

Tells you whether the client is actually connected to anything. 0 for a dedicated server (but dedicated servers don't normally run menuqc anyway), 2 if connecting or connected to a server (but not necessarily spawned+active), 1 for sitting around idle without trying to connect to anything yet.

#### `clienttype`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
float(entity client) clienttype = #455;
```

Part of DP_SV_BOTCLIENT. Reports a client's connection/bot classification; interpret the engine's documented values rather than guessing from its name.

#### `clipboard_get`

**Scope:** MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(int cliptype) clipboard_get = #0:clipboard_get;
```

Attempts to query the system clipboard. Any pasted text will be returned via Menu_InputEvent

#### `clipboard_set`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(int cliptype, string text) clipboard_set = #0:clipboard_set;
```

Changes the system clipboard to the specified text.

#### `clusterevent`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(string dest, string from, string cmd, string info) clusterevent = #0:clusterevent;
```

Only functions in mapcluster mode. Sends an event to whichever server the named player is on. The destination server can then dispatch the event to the client or handle it itself via the SV_ParseClusterEvent entrypoint. If dest is empty, the event is broadcast to ALL servers. If the named player can't be found, the event will be returned to this server with the cmd prefixed with 'error:'.

#### `clustertransfer`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(entity player, optional string newnode) clustertransfer = #0:clustertransfer;
```

Only functions in mapcluster mode. Initiate transfer of the player to a different node. Can take some time. If dest is specified, returns null on error. Otherwise returns the current/new target node (or null if not transferring).

#### `con_draw`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(string conname, vector pos, vector size, float fontsize) con_draw = #393;
```

Part of FTE_CSQC_ALTCONSOLES Draws the named console.

#### `con_getset`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
string(string conname, string field, optional string newvalue) con_getset = #391;
```

Part of FTE_CSQC_ALTCONSOLES Reads or sets a property from a console object. The old value is returned. Iterrate through consoles with the 'next' field. Valid properties: title, name, next, unseen, markup, forceutf8, close, clear, hidden, linecount

#### `con_input`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(string conname, float inevtype, float parama, float paramb, float paramc) con_input = #394;
```

Part of FTE_CSQC_ALTCONSOLES Forwards input events to the named console. Mouse updates should be absolute only.

#### `con_printf`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(string conname, string messagefmt, ...) con_printf = #392;
```

Part of FTE_CSQC_ALTCONSOLES Prints onto a named console.

#### `copyentity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
entity(entity from, optional entity to) copyentity = #400;
```

Part of DP_QC_COPYENTITY Copies all fields from one entity to another.

#### `copyentity`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
void(entity src, entity dst) copyentity = #47;
```

Part of DP_QC_COPYENTITY Copies all entity fields from one entity into another (forgetting any that were previously set on the destination).

#### `coredump`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void() coredump = #28;
```

Writes out a coredump. This contains stack, globals, and field info for all ents. This can be handy for debugging.

#### `coredump`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void() coredump = #30;
```

Takes a dump, writing the qcvm's state to disk. There are normally easier ways to debug, but I suppose this one still beats print spam.

#### `cos`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(float angle) cos = #61;
```

Part of DP_QC_SINCOSSQRTPOW Just cos. Uses radians.

#### `cos`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(float) cos = #39;
```

Part of DP_QC_SINCOSSQRTPOW. Computes sine/cosine with an angle measured in radians, not degrees.

#### `cprint`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(string s, ...) cprint = #338;
```

Print into the center of the screen just as ssqc's centerprint would appear.

#### `cprint`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string text,...) cprint = #7;
```

Tries to show the given message in the centre of the screen, assuming that its not obscured by menus. Oh hey look, you're calling it in menuqc!

#### `crash`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
FTEDEP("Call 'error' instead") void() crash = #72;
```

Demonstrates that no program is bug free.

#### `crc16`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
__deprecated("Use digest_hex") float(float caseinsensitive, string s, ...) crc16 = #494;
```

Part of DP_QC_CRC16. Computes a legacy 16-bit checksum; deprecated in favor of digest_hex and not suitable for authentication.

#### `createbuffer`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void*(int bytes) createbuffer = #0:createbuffer;
```

Returns a temporary buffer that can be written to / read from. The buffer will be garbage collected and thus cannot be explicitly freed. Tempstrings and buffer references must not be stored into the buffer as the garbage collector will not scan these.

#### `crossproduct`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
vector(vector v1, vector v2) crossproduct = #0:crossproduct;
```

Part of FTE_QC_CROSSPRODUCT Small helper function to calculate the crossproduct of two vectors.

#### `CustomTempEnt`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
void(float type, vector pos, ...) CustomTempEnt = #209;
```

Part of FTE_PEXT_CUSTOMTENTS. Emits an effect using a registered custom temporary-effect format. Producer and client must agree on its parameters.

#### `cvar`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(string name) cvar = #14;
```

Reads a console variable as a float; a missing/non-numeric value normally reads as zero.

#### `cvar`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(string) cvar = #45;
```

Returns the numeric value of the named cvar

#### `cvar_defstring`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
string(string name) cvar_defstring = #89;
```

Part of DP_QC_CVAR_DEFSTRING. Reads the console variable's default value as text.

#### `cvar_defstring`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
string(string s) cvar_defstring = #482;
```

Part of DP_QC_CVAR_DEFSTRING. Reads the console variable's default value as text.

#### `cvar_description`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
string(string cvarname) cvar_description = #518;
```

Retrieves the description of a cvar, which might be useful for tooltips or help files. This may still not be useful.

#### `cvar_set`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(string cvarname, string valuetoset) cvar_set = #72;
```

Instantly sets a cvar to the given string value. Warning: the resulting string includes apostrophies surrounding the result. You may wish to use sprintf instead.

#### `cvar_set`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string name, string value) cvar_set = #15;
```

Sets a console variable from a string; the engine may protect or restrict specific variables.

#### `cvar_setf`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(string cvar, float val) cvar_setf = #176;
```

Sets a console variable from a float, converting it to text.

#### `cvar_string`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
string(string cvarname) cvar_string = #448;
```

Part of DP_QC_CVAR_STRING. Part of DP_QC_CVAR_STRING Returns the value of a cvar, as a string.

#### `cvar_string`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
string(string name) cvar_string = #71;
```

Part of DP_QC_CVAR_STRING Returns the value of a cvar, as a string.

#### `cvar_type`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(string name) cvar_type = #495;
```

Part of DP_QC_CVAR_TYPE. Returns console-variable existence/type/flag information; inspect the extension's CVAR_TYPE_* bits.

#### `cvars_haveunsaved`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float() cvars_haveunsaved = #0:cvars_haveunsaved;
```

Returns true if any archived cvar has an unsaved value.

#### `deltalisten`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(string modelname, float(float isnew) updatecallback, float flags) deltalisten = #371;
```

Specifies a per-modelindex callback to listen for engine-networking entity updates. Such entities are automatically interpolated by the engine (unless flags specifies not to). The various standard entity fields will be overwritten each frame before the updatecallback function is called.

#### `digest_hex`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
string(string digest, string data, ...) digest_hex = #639;
```

Computes the requested digest (for example SHA256) of the supplied text and returns hexadecimal text; check algorithm availability.

#### `digest_ptr`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(string digest, void *data, int length, optional int offset) digest_ptr = #0:digest_ptr;
```

Calculates the digest of a single contiguous block of memory (including nulls) using the specified hash function.

#### `dprint`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7, optional string s8) dprint = #25;
```

NQ: Prints the given message on the server's console, but only if the developer cvar is set. Arguments will be concatenated into a single message.

#### `dprint`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string text, ...) dprint = #16;
```

Developer/debug console output, controlled by developer settings.

#### `drawcharacter`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(vector position, float character, vector scale, vector rgb, float alpha, optional float flag) drawcharacter = #454;
```

Draws one character code at a 2D position with scale, color, alpha and blend flags.

#### `drawcharacter`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(vector position, float character, vector size='8 8', vector rgb='1 1 1', float alpha=1, optional float drawflag=0) drawcharacter = #320;
```

Draw the given quake character at the given position. If flag&4, the function will consider the char to be a unicode char instead (or display as a ? if outside the 32-127 range). size should normally be something like '8 8 0'. rgb should normally be '1 1 1' alpha normally 1. Software engines may assume the named defaults. Note that ALL text may be rescaled on the X axis due to variable width fonts. The X axis may even be ignored completely.

#### `drawfill`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(vector position, vector size, vector rgb, float alpha, optional float drawflag) drawfill = #323;
```

Draws a solid block over the given 2d box, with given colour, alpha, and blend mode (specified via flags). flags&3=0 simple blend. flags&3=1 additive blend

#### `drawfill`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(vector position, vector size, vector rgb, float alpha, optional float flag) drawfill = #457;
```

Draws a colored 2D rectangle, with alpha and optional blend flags.

#### `drawgetimagesize`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
vector(string picname) drawgetimagesize = #318;
```

Returns the dimensions of the named image. Images specified with .lmp should give the original .lmp's dimensions even if texture replacements use a different resolution. WARNING: this function may be slow if used without or directly after its initial precache_pic.

#### `drawgetimagesize`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
vector(string picname) drawgetimagesize = #460;
```

Returns a cached image's width and height in the vector's X and Y components.

#### `drawline`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(float width, vector pos1, vector pos2) drawline = #466;
```

Draws a 2D line of the specified width between two positions using the active 2D drawing context.

#### `drawline`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float width, vector pos1, vector pos2, vector rgb, float alpha, optional float drawflag) drawline = #315;
```

Draws a 2d line between the two 2d points.

#### `drawpic`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(vector position, string pic, vector size, vector rgb, float alpha, optional float flag) drawpic = #456;
```

Draws a whole cached image in a 2D rectangle with tint, alpha and blend flags.

#### `drawpic`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(vector position, string pic, vector size, vector rgb='1 1 1', float alpha=1, optional float drawflag=0) drawpic = #322;
```

Draws an shader within the given 2d screen box. Software engines may omit support for rgb+alpha, but must support rescaling, and must clip to the screen without crashing.

#### `drawrawstring`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(vector position, string text, vector scale, vector rgb, float alpha, optional float flag) drawrawstring = #455;
```

Draws text without the colored-text interpretation used by ordinary string drawing.

#### `drawrawstring`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(vector position, string text, vector size, vector rgb, float alpha, optional float drawflag) drawrawstring = #321;
```

Draws the specified string without using any markup at all, even in engines that support it. If UTF-8 is globally enabled in the engine, then that encoding is used (without additional markup), otherwise it is raw quake chars. Software engines may assume a size of '8 8 0', rgb='1 1 1', alpha=1, flag&3=0, but it is not an error to draw out of the screen.

#### `drawresetcliparea`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(void) drawresetcliparea = #325;
```

Reverts the scissor/clip area to the whole screen.

#### `drawresetcliparea`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(void) drawresetcliparea = #459;
```

Removes the current 2D drawing clip rectangle.

#### `drawrotpic`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(vector pivot, vector mins, vector maxs, string pic, vector rgb, float alpha, float angle) drawrotpic = #0:drawrotpic;
```

Draws an image rotating at the pivot. To rotate in the center, use mins+maxs of half the size with mins negated. Angle is in degrees.

#### `drawrotsubpic`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(vector pivot, vector mins, vector maxs, string pic, vector txmin, vector txsize, vector rgb, vector alphaandangles) drawrotsubpic = #0:drawrotsubpic;
```

Overcomplicated draw function for over complicated people. Positions follow drawrotpic, while texture coords follow drawsubpic. Due to argument count limitations in builtins, the alpha value and angles are combined into separate fields of a vector (tip: use fteqcc's [alpha, angle] feature.

#### `drawsetcliparea`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float x, float y, float width, float height) drawsetcliparea = #324;
```

Specifies a 2d clipping region (aka: scissor test). 2d draw calls will all be clipped to this 2d box, the area outside will not be modified by any 2d draw call (even 2d polygons).

#### `drawsetcliparea`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(float x, float y, float width, float height) drawsetcliparea = #458;
```

Restricts subsequent 2D drawing to the supplied rectangle.

#### `drawstring`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(vector position, string text, vector scale, vector rgb, float alpha, float flag=0) drawstring = #467;
```

Draws a string using the active font and supplied position, scale, color, alpha and flags.

#### `drawstring`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(vector position, string text, vector size='8 8', vector rgb='1 1 1', float alpha=1, float drawflag=0) drawstring = #326;
```

Draws a string, interpreting markup and recolouring as appropriate.

#### `drawsubpic`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(vector pos, vector sz, string pic, vector srcpos, vector srcsz, vector rgb, float alpha, float flag) drawsubpic = #469;
```

Draws a subrectangle of an image into a 2D destination rectangle; source coordinates are normalized image coordinates.

#### `drawsubpic`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(vector pos, vector sz, string pic, vector srcpos, vector srcsz, vector rgb, float alpha, optional float drawflag) drawsubpic = #328;
```

Draws a rescaled subsection of an image to the screen.

#### `drawtextfield`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(vector pos, vector size, float alignflags, string text) drawtextfield = #0:drawtextfield;
```

Draws a multi-line block of text, including word wrapping and alignment. alignflags bits are RTLB, typically 3. Returns the total number of lines.

#### `dropclient`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
void(entity player) dropclient = #453;
```

Part of DP_SV_DROPCLIENT. Disconnects the selected player from the server.

#### `droptofloor`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float() droptofloor = #34;
```

Instantly moves the entity downwards until it hits the ground. If the entity is in solid or would need to drop more than 'pr_droptofloorunits' quake units, its position will be considered invalid and the builtin will abort, returning FALSE, otherwise TRUE.

#### `dynamiclight_add`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(vector org, float radius, vector lightcolours, optional float style, optional string cubemapname, optional float pflags) dynamiclight_add = #305;
```

Adds a temporary dlight, ready to be drawn via addscene. Cubemap orientation will be read from v_forward/v_right/v_up.

#### `dynamiclight_get`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
__variant(float lno, float fld) dynamiclight_get = #372;
```

Retrieves a property from the given dynamic/rt light. Return type depends upon the light field requested.

#### `dynamiclight_set`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float lno, float fld, __variant value) dynamiclight_set = #373;
```

Changes a property on the given dynamic/rt light. Value type depends upon the light field to be changed.

#### `dynamiclight_spawnstatic`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(vector org, float radius, vector rgb) dynamiclight_spawnstatic = #0:dynamiclight_spawnstatic;
```

Creates a static persistent light at the given position with the specified colour. Additional properties must be set via dynamiclight_set.

#### `edict_num`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
entity(float entnum) edict_num = #459;
```

Part of DP_QC_EDICT_NUM. Gets an entity from this VM's entity number; validate range and lifetime.

#### `effect`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org, string modelname, float startframe, float endframe, float framerate) effect = #404;
```

Part of DP_SV_EFFECT Spawns a self-animating sprite

#### `entityfieldname`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(float fieldnum) entityfieldname = #497;
```

Part of DP_QC_ENTITYDATA Retrieves the name of the given entity field.

#### `entityfieldref`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
field_t(float fieldnum) entityfieldref = #0:entityfieldref;
```

Returns a field value that can be directly used to read entity fields. Be sure to validate the type with entityfieldtype before using.

#### `entityfieldtype`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(float fieldnum) entityfieldtype = #498;
```

Part of DP_QC_ENTITYDATA Provides information about the type of the field specified by the field num. Returns one of the EV_ values.

#### `entityprotection`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(entity e, float nowreadonly) entityprotection = #0:entityprotection;
```

Changes the protection on the specified entity to protect it from further edits from QC. The return value is the previous setting. Note that this can be used to unprotect the world, but doing so long term is not advised as you will no longer be able to detect invalid entity references. Also, world is not networked, so results might not be seen by clients (or in other words, world.avelocity_y=64 is a bad idea).

#### `eprint`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity e) eprint = #31;
```

Debugging builtin that prints all fields of the given entity to the console.

#### `eprint`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(entity) eprint = #33;
```

Prints an entity's fields for debugging.

#### `error`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(string e) error = #10;
```

Ends the game with an easily readable error message.

#### `error`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string err,...) error = #2;
```

Fatal error that will trigger a crash-to-console that users will actually notice.

#### `etof`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(entity) etof = #79;
```

Converts a MenuQC entity reference to its numeric entity index.

#### `etos`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
string(entity ent) etos = #65;
```

Part of DP_QC_ETOS Returns some pointless 'entity %i' message for debugging. Use generateentitydata if you want its field data, or sprintf("%i",ent) if you want just its number without the clumsy awkward prefix.

#### `etos`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
string(entity) etos = #20;
```

Part of DP_QC_ETOS. Converts an entity reference to a diagnostic text string.

#### `ex_bprint`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(string s, optional string s0, optional string s1, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6) ex_bprint = #0:ex_bprint;
```

Remaster: Sends the strings to all clients, which will order them according to {#}. Also substitutes localised strings for $NAME strings.

#### `ex_centerprint`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(entity ent, string text, optional string s0, optional string s1, optional string s2, optional string s3, optional string s4, optional string s5) ex_centerprint = #0:ex_centerprint;
```

Remaster: Sends the strings to the client, which will order according to {#}. Also substitutes localised strings for $NAME strings.

#### `ex_CheckPlayerEXFlags`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(entity playerEnt) ex_CheckPlayerEXFlags = #0:ex_CheckPlayerEXFlags;
```

Behaviour is undocumented.

#### `ex_clearprompt`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
void(entity player) ex_clearprompt = #0:ex_clearprompt;
```

Part of EX_PROMPT Hides a previously sent prompt.

#### `ex_finaleFinished`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float() ex_finaleFinished = #0:ex_finaleFinished;
```

Behaviour is undocumented.

#### `ex_localsound`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(entity client, string sample) ex_localsound = #0:ex_localsound;
```

Plays a sound only to the single specified player. The sound will be full volume, unattenuated, unaffected by reverb. Suitable for menus and things that are not part of the actual game world.

#### `ex_prompt`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
void(entity player, string text, float numchoices) ex_prompt = #0:ex_prompt;
```

Part of EX_PROMPT Initiates a user prompt. You must call ex_promptchoice exactly once per choice, with no other networking between.

#### `ex_promptchoice`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
void(entity player, string text, float impulse) ex_promptchoice = #0:ex_promptchoice;
```

Part of EX_PROMPT Follows a call to ex_prompt.

#### `ex_sprint`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(entity client, string s, optional string s0, optional string s1, optional string s2, optional string s3, optional string s4, optional string s5) ex_sprint = #0:ex_sprint;
```

Remaster: Sends the strings to the client, which will order according to {#}. Also substitutes localised strings for $NAME strings.

#### `ex_walkpathtogoal`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(float movedist, vector goal) ex_walkpathtogoal = #0:ex_walkpathtogoal;
```

Behaviour is undocumented.

#### `externcall`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
__variant(float prnum, string funcname, ...) externcall = #201;
```

Part of FTE_MULTIPROGS Directly call a function in a different/same progs by its name. prnum=0 is the 'default' or 'main' progs. prnum=-1 means current progs. prnum=-2 will scan through the active progs and will use the first it finds.

#### `externset`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float prnum, __variant newval, string varname) externset = #204;
```

Part of FTE_MULTIPROGS Sets a global in the named progs by name. prnum=0 is the 'default' or 'main' progs. prnum=-1 means current progs. prnum=-2 will scan through the active progs and will use the first it finds.

#### `externvalue`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
__variant(float prnum, string varname) externvalue = #203;
```

Part of FTE_MULTIPROGS Reads a global in the named progs by the name of that global. prnum=0 is the 'default' or 'main' progs. prnum=-1 means current progs. prnum=-2 will scan through the active progs and will use the first it finds.

#### `fabs`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(float) fabs = #18;
```

Returns the absolute value.

#### `fabs`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(float) fabs = #43;
```

Removes the sign of the float, making it positive if it is negative.

#### `fclose`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(filestream fhandle) fclose = #111;
```

Part of FRIK_FILE Closes a file handle returned from fopen. Should be called once for every successful call to fopen, files left open may result in warning messages, double closes are definitely bad too obviously.

#### `fclose`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
void(filestream fhandle) fclose = #49;
```

Part of FRIK_FILE. Closes an open file handle.

#### `fcopy`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(string src, string dst) fcopy = #650;
```

Equivelent to fopen+fread+fwrite+fclose from QC (ie: reads from $gamedir/data/ or $gamedir, but always writes to $gamedir/data/ )

#### `fexists`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(string fname) fexists = #653;
```

Returns true if it exists inside the default writable path. Use whichpack for greater portability.

#### `fgets`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
string(filestream fhandle) fgets = #112;
```

Part of FRIK_FILE Reads a single line out of the file. The new line character is not returned as part of the string. Returns the null string on EOF (use `if not(thereturnedstring)` to easily test for this, which distinguishes it from the empty string which is returned if the line being read is blank.

#### `fgets`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
string(filestream fhandle) fgets = #50;
```

Part of FRIK_FILE. Reads a line from a file handle; check the documented EOF behavior and string lifetime.

#### `find`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
entity(entity start, .string field, string match) find = #24;
```

Searches entities after start for a matching string field; returns world when there is no match.

#### `find`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
entity(entity start, .string fld, string match) find = #18;
```

Scan for the next entity with a given field set to the given 'match' value. start should be either world, or the previous entity that was found. Returns world on failure/if there are no more. If you have many many entities then you may find that hashtables will give more performance (but requires extra upkeep).

#### `find_list`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
entity*(.__variant fld, __variant match, int type=EV_STRING, __out int count) find_list = #0:find_list;
```

Scan for the next entity with a given field set to the given 'match' value. start should be either world, or the previous entity that was found. Returns world on failure/if there are no more. If you have many many entities then you may find that hashtables will give more performance (but requires extra upkeep).

#### `findchain`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
entity(.string field, string match) findchain = #26;
```

Part of DP_QC_FINDCHAIN. Finds entities matching a string field and links them through .chain; world terminates the chain.

#### `findchain`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
entity(.string field, string match, optional .entity chainfield) findchain = #402;
```

Part of DP_QC_FINDCHAIN. Finds entities matching a string field and links them through .chain; world terminates the chain.

#### `findchainflags`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
entity(.float field, float match) findchainflags = #88;
```

Part of DP_QC_FINDCHAINFLAGS. Builds a .chain list of entities with matching field flag bits.

#### `findchainflags`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
entity(.float fld, float match, optional .entity chainfield) findchainflags = #450;
```

Part of DP_QC_FINDCHAINFLAGS. Builds a .chain list of entities with matching field flag bits.

#### `findchainfloat`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
entity(.__variant field, __variant match) findchainfloat = #27;
```

Part of DP_QC_FINDCHAINFLOAT. Finds entities matching a numeric/variant field and links them through .chain.

#### `findchainfloat`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
entity(.float fld, float match, optional .entity chainfield) findchainfloat = #403;
```

Part of DP_QC_FINDCHAINFLOAT. Finds entities matching a numeric/variant field and links them through .chain.

#### `findentityfield`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(string fieldname) findentityfield = #0:findentityfield;
```

Find a field index by name.

#### `findflags`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
entity(entity start, .float field, float match) findflags = #87;
```

Part of DP_QC_FINDFLAGS. Finds the next entity with matching bits in a float field, returning world if absent.

#### `findflags`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
entity(entity start, .float fld, float match) findflags = #449;
```

Part of DP_QC_FINDFLAGS. Finds the next entity with matching bits in a float field, returning world if absent.

#### `findfloat`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
entity(entity start, .__variant field, __variant match) findfloat = #25;
```

Part of DP_QC_FINDFLOAT. Finds an entity after start whose specified numeric/variant field matches; returns world if absent.

#### `findfloat`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
entity(entity start, .__variant fld, __variant match) findfloat = #98;
```

Part of DP_QC_FINDFLOAT Equivelent to the find builtin, but instead of comparing strings contents, this builtin compares the raw values. This builtin requires multiple calls in order to scan all entities - set start to the previous call's return value. world is returned when there are no more entities.

#### `findfont`

**Scope:** CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(string s) findfont = #356;
```

Part of DP_GFX_FONTS Looks up a named font slot. Matches the actual font name as a last resort.

#### `findkeysforcommand`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Does not support modifiers") string(string command, optional float bindmap) findkeysforcommand = #521;
```

Returns a list of keycodes that perform the given console command in a format that can only be parsed via tokenize (NOT tokenize_console). This only and always returns two values - if only one key is actually bound, -1 will be returned. The bindmap argument is listed for compatibility with dp-specific defs, but is ignored in FTE.

#### `findkeysforcommand`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
string(string command, optional float bindmap) findkeysforcommand = #610;
```

Returns key bindings associated with a console command, optionally within a bind map.

#### `findkeysforcommandex`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(string command, optional float bindmap) findkeysforcommandex = #0:findkeysforcommandex;
```

Returns a list of key bindings in keyname format instead of keynums. Use tokenize to parse. This list may contain modifiers. May return large numbers of keys.

#### `findradius`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
entity(vector org, float rad, optional .entity chainfield) findradius = #22;
```

Finds all entities within a distance of the 'org' specified. One entity is returned directly, while other entities are returned via that entity's .chain field. Use findradius_list for an updated alternative without reenterancy issues.

#### `findradius_list`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
entity*(vector org, float rad, __out int foundcount, int sort=0) findradius_list = #0:findradius_list;
```

Finds all entities linked with a bbox within a distance of the 'org' specified, returning the list as a temp-array (world signifies the end). Unlike findradius, sv_gameplayfix_blowupfallenzombies is ignored (use FL_FINDABLE_NONSOLID instead), while sv_gameplayfix_findradiusdistancetobox and dpcompat_findradiusarealinks are force-enabled. The resulting buffer will automatically be cleaned up by the engine and does not need to be freed.

#### `floor`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(float) floor = #35;
```

Rounds downward toward negative infinity.

#### `floor`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(float) floor = #37;
```

Rounds the given float downwards, even when negative.

#### `fopen`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
filestream(string filename, float mode) fopen = #48;
```

Part of FRIK_FILE. Opens a game-relative file in the selected FILE_* mode, returning a handle; check failure and close it.

#### `fopen`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
filestream(string filename, float mode, optional float mmapminsize) fopen = #110;
```

Part of FRIK_FILE Opens a file within quake's filesystem for either read or write access. Returns a negative value on error, or >=0 for success. Due to sandboxing, all filenames should be prefixed with "data/" for maximum compatibility, otherwise writes may be redirected or reads may fail (sandboxing can be disabled in FTE with the `-unsafefopen` arg, but its use is strongly discouraged - hence why its not a cvar or w/e). tcp:// or tls:// schemes may be used in conjunction with FILE_STREAM. The file:/// scheme may be used when the `-allowfileuri` commandline arg was used (again strongly discouraged, but some people have weird requirements. You might just want to use a symlink instead.). Handles will not persist into loaded savegames, so be sure to not let handles linger (unless you're using eg SV_PerformLoad to handle it).

#### `forceinfokey`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
void(entity player, string key, string value) forceinfokey = #213;
```

Part of FTE_FORCEINFOKEY Directly changes a user's info without pinging off the client. Also allows explicitly setting * keys, including *spectator. Does not affect the user's config or other servers.

#### `forceinfokeyblob`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
void(entity player, string key, void *data, int size) forceinfokeyblob = #0:forceinfokeyblob;
```

Part of FTE_INFOBLOBS Directly changes a user's info without pinging off the client. Also allows explicitly setting * keys, including *spectator. Does not affect the user's config or other servers.

#### `fork`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(optional float sleeptime) fork = #210;
```

Part of FTE_MULTITHREADED When called, this builtin simply returns. Twice. The current 'thread' will return instantly with a return value of 0. The new 'thread' will return after sleeptime seconds with a return value of 1. See documentation for the 'sleep' builtin for limitations/requirements concerning the new thread. Note that QC should probably call abort in the new thread, as otherwise the function will return to the calling qc function twice also.

#### `fputs`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
void(filestream fhandle, string s) fputs = #51;
```

Part of FRIK_FILE. Writes a string to an open file handle; include your own newline if required.

#### `fputs`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(filestream fhandle, string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7) fputs = #113;
```

Part of FRIK_FILE Writes the given string(s) into the file. For compatibility with fgets, you should ensure that the string is terminated with a \n - this will not otherwise be done for you. It is up to the engine whether dos or unix line endings are actually written.

#### `frameduration`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(float modidx, float framenum) frameduration = #277;
```

Part of FTE_CSQC_SKELETONOBJECTS Retrieves the duration (in seconds) of the specified framegroup.

#### `frameforaction`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(float modidx, int actionid) frameforaction = #0:frameforaction;
```

Returns a random frame/animation for the specified mod-defined action, or -1 if no animations have the specified action.

#### `frameforname`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(float modidx, string framename) frameforname = #276;
```

Part of FTE_CSQC_SKELETONOBJECTS Looks up a framegroup from a model by name, avoiding the need for hardcoding. Returns -1 on error.

#### `frametoname`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string(float modidx, float framenum) frametoname = #284;
```

Returns the exported animation/frame name for a model frame index.

#### `fread`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
int(filestream fhandle, void *ptr, int size, optional int offset) fread = #0:fread;
```

Part of FTE_QC_FILE_BINARY Reads binary data out of the file. Returns truncated lengths if the read exceeds the length of the file.

#### `freepic`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(string name) freepic = #319;
```

Tells the engine that the image is no longer needed. The image will appear to be new the next time its needed.

#### `fremove`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(string fname) fremove = #652;
```

Deletes the named file - path is relative to data/ subdir, like fopen's FILE_WRITE. Returns 0 on success.

#### `frename`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(string src, string dst) frename = #651;
```

Renames the file, returning 0 on success. Both paths are relative to the data/ subdir.

#### `fseek`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
int(filestream fhandle, optional int newoffset) fseek = #0:fseek;
```

Part of FTE_QC_FILE_BINARY Changes the current position of the file, if specified. Returns prior position, in bytes.

#### `fseek64`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int(filestream fhandle, optional __int64 newoffset) fseek64 = #0:fseek64;
```

Changes the current position of the file, if specified. Returns prior position, in bytes.

#### `fsize`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
int(filestream fhandle, optional int newsize) fsize = #0:fsize;
```

Part of FTE_QC_FILE_BINARY Reports the total size of the file, in bytes. Can also be used to truncate/extend the file

#### `fsize64`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
__int64(filestream fhandle, optional __int64 newsize) fsize64 = #0:fsize64;
```

Reports the total size of the file, in bytes. Can also be used to truncate/extend the file

#### `ftoe`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
entity(float) ftoe = #80;
```

Converts a MenuQC numeric entity index to an entity reference; validate the index.

#### `ftoi`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
int(float) ftoi = #0:ftoi;
```

Part of FTE_QC_INTCONV Converts the given float into a true integer without depending on extended qcvm instructions.

#### `ftos`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string(float val) ftos = #26;
```

Returns a tempstring containing a representation of the given float. Precision depends upon engine.

#### `ftos`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
string(float) ftos = #17;
```

Converts a float to a temporary text string.

#### `ftou`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
__uint(float) ftou = #0:ftou;
```

Converts the given float into a true integer without depending on extended qcvm instructions.

#### `fwrite`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
int(filestream fhandle, void *ptr, int size, optional int offset) fwrite = #0:fwrite;
```

Part of FTE_QC_FILE_BINARY Writes binary data out of the file.

#### `gecko_create`

**Scope:** CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(string name, optional string initialURI) gecko_create = #487;
```

Part of DP_GECKO_SUPPORT Create a new 'browser tab' shader with the specified name that can then be drawn via drawpic (shader should not already exist - including from map/model textures or disk). In order to function correctly, this builtin depends upon external plugins being available. Use gecko_navigate to navigate it to a page of your choosing.

#### `gecko_destroy`

**Scope:** CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
void(string name) gecko_destroy = #488;
```

Part of DP_GECKO_SUPPORT Destroy a shader.

#### `gecko_get_texture_extent`

**Scope:** CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
vector(string name) gecko_get_texture_extent = #493;
```

Part of DP_GECKO_SUPPORT Retrieves a media decoder current image pixel sizes.

#### `gecko_getproperty`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(string shadname, string propname) gecko_getproperty = #0:gecko_getproperty;
```

Queries the media decoder (especially browser ones) for decoder-specific properties. The cef plugin recognises url, title, status.

#### `gecko_keyevent`

**Scope:** CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(string name, float key, float eventtype, optional float charcode) gecko_keyevent = #490;
```

Part of DP_GECKO_SUPPORT Send a key event to a media decoder. This applies only to interactive decoders like browsers.

#### `gecko_mousemove`

**Scope:** CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
void(string name, float x, float y) gecko_mousemove = #491;
```

Part of DP_GECKO_SUPPORT Sets a media decoder shader's mouse position. Values should be 0-1.

#### `gecko_navigate`

**Scope:** CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
void(string name, string URI) gecko_navigate = #489;
```

Part of DP_GECKO_SUPPORT Sends a command to the media decoder attached to the specified shader. In the case of a browser decoder, this changes the url that the browser displays. 'cmd:[un]focus' will tell the decoder that it has focus.

#### `gecko_resize`

**Scope:** CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
void(string name, float w, float h) gecko_resize = #492;
```

Part of DP_GECKO_SUPPORT Request to resize a media decoder.

#### `generateentitydata`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(entity e) generateentitydata = #0:generateentitydata;
```

Dumps the entities fields into a string which can later be parsed with parseentitydata.

#### `getbindmaps`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
vector() getbindmaps = #631;
```

Returns the active bind maps in a vector.

#### `getchannellevel`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(entity e, float channel) getchannellevel = #0:getchannellevel;
```

Reports how load the sound's sample is at its current offset.

#### `getcursormode`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(float effective) getcursormode = #0:getcursormode;
```

Reports the cursor mode this module previously attempted to use. If 'effective' is true, reports the cursor mode currently active (if was overriden by a different module which has precidence, for instance, or if there is only a touchscreen and no mouse).

#### `getentity`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
__variant(float entnum, float fieldnum) getentity = #504;
```

Looks up fields from non-csqc-visible entities. The entity will need to be within the player's pvs. fieldnum should be one of the GE_ constants.

#### `getentityfieldstring`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(float fieldnum, entity ent) getentityfieldstring = #499;
```

Part of DP_QC_ENTITYDATA. Reads a selected entity field as text using its reflected field index.

#### `getentitytoken`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
string(optional string resetstring) getentitytoken = #355;
```

Grab the next token in the map's entity lump. If resetstring is not specified, the next token will be returned with no other sideeffects. If empty, will reset from the map before returning the first token, probably {. If not empty, will tokenize from that string instead. Always returns tempstrings.

#### `getextresponse`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
string() getextresponse = #624;
```

Part of FTE_CSQC_SERVERBROWSER Stub.

#### `getgamedirinfo`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
string(float n, float prop) getgamedirinfo = #626;
```

Queries properties about an indexed gamedir (or -1 for the current gamedir). Returns null strings when out of bounds. Use the GDDI_* constants for the prop arg.

#### `gethostcacheindexforkey`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(string key) gethostcacheindexforkey = #622;
```

Part of FTE_CSQC_SERVERBROWSER Obtains a handle to a named key (to avoid string lookups). The return value can be used in place of the fld arg in related builtins

#### `gethostcachenumber`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(float fld, float hostnr) gethostcachenumber = #621;
```

Part of FTE_CSQC_SERVERBROWSER An alternative to gethostcachestring, for reading numeric values

#### `gethostcachestring`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
string(float fld, float hostnr) gethostcachestring = #612;
```

Part of FTE_CSQC_SERVERBROWSER Reads the value of a serverinfo key from the specified server (up to gethostcachevalue(SLIST_HOSTCACHEVIEWCOUNT)).

#### `gethostcachevalue`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(float type) gethostcachevalue = #611;
```

Part of FTE_CSQC_SERVERBROWSER Obtains one of SLIST_ values.

#### `getinputstate`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(float inputsequencenum) getinputstate = #345;
```

Looks up an input frame from the log, setting the input_* globals accordingly. The sequence number range used for prediction should normally be servercommandframe < sequence <= clientcommandframe. The sequence equal to clientcommandframe will change between input frames.

#### `getkeybind`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
string(float keynum, optional float bindmap, optional float modifier) getkeybind = #342;
```

Returns the current binding for the given key (returning only the command executed when no modifiers are pressed).

#### `getkeydest`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float() getkeydest = #602;
```

Returns the current keyboard input destination.

#### `getlight`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
DEP_SSQC("Broken on dedicated servers, ignores rtlights/etc") vector(vector org) getlight = #92;
```

Part of DP_QC_GETLIGHT Computes the RGB lighting at the specified position.

#### `getlightstyle`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(float style, optional __out vector rgb) getlightstyle = #0:getlightstyle;
```

Obtains the light style string for the given style.

#### `getlightstylergb`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
vector(float style) getlightstylergb = #0:getlightstylergb;
```

Obtains the current rgb value of the specified light style. In csqc, this is correct with regard to the current frame, while ssqc gives no guarentees about time and ignores client cvars. Note: use getlight if you want the actual light value at a point.

#### `getlocalinfo`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int(string keyname, optional void *outptr, int size) getlocalinfo = #0:getlocalinfo;
```

Obtains a copy of a data blob (with spaces) from the server's private localinfo. Will write up to size bytes and return the actual size. Does not null terminate (but memalloc(ret+1) will, if you want to cast the buffer to a string), and the blob may contain embedded nulls. Ignores all special keys, returning only what is actually there.

#### `getlocaluserinfo`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(float seat, string keyname) getlocaluserinfo = #0:getlocaluserinfo;
```

Reads a local userinfo key for the specified local player seat. This is not quite the same as getplayerkeyvalue, due to latency and possible serverside filtering.

#### `getlocaluserinfoblob`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
int(float seat, string keyname, void *outptr, int maxsize) getlocaluserinfoblob = #0:getlocaluserinfoblob;
```

Part of FTE_INFOBLOBS Obtains a copy of the full data blob. Will write up to size bytes but return the full size. Does not null terminate (but memalloc(ret+1) will, if you want to cast the buffer to a string), and the blob may contain embedded nulls. Ignores all special keys, returning only what is actually there.

#### `getlocationname`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(vector pos) getlocationname = #0:getlocationname;
```

Looks up the specified position in the current map's .loc file and reports the nearest marked name.

#### `getmodeleventidx`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(float modidx, float framenum, int eventidx, __out float timestamp, __out int code, __out string data) getmodeleventidx = #0:getmodeleventidx;
```

Part of FTE_GFX_MODELEVENTS Reports an indexed event within a model's animation. Writes to timestamp,code,data arguments on success. Returns false if the animation/event/model was out of range/invalid. Does not consider looping animations (retry from index 0 if it fails and you know that its a looping animation). This builtin is more annoying to use than getnextmodelevent, but can be made to deal with multiple events with the exact same timestamp.

#### `getmodelindex`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(string modelname, optional float queryonly) getmodelindex = #200;
```

Acts as an alternative to precache_model(foo);setmodel(bar, foo); return bar.modelindex; If queryonly is set and the model was not previously precached, the builtin will return 0 without needlessly precaching the model.

#### `getmousepos`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
vector() getmousepos = #344;
```

Nasty convoluted DP extension. Typically returns deltas instead of positions. Use CSQC_InputEvent instead for such things in csqc mods.

#### `getmousepos`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
vector() getmousepos = #66;
```

Obsolete. Return values depend upon the current cursor mode. Implement Menu_InputEvent instead, so you can handle deltas as-is or absolutes if that's all the OS can provide.

#### `getmousetarget`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float() getmousetarget = #604;
```

Sets/gets the legacy mouse event destination; prefer the documented input routing API for new code.

#### `getnextmodelevent`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(float modidx, float framenum, __inout float basetime, float targettime, __out int code, __out string data) getnextmodelevent = #0:getnextmodelevent;
```

Part of FTE_GFX_MODELEVENTS Reports the next event within a model's animation. Returns a boolean if an event was found between basetime and targettime. Writes to basetime,code,data arguments (if an event was found, basetime is set to the event's time, otherwise to targettime). WARNING: this builtin cannot deal with multiple events with the same timestamp (only the first will be reported).

#### `getpackagemanagerinfo`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(int n, int prop) getpackagemanagerinfo = #0:getpackagemanagerinfo;
```

Queries information about a package from the engine's package manager subsystem. Actions can be taken via the pkg console command. prop must be one of the GPMI_ constants.

#### `getplayerkeyblob`

**Scope:** CSQC. **Compatibility:** FTE extension.

```qc
int(float playernum, string keyname, optional void *outptr, int size) getplayerkeyblob = #0:getplayerkeyblob;
```

Part of FTE_INFOBLOBS Obtains a copy of the full data blob. Will write up to size bytes but return the full size. Does not null terminate (but memalloc(ret+1) will, if you want to cast the buffer to a string), and the blob may contain embedded nulls. Ignores all special keys, returning only what is actually there.

#### `getplayerkeyfloat`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(float playernum, string keyname, optional float assumevalue) getplayerkeyfloat = #0:getplayerkeyfloat;
```

Cheaper version of getplayerkeyvalue that avoids the need for so many tempstrings.

#### `getplayerkeyvalue`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
string(float playernum, string keyname) getplayerkeyvalue = #348;
```

Look up a player's userinfo, to discover things like their name, topcolor, bottomcolor, skin, team, *ver. Also includes scoreboard info like frags, ping, pl, userid, entertime, as well as voipspeaking and voiploudness.

#### `getplayerstat`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
__variant(float playernum, float statnum, float stattype) getplayerstat = #0:getplayerstat;
```

Retrieves a specific player's stat, matching the type specified on the server. This builtin is primarily intended for mvd playback where ALL players are known. Return value matches the specified EV_ stattype. For EV_ENTITY, world will be returned if the entity is not in the pvs, use type-punning with EV_INTEGER to get the entity number if you just want to see if its set. STAT_ITEMS should be queried as an EV_INTEGER on account of runes and items2 being packed into the upper bits.

#### `getproperty`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
__variant(float property) getproperty = #309;
```

Retrieve a currently-set (typically view) property, allowing you to read the current viewport or other things. Due to cheat protection, certain values may be unretrievable.

#### `getresolution`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
vector(float vidmode, optional float forfullscreen) getresolution = #608;
```

Supposed to query the driver for supported video modes. FTE does not query drivers in this way, nor would it trust drivers anyway.

#### `getsoundindex`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(string soundname, optional float queryonly) getsoundindex = #0:getsoundindex;
```

Provides a way to query if a sound is already precached or not. The return value can also be checked for <=255 to see if it'll work over any network protocol. The sound index can also be used for writebyte hacks, but this is discouraged - use SOUNDFLAG_UNICAST instead.

#### `getsoundtime`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(entity e, float channel) getsoundtime = #533;
```

Returns the current playback time of the sample on the given entity's channel. Beware CHAN_AUTO (in csqc, channels are not limited by network protocol).

#### `getstatf`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(float stnum, optional float firstbit, optional float bitcount) getstatf = #331;
```

Retrieves the numerical value of the given EV_FLOAT stat. If firstbit and bitcount are specified, then this builtin acts as getstati combined with itof, and which should be used for STAT_ITEMS (but not other stats).

#### `getstati`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
int(float stnum) getstati = #330;
```

Retrieves the full precision of a stat registered as EV_INTEGER.

#### `getstats`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
string(float stnum) getstats = #332;
```

Retrieves the value of the given EV_STRING stat, as a tempstring. Older engines may use 4 consecutive integer stats, with a limit of 15 chars (yes, really. 15.), but FTEQW Delta uses a separate namespace for string stats and has a much higher length limit.

#### `getsurfaceclippedpoint`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
vector(entity e, float s, vector p) getsurfaceclippedpoint = #439;
```

Part of DP_QC_GETSURFACE. Returns a point projected/clipped onto the selected surface.

#### `getsurfacenearpoint`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(entity e, vector p) getsurfacenearpoint = #438;
```

Part of DP_QC_GETSURFACE. Finds the surface nearest a supplied point; interpret coordinate space according to the model API.

#### `getsurfacenormal`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
vector(entity e, float s) getsurfacenormal = #436;
```

Part of DP_QC_GETSURFACE. Returns the selected surface's normal vector.

#### `getsurfacenumpoints`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(entity e, float s) getsurfacenumpoints = #434;
```

Part of DP_QC_GETSURFACE. Returns the number of points on the selected model surface.

#### `getsurfacenumtriangles`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(entity e, float s) getsurfacenumtriangles = #628;
```

Returns the triangle count of the selected surface of an entity model.

#### `getsurfacepoint`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
vector(entity e, float s, float n) getsurfacepoint = #435;
```

Part of DP_QC_GETSURFACE. Returns the selected surface point in the model's coordinate conventions.

#### `getsurfacepointattribute`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
vector(entity e, float s, float n, float a) getsurfacepointattribute = #486;
```

Part of DP_QC_GETSURFACEPOINTATTRIBUTE. Gets the requested SPA_* attribute of a selected surface point.

#### `getsurfacetexture`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
string(entity e, float s) getsurfacetexture = #437;
```

Part of DP_QC_GETSURFACE. Returns the selected surface's texture/material name.

#### `getsurfacetriangle`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector(entity e, float s, float n) getsurfacetriangle = #629;
```

Returns the three vertex indices of the selected surface triangle, encoded in a vector.

#### `gettagindex`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(entity ent, string tagname) gettagindex = #451;
```

Part of DP_QC_GETTAGINFO. Looks up a named model attachment/tag index on an entity.

#### `gettaginfo`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
vector(entity ent, float tagindex) gettaginfo = #452;
```

Part of DP_QC_GETTAGINFO Obtains the current worldspace position+orientation of the bone or tag from the given entity. The return value is the world coord, v_forward, v_right, v_up are also set according to the bone/tag's orientation.

#### `gettime`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float(optional float timetype) gettime = #519;
```

Returns the selected clock: 0=host time cached at frame start (default), 1=milliseconds clock converted to seconds, 5=client simulation time when available. gettimed uses double precision; use time for scheduled server thinks.

#### `gettime`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(optional float timetype) gettime = #67;
```

Returns the selected clock: 0=host time cached at frame start (default), 1=milliseconds clock converted to seconds, 5=client simulation time when available. gettimed uses double precision; use time for scheduled server thinks.

#### `gettimed`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
__double(optional int timetype) gettimed = #0:gettimed;
```

Returns the selected clock: 0=host time cached at frame start (default), 1=milliseconds clock converted to seconds, 5=client simulation time when available. gettimed uses double precision; use time for scheduled server thinks.

#### `globalstat`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float num, float type, string name) globalstat = #233;
```

Specifies what data to use in order to send various stats, in a non-client-specific way. num and type are as in clientstat, name however, is the name of the global to read in the form of a string (pass "foo").

#### `gp_rumble`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float devid, float amp_low, float amp_high, float duration) gp_rumble = #0:gp_rumble;
```

Sends a single rumble event to the game-pad specified in devid. Every time you call this, the previous effect is cancelled out.

#### `gp_rumbletriggers`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float devid, float left, float right, float duration) gp_rumbletriggers = #0:gp_rumbletriggers;
```

Makes the analog triggers rumble of the specified game-pad, like gp_rumble() one call cancels out the previous one on the device.

#### `gp_setledcolor`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float devid, vector color) gp_setledcolor = #0:gp_setledcolor;
```

Updates the game-pad LED color.

#### `gp_settriggerfx`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float devid, void *data, int size) gp_settriggerfx = #0:gp_settriggerfx;
```

Sends a specific effect packet to the controller. On the PlayStation 5's DualSense that can adjust the tension on the analog triggers.

#### `hash_add`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(hashtable table, string name, __variant value, optional float typeandflags) hash_add = #289;
```

Part of FTE_QC_HASHTABLES Adds the given key with the given value to the table. If flags&HASH_REPLACE, the old value will be removed, otherwise if flags&HASH_ADD then a duplicate entry will be added with a second value (can be obtained via hash_get's index argument). The type argument describes how the value should be stored in saved games, as well as providing constraints with the hash_get function. While you can claim that all variables are just vectors, being more precise can result in less issues with tempstrings or saved games - be sure to be explicit with EV_STRING where appropriate because tempstrings may be reclaimed before the get (especially with saved games or table 0).

#### `hash_createtab`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
hashtable(float tabsize, optional float defaulttype) hash_createtab = #287;
```

Part of FTE_QC_HASHTABLES Creates a hash table object. The tabsize argument is a performance hint and should generally be set to something similar to the number of entries expected, typically a power of two assumption. Too high simply wastes memory, too low results in extra string compares but no actual bugs. defaulttype must be one of the EV_* values, if specified. The hash table with index 0 is a game-persistant table and will NEVER be returned by this builtin (except as an error return).

#### `hash_delete`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
__variant(hashtable table, string name) hash_delete = #291;
```

Part of FTE_QC_HASHTABLES removes the named key. returns the value of the object that was destroyed, or 0 on error.

#### `hash_destroytab`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(hashtable table) hash_destroytab = #288;
```

Part of FTE_QC_HASHTABLES Destroys a hash table object.

#### `hash_get`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
__variant(hashtable table, string name, optional __variant deflt, optional float requiretype, optional float index) hash_get = #290;
```

Part of FTE_QC_HASHTABLES Looks up the specified key name in the hash table. Returns deflt if the key was not found. If requiretype is specified then the function will only consider entries of the matching type (allowing you to store both flags+strings under a single name without getting confused). If index is specified then the function will ignore the first N entries with the same key (applicable only with entries added using HASH_ADD, not HASH_REPLACE), allowing you to store multiple entries. Keep querying higher indexes starting from 0 until it returns the deflt value. You will usually need to cast the result of this function to a real datatype.

#### `hash_getkey`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
string(hashtable table, float idx) hash_getkey = #292;
```

Part of FTE_QC_HASHTABLES gets some random key name. add+delete can change return values of this, so don't blindly increment the key index if you're removing all.

#### `hidepic`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
DEP_CSQC void(string slot, optional entity player) hidepic = #105;
```

Part of TEI_SHOWLMP2. Hides a previously named legacy screen-picture slot; prefer CSQC for new HUDs.

#### `htos`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
string(int) htos = #262;
```

Part of FTE_QC_INTCONV Formats an integer as a base16 string, with leading 0s and no prefix. Always returns 8 characters.

#### `infoadd`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
infostring(infostring old, string key, string value) infoadd = #226;
```

Part of FTE_STRINGS Returns a new tempstring infostring with the named value changed (or added if it was previously unspecified). Key and value may not contain the \ character.

#### `infoget`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
string(infostring info, string key) infoget = #227;
```

Part of FTE_STRINGS Reads a named value from an infostring. The returned value is a tempstring

#### `infokey`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
string(entity e, string key) infokey = #80;
```

Part of FTE_QC_INFOKEY, QW_ENGINE If e is world, returns the field 'key' from either the serverinfo or the localinfo. If e is a player, returns the value of 'key' from the player's userinfo string. There are a few special exceptions, like 'ip' which is not technically part of the userinfo.

#### `infokey_blob`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int(entity e, string key, optional void *outbuf, int outbufsize) infokey_blob = #0:infokey_blob;
```

Retrieves a user's blob size, and optionally writes it to the specified buffer.

#### `infokeyf`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(entity e, string key) infokeyf = #0:infokeyf;
```

Identical to regular infokey, except returns a float.

#### `isbackbuffered`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
float(entity player) isbackbuffered = #234;
```

Part of FTE_ISBACKBUFFERED Returns if the given player's network buffer will take multiple network frames in order to clear. If this builtin returns non-zero, you should delay or reduce the amount of reliable (and also unreliable) data that you are sending to that client.

#### `iscachedpic`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(string name) iscachedpic = #316;
```

Checks to see if the image is currently loaded. Engines might lie, or cache between maps.

#### `iscachedpic`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(string name) iscachedpic = #451;
```

Tests whether an image is available in the picture cache.

#### `isdemo`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float() isdemo = #349;
```

Returns if the client is currently playing a demo or not. Returns 2 when playing an mvd (where other player's stats can be queried, or the pov can be changed freely).

#### `isfunction`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(string s) isfunction = #607;
```

Returns true if the named function exists and can be called with the callfunction builtin.

#### `isserver`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float() isserver = #350;
```

Returns non-zero whenever the local console can directly affect the server (ie: listen servers or single-player). Compat note: DP returns 0 for single-player.

#### `isserver`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float() isserver = #60;
```

Returns true if the local engine is running a server, and thus cvars and localcmds are shared with said server.

#### `itof`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(int, optional float shift, float mask=24) itof = #0:itof;
```

Part of FTE_QC_INTCONV Converts the given true integer into a float without depending on extended qcvm instructions. If shift and mask are specified then only specific parts of the integer will be cast to float.

#### `itos`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
string(int) itos = #260;
```

Part of FTE_QC_INTCONV Converts the passed true integer into a base10 string.

#### `json_find_object_child`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
jsonnode(jsonnode node, string) json_find_object_child = #0:json_find_object_child;
```

Find a child of a json object by name. Returns NULL if the handle couldn't be found.

#### `json_free`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(jsonnode) json_free = #0:json_free;
```

Frees a json tree and all of its children. Must only be called on the root node.

#### `json_get_child_at_index`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
jsonnode(jsonnode node, int childindex) json_get_child_at_index = #0:json_get_child_at_index;
```

Get the nth child of a json array or object. Returns NULL if the index is out of range.

#### `json_get_float`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(jsonnode node) json_get_float = #0:json_get_float;
```

Get a float from a json node.

#### `json_get_integer`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int(jsonnode node) json_get_integer = #0:json_get_integer;
```

Get an integer from a json node.

#### `json_get_length`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int(jsonnode node) json_get_length = #0:json_get_length;
```

Get the length of a json array or object. Returns 0 if its not an array.

#### `json_get_name`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(jsonnode node) json_get_name = #0:json_get_name;
```

Gets the object's name (useful if you're using json_get_child_at_index to walk an object's children for whatever reason).

#### `json_get_string`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(jsonnode node) json_get_string = #0:json_get_string;
```

Get a string from a value. Returns a null string if its not a string type.

#### `json_get_value_type`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
json_type_e(jsonnode node) json_get_value_type = #0:json_get_value_type;
```

Get type of a JSON value.

#### `json_parse`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
jsonnode(string) json_parse = #0:json_parse;
```

Parses the given JSON string.

#### `keynumtostring`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
string(float keynum) keynumtostring = #340;
```

Returns a hunam-readable name for the given keycode, as a tempstring.

#### `keynumtostring`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
string(float keynum) keynumtostring = #609;
```

Converts a qscancode key number into a mostly-human-readable name, matching the bind command.

#### `keynumtostring_csqc`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
DEP string(float keynum) keynumtostring_csqc = #340;
```

Returns a hunam-readable name for the given keycode, as a tempstring.

#### `keynumtostring_menu`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
DEP string(float keynum) keynumtostring_menu = #609;
```

Legacy key-number-to-name conversion. Deprecated; use the non-deprecated key naming API for new code.

#### `keynumtostring_omgwtf`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
DEP string(float keynum) keynumtostring_omgwtf = #520;
```

Legacy key-number-to-name conversion. Deprecated; use the non-deprecated key naming API for new code.

#### `lightstyle`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(float lightstyle, string stylestring, optional vector rgb) lightstyle = #35;
```

Specifies an auto-animating string that specifies the light intensity for entities using that lightstyle. a is off, z is fully lit. Should be lower case only. rgb will recolour all lights using that lightstyle.

#### `lightstylestatic`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float style, float val, optional vector rgb) lightstylestatic = #5;
```

Sets the lightstyle to an explicit numerical level. From Hexen2.

#### `loadcustomskin`

**Scope:** CSQC. **Compatibility:** FTE extension.

```qc
float(string skinfilename, optional string skindata) loadcustomskin = #377;
```

Part of FTE_QC_CUSTOMSKINS Creates a new skin object and returns it. These are custom per-entity surface->shader lookups. The skinfilename/data should be in .skin format: surfacename,shadername - makes the named surface use the named shader (legacy format for compat with q3) replace "surfacename" "shadername" - non-legacy equivalent. qwskin "foo" - use an unmodified quakeworld player skin (including crop+repalette rules) q1lower 0xff0000 - specify an override for the entity's lower colour, in this case to red q1upper 0x0000ff - specify an override for the entity's lower colour, in this case to blue h2class 0 - specifies which class to use for hexen2's hacky class-specific player colouring compose "surfacename" "shader" "imagename@x,y:w,h$s,t,s2,t2?r,g,b,a" - compose a skin texture from multiple images. The texture is determined to be sufficient to hold the first named image, additional images can be named as extra tokens on the same line. Use a + at the end of the line to continue reading image tokens from the next line also, the named shader must use 'map $diffuse' to read the composed texture (compatible with the defaultskin shader). Must be matched with a releasecustomskin call later, and is pointless without applycustomskin.

#### `loadfont`

**Scope:** CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(string fontname, string fontmaps, string sizes, float slot, optional float fix_scale, optional float fix_voffset) loadfont = #357;
```

Part of DP_GFX_FONTS too convoluted for me to even try to explain correct usage. Try drawfont = loadfont("", "cour", "16", -1, 0, 0); to switch to the courier font (optimised for 16 virtual pixels high) ('cour' requires mscorefonts installed in linux). Additionally you can add "outline=1" as an extra token in the sizes string, to have more readable outlined fonts.

#### `loadfromdata`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(string s) loadfromdata = #529;
```

Reads a set of entities from the given string. This string should have the same format as a .ent file or a saved game. Entities will be spawned as required. If you need to see the entities that were created, you should use parseentitydata instead. No spawn functions will be called.

#### `loadfromdata`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string s) loadfromdata = #68;
```

Reads a set of entities from the given string. This string should have the same format as a .ent file or a saved game. Entities will be spawned as required. If you need to see the entities that were created, you should use parseentitydata instead.

#### `loadfromfile`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(string s) loadfromfile = #530;
```

Reads a set of entities from the named file. This file should have the same format as a .ent file or a saved game. Entities will be spawned as required. If you need to see the entities that were created, you should use parseentitydata instead. No spawn functions will be called.

#### `loadfromfile`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string s) loadfromfile = #69;
```

Reads a set of entities from the named file. This file should have the same format as a .ent file or a saved game. Entities will be spawned as required. If you need to see the entities that were created, you should use parseentitydata instead.

#### `localcmd`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(string, ...) localcmd = #46;
```

Adds the string to the console command queue. Commands will not be executed immediately, but rather at the start of the following frame.

#### `localcmd`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string,...) localcmd = #13;
```

Queues commands for this VM's host console. Append a newline; never concatenate untrusted text.

#### `localsound`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
DEP_SSQC("This bypasses networking by design, so do NOT call this from ssqc. Use ex_localsound or sound(...,CF_UNICAST) instead.") void(string soundname, optional float channel, optional float volume) localsound = #177;
```

Plays a sound... locally... Also disables reverb.

#### `localsound`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string sample, optional float channel, optional float volume) localsound = #65;
```

Plays a sound, locally. precaching is optional, but recommended.

#### `log`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(float v, optional float base) log = #532;
```

Determines the logarithm of the input value according to the specified base. This can be used to calculate how much something was shifted by.

#### `logarithm`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(float v, optional float base) logarithm = #0:logarithm;
```

Determines the logarithm of the input value according to the specified base. This can be used to calculate how much something was shifted by. When base is omitted then this computes the 'natural log' (often called ln)

#### `logfrag`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
void(entity killer, entity killee) logfrag = #79;
```

Part of QW_ENGINE. Records a frag event for the supplied killer and victim; does not change health or scores itself.

#### `makestatic`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity e) makestatic = #69;
```

Sends a copy of the entity's renderable fields to all clients, and REMOVES the entity, preventing further changes. This means it will be unmutable and non-solid.

#### `makevectors`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(vector vang) makevectors = #1;
```

Takes an angle vector (pitch,yaw,roll) (+x=DOWN). Writes its results into v_forward, v_right, v_up vectors.

#### `matchclientname`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
entity(string match, optional float matchnum) matchclientname = #241;
```

Part of FTE_QC_MATCHCLIENTNAME. Finds a client matching a name, optionally selecting a match; handle no/ambiguous matches.

#### `max`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(float a, float b, ...) max = #95;
```

Part of DP_QC_MINMAXBOUND Returns the highest value of its arguments.

#### `max`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(float,...) max = #44;
```

Part of DP_QC_MINMAXBOUND. Returns the smallest/largest supplied value.

#### `memalloc`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
__variant*(int size) memalloc = #384;
```

Part of FTE_MEMALLOC Allocate an arbitary block of memory

#### `memcmp`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int(__variant *dst, __variant *src, int size, optional int dstoffset, int srcoffset) memcmp = #0:memcmp;
```

Compares two blocks of memory. Returns 0 if equal.

#### `memcpy`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(__variant *dst, __variant *src, int size, optional int dstoffset, int srcoffset) memcpy = #386;
```

Part of FTE_MEMALLOC Copys memory from one location to another

#### `memfill8`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(__variant *dst, int val, int size, optional int offset) memfill8 = #387;
```

Part of FTE_MEMALLOC Sets an entire block of memory to a specified value. Pretty much always 0.

#### `memfree`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(__variant *ptr) memfree = #385;
```

Part of FTE_MEMALLOC Frees a block of memory that was allocated with memalloc

#### `memgetval`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
__variant(__variant *dst, float ofs) memgetval = #388;
```

Looks up the 32bit value stored at a pointer-with-offset.

#### `memptradd`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
__variant*(__variant *base, float ofs) memptradd = #390;
```

Perform some pointer maths. Woo.

#### `memrealloc`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
__variant*(void *oldptr, int newsize) memrealloc = #0:memrealloc;
```

Allocate a new block of memory to replace an old one.

#### `memsetval`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(__variant *dst, float ofs, __variant val) memsetval = #389;
```

Changes the 32bit value stored at the specified pointer-with-offset.

#### `memstrsize`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(string s) memstrsize = #0:memstrsize;
```

strlen, except ignores utf-8

#### `min`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(float a, float b, ...) min = #94;
```

Part of DP_QC_MINMAXBOUND Returns the lowest value of its arguments.

#### `min`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(float,...) min = #43;
```

Part of DP_QC_MINMAXBOUND. Returns the smallest/largest supplied value.

#### `mod`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(float dividend, float divisor) mod = #245;
```

Returns the remainder of a division, so divisor must not be 0. fractional values will give different results. Or just use the % operator...

#### `mod`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(float val, float m) mod = #70;
```

Returns the floating-point remainder of val divided by m. Avoid a zero divisor.

#### `modelframecount`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(float mdlidx) modelframecount = #0:modelframecount;
```

Retrieves the number of frames in the specified model.

#### `modelnameforindex`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string(float mdlindex) modelnameforindex = #334;
```

Retrieves the name of the model based upon a precache index. This can be used to reduce csqc network traffic by enabling model matching (with getmodelindex).

#### `modulo`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(float dividend, float divisor) modulo = #0:modulo;
```

Returns the remainder of a division, so divisor must not be 0. fractional values will give different results... Or just use the % operator.

#### `movepic`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
DEP_CSQC void(string slot, float x, float y, float zone, optional entity player) movepic = #106;
```

Part of TEI_SHOWLMP2. Moves a legacy screen-picture slot to a new position/zone.

#### `movetogoal`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(float step) movetogoal = #67;
```

Runs lots and lots of fancy logic in order to try to step the entity the specified distance towards its goalentity.

#### `msprint`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
DEP void(float clientnum, string text,...) msprint = #6;
```

Deprecated MenuQC client-slot print interface; it is not an SSQC player entity print call.

#### `multicast`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
void(vector where, float set) multicast = #82;
```

Part of EXT_CSQC, FTE_QC_MULTICAST Once the MSG_MULTICAST network message buffer has been filled with data, this builtin is used to dispatch it to the given target, filtering by pvs for reduced network bandwidth.

#### `netaddress_resolve`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
string(string dnsname, optional float defport) netaddress_resolve = #625;
```

Performs a dna lookup and returns the first result as a string, which could be in a whole range of formats. Also blocks until completion.

#### `nextent`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
entity(entity) nextent = #37;
```

Returns the next live entity after the argument, or world at the end.

#### `nextent`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
entity(entity) nextent = #47;
```

Returns the following entity. Skips over removed entities. Returns world when passed the last valid entity.

#### `normalize`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector(vector v) normalize = #9;
```

Shorten or lengthen a direction vector such that it is only one quake unit long.

#### `normalize`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
vector(vector) normalize = #8;
```

Returns a vector with the same direction and unit length; handle zero-length input deliberately.

#### `num_for_edict`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(entity ent) num_for_edict = #512;
```

Converts an entity reference to this VM's entity-slot number, not a cross-VM entity reference.

#### `numentityfields`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float() numentityfields = #496;
```

Part of DP_QC_ENTITYDATA Gives the number of named entity fields. Note that this is not the size of an entity, but rather just the number of unique names (ie: vectors use 4 names rather than 3).

#### `objerror`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(string e) objerror = #11;
```

Displays a non-fatal easily readable error message concerning the self entity, including a field dump. self will be removed!

#### `objerror`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string err,...) objerror = #3;
```

For some reason this has been redefined as non-fatal, and as it won't force the user to look at the console it'll generally be ignored completely so really what's the point? Other than as a convoluted way to remove(self) that is.

#### `openportal`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity portal, float state) openportal = #207;
```

Opens or closes the portals associated with a door or some such on q2 or q3 maps. On Q2BSPs, the entity should be the 'func_areaportal' entity - its style field will say which portal to open. On Q3BSPs, the entity is the door itself, the portal will be determined by the two areas found from a preceding setorigin call.

#### `parseentitydata`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(entity e, string s, optional float offset) parseentitydata = #613;
```

Reads a single entity's fields into an already-spawned entity. s should contain field pairs like in a saved game: {"foo1" "bar" "foo2" "5"}. Returns <=0 on failure, otherwise returns the offset in the string that was read to.

#### `particle`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(vector pos, vector dir, float colour, float count) particle = #48;
```

Spawn 'count' particles around 'pos' moving in the direction 'dir', with a palette colour index between 'colour' and 'colour+8'.

#### `particle2`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org, vector dmin, vector dmax, float colour, float effect, float count) particle2 = #215;
```

Part of FTE_HEXEN2. Emits a Hexen II-style particle effect using the supplied direction range, box or radius and effect parameters.

#### `particle3`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org, vector box, float colour, float effect, float count) particle3 = #216;
```

Part of FTE_HEXEN2. Emits a Hexen II-style particle effect using the supplied direction range, box or radius and effect parameters.

#### `particle4`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org, float radius, float colour, float effect, float count) particle4 = #217;
```

Part of FTE_HEXEN2. Emits a Hexen II-style particle effect using the supplied direction range, box or radius and effect parameters.

#### `particleeffectnum`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
float(string effectname) particleeffectnum = #335;
```

Part of DP_ENT_TRAILEFFECTNUM, FTE_SV_POINTPARTICLES Precaches the named particle effect. If your effect name is of the form 'foo.bar' then particles/foo.cfg will be loaded by the client if foo.bar was not already defined. Different engines will have different particle systems, this specifies the QC API only.

#### `particleeffectquery`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
string(float efnum, float body) particleeffectquery = #374;
```

Retrieves either the name or the body of the effect with the given number. The effect body is regenerated from internal state, and can be changed before being reapplied via the localcmd builtin.

#### `patch_create`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int(float modelidx, int oldpatchid, patchvert_t *in_controlverts, patchinfo_t in_info) patch_create = #0:patch_create;
```

Inserts a new patch into the model. Return value is the new patch's id.

#### `patch_evaluate`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int(patchvert_t *in_controlverts, patchvert_t *out_renderverts, int maxout, patchinfo_t *inout_info) patch_evaluate = #0:patch_evaluate;
```

Calculates the geometry of a hyperthetical patch.

#### `patch_getcp`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int(float modelidx, int patchid, patchvert_t *out_controlverts, int maxcp, patchinfo_t *out_info) patch_getcp = #0:patch_getcp;
```

Queries a patch's information. You must pre-allocate the face array for the builtin to write to. Return value is the total number of control verts that were retrieved, 0 on error.

#### `patch_getmesh`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int(float modelidx, int patchid, patchvert_t *out_verts, int maxverts, patchinfo_t *out_info) patch_getmesh = #0:patch_getmesh;
```

Queries a patch's information. You must pre-allocate the face array for the builtin to write to. Return value is the total number of control verts that were retrieved, 0 on error.

#### `physics_addforce`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity e, vector force, vector relative_ofs) physics_addforce = #541;
```

Apply some impulse directional force upon a MOVETYPE_PHYSICS entity.

#### `physics_addtorque`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity e, vector torque) physics_addtorque = #542;
```

Apply some impulse rotational force upon a MOVETYPE_PHYSICS entity.

#### `physics_enable`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity e, float physics_enabled) physics_enable = #540;
```

Enable or disable the physics attached to a MOVETYPE_PHYSICS entity. Entities which have been disabled in this way will stop taking so much cpu time.

#### `physics_supported`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(optional float forcestate) physics_supported = #0:physics_supported;
```

Queries whether rigid body physics is enabled or not. CSQC and SSQC may report different values. If the force argument is specified then the engine will try to activate or release physics (returning the new state, which may fail if plugins or dlls are missing). Note that restarting the physics engine is likely to result in hitches when collision trees get generated. The state may change if a plugin is disabled mid-map.

#### `pointcontents`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(vector pos) pointcontents = #41;
```

Checks the given point to see what is there. Returns one of the CONTENTS_* constants. Just because a point is empty does not mean that the player can stand there due to the size of the player - use tracebox for such tests.

#### `pointcontentsmask`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
__uint(vector pos, optional float worldonly=1) pointcontentsmask = #0:pointcontentsmask;
```

Checks the given point to see what is there. Returns a mask of the CONTENTBIT_* constants. Just because a point is empty does not mean that the player can stand there due to the size of the player - use tracebox for such tests.

#### `pointerstat`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float num, float type, __variant *address) pointerstat = #0:pointerstat;
```

Specifies what data to use in order to send various stats, in a non-client-specific way. num and type are as in clientstat, address however, is the address of the variable you would like to use (pass &foo).

#### `pointparticles`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(float effectnum, vector origin, optional vector dir, optional float count) pointparticles = #337;
```

Part of FTE_SV_POINTPARTICLES Spawn a load of particles from the given effect at the given point traveling or aiming along the direction specified. The number of particles are scaled by the count argument. For regular particles, the dir vector is multiplied by the 'veladd' property (while orgadd will push the particles along it). Decals will use it as a hint to align to the correct surface. In both cases, it should normally be a unit vector, but other lengths will still work. If it has length 0 then FTE will assume downwards.

#### `pointsound`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector origin, string sample, float volume, float attenuation) pointsound = #483;
```

Part of DP_SV_POINTSOUND. Starts a spatial sound at a fixed world position instead of following an entity.

#### `pow`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(float value, float exp) pow = #97;
```

Part of DP_QC_SINCOSSQRTPOW Computes an exponent, or 'raises value to the power of exp', aka multiplying 'value' by itself 'exp' times. Equivelent to the C function, so fractional exponents are allowed, eg 0.5 to double up as a square root or 1.0/3 for cube root etc.

#### `pow`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(float,float) pow = #46;
```

Part of DP_QC_SINCOSSQRTPOW. Raises the first argument to the second argument's power.

#### `precache_file`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
string(string file) precache_file = #28;
```

Attempts to download the named file from the current server, if it isn't found locally. Not very useful as menuqc is normally meant to work before joining servers too.

#### `precache_file`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string(string s) precache_file = #68;
```

This builtin does nothing. It was used only as a hint for pak generation.

#### `precache_file2`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string(string str) precache_file2 = #77;
```

Identical alternative to the non-2 precache. The different name allowed the vanilla qcc to know to bake these files into pak1.pak instead of pak0.pak - useful as a reminder for all those cheapskates hacking/using engines to use the shareware content with fancy mods.

#### `precache_model`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string(string s) precache_model = #20;
```

Precaches a model, making it known to clients and loading it from disk if it has a .bsp extension. This builtin (strongly) should be called during spawn functions. This must be called for each model name before setmodel may use that model name. Modelindicies precached in SSQC will always be positive. CSQC precaches will be negative if they are not also on the server.

#### `precache_model`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string mname) precache_model = #91;
```

Menuqc-specific version.

#### `precache_model2`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string(string str) precache_model2 = #75;
```

Identical alternative to the non-2 precache. The different name allowed the vanilla qcc to know to bake these files into pak1.pak instead of pak0.pak - useful as a reminder for all those cheapskates hacking/using engines to use the shareware content with fancy mods.

#### `precache_pic`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
string(string name, optional float flags) precache_pic = #317;
```

Forces the engine to load the named image. Flags are a bitmask of the PRECACHE_PIC_* flags.

#### `precache_pic`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
string(string name, optional float flags) precache_pic = #452;
```

Loads/caches an image for 2D drawing; flags control loading behavior. It does not create a world sprite model.

#### `precache_sound`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string(string s) precache_sound = #19;
```

Precaches a sound, making it known to clients and loading it from disk. This builtin (strongly) should be called during spawn functions. This builtin must be called for the sound before the sound builtin is called, or it might not even be heard.

#### `precache_sound`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
string(string sample) precache_sound = #29;
```

Registers a game-relative sound path before use; normally called during map spawning.

#### `precache_sound2`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string(string str) precache_sound2 = #76;
```

Identical alternative to the non-2 precache. The different name allowed the vanilla qcc to know to bake these files into pak1.pak instead of pak0.pak - useful as a reminder for all those cheapskates hacking/using engines to use the shareware content with fancy mods.

#### `precache_vwep_model`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
float(string mname) precache_vwep_model = #532;
```

Part of ZQ_VWEP. Registers a third-person visible weapon model using the ZQ_VWEP interface.

#### `print`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(string s, ...) print = #339;
```

Part of DP_SV_PRINT Unconditionally print on the local system's console, even in ssqc (doesn't care about the value of the developer cvar).

#### `print`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
void(string text,...) print = #4;
```

Part of DP_SV_PRINT Hello, world. Shoves junk on the console. Hopefully people will bother to read it, maybe.

#### `processmodelevents`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float modidx, float framenum, __inout float basetime, float targettime, void(float timestamp, int code, string data) callback) processmodelevents = #0:processmodelevents;
```

Part of FTE_GFX_MODELEVENTS Calls a callback for each event that has been reached. Basetime is set to targettime.

#### `project`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
vector (vector v) project = #311;
```

Transform a 3d world-space point into a 2d screen-space point, according the various origin+angle+fov etc settings set via setproperty.

#### `pushmove`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(entity pusher, vector move, vector amove) pushmove = #0:pushmove;
```

Attempts to move/rotate a pusher and affected entities using the engine's pusher collision handling.

#### `putentityfieldstring`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(float fieldnum, entity ent, string s) putentityfieldstring = #500;
```

Part of DP_QC_ENTITYDATA. Parses text into a selected entity field; validate field type, index and input.

#### `queueaudio`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(int hz, int channels, int type, void *data, unsigned int frames) queueaudio = #0:queueaudio;
```

Queues raw audio. For best results do not vary the hz/channels values. type: -8=__int8, -16==__int16

#### `R_BeginPolygon`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(string texturename, optional float flags, optional float is2d) R_BeginPolygon = #306;
```

Specifies the shader to use for the following polygons, along with optional flags. If is2d, the polygon will be drawn as soon as the EndPolygon call is made, rather than waiting for renderscene. This allows complex 2d effects.

#### `R_EndPolygon`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void() R_EndPolygon = #308;
```

Ends the current polygon. At least 3 verticies must have been specified. You do not need to call beginpolygon again if you wish to draw another polygon with the same shader.

#### `R_EndPolygonRibbon`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float radius, vector texcoordbias) R_EndPolygonRibbon = #0:R_EndPolygonRibbon;
```

Ends the current primitive and duplicates each vertex sideways into a ribbon. The texcoordbias will be added to each duplicated vertex allowing for regular 2d textures. At least 2 verticies must have been specified. You do not need to call beginpolygon again if you wish to draw another polygon with the same shader.

#### `R_PolygonVertex`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex = #307;
```

Specifies a polygon vertex with its various properties.

#### `r_readimage`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
int*(string filename, __out int width, __out int height, __out int format) r_readimage = #0:r_readimage;
```

Part of FTE_CSQC_RAWIMAGES Reads and decodes an image from disk, providing raw R8G8B8A8 pixel data. Should not be used for dds or ktx etc formats. Returns __NULL__ if the image could not be read for any reason. Use memfree to free the data once you're done with it.

#### `r_uploadimage`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(string imagename, int width, int height, void *pixeldata, optional int datasize, optional int format) r_uploadimage = #0:r_uploadimage;
```

Part of FTE_CSQC_RAWIMAGES Updates a texture with the specified rgba data (uploading it to the gpu). Will be created if needed. If datasize is specified then the image is decoded (eg .ktx or .dds data) instead of being raw R8G8B8A data. You'll typically want shaderforname to also generate a shader to use the texture.

#### `random`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float() random = #12;
```

Pseudorandom float between zero and one. The endpoint can be one; clamp computed array indices.

#### `random`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float() random = #7;
```

Returns a random value between 0 and 1. Be warned, this builtin can return 1 in most engines, which can break arrays.

#### `randomvec`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
vector() randomvec = #41;
```

Part of DP_QC_RANDOMVEC Returns a random vector with length <= 1 (each axis may be negative).

#### `randomvec`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
vector() randomvec = #91;
```

Part of DP_QC_RANDOMVEC Returns a random vector with length <= 1 (each axis may be negative).

#### `readangle`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float() readangle = #365;
```

Reads a value matching the unspecified precision written ONLY by WriteAngle.

#### `readbyte`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float() readbyte = #360;
```

Reads an unsigned 8-bit value, pair with WriteByte.

#### `readchar`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float() readchar = #361;
```

Reads a signed 8-bit value. Paired with WriteChar.

#### `readcoord`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float() readcoord = #364;
```

Reads a value matching the unspecified precision written ONLY by WriteCoord.

#### `readdouble`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
__double() readdouble = #0:readdouble;
```

Reads a double-precision float without any truncation nor conversions. Data MUST have originally been written with WriteDouble.

#### `readentitynum`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float() readentitynum = #368;
```

Reads the serverside index of an entity, paired with WriteEntity. There may be nothing else known about the entity yet, so the result typically needs to be saved as-is and re-looked up each frame. This can be done via getentity(NUM, GE_*) for non-csqc ents, or findentity(world,entnum,NUM) - both of which can fail due to latency.

#### `readfloat`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float() readfloat = #367;
```

Reads a float without any truncation nor conversions. Data MUST have originally been written with WriteFloat.

#### `readint`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int() readint = #0:readint;
```

Reads a 32bit singled int without any conversions to float, otherwise interchangable with readlong.

#### `readint64`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
__int64() readint64 = #0:readint64;
```

Reads a 64bit signed int. Paired with WriteInt64.

#### `readlong`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float() readlong = #363;
```

Reads a signed 32-bit value. Paired with WriteLong or WriteInt.

#### `ReadPicture`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
string() ReadPicture = #501;
```

Reads a picture that was written by ReadPicture, and returns a name that can be used in drawpic and other 2d drawing functions. In FTE, this acts as a readstring-with-downloadcheck - the image will appear normally once it has been downloaded, but its size may be incorrect until then.

#### `readshort`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float() readshort = #362;
```

Reads a signed 16-bit value. Paired with WriteShort.

#### `readstring`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
string() readstring = #366;
```

Reads a null-terminated string.

#### `readuint64`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
__uint64() readuint64 = #0:readuint64;
```

Reads a 64bit unsigned int. Paired with WriteUInt64.

#### `redirectcmd`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP void(entity to, string str) redirectcmd = #101;
```

Part of ??MVDSV_BUILTINS Executes a single console command, and sends the text generated by it to the specified player. The command will be executed at the end of the frame once QC is no longer running - you may wish to pre/postfix it with 'echo'.

#### `refreshhostcache`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(optional float dopurge) refreshhostcache = #620;
```

Part of FTE_CSQC_SERVERBROWSER Starts a new ping sequence

#### `registercommand`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(string cmdname, optional string desc) registercommand = #352;
```

Register the given console command, for easy console use. Console commands that are later used will invoke CSQC_ConsoleCommand/m_consolecommand/ConsoleCmd according to module.

#### `registercvar`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(string cvarname, string defaultvalue, optional float flags) registercvar = #93;
```

Part of DP_REGISTERCVAR Creates a new cvar on the fly. If it does not already exist, it will be given the specified value. If it does exist, this is a no-op. This builtin has the limitation that it does not apply to configs or commandlines. Such configs will need to use the set or seta command causing this builtin to be a noop. In engines that support it, you will generally find the autocvar feature easier and more efficient to use.

#### `registercvar`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(string name, string value, optional float flags) registercvar = #42;
```

Part of DP_REGISTERCVAR Creates the cvar if it didn't already exist. This presents issues for setting those cvars via startup configs of course, and autocvars are easier but I suppose they don't get any flags (which are ignored anyway, of course).

#### `RegisterTempEnt`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
float(float attributes, string effectname, ...) RegisterTempEnt = #208;
```

Part of FTE_PEXT_CUSTOMTENTS. Registers a custom temporary-effect type/format for supporting clients; returns its type identifier.

#### `releasecustomskin`

**Scope:** CSQC. **Compatibility:** FTE extension.

```qc
void(float skinobj) releasecustomskin = #379;
```

Part of FTE_QC_CUSTOMSKINS Lets the engine know that the skin will no longer be needed. Thanks to refcounting any ents with the skin already applied will retain their skin until later changed. It is valid to destroy a skin just after applying it to an ent in the same function that it was created in, as the skin will only be destroyed once its refcount rops to 0.

#### `remapshader`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(string shadername, string replacement, float timeoffset) remapshader = #0:remapshader;
```

All surfaces drawn with the specified shader will instead be drawn using the specified replacement shader. Shaders can be remapped to something else later by using the same source shadername. This is mostly useful for worldmodel surfaces (eg showing which team is currently winning). Entities should generally use setcustomskin or forceshader instead. Remaps will be forgotten on vid_reload, but can be reapplied via CSQC_RendererRestarted.

#### `remove`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity e) remove = #15;
```

Destroys the given entity and clears some limited fields (including model, modelindex, solid, classname). Any references to the entity following the call are an error. After half a second the entity will be reused, in the interim you can unfortunatly still read its fields to see if the reference is no longer valid.

#### `remove`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(entity) remove = #23;
```

Removes the given entity. Do not continue using it; its slot may be reused.

#### `removeentity`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(entity ent) removeentity = #0:removeentity;
```

Undoes all addentities added to the scene from the given entity, without removing ALL entities (useful for splitscreen/etc, readd modified versions as desired).

#### `removeinstant`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(entity e) removeinstant = #0:removeinstant;
```

Same thing as the regular remove builtin, but bypasses the half-second rule. The entity slot may be reused instantly. Be CERTAIN that you have no lingering references, because if they're followed they will end up poking an entirely different type of entity! So only use this where you're sure its safe.

#### `renderscene`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void() renderscene = #304;
```

Draws all entities, polygons, and particles on the rentity list (which were added via addentities or addentity), using the various view properties set via setproperty. There is no ordering dependancy. The scene must generally be cleared again before more entities are added, as entities will persist even over to the next frame. You may call this builtin multiple times per frame, but should only be called from CSQC_UpdateView.

#### `resethostcachemasks`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void() resethostcachemasks = #615;
```

Part of FTE_CSQC_SERVERBROWSER Resets filters set by sethostcachemask*

#### `resorthostcache`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void() resorthostcache = #618;
```

Part of FTE_CSQC_SERVERBROWSER Reevalutes whether each server should be visible. Resorts servers. server ids passed to gethostcachestring are likely to change, and may become out of bounds.

#### `resourcestatus`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(float resourcetype, float tryload, string resourcename) resourcestatus = #286;
```

resourcetype must be one of the RESTYPE_ constants. Returns one of the RESSTATE_ constants. Tryload 0 is a query only. Tryload 1 will attempt to reload the content if it was flushed.

#### `respawnedict`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
entity(float entnum, optional __out float wasspawned) respawnedict = #0:respawnedict;
```

Acts like edict_num returning a specific entity number, but also marks it as spawned. If it was previously spawned then all of its prior field data will be LOST (you may wish to use wasfreed(edict_num(idx)) to check.

#### `rint`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(float) rint = #34;
```

Rounds to the nearest integer value, returned as a float.

#### `rint`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(float) rint = #36;
```

Rounds the given float up or down to the closest integeral value. X.5 rounds away from 0

#### `rmtree`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(string path) rmtree = #654;
```

Dangerous, but sandboxed to data/

#### `rotatevectorsbyangle`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(vector angle) rotatevectorsbyangle = #235;
```

rotates the v_forward,v_right,v_up matrix by the specified angles.

#### `rotatevectorsbytag`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
vector(entity ent, float tagnum) rotatevectorsbytag = #244;
```

Transforms the current v_forward/v_right/v_up basis by an entity's model attachment/tag transform.

#### `rotatevectorsbyvectors`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(vector fwd, vector right, vector up) rotatevectorsbyvectors = #236;
```

Transforms the existing v_forward/v_right/v_up basis by the supplied basis vectors.

#### `route_calculate`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(entity ent, vector dest, int denylinkflags, void(entity ent, vector dest, int numnodes, nodeslist_t *nodelist) callback) route_calculate = #0:route_calculate;
```

Begin calculating a route. The callback function will be called once the route has finished being calculated. The route must be memfreed once it is no longer needed. The route must be followed in reverse order (ie: the first node that must be reached is at index numnodes-1). If no route is available then the callback will be called with no nodes.

#### `runstandardplayerphysics`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity ent) runstandardplayerphysics = #347;
```

Perform the engine's standard player movement prediction upon the given entity using the input_* globals to describe movement.

#### `search_begin`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
searchhandle(string pattern, enumflags:float{ SB_CASEINSENSITIVE=1<<0 , SB_FULLPACKAGEPATH=1<<1 , SB_ALLOWDUPES=1<<2 , SB_FORCESEARCH=1<<3 , SB_MULTISEARCH=1<<4 , SB_NAMESORT=1<<5 } flags, float quiet, optional string filterpackage) search_begin = #444;
```

Part of DP_QC_FS_SEARCH, DP_QC_FS_SEARCH_PACKFILE initiate a filesystem scan based upon filenames. Be sure to call search_end on the returned handle. Returns a negative value on error.

#### `search_begin`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
searchhandle(string pattern, enumflags:float{SB_CASEINSENSITIVE=1<<0,SB_FULLPACKAGEPATH=1<<1,SB_ALLOWDUPES=1<<2,SB_FORCESEARCH=1<<3,SB_MULTISEARCH=1<<4,SB_NAMESORT=1<<5} flags, float quiet, optional string package) search_begin = #74;
```

Part of DP_QC_FS_SEARCH, DP_QC_FS_SEARCH_PACKFILE. Part of DP_QC_FS_SEARCH, DP_QC_FS_SEARCH_PACKFILE initiate a filesystem scan based upon filenames. Be sure to call search_end on the returned handle. Returns a negative value on error.

#### `search_end`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(searchhandle handle) search_end = #445;
```

Part of DP_QC_FS_SEARCH, DP_QC_FS_SEARCH_PACKFILE. Releases a filesystem search handle when finished enumerating its results.

#### `search_end`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
void(searchhandle handle) search_end = #75;
```

Part of DP_QC_FS_SEARCH, DP_QC_FS_SEARCH_PACKFILE. Releases a filesystem search handle when finished enumerating its results.

#### `search_fopen`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
filestream(searchhandle handle, float num) search_fopen = #0:search_fopen;
```

Opens the file directly, without getting confused about entries from other packages. Read access only.

#### `search_getfilemtime`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
string(searchhandle handle, float num) search_getfilemtime = #0:search_getfilemtime;
```

Part of FTE_QC_FS_SEARCH_SIZEMTIME Retrieves modification time of one of the files.

#### `search_getfilename`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
string(searchhandle handle, float num) search_getfilename = #447;
```

Part of DP_QC_FS_SEARCH, DP_QC_FS_SEARCH_PACKFILE Retrieves name of one of the files that was found by the initial search.

#### `search_getfilename`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
string(searchhandle handle, float num) search_getfilename = #77;
```

Part of DP_QC_FS_SEARCH, DP_QC_FS_SEARCH_PACKFILE. Part of DP_QC_FS_SEARCH, DP_QC_FS_SEARCH_PACKFILE Retrieves name of one of the files that was found by the initial search.

#### `search_getfilesize`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(searchhandle handle, float num) search_getfilesize = #0:search_getfilesize;
```

Part of FTE_QC_FS_SEARCH_SIZEMTIME Retrieves the size of one of the files that was found by the initial search.

#### `search_getpackagename`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(searchhandle handle, float num) search_getpackagename = #0:search_getpackagename;
```

Retrieves the name of the package containing the file. Search with SB_FULLPACKAGEPATH to see gamedir/package info

#### `search_getsize`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(searchhandle handle) search_getsize = #446;
```

Part of DP_QC_FS_SEARCH, DP_QC_FS_SEARCH_PACKFILE Retrieves the number of files that were found.

#### `search_getsize`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(searchhandle handle) search_getsize = #76;
```

Part of DP_QC_FS_SEARCH, DP_QC_FS_SEARCH_PACKFILE. Part of DP_QC_FS_SEARCH, DP_QC_FS_SEARCH_PACKFILE Retrieves the number of files that were found.

#### `sendevent`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(string evname, string evargs, ...) sendevent = #359;
```

Invoke CSEv_evname_evargs in ssqc. evargs must be a string of initials refering to the types of the arguments to pass. v=vector, e=entity(.entnum field is sent), f=float, i=int. 6 arguments max - you can get more if you pack your floats into vectors.

#### `sendpacket`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(string destaddress, string content) sendpacket = #242;
```

Part of FTE_QC_SENDPACKET Sends a UDP packet to the specified destination. Note that the payload will be prefixed with four 255 bytes as a sort of security feature.

#### `serverkey`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
string(string key) serverkey = #354;
```

Look up a key in the server's public serverinfo string. If the key contains binary data then it will be truncated at the first null.

#### `serverkeyblob`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
int(string key, optional void *ptr, int maxsize) serverkeyblob = #0:serverkeyblob;
```

Part of FTE_INFOBLOBS Version of serverkey that returns data as a blob (ie: binary data that may contain nulls). Returns the full blob size, even if truncated (pass maxsize=0 to query required storage).

#### `serverkeyfloat`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(string key, optional float assumevalue) serverkeyfloat = #0:serverkeyfloat;
```

Version of serverkey that returns the value as a float (which avoids tempstrings).

#### `setattachment`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(entity e, entity tagentity, string tagname) setattachment = #443;
```

Part of DP_GFX_QUAKE3MODELTAGS. Attaches an entity to another entity's named model tag; both model/tag and client protocol support matter.

#### `setbindmaps`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(vector bm) setbindmaps = #632;
```

Selects the active bind maps using the supplied vector.

#### `setcolor`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
__deprecated("No RGB support.") void(entity ent, float colours) setcolor = #401;
```

Part of DP_SV_SETCOLOR Changes a player's colours. The bits 0-3 are the lower/trouser colour, bits 4-7 are the upper/shirt colours.

#### `setcursormode`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(float usecursor, optional string cursorimage, optional vector hotspot, optional float scale) setcursormode = #343;
```

Pass TRUE if you want the engine to release the mouse cursor (absolute input events + touchscreen mode). Pass FALSE if you want the engine to grab the cursor (relative input events + standard looking). If the image name is specified, the engine will use that image for a cursor (use an empty string to clear it again), in a way that will not conflict with the console. Images specified this way will be hardware accelerated, if supported by the platform/port.

#### `setcustomskin`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(entity e, string skinfilename, optional string skindata) setcustomskin = #376;
```

Part of FTE_QC_CUSTOMSKINS Sets an entity's skin overrides to a new skin object. Releases the entities old skin (refcounted).

#### `sethostcachemasknumber`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float mask, float fld, float num, float op) sethostcachemasknumber = #617;
```

Part of FTE_CSQC_SERVERBROWSER sethostcachemaskstring, but with a numeric reference value instead of using string comparisons

#### `sethostcachemaskstring`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float mask, float fld, string str, float op) sethostcachemaskstring = #616;
```

Part of FTE_CSQC_SERVERBROWSER Adds a filter rule for the server queries.

#### `sethostcachesort`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float fld, float descending) sethostcachesort = #619;
```

Part of FTE_CSQC_SERVERBROWSER Specifies which field to sort the server list by, and whether it should be ascending or descending. You will need to resorthostcache() for this to take effect.

#### `setkeybind`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(float key, string bind, optional float bindmap, optional float modifier) setkeybind = #630;
```

Assigns a console command binding to a key, optionally for a bind map/modifier; respect player settings.

#### `setkeydest`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(float dest) setkeydest = #601;
```

Sets the keyboard input destination (game, console or menu according to the engine's routing identifiers).

#### `SetListener`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(vector origin, vector forward, vector right, vector up, optional float reverbtype) SetListener = #351;
```

Sets the position of the view, as far as the audio subsystem is concerned. This should be called once per CSQC_UpdateView as it will otherwise revert to default. For reverbtype, see setup_reverb or treat as 'underwater'.

#### `setlocalinfo`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(string keyname, optional void *outptr, int size) setlocalinfo = #0:setlocalinfo;
```

Changes the server's private localinfo. This data will be available for the following map, and will *usually* reload with saved games.

#### `setlocaluserinfo`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float seat, string keyname, string newvalue) setlocaluserinfo = #0:setlocaluserinfo;
```

Change a userinfo key for the specified local player seat, equivelent to the setinfo console command. The server will normally forward the setting to other clients.

#### `setlocaluserinfoblob`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float seat, string keyname, void *outptr, int size) setlocaluserinfoblob = #0:setlocaluserinfoblob;
```

Part of FTE_INFOBLOBS Sets the userinfo key to a blob that may contain nulls etc. Keys with a leading underscore will be visible to only the server (for user-specific binary settings).

#### `setmodel`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity e, string m) setmodel = #3;
```

Looks up m in the model precache list, and sets both e.model and e.modelindex to match. BSP models will set e.mins and e.maxs accordingly, other models depend upon the value of sv_gameplayfix_setmodelrealbox - for compatibility you should always call setsize after all pickups or non-bsp models. Also relinks collision state.

#### `setmodel`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(entity ent, string mname) setmodel = #90;
```

Menuqc-specific version.

#### `setmodelindex`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(entity e, float mdlindex) setmodelindex = #333;
```

Sets a model by precache index instead of by name. Otherwise identical to setmodel.

#### `setmousepos`

**Scope:** CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(vector newpos) setmousepos = #0:setmousepos;
```

Warps the mouse cursor to the given location. Should normally only be done following setcursormode(TRUE,...). The warp MAY be visible through *_InputEvent, but normally be seen as an IE_ABSMOUSE event anyway. Not all systems support cursor warping (or even cursors), so this is a hint only and you should not depend upon it.

#### `setmousetarget`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(float trg) setmousetarget = #603;
```

Sets/gets the legacy mouse event destination; prefer the documented input routing API for new code.

#### `setorigin`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity e, vector o) setorigin = #2;
```

Changes e's origin to be equal to o. Also relinks collision state (as well as setting absmin+absmax), which is required after changing .solid

#### `setorigin`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(entity ent, vector neworg) setorigin = #92;
```

Menuqc-specific version.

#### `setpause`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(float pause) setpause = #531;
```

SSQC: Sets whether the server should or should not be paused. CSQC: Only works in singleplayer, suitable for menu auto-pause. To pause in multiplayer use eg localcmd("cmd pause ") to ask the server side to pause. Pause state between modules will be ORed, along with engine reasons for auto pausing.

#### `setproperty`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(float property, ...) setproperty = #303;
```

Allows you to override default view properties like viewport, fov, and whether the engine hud will be drawn. Different VF_ values have slightly different arguments, some are vectors, some floats.

#### `setsendneeded`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(entity ent, vector sendflags, entity unicastplayer) setsendneeded = #0:setsendneeded;
```

Flags the entity as needing to be resent. This builtin allows for more bits than supported by the SendEntity field, as well as allows flagging sends to specific players.

#### `setsensitivityscaler`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float sens) setsensitivityscaler = #346;
```

Temporarily scales the player's mouse sensitivity based upon something like zoom, avoiding potential cvar saving and thus corruption.

#### `setserverkey`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(string key, void *ptr, optional int size) setserverkey = #0:setserverkey;
```

Changes the server's serverinfo.

#### `setsize`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity e, vector min, vector max) setsize = #4;
```

Sets the e's mins and maxs fields. Also relinks collision state, which sets absmin and absmax too.

#### `setspawnparms`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(entity player) setspawnparms = #78;
```

Overwrites the parm* globals to match those of the specified client (restored from the last map's SetChangeParms, otherwise SetNewParms)

#### `setup_reverb`

**Scope:** CSQC. **Compatibility:** FTE extension.

```qc
void(float reverbslot, reverbinfo_t *reverbinfo, int sizeofreverinfo_t) setup_reverb = #0:setup_reverb;
```

Part of FTE_CSQC_REVERB Reconfigures a reverb slot for weird effects. Slot 0 is reserved for no effects. Slot 1 is reserved for underwater effects. Reserved slots will be reinitialised on snd_restart, but can otherwise be changed. These reverb slots can be activated with SetListener. Note that reverb will currently only work when using OpenAL.

#### `setwatchpoint`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(string name, float evaltype, void *ptr) setwatchpoint = #0:setwatchpoint;
```

For debugging. Set a watchpoint to monitor an address for changes. This is equivelent to using the watchpoint_* console command, but done from qc can be used on abitrary addresses.

#### `setwindowcaption`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(string newcaption) setwindowcaption = #0:setwindowcaption;
```

Part of FTE_CSQC_WINDOWCAPTION Replaces the title of the game window, as seen when task switching or just running in windowed mode.

#### `shaderforname`

**Scope:** CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(string shadername, optional string defaultshader, ...) shaderforname = #238;
```

Part of FTE_FORCESHADER Caches the named shader and returns a handle to it. If the shader could not be loaded from disk (missing file or ruleset_allow_shaders 0), it will be created from the 'defaultshader' string if specified, or a 'skin shader' default will be used. defaultshader if not empty should include the outer {} that you would ordinarily find in a shader.

#### `showpic`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
DEP_CSQC void(string slot, string picname, float x, float y, float zone, optional entity player) showpic = #104;
```

Part of TEI_SHOWLMP2 Instructs the client that it should display some image somewhere on the screen (relative to the specified zone, to deal with unknown video modes). This is an earlier builtin form of the showpic console command, and thus lacks support for touchscreen events.

#### `sin`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(float angle) sin = #60;
```

Part of DP_QC_SINCOSSQRTPOW Forgive me father, for I have trigonometry homework. Uses radians.

#### `sin`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(float) sin = #38;
```

Part of DP_QC_SINCOSSQRTPOW. Computes sine/cosine with an angle measured in radians, not degrees.

#### `skel_build`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(float skel, entity ent, float modelindex, float retainfrac, float firstbone, float lastbone, optional float addfrac) skel_build = #264;
```

Part of FTE_CSQC_SKELETONOBJECTS Animation data (according to the entity's frame info) is pulled from the specified model and blended into the specified skeletal object. If retainfrac is set to 0 on the first call and 1 on the others, you can blend multiple animations together according to the addfrac value. The final weight should be 1. Other values will result in scaling and/or other weirdness. You can use firstbone and lastbone to update only part of the skeletal object, to allow legs to animate separately from torso, use 0 for both arguments to specify all, as bones are 1-based.

#### `skel_build_ptr`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(float skel, int numblends, skelblend_t *weights, int structsize) skel_build_ptr = #0:skel_build_ptr;
```

Like skel_build, but slightly simpler.

#### `skel_copybones`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float skeldst, float skelsrc, float startbone, float entbone) skel_copybones = #274;
```

Part of FTE_CSQC_SKELETONOBJECTS Copy bone data from one skeleton directly into another.

#### `skel_create`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(float modlindex, optional float useabstransforms) skel_create = #263;
```

Part of FTE_CSQC_SKELETONOBJECTS Allocates a new uninitiaised skeletal object, with enough bone info to animate the given model. eg: self.skeletonobject = skel_create(self.modelindex);

#### `skel_delete`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float skel) skel_delete = #275;
```

Part of FTE_CSQC_SKELETONOBJECTS Deletes a skeletal object. The actual delete is delayed, allowing the skeletal object to be deleted in an entity's predraw function yet still be valid by the time the addentity+renderscene builtins need it. Also uninstanciates any ragdoll currently in effect on the skeletal object.

#### `skel_find_bone`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(float skel, string tagname) skel_find_bone = #268;
```

Part of FTE_CSQC_SKELETONOBJECTS Finds a bone by its name, from the model that was used to create the skeletal object.

#### `skel_get_boneabs`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
vector(float skel, float bonenum) skel_get_boneabs = #270;
```

Part of FTE_CSQC_SKELETONOBJECTS Gets the bone position and orientation relative to the entity. Return value is the offset, and v_forward, v_right, v_up contain the orientation. Use gettaginfo for world coord+orientation.

#### `skel_get_bonename`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
string(float skel, float bonenum) skel_get_bonename = #266;
```

Part of FTE_CSQC_SKELETONOBJECTS Retrieves the name of the specified bone. Mostly only for debugging.

#### `skel_get_boneparent`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(float skel, float bonenum) skel_get_boneparent = #267;
```

Part of FTE_CSQC_SKELETONOBJECTS Retrieves which bone this bone's position is relative to. Bone 0 refers to the entity's position rather than an actual bone

#### `skel_get_bonerel`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
vector(float skel, float bonenum) skel_get_bonerel = #269;
```

Part of FTE_CSQC_SKELETONOBJECTS Gets the bone position and orientation relative to the bone's parent. Return value is the offset, and v_forward, v_right, v_up contain the orientation.

#### `skel_get_numbones`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(float skel) skel_get_numbones = #265;
```

Part of FTE_CSQC_SKELETONOBJECTS Retrives the number of bones in the model. The valid range is 1<=bone<=numbones.

#### `skel_mmap`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
float*(float skel) skel_mmap = #282;
```

Part of FTE_QC_RAGDOLL_WIP Map the bones in VM memory. They can then be accessed via pointers. Each bone is 12 floats, the four vectors interleaved (sadly).

#### `skel_postmul_bone`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float skel, float bonenum, vector org, optional vector fwd, optional vector right, optional vector up) skel_postmul_bone = #0:skel_postmul_bone;
```

Transforms a single bone by a matrix. You can use makevectors to generate a rotation matrix from an angle.

#### `skel_premul_bone`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float skel, float bonenum, vector org, optional vector fwd, optional vector right, optional vector up) skel_premul_bone = #272;
```

Part of FTE_CSQC_SKELETONOBJECTS Transforms a single bone by a matrix. You can use makevectors to generate a rotation matrix from an angle.

#### `skel_premul_bones`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float skel, float startbone, float endbone, vector org, optional vector fwd, optional vector right, optional vector up) skel_premul_bones = #273;
```

Part of FTE_CSQC_SKELETONOBJECTS Transforms an entire consecutive range of bones by a matrix. You can use makevectors to generate a rotation matrix from an angle, but you'll probably want to divide the angle by the number of bones.

#### `skel_ragupdate`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
float(entity skelent, string dollcmd, float animskel) skel_ragupdate = #281;
```

Part of FTE_QC_RAGDOLL_WIP Updates the skeletal object attached to the entity according to its origin and other properties. if animskel is non-zero, the ragdoll will animate towards the bone state in the animskel skeletal object, otherwise they will pick up the model's base pose which may not give nice results. If dollcmd is not set, the ragdoll will update (this should be done each frame). If the doll is updated without having a valid doll, the model's default .doll will be instanciated. commands: doll foo.doll : sets up the entity to use the named doll file dollstring TEXT : uses the doll file directly embedded within qc, with that extra prefix. cleardoll : uninstanciates the doll without destroying the skeletal object. animate 0.5 : specifies the strength of the ragdoll as a whole animatebody somebody 0.5 : specifies the strength of the ragdoll on a specific body (0 will disable ragdoll animations on that body). enablejoint somejoint 1 : enables (or disables) a joint. Disabling joints will allow the doll to shatter.

#### `skel_set_bone`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float skel, float bonenum, vector org, optional vector fwd, optional vector right, optional vector up) skel_set_bone = #271;
```

Part of FTE_CSQC_SKELETONOBJECTS Sets a bone position relative to its parent. If the orientation arguments are not specified, v_forward+v_right+v_up are used instead.

#### `skel_set_bone_world`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(entity ent, float bonenum, vector org, optional vector angorfwd, optional vector right, optional vector up) skel_set_bone_world = #283;
```

Part of FTE_QC_RAGDOLL_WIP Sets the world position of a bone within the given entity's attached skeletal object. The world position is dependant upon the owning entity's position. If no orientation argument is specified, v_forward+v_right+v_up are used for the orientation instead. If 1 is specified, it is understood as angles. If 3 are specified, they are the forawrd/right/up vectors to use.

#### `skinforname`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(float mdlindex, string skinname) skinforname = #237;
```

Finds a model's skin index by its exported skin name; check the returned index before use.

#### `skintoname`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string(float modidx, float skin) skintoname = #285;
```

Returns the exported skin/material name for a model skin index.

#### `sleep`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
void(float sleeptime) sleep = #212;
```

Part of FTE_MULTITHREADED Suspends the current QC execution thread for 'sleeptime' seconds. Other QC functions can and will be executed in the interim, including changing globals and field state (but not simultaneously). The self and other globals will be restored when the thread wakes up (or set to world if they were removed since the thread started sleeping). Locals will be preserved, but will not be protected from remove calls. If the engine is expecting the QC to return a value (even in the parent/root function), the value 0 shall be used instead of waiting for the qc to resume.

#### `sound`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity e, float chan, string samp, float vol, float atten, optional float speedpct, optional float flags, optional float timeofs) sound = #8;
```

Starts a sound centered upon the given entity. chan is the entity sound channel to use, channel 0 will allow you to mix many samples at once, others will replace the old sample 'samp' must have been precached first if specified, 'speedpct' should normally be around 100 (or =0), 200 for double speed or 50 for half speed. If flags is specified, the reliable flag in the channels argument is used for additional channels. Flags should be made from SOUNDFLAG_* constants timeofs should be negative in order to provide a delay before the sound actually starts.

#### `soundlength`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(string sample) soundlength = #534;
```

Provides a way to query the duration of a sound sample, allowing you to set up a timer to chain samples.

#### `soundnameforindex`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(float sndindex) soundnameforindex = #0:soundnameforindex;
```

Retrieves the name of the sound based upon a precache index. This can be used to reduce csqc network traffic by enabling sound matching (with getsoundindex).

#### `soundupdate`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(entity e, float channel, string newsample, float volume, float attenuation, float pitchpct, float flags, float timeoffset) soundupdate = #0:soundupdate;
```

Changes the properties of the current sound being played on the given entity channel. newsample may be empty, and will be ignored in this case. timeoffset is relative to the current position (subtract the result of getsoundtime for absolute positions). Negative volume can be used to stop the sound. Return value is a fractional value based upon the number of audio devices that could be updated - test against TRUE rather than non-zero.

#### `spawn`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
entity() spawn = #14;
```

Adds a brand new entity into the world! Hurrah, you're now a parent!

#### `spawn`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
entity() spawn = #22;
```

Allocates a new entity with initially cleared fields. Configure, link and eventually remove it.

#### `spawnclient`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
entity() spawnclient = #454;
```

Part of DP_SV_BOTCLIENT. Creates a server-side client/bot entity; game code must implement its game behavior.

#### `sprint`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
void(entity client, float msglvl, string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6) sprint = #24;
```

QW: Concatenates all string arguments, and prints the messsage on the named client's console, but only if that client's 'msg' infokey is set lower or equal to the supplied 'msglvl' argument.

#### `sprint`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
void(entity client, string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7) sprint = #24;
```

NQ: Concatenates all string arguments, and prints the messsage on the named client's console

#### `sprintf`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(string fmt, ...) sprintf = #627;
```

Part of DP_QC_SPRINTF 'prints' to a formatted temp-string. Mostly acts as in C, however %d assumes floats (fteqcc has arg checking. Use it.). type conversions: l=arg is an int, h=arg is a float, and will work as a prefix for any float or int representation. float representations: d=decimal, e,E=exponent-notation, f,F=floating-point notation, g,G=terse float, c=char code, x,X=hex other representations: i=int, s=string, S=quoted and marked-up string, v=vector, p=pointer so %ld will accept an int arg, while %hi will expect a float arg. entities, fields, and functions will generally need to be printed as ints with %i.

#### `spriteframe`

**Scope:** CSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(string modelname, int frame, float frametime) spriteframe = #0:spriteframe;
```

Obtains a suitable shader name to draw a sprite's shader via drawpic/R_BeginPolygon/etc, instead of needing to create a scene.

#### `sqlclosequery`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
void(float serveridx, float queryidx) sqlclosequery = #253;
```

Part of FTE_SQL. Releases an open SQL query/result identifier.

#### `sqlconnect`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
float(optional string host, optional string user, optional string pass, optional string defaultdb, optional string driver) sqlconnect = #250;
```

Part of FTE_SQL. Opens a SQL connection using the selected driver; requires a usable backend/plugin and returns a connection identifier.

#### `sqldisconnect`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
void(float serveridx) sqldisconnect = #251;
```

Part of FTE_SQL. Closes the selected SQL connection.

#### `sqlerror`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
string(float serveridx, optional float queryidx) sqlerror = #255;
```

Part of FTE_SQL. Gets the selected SQL connection/query's error description.

#### `sqlescape`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
string(float serveridx, string data) sqlescape = #256;
```

Part of FTE_SQL. Escapes a string using the SQL backend's rules; do not treat untrusted text as raw SQL syntax.

#### `sqlescapeblob`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(float serveridx, __variant *ptr, int maxsize) sqlescapeblob = #0:sqlescapeblob;
```

Encodes/escapes binary data for the selected SQL backend. Validate buffer size and backend support.

#### `sqlopenquery`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
float(float serveridx, void(float serveridx, float queryidx, float rows, float columns, float eof, float firstrow) callback, float querytype, string query) sqlopenquery = #252;
```

Part of FTE_SQL. Submits a SQL query and invokes its callback with result metadata; lifetime depends on query type/backend.

#### `sqlreadblob`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
int(float serveridx, float queryidx, float row, float column, __variant *ptr, int maxsize) sqlreadblob = #0:sqlreadblob;
```

Copies a query cell's binary data into the caller's buffer, bounded by maxsize; requires a SQL backend.

#### `sqlreadfield`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
string(float serveridx, float queryidx, float row, float column) sqlreadfield = #254;
```

Part of FTE_SQL. Reads a result cell as a string.

#### `sqlreadfloat`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
float(float serveridx, float queryidx, float row, float column) sqlreadfloat = #258;
```

Part of FTE_SQL. Reads a SQL result cell as a float.

#### `sqlversion`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
string(float serveridx) sqlversion = #257;
```

Part of FTE_SQL. Returns the selected SQL backend's version text.

#### `sqrt`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(float value) sqrt = #62;
```

Part of DP_QC_SINCOSSQRTPOW Square Root. Use pow for other exponents.

#### `sqrt`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(float) sqrt = #40;
```

Part of DP_QC_SINCOSSQRTPOW. Returns the square root; require a nonnegative input for a real result.

#### `stackdump`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void() stackdump = #73;
```

Prints out the QC's stack, for console-based error reports.

#### `stof`

**Scope:** MenuQC. **Compatibility:** FTE extension.

```qc
float(string) stof = #21;
```

Part of FRIK_FILE, FTE_QC_INFOKEY, FTE_STRINGS, QW_ENGINE, ZQ_QC_STRINGS. Parses a numeric string into a float; validate input rather than interpreting a zero result as proof of validity.

#### `stof`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
float(string) stof = #81;
```

Part of FRIK_FILE, FTE_QC_INFOKEY, FTE_STRINGS, QW_ENGINE, ZQ_QC_STRINGS. Parses a numeric string into a float; validate input rather than interpreting a zero result as proof of validity.

#### `stoh`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
int(string) stoh = #261;
```

Part of FTE_QC_INTCONV Reads a base-16 string (with or without 0x prefix) as an integer. Bugs out if given a base 8 or base 10 string. :P

#### `stoi`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
int(string) stoi = #259;
```

Part of FTE_QC_INTCONV Converts the given string into a true integer. Base 8, 10, or 16 is determined based upon the format of the string.

#### `stov`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
vector(string s) stov = #117;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS parses 3 space+tab separated floats and returns their values as a vector. optional single-quotes are accepted.

#### `stov`

**Scope:** MenuQC. **Compatibility:** FTE extension.

```qc
vector(string) stov = #55;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS. Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS parses 3 space+tab separated floats and returns their values as a vector. optional single-quotes are accepted.

#### `str2chr`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(string str, float index) str2chr = #222;
```

Part of FTE_STRINGS Retrieves the character value at offset 'index'.

#### `strcasecmp`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(string s1, string s2) strcasecmp = #229;
```

Part of FTE_STRINGS Compares the two strings without case sensitivity. Returns 0 if they are equal. The sign of the return value may be significant, but should not be depended upon.

#### `strcat`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
string(string s1, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7, optional string s8) strcat = #115;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS Concatenate up to 8 strings. You should consider using sprintf instead - it may be more readable and need fewer args. Returns a tempstring, which may cause issues in other engines.

#### `strcat`

**Scope:** MenuQC. **Compatibility:** FTE extension.

```qc
string(string, optional string, optional string, optional string, optional string, optional string, optional string, optional string) strcat = #53;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS. Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS Concatenate up to 8 strings. You should consider using sprintf instead - it may be more readable and need fewer args. Returns a tempstring, which may cause issues in other engines.

#### `strconv`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
string(float ccase, float redalpha, float redchars, string str, ...) strconv = #224;
```

Part of FTE_STRINGS Converts quake chars in the input string amongst different representations. ccase specifies the new case for letters. 0: not changed. 1: forced to lower case. 2: forced to upper case. redalpha and redchars switch between colour ranges. 0: no change. 1: Forced white. 2: Forced red. 3: Forced gold(low) (numbers only). 4: Forced gold (high) (numbers only). 5+6: Forced to white and red alternately. You should not use this builtin in combination with UTF-8.

#### `strdecolorize`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(string s) strdecolorize = #477;
```

Part of DP_QC_STRINGCOLORFUNCTIONS Flattens any markup/colours, removing them from the string.

#### `strftime`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(float uselocaltime, string format, ...) strftime = #478;
```

Part of DP_QC_STRFTIME. Formats a clock/calendar timestamp using the supplied format; uselocaltime selects local time or UTC.

#### `stringtokeynum`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(string key) stringtokeynum = #614;
```

Returns the qscancode of a key from its name. Names are identical to the bind command. ctrl/shift/alt modifiers are ignored.

#### `stringtokeynum`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(string keyname) stringtokeynum = #341;
```

Looks up the key name in the same way that the bind command would, returning the keycode for that key.

#### `stringtokeynum_csqc`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
DEP float(string keyname) stringtokeynum_csqc = #341;
```

Looks up the key name in the same way that the bind command would, returning the keycode for that key.

#### `stringtokeynum_menu`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(string key) stringtokeynum_menu = #614;
```

Looks up a single key name in the same way that the bind command would, returning the keycode for that key

#### `stringwidth`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(string text, float usecolours, vector fontsize='8 8') stringwidth = #327;
```

Calculates the width of the screen in virtual pixels. If usecolours is 1, markup that does not affect the string width will be ignored. Will always be decoded as UTF-8 if UTF-8 is globally enabled. If the char size is not specified, '8 8 0' will be assumed.

#### `stringwidth`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(string text, float usecolours, vector fontsize='8 8') stringwidth = #468;
```

Measures text width at the supplied font size; usecolours controls colored-text parsing.

#### `strireplace`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(string search, string replace, string subject) strireplace = #485;
```

Part of DP_QC_STRREPLACE. Replaces matching substrings in subject, ignoring case.

#### `strlen`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
float(string s) strlen = #114;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS Returns the number of bytes in the string not including the null terminator. If utf8_enable is set then returns codepoints instead.

#### `strlen`

**Scope:** MenuQC. **Compatibility:** FTE extension.

```qc
float(string) strlen = #52;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS. Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS Returns the number of bytes in the string not including the null terminator. If utf8_enable is set then returns codepoints instead.

#### `strlennocol`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(string s) strlennocol = #476;
```

Part of DP_QC_STRINGCOLORFUNCTIONS Returns the number of characters in the string after any colour codes or other markup has been parsed.

#### `strncasecmp`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(string s1, string s2, float len, optional float s1ofs, optional float s2ofs) strncasecmp = #230;
```

Part of FTE_STRINGS Compares up to 'len' chars in the two strings without case sensitivity. s1ofs allows you to treat s2 as a substring to compare against, or should be 0. Returns 0 if they are equal. The sign of the return value may be significant, but should not be depended upon.

#### `strncmp`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(string s1, string s2, optional float len, optional float s1ofs, optional float s2ofs) strncmp = #228;
```

Part of FTE_STRINGS Compares up to 'len' chars in the two strings. s1ofs allows you to treat s2 as a substring to compare against, or should be 0. Returns 0 if the two strings are equal, a negative value if s1 appears numerically lower, and positive if s1 appears numerically higher.

#### `strpad`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
string(float pad, string str1, ...) strpad = #225;
```

Part of FTE_STRINGS Pads the string with spaces, to ensure its a specific length (so long as a fixed-width font is used, anyway). If pad is negative, the spaces are added on the left. If positive the padding is on the right.

#### `strreplace`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(string search, string replace, string subject) strreplace = #484;
```

Part of DP_QC_STRREPLACE. Replaces matching substrings in subject, with case-sensitive matching.

#### `strstrofs`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
float(string s1, string sub, optional float startidx) strstrofs = #221;
```

Part of FTE_STRINGS Returns the 0-based offset of sub within the s1 string, or -1 if sub is not in s1. If startidx is set, this builtin will ignore matches before that 0-based offset.

#### `strtolower`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(string s) strtolower = #480;
```

Part of DP_QC_STRING_CASE_FUNCTIONS. Returns the supplied string converted to lower/upper case according to the builtin's character handling.

#### `strtoupper`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(string s) strtoupper = #481;
```

Part of DP_QC_STRING_CASE_FUNCTIONS. Returns the supplied string converted to lower/upper case according to the builtin's character handling.

#### `strtrim`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
string(string s) strtrim = #0:strtrim;
```

Trims the whitespace from the start+end of the string.

#### `strunzone`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
FTEDEP("Redundant") void(string s) strunzone = #119;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS Destroys a string that was allocated by strunzone. Further references to the string MAY crash the game. In FTE, this function became redundant and now does nothing.

#### `strunzone`

**Scope:** MenuQC. **Compatibility:** FTE extension.

```qc
FTEDEP("Redundant") void(string) strunzone = #57;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS Exists in FTE for compat only, does nothing.

#### `strzone`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
FTEDEP("Redundant") string(string s, ...) strzone = #118;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS Create a semi-permanent copy of a string that only becomes invalid once strunzone is called on the string (instead of when the engine assumes your string has left scope). This builtin has become redundant in FTEQW due to the FTE_QC_PERSISTENTTEMPSTRINGS extension and is now functionally identical to strcat for compatibility with old engines+mods.

#### `strzone`

**Scope:** MenuQC. **Compatibility:** FTE extension.

```qc
FTEDEP("Redundant") string(string) strzone = #56;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS Exists in FTE for compat only, no different from strcat.

#### `stuffcmd`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(entity client, string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7) stuffcmd = #21;
```

Sends a console command (or cvar) to the client, where it will be executed. Different clients support different commands. Do NOT forget the final \n. This builtin is generally considered evil.

#### `stuffcmdflags`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
void(entity client, float flags, string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6) stuffcmdflags = #0:stuffcmdflags;
```

Part of FTE_QC_STUFFCMDFLAGS Sends a console command (or cvar) to the client, where it will be executed. Different clients support different commands. Do NOT forget the final \n. This (just as evil) variant allows specifying some flags too. See the STUFFCMD_* constants.

#### `substring`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
string(string s, float start, float length) substring = #116;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS Returns a portion of the inputt string. If start is negative then will be treated as relative to the end, if length is negative then it will be interpretted relative to the end of the null terminator (eg -5 to skip the a 3-char filename extension including its dot) [Portability Note: these negative values are part of FTE_STRINGS, not FRIK_FILE et al]. Returns a tempstring, which may cause issues in other engines. When utf8_enable is set then operates on codepoints, but otherwise typically on bytes.

#### `substring`

**Scope:** MenuQC. **Compatibility:** FTE extension.

```qc
string(string s, float start, float length) substring = #54;
```

Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS. Part of FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS Returns a portion of the inputt string. If start is negative then will be treated as relative to the end, if length is negative then it will be interpretted relative to the end of the null terminator (eg -5 to skip the a 3-char filename extension including its dot) [Portability Note: these negative values are part of FTE_STRINGS, not FRIK_FILE et al]. Returns a tempstring, which may cause issues in other engines. When utf8_enable is set then operates on codepoints, but otherwise typically on bytes.

#### `tan`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(float a) tan = #475;
```

Part of DP_QC_ASINACOSATANATAN2TAN Forgive me father, for I have a sunbed and I'm not afraid to use it.

#### `te_beam`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(entity own, vector start, vector end) te_beam = #431;
```

Part of DP_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: beam. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_blood`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org, vector dir, float count) te_blood = #405;
```

Part of DP_TE_BLOOD. Emits a temporary visual effect: blood. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_bloodqw`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org, optional float count) te_bloodqw = #239;
```

Part of FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: bloodqw. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_bloodshower`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower = #406;
```

Part of _DP_TE_BLOODSHOWER. Emits a temporary visual effect: bloodshower. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_customflash`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org, float radius, float lifetime, vector color) te_customflash = #417;
```

Part of DP_TE_CUSTOMFLASH. Emits a temporary visual effect: customflash. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_explosion`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org) te_explosion = #421;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: explosion. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_explosion2`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org, float color, float colorlength) te_explosion2 = #427;
```

Part of DP_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: explosion2. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_explosionquad`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org) te_explosionquad = #415;
```

Part of _DP_TE_QUADEFFECTS1. Emits a temporary visual effect: explosionquad. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_explosionrgb`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org, vector color) te_explosionrgb = #407;
```

Part of DP_TE_EXPLOSIONRGB. Emits a temporary visual effect: explosionrgb. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_flamejet`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org, vector vel, float howmany) te_flamejet = #457;
```

Part of _DP_TE_FLAMEJET. Emits a temporary visual effect: flamejet. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_gunshot`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org, optional float count) te_gunshot = #418;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: gunshot. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_gunshotquad`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org) te_gunshotquad = #412;
```

Part of _DP_TE_QUADEFFECTS1. Emits a temporary visual effect: gunshotquad. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_knightspike`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org) te_knightspike = #424;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: knightspike. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_lavasplash`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org) te_lavasplash = #425;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: lavasplash. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_lightning1`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(entity own, vector start, vector end) te_lightning1 = #428;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: lightning1. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_lightning2`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(entity own, vector start, vector end) te_lightning2 = #429;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: lightning2. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_lightning3`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(entity own, vector start, vector end) te_lightning3 = #430;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: lightning3. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_lightningblood`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector pos) te_lightningblood = #219;
```

Part of FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: lightningblood. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_muzzleflash`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(entity ent) te_muzzleflash = #0:te_muzzleflash;
```

Requests a muzzle-flash effect at the supplied entity; does not implement weapon damage.

#### `te_particlecube`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube = #408;
```

Part of DP_TE_PARTICLECUBE. Emits a temporary visual effect: particlecube. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_particlerain`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain = #409;
```

Part of DP_TE_PARTICLERAIN. Emits a temporary visual effect: particlerain. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_particlesnow`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow = #410;
```

Part of DP_TE_PARTICLESNOW. Emits a temporary visual effect: particlesnow. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_plasmaburn`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org) te_plasmaburn = #433;
```

Part of _DP_TE_PLASMABURN. Emits a temporary visual effect: plasmaburn. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_smallflash`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org) te_smallflash = #416;
```

Part of DP_TE_SMALLFLASH. Emits a temporary visual effect: smallflash. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_spark`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org, vector vel, float howmany) te_spark = #411;
```

Part of DP_TE_SPARK. Emits a temporary visual effect: spark. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_spike`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org) te_spike = #419;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: spike. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_spikequad`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org) te_spikequad = #413;
```

Part of _DP_TE_QUADEFFECTS1. Emits a temporary visual effect: spikequad. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_superspike`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org) te_superspike = #420;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: superspike. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_superspikequad`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector org) te_superspikequad = #414;
```

Part of _DP_TE_QUADEFFECTS1. Emits a temporary visual effect: superspikequad. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_tarexplosion`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org) te_tarexplosion = #422;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: tarexplosion. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_teleport`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org) te_teleport = #426;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: teleport. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `te_wizspike`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(vector org) te_wizspike = #423;
```

Part of DP_TE_STANDARDEFFECTBUILTINS, FTE_TE_STANDARDEFFECTBUILTINS. Emits a temporary visual effect: wizspike. Parameters select location/direction/color/count as declared; this does not implement damage.

#### `terrain_edit`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
__variant(float action, optional vector pos, optional float radius, optional float quant, ...) terrain_edit = #278;
```

Part of FTE_TERRAIN_MAP Realtime terrain editing. Actions are the TEREDIT_ constants.

#### `tokenize`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
float(string s) tokenize = #441;
```

Part of KRIMZON_SV_PARSECLIENTCOMMAND. Part of KRIMZON_SV_PARSECLIENTCOMMAND Splits up the given string into its different components (what constitutes a token separator is not well defined and has been hacked about with over the years so have fun with that), returning the number of tokens that were found. Call argv(0 through ret-1) to retrieve each individual token. Take care to not use this recursively.

#### `tokenize`

**Scope:** MenuQC. **Compatibility:** Shared extension.

```qc
float(string) tokenize = #58;
```

Part of KRIMZON_SV_PARSECLIENTCOMMAND Splits up the given string into its different components (what constitutes a token separator is not well defined and has been hacked about with over the years so have fun with that), returning the number of tokens that were found. Call argv(0 through ret-1) to retrieve each individual token. Take care to not use this recursively.

#### `tokenize_console`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(string str) tokenize_console = #514;
```

Tokenize a string exactly as the console's tokenizer would do so. The regular tokenize builtin became bastardized for convienient string parsing, which resulted in a large disparity that can be exploited to bypass checks implemented in a naive SV_ParseClientCommand function, therefore you can use this builtin to make sure it exactly matches.

#### `tokenizebyseparator`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(string s, string separator1, ...) tokenizebyseparator = #479;
```

Part of DP_QC_TOKENIZEBYSEPARATOR Splits up the string using only the specified delimiters/separators. Multiple delimiters can be given, they are each considered equivelent (though should start with the longest if you want to do weird subseparator stuff). The resulting tokens can be queried via argv (and argv_start|end_index builtins, if you want to determine which of the separators was present between two tokens). Note that while an input string containing JUST a separator will return 2, a string with no delimiter will return 1, while (in FTE) an empty string will ALWAYS return 0.

#### `touchtriggers`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(optional entity ent, optional vector neworigin) touchtriggers = #279;
```

Triggers a touch events between self and every SOLID_TRIGGER entity that it is in contact with. This should typically just be the triggers touch functions. Also optionally updates the origin of the moved entity.

#### `tracebox`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector start, vector mins, vector maxs, vector end, float nomonsters, entity ent) tracebox = #90;
```

Part of DP_QC_TRACEBOX Exactly like traceline, but a box instead of a uselessly thin point. Acceptable sizes are limited by bsp format, q1bsp has strict acceptable size values.

#### `traceline`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(vector v1, vector v2, float flags, entity ent) traceline = #16;
```

Traces a thin line through the world from v1 towards v2. Will not collide with ent, ent.owner, or any entity who's owner field refers to ent. The passed entity will also be used to determine whether to use a capsule trace, the contents that the trace should impact, and a couple of other extra fields that define the trace. There are no side effects beyond the trace_* globals being written. flags&MOVE_NOMONSTERS will not impact on non-bsp entities. flags&MOVE_MISSILE will impact with increased size. flags&MOVE_HITMODEL will impact upon model meshes, instead of their bounding boxes. flags&MOVE_TRIGGERS will also stop on triggers flags&MOVE_EVERYTHING will stop if it hits anything, even non-solid entities. flags&MOVE_LAGGED will backdate entity positions for the purposes of this builtin according to the indicated player ent's latency, to provide lag compensation.

#### `traceoff`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void() traceoff = #30;
```

Disables tracing again.

#### `traceoff`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void() traceoff = #32;
```

Disables single-stepping. Which sucks if you started said singlestepping outside of qc.

#### `traceon`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void() traceon = #29;
```

Enables tracing. This may be spammy, slow, and stuff. Set debugger 1 in order to use fte's qc debugger.

#### `traceon`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void() traceon = #31;
```

Enables single-stepping. Its generally easier to just set a breakpoint.

#### `tracetoss`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void(entity ent, entity ignore) tracetoss = #64;
```

Attempts to guess were a movetype_toss entity will end up

#### `trailparticles`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(float effectnum, entity ent, vector start, vector end) trailparticles = #336;
```

Part of FTE_SV_POINTPARTICLES Draws the given effect between the two named points. If ent is not world, distances will be cached in the entity in order to avoid framerate dependancies. The entity is not otherwise used.

#### `unproject`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
vector (vector v) unproject = #310;
```

Transform a 2d screen-space point (with depth) into a 3d world-space point, according the various origin+angle+fov etc settings set via setproperty.

#### `uri_escape`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(string in) uri_escape = #510;
```

Part of DP_QC_URI_ESCAPE Uses percent-encoding to encode any bytes in the input string which are not ascii alphanumeric, period, hyphen, or underscore. All other bytes will expand to eg '%20' for a single space char. This encoding scheme is compatible with http and other uris.

#### `uri_get`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
float(string uril, float id, optional string postmimetype, optional string postdata) uri_get = #513;
```

Part of DP_QC_URI_GET, DP_QC_URI_POST uri_get() gets content from an URL and calls a callback "uri_get_callback" with it set as string; an unique ID of the transfer is returned returns 1 on success, and then calls the callback with the ID, 0 or the HTTP status code, and the received data in a string For a POST request, you will typically want the postmimetype set to application/x-www-form-urlencoded. For a GET request, omit the mime+data entirely. Consult your webserver/php/etc documentation for best-practise.

#### `uri_unescape`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(string in) uri_unescape = #511;
```

Part of DP_QC_URI_ESCAPE Undo any percent-encoding in the input string, hopefully resulting in the same original sequence of bytes (and thus chars too).

#### `utof`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Unnumbered/FTE binding.

```qc
float(__uint, optional float shift, float mask=24) utof = #0:utof;
```

Converts the given true integer into a float without depending on extended qcvm instructions. If shift and mask are specified then only specific parts of the integer will be cast to float.

#### `validstring`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(string) validstring = #81;
```

Returns true if str isn't null. In case 'if [not](str)' was configured to test for empty instead of null.

#### `vectoangles`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector(vector fwd, optional vector up) vectoangles = #51;
```

Returns the angles (+x=UP) required to orient an entity to look in the given direction. The 'up' argument is required if you wish to set a roll angle, otherwise it will be limited to just monster-style turning.

#### `vectoangles`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
vector(vector) vectoangles = #11;
```

Converts a direction vector to Euler angles in degrees; optional roll/up conventions depend on the signature.

#### `vectorvectors`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Shared extension.

```qc
void(vector dir) vectorvectors = #432;
```

Part of DP_QC_VECTORVECTORS. Builds v_forward/v_right/v_up from a direction vector rather than Euler angles.

#### `vectoyaw`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(vector v, optional entity reference) vectoyaw = #13;
```

Given a direction vector, returns the yaw angle in which that direction vector points. If an entity is passed, the yaw angle will be relative to that entity's gravity direction.

#### `vectoyaw`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(vector) vectoyaw = #10;
```

Returns the horizontal yaw angle of a direction vector, in degrees.

#### `vlen`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(vector v) vlen = #12;
```

Returns the square root of the dotproduct of a vector with itself. Or in other words the length of a distance vector, in quake units.

#### `vlen`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(vector) vlen = #9;
```

Returns a vector's Euclidean length in game units.

#### `vtos`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string(vector val) vtos = #27;
```

Returns a tempstring containing a representation of the given vector. Precision depends upon engine.

#### `vtos`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
string(vector) vtos = #19;
```

Converts a vector to a temporary text string.

#### `walkmove`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float(float yaw, float dist, optional float settraceglobals) walkmove = #32;
```

Attempt to walk the entity at a given angle for a given distance. if settraceglobals is set, the trace_* globals will be set, showing the results of the movement. This function will trigger touch events.

#### `wasfreed`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float(entity ent) wasfreed = #353;
```

Quickly check to see if the entity is currently free. This function is only valid during the half-second non-reuse window, after that it may give bad results. Try one second to make it more robust.

#### `whichpack`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
string(string filename, optional enumflags:float{WP_REFERENCEPACKAGE,WP_FULLPACKAGEPATH} flags) whichpack = #503;
```

Part of DP_QC_WHICHPACK Returns the pak file name that contains the file specified. progs/player.mdl will generally return something like 'pak0.pak'. If WP_REFERENCE, clients will automatically be told that the returned package should be pre-downloaded and used, even if allow_download_refpackages is not set.

#### `WriteAngle`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float to, float val) WriteAngle = #57;
```

Writes a single value suitable for an angle axis. The precision is not strictly specified but is assumed to be 8bit, giving 256 notches instead of the assumed 360 range passed in.

#### `WriteByte`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float to, float val) WriteByte = #52;
```

Writes a single byte into a network message buffer. Typically you will find a more correct alternative to writing arbitary data. 'to' should be one of the MSG_* constants. MSG_ONE must have msg_entity set first.

#### `WriteChar`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float to, float val) WriteChar = #53;
```

Writes a signed value between -128 and 127.

#### `WriteCoord`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float to, float val) WriteCoord = #56;
```

Writes a single value suitable for a map coordinate axis. The precision is not strictly specified but is assumed to be of at least 13.3 fixed-point precision (ie: +/-4k with 1/8th precision).

#### `WriteDouble`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float buf, __double dbl) WriteDouble = #0:WriteDouble;
```

Writes a full 64bit double-precision float without any data conversions at all, for excessive precision.

#### `WriteEntity`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float to, entity val) WriteEntity = #59;
```

Writes the index of the specified entity (the network data size is not specified). This can be read clientside using the readentitynum builtin, with caveats.

#### `WriteFloat`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float buf, float fl) WriteFloat = #280;
```

Writes a full 32bit float without any data conversions at all, for full precision.

#### `WriteInt`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float buf, int fl) WriteInt = #0:WriteInt;
```

Writes all 4 bytes of a 32bit integer without truncating to a float first before converting back to an int (unlike WriteLong does, but otherwise equivelent).

#### `WriteInt64`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float buf, __int64 fl) WriteInt64 = #0:WriteInt64;
```

Writes all 8 bytes of a 64bit integer. This uses variable-length coding and will send only a single byte for any value between -64 and 63.

#### `WriteLong`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float to, float val) WriteLong = #55;
```

Writes a signed 32bit integer. Note that the input argument being of float type limits the resulting integer to a mere 24 consecutive bits of validity. Use WriteInt if you want to write an entire 32bit int without data loss.

#### `WritePicture`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
void(float to, string s, float sz) WritePicture = #501;
```

Part of DP_SV_WRITEPICTURE Encodes the named image across the network as-is adhering to some size limit. In FTE, this simply writes the string and is equivelent to writestring and sz is ignored. WritePicture should be paired with ReadPicture in csqc.

#### `WriteShort`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float to, float val) WriteShort = #54;
```

Writes a signed value between -32768 and 32767. As an exception, values up to 65535 will not trigger warnings (but readshort will read the result as negative!)

#### `WriteString`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float to, string val) WriteString = #58;
```

Writes a variable-length null terminated string. There are length limits. The codepage is not translated, so be sure that client+server agree on whether utf-8 is being used or not (or just stick to ascii+markup).

#### `writetofile`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(filestream fh, entity e) writetofile = #606;
```

Writes an entity's fields to the named frik_file file handle.

#### `WriteUInt`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float buf, __uint fl) WriteUInt = #0:WriteUInt;
```

Writes all 4 bytes of a 32bit unsigned integer without truncating to a float first.

#### `WriteUInt64`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Unnumbered/FTE binding.

```qc
void(float buf, __uint64 fl) WriteUInt64 = #0:WriteUInt64;
```

Writes all 8 bytes of a 64bit unsigned integer. Values between 0-127 will be sent in a single byte.

#### `WriteUnterminatedString`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
void(float target, string str) WriteUnterminatedString = #456;
```

Part of DP_SV_WRITEUNTERMINATEDSTRING. Writes string bytes without a terminating NUL to a selected network message buffer; receiver must know the length.

### Callbacks you implement

72 declarations.

#### `Cef_GeneratePage`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
string(string uri, string method, string postdata, __in string requestheaders, __inout string responseheaders) Cef_GeneratePage;
```

Provides an entrypoint to generate pages for the CEF plugin from within QC. Headers are -separated key/value pairs (use tokenizebyseparator).

#### `ClientConnect`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() ClientConnect;
```

Called after the connecting client has finished loading and is ready to receive active entities. Note that this is NOT the first place that a client might be referred to. To determine if the client has csqc active (and kick anyone that doesn't), you can use if(infokeyf(self,INFOKEY_P_CSQCACTIVE)) {sprint(self, "CSQC is required for this server\n");dropclient(self);}

#### `ClientDisconnect`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() ClientDisconnect;
```

Called once a client disconnects or times out. Not guarenteed to be called on map changes.

#### `ClientKill`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() ClientKill;
```

Called in response to 'cmd kill' (or just 'kill').

#### `CSQC_ConsoleCommand`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(string cmd) CSQC_ConsoleCommand;
```

Called if the user uses any console command registed via registercommand.

#### `CSQC_ConsoleLink`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(string text, string info) CSQC_ConsoleLink;
```

Called if the user clicks a ^[text\infokey\infovalue^] link. Use infoget to read/check each supported key. Return true if you wish the engine to not attempt to handle the link itself. WARNING: link text can potentially come from other players, so be careful about what you allow to be changed.

#### `CSQC_DrawHud`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(vector viewsize, float scoresshown) CSQC_DrawHud;
```

Part of simple csqc, called after drawing the 3d view whenever CSQC_UpdateView is not defined.

#### `CSQC_DrawScores`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(vector viewsize, float scoresshown) CSQC_DrawScores;
```

Part of simple csqc, called after CSQC_DrawHud whenever CSQC_UpdateView is not defined, and when there are no menus/console active.

#### `CSQC_Ent_Remove`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void() CSQC_Ent_Remove;
```

Client callback when a custom network entity is removed; release its client-side resources/state.

#### `CSQC_Ent_Spawn`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float entnum) CSQC_Ent_Spawn;
```

Clumsily defined function for compat with DP. Should call spawn, set that ent's entnum field, and return the entity inside the 'self' global which will then be used for fllowing Ent_Updates. MUST NOT PARSE ANY NETWORK DATA (which makes it kinda useless).

#### `CSQC_Ent_Update`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float isnew) CSQC_Ent_Update;
```

Parses the data sent by ssqc's various SendEntity functions (must use the exact same reads as the ssqc used writes - to debug this rule more easily, you may wish to use sv_csqcdebug). 'self' provides context between frames, and self.entnum should normally report which ssqc entity . Be aware that interpolation will need to happen separately.

#### `CSQC_Event_Sound`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(float entnum, float channel, string soundname, float vol, float attenuation, vector pos, float pitchmod, float flags) CSQC_Event_Sound;
```

Client sound-event callback. Follow the exact signature and return convention for event handling.

#### `CSQC_GenerateMaterial`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
string(string shadername) CSQC_GenerateMaterial;
```

Returns the material text to use for the named material, for automatically importing foreign materials.

#### `CSQC_Init`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float apilevel, string enginename, float engineversion) CSQC_Init;
```

Called at startup. enginename and engineversion are arbitary hints and can take any form. enginename should be consistant between revisions, but this cannot truely be relied upon.

#### `CSQC_Input_Frame`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
__used void() CSQC_Input_Frame;
```

Called just before each time clientcommandframe is updated. You can edit the input_* globals in order to apply your own player inputs within csqc, which may allow you a convienient way to pass certain info to ssqc.

#### `CSQC_InputEvent`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(float evtype, float scanx, float chary, float devid) CSQC_InputEvent;
```

Called whenever a key is pressed, the mouse is moved, etc. evtype will be one of the IE_* constants. The other arguments vary depending on the evtype. Key presses are not guarenteed to have both scan and unichar values set at the same time.

#### `CSQC_MapEntityEdited`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(int entidx, string newentdata) CSQC_MapEntityEdited;
```

Client notification of a map-entity edit; refresh any dependent editor/view state.

#### `CSQC_Parse_CenterPrint`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(string msg) CSQC_Parse_CenterPrint;
```

Gives the CSQC a chance to intercept centerprints. Return true if you wish the engine to otherwise ignore the centerprint.

#### `CSQC_Parse_Damage`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(float save, float take, vector inflictororg) CSQC_Parse_Damage;
```

Called as a result of player.dmg_save or player.dmg_take being set on the server. Return true to completely inhibit the engine's colour shift and damage rolls, allowing you to do your own thing. You can use punch_roll += (normalize(inflictororg-player.origin)*v_right)*(take+save)*autocvar_v_kickroll; as a modifier for the roll angle should the player be hit from the side, and slowly fade it away over time.

#### `CSQC_Parse_Event`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void() CSQC_Parse_Event;
```

Called when the client receives an SVC_CGAMEPACKET. The csqc should read the data or call the error builtin if it does not recognise the message.

#### `CSQC_Parse_Print`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(string printmsg, float printlvl) CSQC_Parse_Print;
```

Gives the CSQC a chance to intercept sprint/bprint builtin calls. CSQC should filter by the client's current msg setting and then pass the message on to the print command, or handle them itself.

#### `CSQC_Parse_SetAngles`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float(vector angles, float isdelta) CSQC_Parse_SetAngles;
```

Client callback receiving a server view-angle update.

#### `CSQC_Parse_StuffCmd`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(string msg) CSQC_Parse_StuffCmd;
```

Gives the CSQC a chance to intercept stuffcmds. Use the tokenize builtin to parse the message. Unrecognised commands would normally be localcmded, but its probably better to drop unrecognised stuffcmds completely.

#### `CSQC_Parse_TempEntity`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float() CSQC_Parse_TempEntity;
```

Please don't use this. Use CSQC_Parse_Event and multicasts instead. The use of serverside protocol translation to handle QW vs NQ protocols mean that you're likely to end up reading slightly different data. Which is bad. Return true to say that you fully handled the tempentity. Return false to have the client attempt to rewind the network stream and parse the message itself.

#### `CSQC_PlayerInfoChanged`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float playernum) CSQC_PlayerInfoChanged;
```

Client callback when information for a player changes; refresh cached UI information.

#### `CSQC_RendererRestarted`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(string rendererdescription) CSQC_RendererRestarted;
```

Called by the engine after the video was restarted. This serves to notify the CSQC that any render targets that it may have cached were purged, and will need to be regenerated.

#### `CSQC_ServerInfoChanged`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void() CSQC_ServerInfoChanged;
```

Client callback when server information changes.

#### `CSQC_ServerSound`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
DEP("use CSQC_Event_Sound") float(float channel, string soundname, vector pos, float vol, float attenuation, float flags) CSQC_ServerSound;
```

Client callback for a server sound event; use the passed origin/entity/sample information.

#### `CSQC_Shutdown`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void() CSQC_Shutdown;
```

Specifies that the csqc is going down. Save your persistant settings here.

#### `CSQC_UpdateView`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float vwidth, float vheight, float notmenu) CSQC_UpdateView;
```

Called every single video frame. The CSQC is responsible for rendering the entire screen.

#### `CSQC_UpdateViewLoading`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void(float vwidth, float vheight, float notmenu) CSQC_UpdateViewLoading;
```

Alternative to CSQC_UpdateView, called when the engine thinks there should be a loading screen. If present, will inhibit the engine's normal loading screen, deferring to qc to draw it.

#### `CSQC_WorldLoaded`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
void() CSQC_WorldLoaded;
```

Called after the server's model+sound precaches have been executed. Gives a chance for the qc to read the entity lump from the bsp (via getentitytoken).

#### `EndFrame`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void() EndFrame;
```

Called after non-player entities have been run at the end of the physics frame. Player physics is performed out of order and can/will still occur between EndFrame and BeginFrame.

#### `GameCommand`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(string cmdtext) GameCommand;
```

Entry point for commands explicitly sent to the QC module; validate arguments before acting.

#### `HTTP_GeneratePage`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
string(string uri, string method, string postdata, __in string requestheaders, __inout string responseheaders) HTTP_GeneratePage;
```

Provides an entrypoint to generate pages for pages requested over http (sv_port_tcp+net_enable_http). Headers are -separated key/value pairs (use tokenizebyseparator). Return __NULL__ to let the engine handle it, an empty string for a 404, and any other text for a regular 200 response.

#### `init`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void(float prevprogs) init;
```

Part of FTE_MULTIPROGS. Called as soon as a progs is loaded, called at a time when entities are not valid. This is the only time when it is safe to call addprogs without field assignment. As it is also called as part of addprogs, this also gives you a chance to hook functions in modules that are already loaded (via externget+externget).

#### `initents`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** FTE extension.

```qc
void() initents;
```

Part of FTE_MULTIPROGS. Called after fields have been finalized. This is the first point at which it is safe to call spawn(), and is called before any entity fields have been parsed. You can use this entrypoint to send notifications to other modules.

#### `m_consolecommand`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(string cmd) m_consolecommand;
```

Menu console-command handler; return the declared result when deciding whether a command was handled.

#### `m_draw`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(vector screensize) m_draw;
```

Provides the menuqc with a chance to draw. Will be called even if the menu does not have focus, so be sure to avoid that. COMPAT: screensize is not provided in DP.

#### `m_drawloading`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(vector screensize, float opaque) m_drawloading;
```

Additional drawing function to draw loading screens. If opaque is set, then this function must ensure that the entire screen is overdrawn (even if just by a black drawfill).

#### `m_gethostcachecategory`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(float idx) m_gethostcachecategory;
```

Menu callback classifying a server browser entry for category display.

#### `m_init`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void() m_init;
```

Initializes the custom menu module.

#### `m_keydown`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
__deprecated("Use Menu_InputEvent") void(float scan, float chr) m_keydown;
```

Menu keyboard callbacks for key press/release; use the exact key/character parameters.

#### `m_keyup`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
__deprecated("Use Menu_InputEvent") void(float scan, float chr) m_keyup;
```

Menu keyboard callbacks for key press/release; use the exact key/character parameters.

#### `m_shutdown`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void() m_shutdown;
```

Shuts down the custom menu module; free resources it owns.

#### `m_toggle`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(float wantmode) m_toggle;
```

Callback for toggling the custom menu's visibility/state.

#### `main`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() main;
```

This function is never called, and is effectively dead code.

#### `Menu_InputEvent`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
float(float evtype, float scanx, float chary, float devid) Menu_InputEvent;
```

If present, this is called instead of m_keydown and m_keyup Called whenever a key is pressed, the mouse is moved, etc. evtype will be one of the IE_* constants. The other arguments vary depending on the evtype. Key presses are not guarenteed to have both scan and unichar values set at the same time.

#### `Menu_RendererRestarted`

**Scope:** MenuQC. **Compatibility:** Engine interface.

```qc
void(string rendererdescription) Menu_RendererRestarted;
```

Called by the engine after the video was restarted. This serves to notify the MenuQC that any render targets that it may have cached were purged, and will need to be regenerated.

#### `PlayerPostThink`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() PlayerPostThink;
```

Called after the player's input commands are processed.

#### `PlayerPreThink`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() PlayerPreThink;
```

With Prediction(QW compat/FTE default): Called before the player's input commands are processed. No Prediction(NQ compat): Called AFTER the player's movement intents have already been processed (ie: velocity will have already changed according to input_*, but before the actual position change.

#### `PutClientInServer`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() PutClientInServer;
```

Enginewise, this is only ever called immediately after ClientConnect and is thus a little redundant. Modwise, this is also called for respawning a player etc.

#### `RestoreGame`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Shared extension.

```qc
void() RestoreGame;
```

Called for each reconnecting player as part of loading a saved game. Part of NEH_RESTOREGAME.

#### `SetChangeParms`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() SetChangeParms;
```

Called for each client on map changes. Should copy various entity fields to the parm* globals.

#### `SetNewParms`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() SetNewParms;
```

Called without context when a new client initially connects (before ClientConnect is even called). This function is expected to only set the parm* globals so that they can be decoded properly later. You should not rely on 'self' being set.

#### `SetTransferParms`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** FTE extension.

```qc
void() SetTransferParms;
```

Called as an alternative to SetChangeParms when a player is transferring to another server. Part of FTE_SV_CLUSTER.

#### `SpectatorConnect`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() SpectatorConnect;
```

Called when a spectator joins the game.

#### `SpectatorDisconnect`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() SpectatorDisconnect;
```

Called when a spectator disconnects from the game.

#### `SpectatorThink`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() SpectatorThink;
```

Called each frame for each spectator.

#### `StartFrame`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
void() StartFrame;
```

Called at the start of each new physics frame. Player entities may think out of sequence so try not to depend upon explicit ordering too much.

#### `SV_AddDebugPolygons`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() SV_AddDebugPolygons;
```

Called each video frame. This is the only place where ssqc is allowed to call the R_BeginPolygon/R_PolygonVertex/R_EndPolygon builtins. This is exclusively for debugging, and will break in anything but single player as it will not be called if the engine is not running both a client and a server.

#### `SV_CheckRejectConnection`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
string(string addr, string uinfo, string features) SV_CheckRejectConnection;
```

Called to give the mod a chance to ignore connection requests based upon client protocol support or other properties. Use infoget to read the uinfo and features arguments.

#### `SV_ParseClientCommand`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(string cmd) SV_ParseClientCommand;
```

Provides QC with a way to intercept 'cmd foo' commands from the client. Very handy. Self will be set to the sending client, while the 'cmd' argument can be tokenize()d and each element retrieved via argv(argno). Unrecognised cmds MUST be passed on to the clientcommand builtin.

#### `SV_ParseClusterEvent`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(string dest, string from, string cmd, string info) SV_ParseClusterEvent;
```

Part of cluster mode. Handles cross-node events that were sent via clusterevent, on behalf of the named client.

#### `SV_ParseConnectionlessPacket`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float(string sender, string body) SV_ParseConnectionlessPacket;
```

Provides QC with a way to communicate between servers, or with client server browsers. Sender is the sender's ip. Body is the body of the message. You'll need to add your own password/etc support as required. Self is not valid.

#### `SV_PausedTic`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float pauseduration) SV_PausedTic;
```

For each frame that the server is paused, this function will be called to give the gamecode a chance to unpause the server again. the pauseduration argument says how long the server has been paused for (the time global is frozen and will not increment while paused). Self is not valid.

#### `SV_PerformLoad`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float fd, float entcount, float playerslots) SV_PerformLoad;
```

Called by the engine to restore a saved game that was saved via SV_PerformSave, the server will have already started the game to its inital state (for precaches+makestatic calls), so entities+globals will have their post-spawn values (you will likely want to remove most of them, you can use the two extra args as helpers for that). You can use respawn_edict to un-remove specific entities, with parseentitydata or putentityfieldstring(findentityfield(NAME), ent, value) to assign to fields by name strings. Don't expect bprints/etc to work - players are likely in a pending state. Remember to close the passed file.

#### `SV_PerformSave`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void(float fd, float entcount, float playerslots) SV_PerformSave;
```

Called by the engine as part of saved games. The QC is responsible for writing any data that will be required to restore the game. Save files are not limited to just text, you can use fwrite (and later fread) for binary data. Remember to close the passed file.

#### `SV_PlayerPhysics`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP_CSQC void() SV_PlayerPhysics;
```

Compatibility method to tweak player input that does not reliably work with prediction (prediction WILL break). Mods that care about prediction should use SV_RunClientCommand instead. If pr_no_playerphysics is set to 1, this function will never be called, which will either fix prediction or completely break player movement depending on whether the feature was even useful.

#### `SV_RunClientCommand`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
void() SV_RunClientCommand;
```

Called each time a player movement packet was received from a client. Self is set to the player entity which should be updated, while the input_* globals specify the various properties stored within the input packet. The contents of this function should be somewaht identical to the equivelent function in CSQC, or prediction misses will occur. If you're feeling lazy, you can simply call 'runstandardplayerphysics' after modifying the inputs.

#### `SV_ShouldPause`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float(float newstatus) SV_ShouldPause;
```

Called to give the qc a change to block pause/unpause requests. Return false for the pause request to be ignored. newstatus is 1 if the user is trying to pause the game. For the duration of the call, self will be set to the player who tried to pause, or to world if it was triggered by a server-side event.

#### `URI_Get_Callback`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void(float reqid, float responsecode, string resourcebody, int resourcebytes) URI_Get_Callback;
```

Called as an eventual result of the uri_get builtin.

### Engine globals and layout markers

182 declarations.

#### `__fullspawndata`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
__unused var string __fullspawndata;
```

Set by the engine before calls to spawn functions, and is most easily parsed with the tokenize builtin. This allows you to handle halflife's multiple-fields-with-the-same-name (or target-specific fields).

#### `autocvar_vid_conheight`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float autocvar_vid_conheight;
```

Compiler autocvar-backed console/UI width and height values; use for virtual 2D layout.

#### `autocvar_vid_conwidth`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float autocvar_vid_conwidth;
```

Compiler autocvar-backed console/UI width and height values; use for virtual 2D layout.

#### `clframetime`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float clframetime;
```

Client frame delta in seconds; different from server simulation time.

#### `clientcommandframe`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float clientcommandframe;
```

This is the input-frame sequence. frames < clientcommandframe have been sent to the server. frame==clientcommandframe is still being generated and can still change.

#### `cltime`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float cltime;
```

A local timer that ticks relative to local time regardless of latency, packetloss, or pause.

#### `coop`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float coop;
```

ssqc global

#### `cycle_wrapped`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float cycle_wrapped;
```

ssqc global

#### `deathmatch`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float deathmatch;
```

ssqc global

#### `dimension_default`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float dimension_default;
```

ssqc global

#### `dimension_send`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
var float dimension_send;
```

Used by multicast functionality. Multicasts (and related builtins that multicast internally) will only be sent to players where (player.dimension_see & dimension_send) is non-zero.

#### `drawfont`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float drawfont;
```

Allows you to choose exactly which font is to be used to draw text. Fonts can be registered/allocated with the loadfont builtin.

#### `end_sys_fields`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void end_sys_fields;
```

Layout/CRC boundary marker for system globals/fields. Keep generated order; it is not a gameplay variable to mutate.

#### `end_sys_globals`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
void end_sys_globals;
```

Layout/CRC boundary marker for system globals/fields. Keep generated order; it is not a gameplay variable to mutate.

#### `force_retouch`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float force_retouch;
```

If positive, causes all entities to check for triggers.

#### `found_secrets`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float found_secrets;
```

Game-maintained progress counters; increment according to your rules.

#### `frametime`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float frametime;
```

The time since the last physics/render/input frame.

#### `gamespeed`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float gamespeed;
```

Set by the engine, this is the value of the sv_gamespeed cvar

#### `gamestate`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
hashtable gamestate;
```

Special hash table index for hash_add and hash_get. Entries in this table will persist over map changes (and doesn't need to be created/deleted).

#### `input_angles`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_angles;
```

+x=DOWN

#### `input_buttons`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float input_buttons;
```

Bitmask of button states in the current input command.

#### `input_clienttime`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float input_clienttime;
```

Timestamp of the current client input command in the relevant client clock domain.

#### `input_cursor_entitynumber`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float input_cursor_entitynumber;
```

ssqc global

#### `input_cursor_screen`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_cursor_screen;
```

ssqc global

#### `input_cursor_trace_endpos`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_cursor_trace_endpos;
```

ssqc global

#### `input_cursor_trace_start`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_cursor_trace_start;
```

ssqc global

#### `input_head_angles`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_head_angles;
```

ssqc global

#### `input_head_avelocity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_head_avelocity;
```

ssqc global

#### `input_head_origin`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_head_origin;
```

ssqc global

#### `input_head_status`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
__uint64 input_head_status;
```

ssqc global

#### `input_head_velocity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_head_velocity;
```

ssqc global

#### `input_head_weapon`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
__uint64 input_head_weapon;
```

ssqc global

#### `input_impulse`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float input_impulse;
```

Current command's impulse number; game code assigns meaning to impulses.

#### `input_left_angles`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_left_angles;
```

ssqc global

#### `input_left_avelocity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_left_avelocity;
```

ssqc global

#### `input_left_origin`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_left_origin;
```

ssqc global

#### `input_left_status`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
__uint64 input_left_status;
```

ssqc global

#### `input_left_velocity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_left_velocity;
```

ssqc global

#### `input_left_weapon`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
__uint64 input_left_weapon;
```

ssqc global

#### `input_lightlevel`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float input_lightlevel;
```

ssqc global

#### `input_movevalues`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_movevalues;
```

Current command's intended forward, sideways and upward movement components.

#### `input_right_angles`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_right_angles;
```

ssqc global

#### `input_right_avelocity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_right_avelocity;
```

ssqc global

#### `input_right_origin`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_right_origin;
```

ssqc global

#### `input_right_status`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
__uint64 input_right_status;
```

ssqc global

#### `input_right_velocity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector input_right_velocity;
```

ssqc global

#### `input_right_weapon`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
__uint64 input_right_weapon;
```

ssqc global

#### `input_sequence`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float input_sequence;
```

This is the client-generated input sequence number. 0 for unsequenced movements.

#### `input_servertime`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float input_servertime;
```

Server's timestamp of the client's interpolation state.

#### `input_timelength`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float input_timelength;
```

Duration of the current input command, in seconds; use it for input-driven simulation.

#### `input_weapon`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
__uint64 input_weapon;
```

ssqc global

#### `intermission`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float intermission;
```

Client intermission state; use it to change HUD/view presentation.

#### `intermission_time`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float intermission_time;
```

Client time associated with the current intermission.

#### `killed_monsters`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float killed_monsters;
```

Game-maintained progress counters; increment according to your rules.

#### `mapname`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string mapname;
```

The short name of the map.

#### `maxclients`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float maxclients;
```

Maximum number of player slots on the server.

#### `msg_entity`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
entity msg_entity;
```

Player entity selecting the recipient for MSG_ONE messages and related unicast operations.

#### `newmis`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
entity newmis;
```

ssqc global

#### `numclientseats`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float numclientseats;
```

This is the number of splitscreen clients currently running on this client.

#### `other`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
entity other;
```

Valid in touch functions, this is the entity that we touched.

#### `parm1`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm1;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm10`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm10;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm11`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm11;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm12`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm12;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm13`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm13;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm14`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm14;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm15`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm15;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm16`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm16;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm17`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm17;
```

Additional spawn parms, following the same parmN theme.

#### `parm18`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm18;
```

Additional spawn parms, following the same parmN theme.

#### `parm19`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm19;
```

Additional spawn parms, following the same parmN theme.

#### `parm2`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm2;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm20`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm20;
```

Additional spawn parms, following the same parmN theme.

#### `parm21`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm21;
```

Additional spawn parms, following the same parmN theme.

#### `parm22`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm22;
```

Additional spawn parms, following the same parmN theme.

#### `parm23`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm23;
```

Additional spawn parms, following the same parmN theme.

#### `parm24`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm24;
```

Additional spawn parms, following the same parmN theme.

#### `parm25`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm25;
```

Additional spawn parms, following the same parmN theme.

#### `parm26`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm26;
```

Additional spawn parms, following the same parmN theme.

#### `parm27`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm27;
```

Additional spawn parms, following the same parmN theme.

#### `parm28`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm28;
```

Additional spawn parms, following the same parmN theme.

#### `parm29`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm29;
```

Additional spawn parms, following the same parmN theme.

#### `parm3`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm3;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm30`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm30;
```

Additional spawn parms, following the same parmN theme.

#### `parm31`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm31;
```

Additional spawn parms, following the same parmN theme.

#### `parm32`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm32;
```

Additional spawn parms, following the same parmN theme.

#### `parm33`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm33;
```

Player map-transfer parameter 33: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm34`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm34;
```

Player map-transfer parameter 34: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm35`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm35;
```

Player map-transfer parameter 35: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm36`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm36;
```

Player map-transfer parameter 36: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm37`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm37;
```

Player map-transfer parameter 37: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm38`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm38;
```

Player map-transfer parameter 38: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm39`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm39;
```

Player map-transfer parameter 39: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm4`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm4;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm40`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm40;
```

Player map-transfer parameter 40: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm41`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm41;
```

Player map-transfer parameter 41: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm42`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm42;
```

Player map-transfer parameter 42: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm43`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm43;
```

Player map-transfer parameter 43: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm44`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm44;
```

Player map-transfer parameter 44: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm45`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm45;
```

Player map-transfer parameter 45: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm46`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm46;
```

Player map-transfer parameter 46: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm47`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm47;
```

Player map-transfer parameter 47: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm48`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm48;
```

Player map-transfer parameter 48: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm49`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm49;
```

Player map-transfer parameter 49: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm5`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm5;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm50`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm50;
```

Player map-transfer parameter 50: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm51`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm51;
```

Player map-transfer parameter 51: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm52`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm52;
```

Player map-transfer parameter 52: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm53`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm53;
```

Player map-transfer parameter 53: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm54`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm54;
```

Player map-transfer parameter 54: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm55`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm55;
```

Player map-transfer parameter 55: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm56`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm56;
```

Player map-transfer parameter 56: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm57`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm57;
```

Player map-transfer parameter 57: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm58`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm58;
```

Player map-transfer parameter 58: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm59`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm59;
```

Player map-transfer parameter 59: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm6`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm6;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm60`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm60;
```

Player map-transfer parameter 60: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm61`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm61;
```

Player map-transfer parameter 61: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm62`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm62;
```

Player map-transfer parameter 62: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm63`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm63;
```

Player map-transfer parameter 63: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm64`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm64;
```

Player map-transfer parameter 64: set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.

#### `parm7`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm7;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm8`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm8;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm9`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float parm9;
```

Player specific mod-defined values that are transferred from one map to the next. These are set by the qc inside calls to SetNewParms and SetChangeParms, and can then be read on a per-player basis after a call to the setspawnparms builtin. They are not otherwise valid.

#### `parm_string`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
string parm_string;
```

Like the regular parmN globals, but preserves string contents.

#### `player_localentnum`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float player_localentnum;
```

This is entity number the player is seeing from/spectating, or the player themself, can change mid-map.

#### `player_localnum`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float player_localnum;
```

The 0-based player index, valid for getplayerkeyvalue calls.

#### `pmove_jump_held`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float pmove_jump_held;
```

Prediction jump-button latch for preventing repeated jumping while held.

#### `pmove_maxs`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
vector pmove_maxs;
```

Player prediction collision-box extents; keep them consistent with server movement rules.

#### `pmove_mins`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
vector pmove_mins;
```

Player prediction collision-box extents; keep them consistent with server movement rules.

#### `pmove_onground`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float pmove_onground;
```

Reports the onground state of the engineside player (after prediction). Does not work when the player is a csqc-owned entity.

#### `pmove_org`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
vector pmove_org;
```

Reports the origin of the engineside player (after prediction). Does not work when the player is a csqc-owned entity.

#### `pmove_vel`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
vector pmove_vel;
```

Reports the velocity of the engineside player (after prediction). Does not work when the player is a csqc-owned entity.

#### `pmove_waterjumptime`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float pmove_waterjumptime;
```

Prediction timer for water-jump movement behavior.

#### `self`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
entity self;
```

The magic me

#### `servercommandframe`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float servercommandframe;
```

This is the input-frame that was last acknowledged by the server. Input frames greater than this should be applied to the player's entity.

#### `serverdeltatime`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float serverdeltatime;
```

Difference between server update times, for interpolation/simulation bookkeeping.

#### `serverflags`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float serverflags;
```

Game-managed persistent server/map progression bits, traditionally carried across map changes.

#### `serverid`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
int serverid;
```

The unique id of this server within the server cluster.

#### `serverprevtime`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float serverprevtime;
```

Current/previous server update time exposed to CSQC for interpolation.

#### `servertime`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float servertime;
```

Current/previous server update time exposed to CSQC for interpolation.

#### `startspot`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
string startspot;
```

Receives the value of the second argument to changelevel from the previous map.

#### `teamplay`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float teamplay;
```

ssqc global

#### `time`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
float time;
```

The current game time. Stops when paused.

#### `total_monsters`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float total_monsters;
```

Game-maintained totals advertised to the client; the engine does not discover your custom secrets/enemies.

#### `total_secrets`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float total_secrets;
```

Game-maintained totals advertised to the client; the engine does not discover your custom secrets/enemies.

#### `trace_allsolid`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float trace_allsolid;
```

Nonzero when the trace remains entirely in solid geometry.

#### `trace_bone_id`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
int trace_bone_id;
```

1-based. 0 if not known. typically needs MOVE_HITMODEL.

#### `trace_brush_faceid`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
int trace_brush_faceid;
```

Identifiers for the brush/brush face hit by a trace, usable with the matching map-editing API.

#### `trace_brush_id`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
int trace_brush_id;
```

Identifiers for the brush/brush face hit by a trace, usable with the matching map-editing API.

#### `trace_dphitcontents`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
DEP("Does not support mod-specific contents.") float trace_dphitcontents;
```

Legacy DarkPlaces-compatible trace contents/surface information; prefer the current typed trace fields.

#### `trace_dphitcontents`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float trace_dphitcontents;
```

ssqc global

#### `trace_dphitq3surfaceflags`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
DEP("Does not support mod-specific surface flags") float trace_dphitq3surfaceflags;
```

Legacy DarkPlaces-compatible trace contents/surface information; prefer the current typed trace fields.

#### `trace_dphitq3surfaceflags`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float trace_dphitq3surfaceflags;
```

ssqc global

#### `trace_dphittexturename`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string trace_dphittexturename;
```

ssqc global

#### `trace_dpstartcontents`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
DEP("Does not support mod-specific contents.") float trace_dpstartcontents;
```

Legacy DarkPlaces-compatible trace contents/surface information; prefer the current typed trace fields.

#### `trace_dpstartcontents`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float trace_dpstartcontents;
```

ssqc global

#### `trace_endcontents`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
int trace_endcontents;
```

Contents at the trace endpoint; the f variant is a legacy floating-point representation.

#### `trace_endcontentsf`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
DEP("Does not support all mod-specific contents.") float trace_endcontentsf;
```

Contents at the trace endpoint; the f variant is a legacy floating-point representation.

#### `trace_endcontentsf`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float trace_endcontentsf;
```

ssqc global

#### `trace_endcontentsi`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
int trace_endcontentsi;
```

ssqc global

#### `trace_endpos`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector trace_endpos;
```

End position of the latest trace.

#### `trace_ent`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
entity trace_ent;
```

Entity hit by the latest trace; world can indicate world geometry or the null result depending on other trace outputs.

#### `trace_fraction`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float trace_fraction;
```

Completed fraction of the requested movement, from zero to one; one means no blocking impact.

#### `trace_inopen`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float trace_inopen;
```

Legacy trace flags recording passage through open/water contents.

#### `trace_inwater`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float trace_inwater;
```

Legacy trace flags recording passage through open/water contents.

#### `trace_networkentity`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
float trace_networkentity;
```

Repots which ssqc entnum was hit when a csqc traceline impacts an ssqc-based brush entity.

#### `trace_plane_dist`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float trace_plane_dist;
```

Plane distance for the latest trace's impact plane.

#### `trace_plane_normal`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector trace_plane_normal;
```

Normal of the surface hit by the latest trace; meaningful for a valid impact.

#### `trace_startsolid`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
float trace_startsolid;
```

Nonzero when the trace begins inside a solid.

#### `trace_surface_id`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
int trace_surface_id;
```

1-based. 0 if not known.

#### `trace_surfaceflags`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
int trace_surfaceflags;
```

Surface flags for the trace hit; the f variant is legacy. Do not confuse flags with contents.

#### `trace_surfaceflagsf`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
DEP("Does not support all mod-specific surface flags.") float trace_surfaceflagsf;
```

Surface flags for the trace hit; the f variant is legacy. Do not confuse flags with contents.

#### `trace_surfaceflagsf`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
float trace_surfaceflagsf;
```

ssqc global

#### `trace_surfaceflagsi`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
int trace_surfaceflagsi;
```

ssqc global

#### `trace_surfacename`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
string trace_surfacename;
```

ssqc global

#### `trace_triangle_id`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
int trace_triangle_id;
```

1-based. 0 if not known.

#### `v_forward`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector v_forward;
```

Direction-basis result globals written by makevectors/vectorvectors and related operations; overwritten by the next call.

#### `v_right`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector v_right;
```

Direction-basis result globals written by makevectors/vectorvectors and related operations; overwritten by the next call.

#### `v_up`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector v_up;
```

Direction-basis result globals written by makevectors/vectorvectors and related operations; overwritten by the next call.

#### `view_angles`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
vector view_angles;
```

+x=DOWN

#### `world`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
entity world;
```

The null entity. Hurrah. Readonly after map spawn time.

### Entity fields

200 declarations.

#### `abslight`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float abslight;
```

Allows overriding light levels. Use drawflags to state that this field should actually be used.

#### `absmax`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector absmax;
```

Set by the engine when the entity is relinked (by setorigin, setsize, or setmodel). This is in world coordinates.

#### `absmin`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector absmin;
```

Set by the engine when the entity is relinked (by setorigin, setsize, or setmodel). This is in world coordinates.

#### `aiment`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.entity aiment;
```

Attachment/follow target entity for the corresponding movement mode.

#### `alpha`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float alpha;
```

The transparency of the entity. 1 means opaque, 0.0001 means virtually invisible. 0 is remapped to 1, for compatibility.

#### `ammo_cells`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float ammo_cells;
```

Conventional Quake ammunition fields/stats; your conversion may choose how to use them.

#### `ammo_nails`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float ammo_nails;
```

Conventional Quake ammunition fields/stats; your conversion may choose how to use them.

#### `ammo_rockets`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float ammo_rockets;
```

Conventional Quake ammunition fields/stats; your conversion may choose how to use them.

#### `ammo_shells`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float ammo_shells;
```

Conventional Quake ammunition fields/stats; your conversion may choose how to use them.

#### `angles`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector angles;
```

The eular angles the entity is facing in, in pitch, yaw, roll order. Due to a legacy bug, mdl/iqm/etc formats use +x=UP, bsp/spr/etc formats use +x=DOWN.

#### `armortype`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float armortype;
```

Conventional game-managed armor absorption fraction and amount; implement damage absorption explicitly.

#### `armorvalue`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float armorvalue;
```

Conventional game-managed armor absorption fraction and amount; implement damage absorption explicitly.

#### `avelocity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector avelocity;
```

The amount the entity's angles change by per second. Note that this is direct eular angles, and thus the angular change is non-linear and often just looks buggy if you're changing more than one angle at a time.

#### `basebone`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float basebone;
```

The base* frame animations are equivelent to their non-base versions, except that they only affect bone numbers below the 'basebone' value. This means that the base* animation can affect the legs of a skeletal model independantly of the normal animation fields affecting the torso area. For more complex animation than this, use skeletal objects.

#### `baseframe`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float baseframe;
```

See basebone

#### `baseframe1time`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float baseframe1time;
```

See basebone

#### `baseframe2`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float baseframe2;
```

See basebone

#### `baseframe2time`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float baseframe2time;
```

See basebone

#### `baselerpfrac`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float baselerpfrac;
```

See basebone

#### `basesubblend2frac`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float basesubblend2frac;
```

See basebone

#### `basesubblendfrac`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float basesubblendfrac;
```

See basebone

#### `blocked`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.void() blocked;
```

Pusher callback when an obstruction prevents movement; other identifies the blocker.

#### `bonecontrol1`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float bonecontrol1;
```

Halflife model format bone controller. On player models, this typically affects the spine's yaw.

#### `bonecontrol2`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float bonecontrol2;
```

Halflife model format bone controller. On player models, this typically affects the spine's yaw.

#### `bonecontrol3`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float bonecontrol3;
```

Halflife model format bone controller. On player models, this typically affects the spine's yaw.

#### `bonecontrol4`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float bonecontrol4;
```

Halflife model format bone controller. On player models, this typically affects the spine's yaw.

#### `bonecontrol5`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float bonecontrol5;
```

Halflife model format bone controller. This typically affects the mouth.

#### `bouncefactor`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float bouncefactor;
```

Rigid-body restitution and stopping threshold parameters; backend dependent.

#### `bouncestop`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float bouncestop;
```

Rigid-body restitution and stopping threshold parameters; backend dependent.

#### `button0`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float button0;
```

Player attack button state from input.

#### `button1`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float button1;
```

Legacy player button field; do not assume a universal gameplay action for it.

#### `button10`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.float button10;
```

For DP compat only.

#### `button11`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.float button11;
```

For DP compat only.

#### `button12`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.float button12;
```

For DP compat only.

#### `button13`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.float button13;
```

For DP compat only.

#### `button14`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.float button14;
```

For DP compat only.

#### `button15`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.float button15;
```

For DP compat only.

#### `button16`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.float button16;
```

For DP compat only.

#### `button2`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float button2;
```

Player jump button state; game code implements its jump policy.

#### `button3`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float button3;
```

Additional player input button state; bind its action explicitly rather than assuming classic Quake semantics.

#### `button4`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float button4;
```

Additional player input button state; bind its action explicitly rather than assuming classic Quake semantics.

#### `button5`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float button5;
```

Additional player input button state; bind its action explicitly rather than assuming classic Quake semantics.

#### `button6`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float button6;
```

Additional player input button state; bind its action explicitly rather than assuming classic Quake semantics.

#### `button7`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float button7;
```

Additional player input button state; bind its action explicitly rather than assuming classic Quake semantics.

#### `button8`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float button8;
```

Additional player input button state; bind its action explicitly rather than assuming classic Quake semantics.

#### `button9`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.float button9;
```

For DP compat only.

#### `buttonchat`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.float buttonchat;
```

For DP compat only.

#### `buttonuse`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.float buttonuse;
```

For DP compat only.

#### `camera_transform`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector(vector org, vector ang) camera_transform;
```

A callback that provides portal transform information for portal surfaces attached to this entity. Also used to open up pvs in ssqc.

#### `chain`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.entity chain;
```

Linked-list field used by findchain APIs or your game code; calls can overwrite it.

#### `chainmoved`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.void() chainmoved;
```

Called when the entity is moved as a result of being part of another entity's .movechain

#### `classname`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.string classname;
```

Identifies the class/type of the entity. Useful for debugging, also used for loading, but its value is not otherwise significant to the engine, this leaves the mod free to set it to whatever it wants and randomly test strings for values in whatever inefficient way it chooses fit.

#### `clientcamera`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.entity clientcamera;
```

Controls which entity to use for this client's camera.

#### `clientcolors`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Doesn't support RGB player colours.") .float clientcolors;
```

Legacy combined player-color value, deprecated because it cannot represent RGB player colors.

#### `color`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector color;
```

This affects the colour of realtime lights that were enabled via the pflags field.

#### `colormap`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float colormap;
```

Legacy player-color translation/index information; model and protocol conventions apply.

#### `colormod`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector colormod;
```

Provides a colour tint for the entity (does not affect fullbrights).

#### `contentstransition`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.void(float old, float new) contentstransition;
```

This function is called when the entity moves between water and air. If specified, default splash sounds will be disabled allowing you to provide your own.

#### `currentammo`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float currentammo;
```

Game-managed ammunition displayed for the current weapon.

#### `cursor_active`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.float cursor_active;
```

For DP compat only.

#### `cursor_entitynumber`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.entity cursor_entitynumber;
```

For DP compat only.

#### `cursor_impact`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.vector cursor_impact;
```

For DP compat only.

#### `cursor_screen`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.vector cursor_screen;
```

For DP compat only.

#### `cursor_start`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
.vector cursor_start;
```

For DP compat only.

#### `customizeentityforclient`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP_CSQC .float() customizeentityforclient;
```

Called just before an entity is sent to a client (non-csqc protocol). This gives you a chance to tailor 'self' according to what 'other' should see.

#### `customphysics`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.void() customphysics;
```

Called once each physics frame, overriding the entity's .movetype field and associated logic. You'll probably want to use tracebox to move it through the world. Be sure to call .think as appropriate.

#### `damp_angular`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float damp_angular;
```

Rigid-body linear/angular damping and angular-speed limit; backend dependent.

#### `damp_linear`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float damp_linear;
```

Rigid-body linear/angular damping and angular-speed limit; backend dependent.

#### `deadflag`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float deadflag;
```

Conventional game-managed dead/alive state; implement death transitions yourself.

#### `dimension_ghost`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Does not work with MVDs nor splitscreen.") .float dimension_ghost;
```

If this entity is visible only within these dimensions, it will become transparent, as if a ghost.

#### `dimension_ghost_alpha`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Does not work with MVDs nor splitscreen.") .float dimension_ghost_alpha;
```

If this entity is subject to dimension_ghost, this is the scaler for its alpha value. If 0, 0.5 will be used instead.

#### `dimension_hit`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float dimension_hit;
```

This is the bitmask of dimensions which the entity will be blocked by. If other.dimension_solid & self.dimension_hit, our traces will impact and not proceed. If its false, the traces will NOT impact, allowing self to pass straight through.

#### `dimension_see`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Does not work with MVDs nor splitscreen.") .float dimension_see;
```

This is the dimension mask (bitfield) that the client is allowed to see. Entities and events not in this dimension mask will be invisible.

#### `dimension_seen`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Does not work with MVDs nor splitscreen.") .float dimension_seen;
```

This is the dimension mask (bitfield) that the client is visible within. Clients that cannot see this dimension mask will not see this entity.

#### `dimension_solid`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float dimension_solid;
```

This is the bitmask of dimensions which the entity is solid within. This is not networked, instead csqc traces impacting ssqc entities assumes the ssqc entity to have a dimension_solid of 1.

#### `dmg_inflictor`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.entity dmg_inflictor;
```

Entity identifying the source position of damage feedback.

#### `dmg_save`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float dmg_save;
```

Player damage-feedback amounts accumulated for client feedback; actual health/armor rules are game code.

#### `dmg_take`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float dmg_take;
```

Player damage-feedback amounts accumulated for client feedback; actual health/armor rules are game code.

#### `dphitcontentsmask`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Does not support mod-specific contents.") .float dphitcontentsmask;
```

Some crappy field that inefficiently requires translating to the native contents flags. Ditch the 'dp', do it properly.

#### `drawflags`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float drawflags;
```

Various flags that affect lighting values and scaling. Typically set to 96 in quake for proper compatibility with DP_QC_SCALE.

#### `drawmask`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float drawmask;
```

Acts as a filter in the addentities call.

#### `drawonlytoclient`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Cannot be recorded in MVDs, nor work properly with splitscreen. Use CSQC instead.") .entity drawonlytoclient;
```

This entity will be sent *only* to the player named by this field. To other players they will be invisible and not emit dlights/particles. Does not work in MVD-recorded game.

#### `effects`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float effects;
```

Lots of random flags that change random effects. See EF_* constants.

#### `emiteffectnum`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float emiteffectnum;
```

This should be set to the result of particleeffectnum, in order to continually spawn particles in the direction that this entity faces.

#### `enemy`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.entity enemy;
```

Game-managed target/chase references used by AI/movement logic; validate their lifetime.

#### `entnum`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float entnum;
```

The entity number as its known on the server.

#### `erp`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float erp;
```

Rigid-body surface friction/error reduction parameters; meaning and availability depend on the physics backend.

#### `exteriormodeltoclient`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Cannot be recorded in MVDs, nor work properly with splitscreen. Use CSQC instead.") .entity exteriormodeltoclient;
```

This entity will be invisible to the player named by this field, except in mirrors or mirror-like surfaces, where it will be visible as normal. It may still cast shadows as normal, and generate lights+particles, depending on client settings. Does not affect how other players see the entity.

#### `fatness`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float fatness;
```

How many QuakeUnits to push the entity's verticies along their normals by.

#### `fixangle`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float fixangle;
```

Forces the clientside view angles to change to the value of .angles (has some lag). If set to 1/TRUE, the server will guess whether to send a delta or an explicit angle. If 2, will always send a delta (due to lag between transmission and acknowledgement, this cannot be spammed reliably). If 3, will always send an explicit angle.

#### `flags`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float flags;
```

Entity FL_* bitmask, combining engine-maintained movement state and game policy bits.

#### `forceshader`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float forceshader;
```

Contains a shader handle used to replace all surfaces upon the entity.

#### `frags`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float frags;
```

Player score exposed to client scoreboard/stat handling; game code updates it.

#### `frame`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float frame;
```

The current frame the entity is meant to be displayed in. In CSQC, note the lerpfrac and frame2 fields as well. if it specifies a framegroup, the framegroup will autoanimate in ssqc, but not in csqc.

#### `frame1time`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float frame1time;
```

This controls the time into the framegroup/animation named by .frame, you should increment this value according to frametime or to distance moved, depending on the sort of animation you're attempting. You may wish to avoid incrementing this while lerpfrac is still changing, to avoid wasting parts of the animation.

#### `frame2`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float frame2;
```

The alternative frame. Visible only when lerpfrac is set to 1.

#### `frame2time`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float frame2time;
```

The absolute time into the animation/framegroup specified by .frame2.

#### `frame3`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float frame3;
```

Some people just don't understand how to use framegroups...

#### `frame3time`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float frame3time;
```

.frame3 equivelent of frame1time.

#### `frame4`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float frame4;
```

Additional animation frame and blend fraction used by extended animation blending.

#### `frame4time`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float frame4time;
```

.frame4 equivelent of frame1time.

#### `friction`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float friction;
```

Rigid-body surface friction/error reduction parameters; meaning and availability depend on the physics backend.

#### `geomtype`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float geomtype;
```

Rigid-body geometry selector from GEOMTYPE_*; requires an available physics backend.

#### `glow_color`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float glow_color;
```

Some outdated particle trail thing.

#### `glow_size`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float glow_size;
```

Some outdated particle trail thing.

#### `glow_trail`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float glow_trail;
```

Some outdated particle trail thing.

#### `glowmod`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector glowmod;
```

Scaler for an entity's fullbright textures.

#### `goalentity`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.entity goalentity;
```

Game-managed target/chase references used by AI/movement logic; validate their lifetime.

#### `gravity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float gravity;
```

Multiplier applied in addition to sv_gravity (not absolute units), to control the gravity affecting this entity specifically.

#### `gravitydir`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector gravitydir;
```

Specifies the direction in which gravity acts. Must be normalised. '0 0 0' also means down. Use '0 0 1' if you want the player to be able to run on ceilings.

#### `groundentity`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.entity groundentity;
```

Entity supporting this entity when grounded; inspect FL_ONGROUND as well.

#### `hasted`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float hasted;
```

Hexen II-compatible player-class/haste state; not generic Quake inventory semantics.

#### `health`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float health;
```

Game-managed current/maximum health. Implement your own damage, healing and death policy.

#### `hitcontentsmaski`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.int hitcontentsmaski;
```

Traces performed for this entity will impact against surfaces that match this contents mask (CONTENTBITS_* constants).

#### `hull`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float hull;
```

Overrides the hull used by the entity for walkmove/movetogoal and not traceline/tracebox.

#### `ideal_yaw`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float ideal_yaw;
```

Desired yaw and maximum yaw adjustment used by changeyaw/AI.

#### `idealpitch`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float idealpitch;
```

Desired pitch, used by changepitch and game view/AI logic.

#### `impulse`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float impulse;
```

Player input impulse command; consume/reset it according to your game's command policy.

#### `items`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float items;
```

Conventional inventory bitmask sent to clients; set/clear bits according to your game's inventory rules.

#### `joint_suspension_damping`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Fork-only (FTEQW Delta).

```qc
.float joint_suspension_damping;
```

FTEQW Delta only. Hinge2 linear damping in mass per second (Quake force/velocity units). Must be nonnegative; nonzero damping requires positive stiffness. Jolt backend only.

#### `joint_suspension_stiffness`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Fork-only (FTEQW Delta).

```qc
.float joint_suspension_stiffness;
```

FTEQW Delta only. Hinge2 linear spring stiffness in mass per second squared (Quake force/distance units). Zero with zero damping keeps the pivot rigid. Jolt backend only.

#### `jointgroup`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float jointgroup;
```

Rigid-body joint type/group selection; requires a compatible physics backend and related entity setup.

#### `jointtype`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float jointtype;
```

Rigid-body joint type/group selection; requires a compatible physics backend and related entity setup.

#### `lastruntime`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float lastruntime;
```

This field used to be used to avoid running an entity multiple times in a single frame due to quakeworld's out-of-order thinks. It is no longer used by FTE due to precision issues, but may still be updated for compatibility reasons.

#### `lerpfrac`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float lerpfrac;
```

If 0, use frame1 only. If 1, use frame2 only. Mix them together for values between.

#### `lerpfrac3`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float lerpfrac3;
```

Weight of .frame3 - .frame's weight is automatically calculated as 1-(lerpfrac+lerpfrac3+lerpfrac4), as a result these fields should NEVER add to above 1.

#### `lerpfrac4`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float lerpfrac4;
```

Additional animation frame and blend fraction used by extended animation blending.

#### `light_lev`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float light_lev;
```

This is the radius of an entity's light. This is not normally used by the engine, but is used for realtime lights (ones that are enabled with the pflags field).

#### `light_level`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float light_level;
```

Used by hexen2 to indicate the light level where the player is standing.

#### `ltime`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float ltime;
```

On MOVETYPE_PUSH entities, this is used as an alternative to the 'time' global, and .nextthink is synced to this instead of time. This allows time to effectively freeze if the entity is blocked, ensuring the think happens when the entity reaches the target point instead of randomly.

#### `mass`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float mass;
```

Rigid-body mass; not a replacement for ordinary MOVETYPE_WALK player movement rules.

#### `max_angular`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float max_angular;
```

Rigid-body linear/angular damping and angular-speed limit; backend dependent.

#### `max_health`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float max_health;
```

Game-managed current/maximum health. Implement your own damage, healing and death policy.

#### `maxs`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector maxs;
```

like mins, but in the other direction.

#### `maxspeed`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float maxspeed;
```

Per-entity/player movement speed limit in the supported movement path.

#### `message`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.string message;
```

Allows the csqc to read the map description from the server.

#### `mins`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector mins;
```

The minimum extent of the model (ie: the bottom-left coordinate relative to the entity's origin). Change via setsize. May also be changed by setmodel.

#### `model`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.string model;
```

The model name that was set via setmodel, in theory. Often, this is cleared to null to prevent the engine from being seen by clients while not changing modelindex. This behaviour allows inline models to remain solid yet be invisible.

#### `modelflags`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float modelflags;
```

Used to override the flags set in the entity's model. Should be set according to the MF_ constants. Use effects|=EF_NOMODELFLAGS to ignore the model's flags completely. The traileffectnum field is more versatile.

#### `modelindex`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float modelindex;
```

This is the model precache index for the model that was set on the entity, instead of having to look up the model according to the .model field. Use setmodel to change it.

#### `movechain`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.entity movechain;
```

This is a linked list of entities which will be moved whenever this entity moves, logically they are attached to this entity.

#### `movedir`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.vector movedir;
```

Direction convention used by game movers/triggers; initialize it from your map conventions.

#### `movement`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.vector movement;
```

These are the directions that the player is currently trying to move in (ie: which +forward/+moveright/+moveup etc buttons they have held), expressed relative to that player's angles. Order is forward, right, up.

#### `movetype`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float movetype;
```

Describes how the entity moves. One of the MOVETYPE_ constants.

#### `netname`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.string netname;
```

Player's displayed network name; handle it as untrusted text.

#### `nextthink`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float nextthink;
```

The time at which the entity is next scheduled to fire its think event. For MOVETYPE_PUSH entities, this is relative to that entity's ltime field, for all other entities it is relative to the time gloal.

#### `nodrawtoclient`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Cannot be recorded in MVDs, nor work properly with splitscreen. Use CSQC instead.") .entity nodrawtoclient;
```

This entity will not be sent to the player named by this field. They will be invisible and not emit dlights/particles. Does not work in MVD-recorded game.

#### `noise`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.string noise;
```

Conventional per-entity sound-path fields; precache and play them explicitly.

#### `noise1`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.string noise1;
```

Conventional per-entity sound-path fields; precache and play them explicitly.

#### `noise2`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.string noise2;
```

Conventional per-entity sound-path fields; precache and play them explicitly.

#### `noise3`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.string noise3;
```

Conventional per-entity sound-path fields; precache and play them explicitly.

#### `oldorigin`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector oldorigin;
```

This is often used on players to reset the player back to where they were last frame if they somehow got stuck inside something due to fpu precision. Never change a player's oldorigin field to inside a solid, because that might cause them to become pemanently stuck.

#### `origin`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector origin;
```

The current location of the entity in world space. Inline bsp entities (ie: ones placed by a mapper) will typically have a value of '0 0 0' in their neutral pose, as the geometry is offset from that. It is the reference point of the entity rather than the center of its geometry, for non-bsp models, this is often not a significant distinction.

#### `owner`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.entity owner;
```

Owning entity, commonly the projectile's shooter; used by relevant collision/game rules.

#### `pflags`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float pflags;
```

Realtime lighting flags

#### `pitch_speed`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float pitch_speed;
```

Maximum pitch adjustment used by changepitch.

#### `playerclass`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float playerclass;
```

Hexen II-compatible player-class/haste state; not generic Quake inventory semantics.

#### `pmove_flags`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float pmove_flags;
```

Movement/prediction flags for the corresponding physics path; keep client/server interpretation consistent.

#### `predraw`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float() predraw;
```

Called by addentities after the filter and before the entity is actually drawn. Do your interpolation and animation in here. Should return one of the PREDRAW_* constants.

#### `punchangle`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector punchangle;
```

Player view kick angles; game code can set them for impacts/recoil.

#### `pvsflags`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float pvsflags;
```

Reconfigures when the entity is visible to clients

#### `renderflags`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float renderflags;
```

Client rendering RF_* bitmask; use flags valid for this VM/render API.

#### `scale`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float scale;
```

Multiplier that resizes the entity. 1 is normal sized, 2 is double sized. scale 0 is remapped to 1. In SSQC, this is limited to 1/16th precision, with a maximum just shy of 16.

#### `SendEntity`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float(entity playerent, float changedflags) SendEntity;
```

Called by the engine whenever an entity needs to be (re)sent to a client's csprogs, either because SendFlags was set or because data was lost. Must write its data to the MSG_ENTITY buffer. Will be called at the engine's leasure.

#### `SendFlags`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float SendFlags;
```

Indicates that something in the entity has been changed, and that it needs to be updated to all players that can see it. The engine will clear it at some point, with the cleared bits appearing in the 'changedflags' argument of the SendEntity method.

#### `size`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector size;
```

maxs-mins. Updated when the entity is relinked (by setorigin, setsize, setmodel)

#### `skeletonindex`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float skeletonindex;
```

This object serves as a container for the skeletal bone states used to override the animation data.

#### `skin`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float skin;
```

The skin index to use. on a bsp entity, setting this to 1 will switch to the 'activated' texture instead. A negative value will be understood as a replacement contents value, so setting it to CONTENTS_WATER will make a movable pool of water.

#### `solid`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float solid;
```

Describes whether the entity is solid or not, and any special properties infered by that. Must be one of the SOLID_ constants

#### `sounds`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float sounds;
```

On worldspawn selects map music track; on other entities it is a conventional game-defined sound selector.

#### `spawnflags`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float spawnflags;
```

Bitmask parsed from the map; engine spawn-filter bits and class-specific game flags have different roles.

#### `style`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float style;
```

Used by the light util to decide how an entity's light should animate. On an entity with pflags set, this also affects realtime lights.

#### `subblend2frac`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float subblend2frac;
```

Weird animation value specific to halflife models. I've no idea what this does, probably nothing for most models.

#### `subblendfrac`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
.float subblendfrac;
```

Weird animation value specific to halflife models. On player models, this typically affects the spine's pitch, or yaw, or...

#### `tag_entity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.entity tag_entity;
```

Specifies which entity this entity's origin+angles is 'attached' to.

#### `tag_index`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.float tag_index;
```

Specifies the tag or bone on the parent entity that we're attached to. If this is -1 then the entity is instead a q3-like camera portal, with the tag_entity saying the entity to display for. If tag_entity is world then this is a q3-like portal surface marker with a separate camera (with a tag_entity referring to the portal surface).

#### `takedamage`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float takedamage;
```

DAMAGE_* eligibility setting used by game damage code; it does not supply a complete damage implementation.

#### `target`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.string target;
```

Map strings for game-defined activation links; your code performs lookup and activation.

#### `targetname`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.string targetname;
```

Map strings for game-defined activation links; your code performs lookup and activation.

#### `team`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float team;
```

Conventional game-managed team number; does not implement friendly-fire/team rules itself.

#### `teleport_time`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float teleport_time;
```

While active, prevents the player from using the +back command, also blocks waterjumping.

#### `think`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.void() think;
```

Entity callback scheduled by nextthink; reschedule explicitly for repeated work.

#### `touch`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.void() touch;
```

Entity callback when collision/touch processing contacts another entity; self is this entity and other is the contact.

#### `traileffectnum`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float traileffectnum;
```

This should be set to the result of particleeffectnum, in order to attach a custom trail effect to an entity as it moves.

#### `uniquespawnid`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float uniquespawnid;
```

Incremented by 1 whenever the entity is respawned. Persists across remove calls, for when the two-second grace period is insufficient.

#### `use`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.void() use;
```

Game-defined entity activation callback; your target/interaction code invokes it.

#### `v_angle`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.vector v_angle;
```

The angles a player is viewing. +x is DOWN (pitch, yaw, roll)

#### `velocity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
.vector velocity;
```

The direction and speed that the entity is moving in world space.

#### `Version`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Use SendFlags instead.") .float Version;
```

Obsolete

#### `view2`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.entity view2;
```

defines a second viewpoint, typically displayed in a corner of the screen (also punches open pvs).

#### `view_ofs`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.vector view_ofs;
```

Player eye offset from origin, also used when constructing aiming traces.

#### `viewmodelforclient`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Redundant. Cannot be recorded in MVDs, nor work properly with splitscreen. Use CSQC instead.") .entity viewmodelforclient;
```

This entity will be sent only to the player named by this field, and this entity will be attached to the player's view as an additional weapon model.

#### `viewzoom`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float viewzoom;
```

Player view/FOV scale communicated by supporting protocols.

#### `vw_index`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float vw_index;
```

This acts as a second modelindex, using the same frames etc.

#### `waterlevel`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float waterlevel;
```

Engine movement state for liquid immersion, conventionally 0=dry, 1=feet, 2=waist, 3=submerged.

#### `watertype`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float watertype;
```

Liquid contents identifier where the entity is immersed; use CONTENT_* values.

#### `weapon`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float weapon;
```

Game-managed weapon selection/stat identifier; the engine does not implement firing from this value alone.

#### `weaponframe`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float weaponframe;
```

Animation frame of the player's first-person viewmodel.

#### `weaponmodel`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.string weaponmodel;
```

Player first-person viewmodel path; precache before assigning.

#### `yaw_speed`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
.float yaw_speed;
```

Desired yaw and maximum yaw adjustment used by changeyaw/AI.

### Constants

709 declarations.

#### `ATTN_IDLE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float ATTN_IDLE = 2;
```

Extra attenuation so that sounds don't travel too far.

#### `ATTN_NONE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float ATTN_NONE = 0;
```

Sounds with this attenuation can be heard throughout the map

#### `ATTN_NORM`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float ATTN_NORM = 1;
```

Standard attenuation

#### `ATTN_STATIC`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float ATTN_STATIC = 3;
```

Even more attenuation to avoid torches drowing out everything else throughout the map.

#### `CHAN_AUTO`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CHAN_AUTO = 0;
```

The automatic channel, play as many sounds on this channel as you want, and they'll all play, however the other channels will replace each other.

#### `CHAN_BODY`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CHAN_BODY = 4;
```

Conventional nonzero entity sound channels for weapon, voice, item or body events; replace the previous sound on that entity/channel.

#### `CHAN_ITEM`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CHAN_ITEM = 3;
```

Conventional nonzero entity sound channels for weapon, voice, item or body events; replace the previous sound on that entity/channel.

#### `CHAN_VOICE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CHAN_VOICE = 2;
```

Conventional nonzero entity sound channels for weapon, voice, item or body events; replace the previous sound on that entity/channel.

#### `CHAN_WEAPON`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CHAN_WEAPON = 1;
```

Conventional nonzero entity sound channels for weapon, voice, item or body events; replace the previous sound on that entity/channel.

#### `CHANF_RELIABLE`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
const float CHANF_RELIABLE = 8;
```

Only valid if the flags argument is not specified. The sound will be sent reliably, which is important if it is intended to replace looping sounds on doors etc.

#### `CLIENTTYPE_BOT`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float CLIENTTYPE_BOT = 2;
```

This player slot does not correlate to a real player, any messages sent to this client will be ignored.

#### `CLIENTTYPE_DISCONNECTED`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float CLIENTTYPE_DISCONNECTED = 0;
```

Return value from clienttype() builtin. This entity is a player slot that is currently empty.

#### `CLIENTTYPE_NOTACLIENT`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float CLIENTTYPE_NOTACLIENT = 3;
```

This entity is not even a player slot. This is typically an error condition.

#### `CLIENTTYPE_REAL`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float CLIENTTYPE_REAL = 1;
```

This is a real player, and not a bot.

#### `CONTENT_EMPTY`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CONTENT_EMPTY = -1;
```

Legacy contents value for empty space.

#### `CONTENT_LADDER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CONTENT_LADDER = -16;
```

If this value is assigned to a solid_bsp's .skin field, the entity will become a ladder volume.

#### `CONTENT_LAVA`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CONTENT_LAVA = -5;
```

Legacy liquid contents values identifying water, slime or lava; damage/drowning behavior is game code.

#### `CONTENT_SKY`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CONTENT_SKY = -6;
```

Legacy contents value for sky geometry.

#### `CONTENT_SLIME`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CONTENT_SLIME = -4;
```

Legacy liquid contents values identifying water, slime or lava; damage/drowning behavior is game code.

#### `CONTENT_SOLID`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CONTENT_SOLID = -2;
```

Legacy contents value for solid world geometry.

#### `CONTENT_WATER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float CONTENT_WATER = -3;
```

Legacy liquid contents values identifying water, slime or lava; damage/drowning behavior is game code.

#### `CONTENTBIT_BODY`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_BODY = 0x02000000i;
```

Content bit that indicates collisions against SOLID_BBOX/SOLID_SLIDEBOX entities.

#### `CONTENTBIT_CORPSE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_CORPSE = 0x04000000i;
```

Content bit that indicates collisions against SOLID_CORPSE entities.

#### `CONTENTBIT_FTELADDER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_FTELADDER = 0x00004000i;
```

Content bit used for .skin=CONTENT_LADDER entities.

#### `CONTENTBIT_LAVA`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_LAVA = 0x00000008i;
```

Extended contents-mask bit for lava; test bitwise, independently of legacy CONTENT_* values.

#### `CONTENTBIT_MONSTERCLIP`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_MONSTERCLIP = 0x00020000i;
```

Extended contents-mask bit for monsterclip; test bitwise, independently of legacy CONTENT_* values.

#### `CONTENTBIT_NONE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_NONE = 0x00000000i;
```

No flags in an extended contents bitmask.

#### `CONTENTBIT_PLAYERCLIP`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_PLAYERCLIP = 0x00010000i;
```

Extended contents-mask bit for playerclip; test bitwise, independently of legacy CONTENT_* values.

#### `CONTENTBIT_Q2LADDER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_Q2LADDER = 0x20000000i;
```

Content bit specific to q2bsp (conflicts with q3bsp contents so use with caution).

#### `CONTENTBIT_SKY`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_SKY = 0x80000000i;
```

Content bit somewhat specific to q1bsp (aliases to NODROP in q3bsp), but you should probably check surfaceflags&SURF_SKY as well for q2+q3bsp too.

#### `CONTENTBIT_SLIME`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_SLIME = 0x00000010i;
```

Extended contents-mask bit for slime; test bitwise, independently of legacy CONTENT_* values.

#### `CONTENTBIT_SOLID`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_SOLID = 0x00000001i;
```

Extended contents-mask bit for solid; test bitwise, independently of legacy CONTENT_* values.

#### `CONTENTBIT_WATER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBIT_WATER = 0x00000020i;
```

Extended contents-mask bit for water; test bitwise, independently of legacy CONTENT_* values.

#### `CONTENTBITS_BOXSOLID`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBITS_BOXSOLID = CONTENTBIT_SOLID|0x00000002i|CONTENTBIT_BODY|CONTENTBIT_PLAYERCLIP;
```

Bits that tracebox would normally consider solid

#### `CONTENTBITS_FLUID`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBITS_FLUID = CONTENTBIT_WATER|CONTENTBIT_SLIME|CONTENTBIT_LAVA|CONTENTBIT_SKY;
```

Combined extended liquid-contents mask; use bit tests, not legacy negative contents comparisons.

#### `CONTENTBITS_POINTSOLID`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const int CONTENTBITS_POINTSOLID = CONTENTBIT_SOLID|0x00000002i|CONTENTBIT_BODY;
```

Bits that traceline would normally consider solid

#### `CVAR_TYPEFLAG_ENGINE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float CVAR_TYPEFLAG_ENGINE = 8;
```

Cvar was created by the engine itself (not user/mod created).

#### `CVAR_TYPEFLAG_EXISTS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float CVAR_TYPEFLAG_EXISTS = 1;
```

Cvar name actually exists.

#### `CVAR_TYPEFLAG_HASDESCRIPTION`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float CVAR_TYPEFLAG_HASDESCRIPTION = 16;
```

cvar_description will return something (hopefully) useful.

#### `CVAR_TYPEFLAG_PRIVATE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float CVAR_TYPEFLAG_PRIVATE = 4;
```

QC is not allowed to read.

#### `CVAR_TYPEFLAG_READONLY`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float CVAR_TYPEFLAG_READONLY = 32;
```

cvar may not be changed by qc.

#### `CVAR_TYPEFLAG_SAVED`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float CVAR_TYPEFLAG_SAVED = 2;
```

Cvar is flaged for archival (might need cfg_save to actually save).

#### `DAMAGE_AIM`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float DAMAGE_AIM = 2;
```

Game damage convention: damageable and eligible for traditional aim assistance.

#### `DAMAGE_NO`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float DAMAGE_NO = 0;
```

Game damage convention: not damageable.

#### `DAMAGE_YES`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float DAMAGE_YES = 1;
```

Game damage convention: damageable.

#### `DRAWFLAG_2D`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float DRAWFLAG_2D = 4;
```

For use with beginpolygon. The polygon will be drawn to the 2d screen, instead of being added to the 3d scene.

#### `DRAWFLAG_ADD`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float DRAWFLAG_ADD = 1;
```

Forces additive blending, overriding any shader settings.

#### `DRAWFLAG_LINES`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float DRAWFLAG_LINES = 2048;
```

For use with beginpolygon. The surface verticies should be interpreted as a line loop, instead of a triangle fan.

#### `DRAWFLAG_MODULATE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float DRAWFLAG_MODULATE = 2;
```

Forces alpha blending, overriding any shader settings.

#### `DRAWFLAG_NORMAL`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float DRAWFLAG_NORMAL = 0;
```

Args for drawpic/drawfill/beginpolygon. Not to be confused with the hexen2-compatibility feature.

#### `DRAWFLAG_TWOSIDED`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float DRAWFLAG_TWOSIDED = 1024;
```

For use with beginpolygon. The polygon will be two-sided without any backface culling.

#### `drawfontscale`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
var vector drawfontscale = '1 1 0';
```

Specifies a scaler for all text rendering. There are other ways to implement this.

#### `EF_ADDITIVE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_ADDITIVE = 32;
```

The entity will be drawn with an additive blend. This is NOT supported on players in any quakeworld engine.

#### `EF_BLUE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_BLUE = 64;
```

A blue glow

#### `EF_BRIGHTFIELD`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_BRIGHTFIELD = 1;
```

Classic particle field effect on an entity.

#### `EF_BRIGHTLIGHT`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_BRIGHTLIGHT = 4;
```

Classic bright/dim dynamic light effect bits; renderer settings may disable their presentation.

#### `EF_DIMLIGHT`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_DIMLIGHT = 8;
```

Classic bright/dim dynamic light effect bits; renderer settings may disable their presentation.

#### `EF_FLAG1`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
const float EF_FLAG1 = 16;
```

QuakeWorld player flag-carrying presentation bits.

#### `EF_FLAG2`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
const float EF_FLAG2 = 32;
```

QuakeWorld player flag-carrying presentation bits.

#### `EF_FULLBRIGHT`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_FULLBRIGHT = 512;
```

This entity will ignore lighting

#### `EF_GREEN`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_GREEN = 262144;
```

A green glow

#### `EF_MUZZLEFLASH`

**Scope:** NQ SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_MUZZLEFLASH = 2;
```

Classic muzzle-flash effect bit; visual effect only.

#### `EF_NODEPTHTEST`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_NODEPTHTEST = 8192;
```

This entity will be drawn over the top of other things that are closer.

#### `EF_NODRAW`

**Scope:** NQ SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_NODRAW = 16;
```

Disables drawing of the model. Does NOT work on QW players.

#### `EF_NOMODELFLAGS`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float EF_NOMODELFLAGS = 8388608;
```

Surpresses the normal flags specified in the model.

#### `EF_NOSHADOW`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_NOSHADOW = 4096;
```

This entity will not cast shadows

#### `EF_RED`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float EF_RED = 128;
```

A red glow

#### `EV_DOUBLE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_DOUBLE = 12;
```

Reflected QC value-type identifier: double; used by typed field/hash/reflection APIs.

#### `EV_ENTITY`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_ENTITY = 4;
```

Reflected QC value-type identifier: entity; used by typed field/hash/reflection APIs.

#### `EV_FIELD`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_FIELD = 5;
```

Reflected QC value-type identifier: field; used by typed field/hash/reflection APIs.

#### `EV_FLOAT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_FLOAT = 2;
```

Reflected QC value-type identifier: float; used by typed field/hash/reflection APIs.

#### `EV_FUNCTION`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_FUNCTION = 6;
```

Reflected QC value-type identifier: function; used by typed field/hash/reflection APIs.

#### `EV_INT64`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_INT64 = 10;
```

Reflected QC value-type identifier: int64; used by typed field/hash/reflection APIs.

#### `EV_INTEGER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_INTEGER = 8;
```

Reflected QC value-type identifier: integer; used by typed field/hash/reflection APIs.

#### `EV_POINTER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_POINTER = 7;
```

Reflected QC value-type identifier: pointer; used by typed field/hash/reflection APIs.

#### `EV_STRING`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_STRING = 1;
```

Reflected QC value-type identifier: string; used by typed field/hash/reflection APIs.

#### `EV_UINT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_UINT = 9;
```

Reflected QC value-type identifier: uint; used by typed field/hash/reflection APIs.

#### `EV_UINT64`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_UINT64 = 11;
```

Reflected QC value-type identifier: uint64; used by typed field/hash/reflection APIs.

#### `EV_VECTOR`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float EV_VECTOR = 3;
```

Reflected QC value-type identifier: vector; used by typed field/hash/reflection APIs.

#### `FALSE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float FALSE = 0;
```

Boolean false, represented as zero. Not a file handle or error-string value by itself.

#### `FILE_APPEND`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float FILE_APPEND = 1;
```

Like FILE_WRITE, but writing starts at the end of the file.

#### `FILE_MMAP_READ`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float FILE_MMAP_READ = 5;
```

The file will be loaded into memory. fgets returns a pointer to the first byte (and will always return the same value for this file). Cast this to your datatype.

#### `FILE_MMAP_RW`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float FILE_MMAP_RW = 6;
```

Like FILE_MMAP_READ, except any changes to the data will be written back to disk once the file is closed.

#### `FILE_READ`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float FILE_READ = 0;
```

The file may be read via fgets to read a single line at a time.

#### `FILE_READNL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float FILE_READNL = 4;
```

Like FILE_READ, except newlines are not special. fgets reads the entire file into a tempstring.

#### `FILE_STREAM`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float FILE_STREAM = -1;
```

The filename is a tcp:// or tls:// network scheme that should be used instead of direct file access. Such file handles should be read as binary with fread instead of using fgets, in order to distinguish between empty-line, eof, and would-block - the qc is responsible for periodically checking if new data is available while being aware that ANY response may be received a single byte at a time.

#### `FILE_WRITE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float FILE_WRITE = 2;
```

fputs will be used to write to the file.

#### `FL_CLIENT`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float FL_CLIENT = 8;
```

Marks a client/player entity; do not arbitrarily turn a non-client entity into a network client with this bit.

#### `FL_FINDABLE_NONSOLID`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float FL_FINDABLE_NONSOLID = 16384;
```

Allows this entity to be found with findradius

#### `FL_FLY`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float FL_FLY = 1;
```

Entity capability flags for flying/swimming movement paths.

#### `FL_GODMODE`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float FL_GODMODE = 64;
```

Conventional game policy flags for invulnerability or exclusion from AI targeting; your rules must respect them.

#### `FL_INWATER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float FL_INWATER = 16;
```

Engine movement flag for immersion in liquid.

#### `FL_ITEM`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float FL_ITEM = 256;
```

Item/pickup collision convention flag.

#### `FL_JUMPRELEASED`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
const float FL_JUMPRELEASED = 4096;
```

Legacy game jump-button latch convention.

#### `FL_LAGGEDMOVE`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float FL_LAGGEDMOVE = 65536;
```

Enables anti-lag on rockets etc.

#### `FL_MONSTER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float FL_MONSTER = 32;
```

Marks a monster for applicable collision/trace/game conventions; it does not implement AI.

#### `FL_NOTARGET`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float FL_NOTARGET = 128;
```

Conventional game policy flags for invulnerability or exclusion from AI targeting; your rules must respect them.

#### `FL_ONGROUND`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float FL_ONGROUND = 512;
```

Engine movement flag indicating ground support; check before game actions such as jumping.

#### `FL_PARTIALGROUND`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float FL_PARTIALGROUND = 1024;
```

Movement flag for partial ground support in step/monster movement.

#### `FL_SWIM`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float FL_SWIM = 2;
```

Entity capability flags for flying/swimming movement paths.

#### `FL_WATERJUMP`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float FL_WATERJUMP = 2048;
```

Movement flag for an active water-jump state.

#### `FONT_DEFAULT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float FONT_DEFAULT = 0;
```

Default font identifier for client/menu text drawing.

#### `GE_ABSLIGHT`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_ABSLIGHT = 207;
```

Valid for getentity. Guesses the entity's .abslight float.

#### `GE_ABSMAX`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_ABSMAX = 15;
```

Valid for getentity. Guesses the entity's .absmax vector.

#### `GE_ABSMIN`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_ABSMIN = 14;
```

Valid for getentity. Guesses the entity's .absmin vector.

#### `GE_ACTIVE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_ACTIVE = 0;
```

Valid for getentity. Returns whether this entity is known to the client or not.

#### `GE_ALPHA`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_ALPHA = 7;
```

Valid for getentity. Returns the entity alpha.

#### `GE_ANGLES`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_ANGLES = 204;
```

Valid for getentity. Guesses the entity's .angles vector.

#### `GE_COLORMOD`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_COLORMOD = 8;
```

Valid for getentity. Returns the colormod vector.

#### `GE_DRAWFLAGS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_DRAWFLAGS = 206;
```

Valid for getentity. Guesses the entity's .drawflags float.

#### `GE_EFFECTS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_EFFECTS = 202;
```

Valid for getentity. Guesses the entity's .effects float.

#### `GE_FATNESS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_FATNESS = 205;
```

Valid for getentity. Guesses the entity's .fatness float.

#### `GE_FORWARD`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_FORWARD = 2;
```

Valid for getentity. Returns the interpolated forward vector.

#### `GE_FRAME`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_FRAME = 203;
```

Valid for getentity. Guesses the entity's .frame float.

#### `GE_GLOWCOLOUR`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_GLOWCOLOUR = 210;
```

Valid for getentity. Guesses the entity's .glowcolor float.

#### `GE_GLOWMOD`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_GLOWMOD = 208;
```

Valid for getentity. Guesses the entity's .glowmod vector.

#### `GE_GLOWSIZE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_GLOWSIZE = 209;
```

Valid for getentity. Guesses the entity's .glowsize float.

#### `GE_GRAVITYDIR`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_GRAVITYDIR = 217;
```

Valid for getentity. Guesses the entity's .gravitydir vector.

#### `GE_MAXENTS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_MAXENTS = -1;
```

Valid for getentity, ignores the entity argument. Returns the maximum number of entities which may be valid, to avoid having to poll 65k when only 100 are used.

#### `GE_MAXS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_MAXS = 13;
```

Valid for getentity. Guesses the entity's .max vector.

#### `GE_MINS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_MINS = 12;
```

Valid for getentity. Guesses the entity's .min vector.

#### `GE_MODELINDEX`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_MODELINDEX = 200;
```

Valid for getentity. Guesses the entity's .modelindex float.

#### `GE_MODELINDEX2`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_MODELINDEX2 = 201;
```

Valid for getentity. Guesses the entity's .vw_index float.

#### `GE_ORIGIN`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_ORIGIN = 1;
```

Valid for getentity. Returns the interpolated .origin.

#### `GE_ORIGINANDVECTORS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_ORIGINANDVECTORS = 6;
```

Valid for getentity. Returns interpolated .origin, but also sets v_forward, v_right, and v_up accordingly. Use vectoangles(v_forward,v_up) to determine the angles.

#### `GE_PANTSCOLOR`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_PANTSCOLOR = 9;
```

Valid for getentity. Returns the entity's lower color (from .colormap), as a palette range value.

#### `GE_RIGHT`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_RIGHT = 3;
```

Valid for getentity. Returns the entity's right vector.

#### `GE_RTCOLOUR`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_RTCOLOUR = 213;
```

Valid for getentity. Guesses the entity's .color vector.

#### `GE_RTPFLAGS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_RTPFLAGS = 212;
```

Valid for getentity. Guesses the entity's .pflags float.

#### `GE_RTRADIUS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_RTRADIUS = 214;
```

Valid for getentity. Guesses the entity's .light_lev float.

#### `GE_RTSTYLE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_RTSTYLE = 211;
```

Valid for getentity. Guesses the entity's .style float.

#### `GE_SCALE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_SCALE = 5;
```

Valid for getentity. Returns the entity .scale.

#### `GE_SHIRTCOLOR`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_SHIRTCOLOR = 10;
```

Valid for getentity. Returns the entity's lower color (from .colormap), as a palette range value.

#### `GE_SKIN`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_SKIN = 11;
```

Valid for getentity. Returns the entity's .skin index.

#### `GE_TAGENTITY`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_TAGENTITY = 215;
```

Valid for getentity. Guesses the entity's .tag_entity float.

#### `GE_TAGINDEX`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_TAGINDEX = 216;
```

Valid for getentity. Guesses the entity's .tag_index float.

#### `GE_TRAILEFFECTNUM`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_TRAILEFFECTNUM = 218;
```

Valid for getentity. Guesses the entity's .traileffectnum float.

#### `GE_UP`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float GE_UP = 4;
```

Valid for getentity. Returns the entity's up vector.

#### `GEOMTYPE_BOX`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_BOX = 1;
```

Rigid-body geometry selector for box; use with .geomtype and a supporting physics backend.

#### `GEOMTYPE_CAPSULE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_CAPSULE = 3;
```

Rigid-body geometry selector for capsule; use with .geomtype and a supporting physics backend.

#### `GEOMTYPE_CAPSULE_X`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_CAPSULE_X = 6;
```

Rigid-body geometry selector for capsule x; use with .geomtype and a supporting physics backend.

#### `GEOMTYPE_CAPSULE_Y`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_CAPSULE_Y = 7;
```

Rigid-body geometry selector for capsule y; use with .geomtype and a supporting physics backend.

#### `GEOMTYPE_CAPSULE_Z`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_CAPSULE_Z = 8;
```

Rigid-body geometry selector for capsule z; use with .geomtype and a supporting physics backend.

#### `GEOMTYPE_CYLINDER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_CYLINDER = 5;
```

Rigid-body geometry selector for cylinder; use with .geomtype and a supporting physics backend.

#### `GEOMTYPE_CYLINDER_X`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_CYLINDER_X = 9;
```

Rigid-body geometry selector for cylinder x; use with .geomtype and a supporting physics backend.

#### `GEOMTYPE_CYLINDER_Y`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_CYLINDER_Y = 10;
```

Rigid-body geometry selector for cylinder y; use with .geomtype and a supporting physics backend.

#### `GEOMTYPE_CYLINDER_Z`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_CYLINDER_Z = 11;
```

Rigid-body geometry selector for cylinder z; use with .geomtype and a supporting physics backend.

#### `GEOMTYPE_NONE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_NONE = -1;
```

No rigid-body collision geometry.

#### `GEOMTYPE_SOLID`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_SOLID = 0;
```

Derive rigid-body collision geometry from the entity's solid/model configuration.

#### `GEOMTYPE_SPHERE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_SPHERE = 2;
```

Rigid-body geometry selector for sphere; use with .geomtype and a supporting physics backend.

#### `GEOMTYPE_TRIMESH`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float GEOMTYPE_TRIMESH = 4;
```

Rigid-body geometry selector for trimesh; use with .geomtype and a supporting physics backend.

#### `GGDI_DESCRIPTION`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float GGDI_DESCRIPTION = 1;
```

The human-readable title of the mod. Empty when no data is known (ie: the gamedir just contains some maps).

#### `GGDI_GAMEDIR`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float GGDI_GAMEDIR = 0;
```

Used with getgamedirinfo to query the mod's public gamedir. There is often other info that cannot be expressed with just a gamedir name, resulting in dupes or other weirdness.

#### `GGDI_GAMEDIRLIST`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float GGDI_GAMEDIRLIST = 5;
```

A semi-colon delimited list of gamedirs that the mod's content can be loaded through.

#### `GGDI_ICON`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float GGDI_ICON = 4;
```

The mod's Icon path, ready for drawpic.

#### `GGDI_LOADCOMMAND`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float GGDI_LOADCOMMAND = 3;
```

The console command needed to actually load the mod.

#### `GGDI_OVERRIDES`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float GGDI_OVERRIDES = 2;
```

A list of settings overrides.

#### `global_gravitydir`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
vector global_gravitydir = '0 0 -1';
```

The direction gravity should act in if not otherwise specified per entity.

#### `GPMI_ACTION`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_ACTION = 9;
```

The action that will be carried out when a 'pkg apply' command is issued(and confirmed).

#### `GPMI_AUTHOR`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_AUTHOR = 6;
```

Who owns the respective (additional) copyrights.

#### `GPMI_AVAILABLE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_AVAILABLE = 10;
```

Specifies whether the package may be downloaded.

#### `GPMI_CATEGORY`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_CATEGORY = 1;
```

Refers to the package's category.

#### `GPMI_DESCRIPTION`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_DESCRIPTION = 4;
```

Fairly long textural description of the package which may include gotchas/warnings and other fairly important info.

#### `GPMI_FILESIZE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_FILESIZE = 11;
```

The download size of the package.

#### `GPMI_GAMEDIR`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_GAMEDIR = 12;
```

Which gamedir this package will be installed into.

#### `GPMI_INSTALLED`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_INSTALLED = 8;
```

Reports the package's installation status. May be enabled(fully installed and active), present(previously installed, but not currently in use), corrupt(enabling will delete+redownload), pending(queued for installation), or a percentage(currently downloading). If empty means the user does not have a copy nor do they currently intend to install it.

#### `GPMI_LICENSE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_LICENSE = 5;
```

The approximate name of the license the package is covered by, if known.

#### `GPMI_MAPS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_MAPS = 13;
```

Retrieves a tokenisable list of map names provided in this package, loadable via `map pkgname:mapname`.

#### `GPMI_NAME`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_NAME;
```

Used with getpackagemanagerinfo. Refers to the package's name.

#### `GPMI_PREVIEWIMG`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_PREVIEWIMG = 14;
```

An image to drawpic for a preview.

#### `GPMI_TITLE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_TITLE = 2;
```

The human-readable title of the mod with spaces and things.

#### `GPMI_VERSION`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_VERSION = 3;
```

The package's version. There may be multiple packages with the same name.

#### `GPMI_WEBSITE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int GPMI_WEBSITE = 7;
```

Website associated with the third-party package.

#### `HASH_ADD`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float HASH_ADD = 512;
```

Used with hash_add. The new entry will be inserted in addition to the existing entry.

#### `HASH_REPLACE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float HASH_REPLACE = 256;
```

Used with hash_add. Attempts to remove the old value instead of adding two values for a single key.

#### `IE_ACCELEROMETER`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IE_ACCELEROMETER = 4;
```

Input-event types carrying accelerometer/gyroscope data on supported devices.

#### `IE_FOCUS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IE_FOCUS = 5;
```

Specifies that input focus was given. parama says mouse focus, paramb says keyboard focus. If either are -1, then it is unchanged.

#### `IE_GYROSCOPE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IE_GYROSCOPE = 7;
```

Input-event types carrying accelerometer/gyroscope data on supported devices.

#### `IE_JOYAXIS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IE_JOYAXIS = 6;
```

Specifies that what value a joystick/controller axis currently specifies. x=axis, y=value. Will be called multiple times, once for each axis of each active controller.

#### `IE_KEYDOWN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IE_KEYDOWN = 0;
```

Specifies that a key was pressed. Second argument is the scan code. Third argument is the unicode (printable) char value. Fourth argument denotes which keyboard(or mouse, if its a mouse 'scan' key) the event came from. Note that some systems may completely separate scan codes and unicode values, with a 0 value for the unspecified argument.

#### `IE_KEYUP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IE_KEYUP = 1;
```

Specifies that a key was released. Arguments are the same as IE_KEYDOWN. On some systems, this may be fired instantly after IE_KEYDOWN was fired.

#### `IE_MOUSEABS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IE_MOUSEABS = 3;
```

Specifies that a mouse cursor or touch event was moved to a specific location relative to the virtual screen space. Second argument is the new X position, third argument is the new Y position. Fourth argument is which mouse or touch event triggered the event.

#### `IE_MOUSEDELTA`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IE_MOUSEDELTA = 2;
```

Specifies that a mouse was moved (touch screens and tablets typically give IE_MOUSEABS events instead, use in_windowed_mouse 0 to test code to cope with either). Second argument is the X displacement, third argument is the Y displacement. Fourth argument is which mouse or touch event triggered the event.

#### `IMGFMT_A2B10G10R10`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_A2B10G10R10 = 10;
```

Packed 32-bit packed 10-bit colour pixel format. Requires gl3 support.

#### `IMGFMT_D16`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_D16 = 4;
```

16-bit depth pixel format. Must not be used with VF_RT_DESTCOLOUR*.

#### `IMGFMT_D24`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_D24 = 5;
```

24-bit depth pixel format. Must not be used with VF_RT_DESTCOLOUR*.

#### `IMGFMT_D32`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_D32 = 6;
```

32-bit depth pixel format. Must not be used with VF_RT_DESTCOLOUR*.

#### `IMGFMT_P8_RGB8`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_P8_RGB8 = 15;
```

8bit paletted format, with 768 bytes of RGB8 palette directly following the image data. Exclusively for r_uploadimage.

#### `IMGFMT_P8_RGBA8`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_P8_RGBA8 = 16;
```

8bit paletted format, with 1024 bytes of RGBA8 palette directly following the image data. Exclusively for r_uploadimage.

#### `IMGFMT_R16F`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_R16F = 8;
```

Single channel red-only Half-Float pixel format. Requires gl3 support.

#### `IMGFMT_R16G16B16A16F`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_R16G16B16A16F = 2;
```

Half-Float pixel format. Requires gl3 support.

#### `IMGFMT_R32F`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_R32F = 9;
```

Single channel red-only Float pixel format. Requires gl3 support.

#### `IMGFMT_R32G32B32A32F`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_R32G32B32A32F = 3;
```

Regular Float pixel format. Requires gl3 support.

#### `IMGFMT_R32G32B32F`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_R32G32B32F = 14;
```

A pixel format that matches QC's vector type.

#### `IMGFMT_R4G4B4A4`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_R4G4B4A4 = 12;
```

Packed 16-bit colour pixel format, with alpha

#### `IMGFMT_R5G6B5`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_R5G6B5 = 11;
```

Packed 16-bit colour pixel format.

#### `IMGFMT_R8`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_R8 = 7;
```

Single channel red-only 8bit pixel format.

#### `IMGFMT_R8G8`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_R8G8 = 13;
```

16-bit two-channel pixel format.

#### `IMGFMT_R8G8B8A8`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float IMGFMT_R8G8B8A8 = 1;
```

Typical 32bit rgba pixel format.

#### `INFOKEY_P_BOTTOMCOLOR`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_BOTTOMCOLOR = "bottomcolor";
```

The player's lower/pants/trouser colour (palette index).

#### `INFOKEY_P_BOTTOMCOLOR_RGB`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_BOTTOMCOLOR_RGB = "bottomcolor_rgb";
```

The player's lower/pants/trouser colour as an rgb value in a format usable with stov.

#### `INFOKEY_P_CERT_DN`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_CERT_DN = "*cert_dn";
```

Obtains the client's (d)tls certificate's Distinguished Name string.

#### `INFOKEY_P_CERT_SHA1`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_CERT_SHA1 = "*cert_sha1";
```

Obtains the client's (d)tls certificate's fingerprint.

#### `INFOKEY_P_CHALLENGE`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_CHALLENGE = "challenge";
```

Player connection challenge infokey; not a substitute for an authentication scheme.

#### `INFOKEY_P_CSQCACTIVE`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_CSQCACTIVE = "csqcactive";
```

Client has csqc enabled. CSQC ents etc will be sent to this player.

#### `INFOKEY_P_DOWNLOADPCT`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_DOWNLOADPCT = "download";
```

The client's download percentage for the current file. Additional files are not known.

#### `INFOKEY_P_ENTERTIME`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_ENTERTIME = "entertime";
```

Reads the timestamp at which the player entered the game, in terms of csqc's time global.

#### `INFOKEY_P_FRAGS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_FRAGS = "frags";
```

Reads a player's frag count.

#### `INFOKEY_P_GUID`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_GUID = "guid";
```

Some hash string which should be reasonably unique to this player's quake installation.

#### `INFOKEY_P_IP`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_IP = "ip";
```

The apparent ip address of the client. This may be a proxy's ip address.

#### `INFOKEY_P_ISCRIPPLED`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_ISCRIPPLED = "*ismuted";
```

1 if the player has the cripple penalty, and their movement values are ignored (.movement is locked to 0).

#### `INFOKEY_P_ISCUFFED`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_ISCUFFED = "*ismuted";
```

1 if the player has the cuff penalty, and is unable to attack or use impulses(.button0 and .impulse fields are locked to 0).

#### `INFOKEY_P_ISDEAF`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_ISDEAF = "*isdeaf";
```

1 if the player has the 'deaf' penalty and cannot see other people's say/say_team commands.

#### `INFOKEY_P_ISLAGGED`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_ISLAGGED = "*ismuted";
```

1 if the player has the fakelag penalty and has an extra 200ms of lag.

#### `INFOKEY_P_ISMUTED`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_ISMUTED = "*ismuted";
```

1 if the player has the 'mute' penalty and is not allowed to use the say/say_team commands.

#### `INFOKEY_P_MUTED`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_MUTED = "ignored";
```

0: we can see the result of the player's say/say_team commands. 1: we see no say/say_team messages from this player. Use the ignore command to toggle this value.

#### `INFOKEY_P_NAME`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_NAME = "name";
```

The player's name.

#### `INFOKEY_P_PACKETLOSS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_PACKETLOSS = "pl";
```

Reads a player's packetloss, as a percentage.

#### `INFOKEY_P_PING`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_PING = "ping";
```

The player's ping time, in milliseconds.

#### `INFOKEY_P_PROTOCOL`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_PROTOCOL = "protocol";
```

The network protocol the client is using to connect to the server.

#### `INFOKEY_P_REALIP`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_REALIP = "realip";
```

If sv_getrealip is set, this gives the ip as determine using that algorithm.

#### `INFOKEY_P_SPECTATOR`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_SPECTATOR = "*spectator";
```

Whether the player is a spectator or not.

#### `INFOKEY_P_SVPING`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_SVPING = "svping";
```

Player infokey for server-measured ping.

#### `INFOKEY_P_TOPCOLOR`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_TOPCOLOR = "topcolor";
```

The player's upper/shirt colour (palette index).

#### `INFOKEY_P_TOPCOLOR_RGB`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_TOPCOLOR_RGB = "topcolor_rgb";
```

The player's upper/shirt colour as an rgb value in a format usable with stov.

#### `INFOKEY_P_TRUSTLEVEL`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_TRUSTLEVEL = "trustlevel";
```

Player trust/authentication-level infokey; interpret the server's authentication policy.

#### `INFOKEY_P_USERID`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_USERID = "*userid";
```

Player server-assigned user identifier infokey; not an entity number.

#### `INFOKEY_P_VIP`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_VIP = "*VIP";
```

1 if the player has the VIP 'penalty'.

#### `INFOKEY_P_VOIP_MUTED`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_VOIP_MUTED = "vignored";
```

0: we can hear this player when they speak (assuming voip is generally enabled). 1: we ignore everything this player says. Use cl_voip_mute to change the values.

#### `INFOKEY_P_VOIPLOUDNESS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_VOIPLOUDNESS = "voiploudness";
```

Only valid for the local player. Gives a value between 0 and 1 to indicate to the user how loud their mic is.

#### `INFOKEY_P_VOIPSPEAKING`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string INFOKEY_P_VOIPSPEAKING = "voipspeaking";
```

Boolean value that says whether the given player is currently sending voice information.

#### `JOINTTYPE_FIXED`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float JOINTTYPE_FIXED = -1;
```

Rigid-body joint selector for fixed; use with .jointtype and backend-specific joint setup.

#### `JOINTTYPE_HINGE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float JOINTTYPE_HINGE = 2;
```

Rigid-body joint selector for hinge; use with .jointtype and backend-specific joint setup.

#### `JOINTTYPE_HINGE2`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float JOINTTYPE_HINGE2 = 5;
```

Rigid-body joint selector for hinge2; use with .jointtype and backend-specific joint setup.

#### `JOINTTYPE_POINT`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float JOINTTYPE_POINT = 1;
```

Rigid-body joint selector for point; use with .jointtype and backend-specific joint setup.

#### `JOINTTYPE_SLIDER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float JOINTTYPE_SLIDER = 3;
```

Rigid-body joint selector for slider; use with .jointtype and backend-specific joint setup.

#### `JOINTTYPE_UNIVERSAL`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float JOINTTYPE_UNIVERSAL = 4;
```

Rigid-body joint selector for universal; use with .jointtype and backend-specific joint setup.

#### `JSON_TYPE_ARRAY`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int JSON_TYPE_ARRAY = 3;
```

JSON node kind: array. Returned by json_get_value_type.

#### `JSON_TYPE_FALSE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int JSON_TYPE_FALSE = 5;
```

JSON node kind: false. Returned by json_get_value_type.

#### `JSON_TYPE_NULL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int JSON_TYPE_NULL = 6;
```

JSON node kind: null. Returned by json_get_value_type.

#### `JSON_TYPE_NUMBER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int JSON_TYPE_NUMBER = 1;
```

JSON node kind: number. Returned by json_get_value_type.

#### `JSON_TYPE_OBJECT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int JSON_TYPE_OBJECT = 2;
```

JSON node kind: object. Returned by json_get_value_type.

#### `JSON_TYPE_STRING`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int JSON_TYPE_STRING = 0;
```

JSON node kind: string. Returned by json_get_value_type.

#### `JSON_TYPE_TRUE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const int JSON_TYPE_TRUE = 4;
```

JSON node kind: true. Returned by json_get_value_type.

#### `K_APOSTROPHE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_APOSTROPHE = 39;
```

Key/event code for apostrophe, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_APP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_APP = -276;
```

Key/event code for app, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX1`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX1 = 800;
```

Key/event code for aux1, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX10`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX10 = 809;
```

Key/event code for aux10, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX11`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX11 = 810;
```

Key/event code for aux11, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX12`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX12 = 811;
```

Key/event code for aux12, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX13`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX13 = 812;
```

Key/event code for aux13, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX14`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX14 = 813;
```

Key/event code for aux14, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX15`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX15 = 814;
```

Key/event code for aux15, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX16`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX16 = 815;
```

Key/event code for aux16, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX2`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX2 = 801;
```

Key/event code for aux2, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX3`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX3 = 802;
```

Key/event code for aux3, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX4`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX4 = 803;
```

Key/event code for aux4, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX5`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX5 = 804;
```

Key/event code for aux5, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX6`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX6 = 805;
```

Key/event code for aux6, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX7`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX7 = 806;
```

Key/event code for aux7, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX8`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX8 = 807;
```

Key/event code for aux8, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_AUX9`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_AUX9 = 808;
```

Key/event code for aux9, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_BACKQUOTE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_BACKQUOTE = 96;
```

Key/event code for backquote, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_BACKSLASH`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_BACKSLASH = 92;
```

Key/event code for backslash, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_BACKSPACE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_BACKSPACE = 127;
```

Key/event code for backspace, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_CAPSLOCK`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_CAPSLOCK = 155;
```

Key/event code for capslock, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_DEL`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_DEL = 148;
```

Key/event code for del, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_DOWNARROW`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_DOWNARROW = 129;
```

Key/event code for downarrow, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_END`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_END = 152;
```

Key/event code for end, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_ENTER`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_ENTER = 13;
```

Key/event code for enter, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_ESCAPE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_ESCAPE = 27;
```

Key/event code for escape, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F1`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F1 = 135;
```

Key/event code for f1, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F10`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F10 = 144;
```

Key/event code for f10, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F11`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F11 = 145;
```

Key/event code for f11, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F12`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F12 = 146;
```

Key/event code for f12, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F2`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F2 = 136;
```

Key/event code for f2, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F3`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F3 = 137;
```

Key/event code for f3, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F4`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F4 = 138;
```

Key/event code for f4, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F5`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F5 = 139;
```

Key/event code for f5, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F6`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F6 = 140;
```

Key/event code for f6, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F7`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F7 = 141;
```

Key/event code for f7, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F8`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F8 = 142;
```

Key/event code for f8, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_F9`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_F9 = 143;
```

Key/event code for f9, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_A`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_A = 826;
```

Key/event code for gp a, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_B`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_B = 827;
```

Key/event code for gp b, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_DPAD_DOWN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_DPAD_DOWN = 817;
```

Key/event code for gp dpad down, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_DPAD_LEFT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_DPAD_LEFT = 818;
```

Key/event code for gp dpad left, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_DPAD_RIGHT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_DPAD_RIGHT = 819;
```

Key/event code for gp dpad right, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_DPAD_UP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_DPAD_UP = 816;
```

Key/event code for gp dpad up, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_GUIDE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_GUIDE = -238;
```

Key/event code for gp guide, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_LSHOULDER`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_LSHOULDER = 824;
```

Key/event code for gp lshoulder, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_LTHUMB`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_LTHUMB = 822;
```

Key/event code for gp lthumb, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_LTHUMB_DOWN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_LTHUMB_DOWN = 833;
```

Key/event code for gp lthumb down, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_LTHUMB_LEFT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_LTHUMB_LEFT = 834;
```

Key/event code for gp lthumb left, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_LTHUMB_RIGHT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_LTHUMB_RIGHT = 835;
```

Key/event code for gp lthumb right, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_LTHUMB_UP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_LTHUMB_UP = 832;
```

Key/event code for gp lthumb up, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_LTRIGGER`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_LTRIGGER = 830;
```

Key/event code for gp ltrigger, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_MENU`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_MENU = 820;
```

Key/event code for gp menu, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_PADDLE1`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_PADDLE1 = -249;
```

Key/event code for gp paddle1, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_PADDLE2`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_PADDLE2 = -250;
```

Key/event code for gp paddle2, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_PADDLE3`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_PADDLE3 = -251;
```

Key/event code for gp paddle3, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_PADDLE4`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_PADDLE4 = -252;
```

Key/event code for gp paddle4, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_RSHOULDER`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_RSHOULDER = 825;
```

Key/event code for gp rshoulder, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_RTHUMB`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_RTHUMB = 823;
```

Key/event code for gp rthumb, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_RTHUMB_DOWN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_RTHUMB_DOWN = 837;
```

Key/event code for gp rthumb down, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_RTHUMB_LEFT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_RTHUMB_LEFT = 838;
```

Key/event code for gp rthumb left, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_RTHUMB_RIGHT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_RTHUMB_RIGHT = 839;
```

Key/event code for gp rthumb right, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_RTHUMB_UP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_RTHUMB_UP = 836;
```

Key/event code for gp rthumb up, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_RTRIGGER`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_RTRIGGER = 831;
```

Key/event code for gp rtrigger, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_SHARE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_SHARE = -248;
```

Key/event code for gp share, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_TOUCHPAD`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_TOUCHPAD = -253;
```

Key/event code for gp touchpad, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_UNKNOWN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_UNKNOWN = -264;
```

Key/event code for gp unknown, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_VIEW`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_VIEW = 821;
```

Key/event code for gp view, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_X`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_X = 828;
```

Key/event code for gp x, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_GP_Y`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_GP_Y = 829;
```

Key/event code for gp y, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_HOME`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_HOME = 151;
```

Key/event code for home, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_INS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_INS = 147;
```

Key/event code for ins, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY1`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY1 = 768;
```

Key/event code for joy1, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY10`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY10 = 777;
```

Key/event code for joy10, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY11`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY11 = 778;
```

Key/event code for joy11, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY12`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY12 = 779;
```

Key/event code for joy12, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY13`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY13 = 780;
```

Key/event code for joy13, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY14`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY14 = 781;
```

Key/event code for joy14, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY15`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY15 = 782;
```

Key/event code for joy15, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY16`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY16 = 783;
```

Key/event code for joy16, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY17`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY17 = 784;
```

Key/event code for joy17, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY18`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY18 = 785;
```

Key/event code for joy18, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY19`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY19 = 786;
```

Key/event code for joy19, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY2`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY2 = 769;
```

Key/event code for joy2, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY20`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY20 = 787;
```

Key/event code for joy20, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY21`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY21 = 788;
```

Key/event code for joy21, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY22`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY22 = 789;
```

Key/event code for joy22, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY23`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY23 = 790;
```

Key/event code for joy23, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY24`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY24 = 791;
```

Key/event code for joy24, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY25`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY25 = 792;
```

Key/event code for joy25, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY26`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY26 = 793;
```

Key/event code for joy26, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY27`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY27 = 794;
```

Key/event code for joy27, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY28`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY28 = 795;
```

Key/event code for joy28, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY29`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY29 = 796;
```

Key/event code for joy29, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY3`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY3 = 770;
```

Key/event code for joy3, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY30`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY30 = 797;
```

Key/event code for joy30, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY31`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY31 = 798;
```

Key/event code for joy31, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY32`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY32 = 799;
```

Key/event code for joy32, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY4`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY4 = 771;
```

Key/event code for joy4, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY5`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY5 = 772;
```

Key/event code for joy5, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY6`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY6 = 773;
```

Key/event code for joy6, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY7`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY7 = 774;
```

Key/event code for joy7, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY8`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY8 = 775;
```

Key/event code for joy8, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_JOY9`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_JOY9 = 776;
```

Key/event code for joy9, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_5`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_5 = 162;
```

Key/event code for kp 5, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_DEL`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_DEL = 167;
```

Key/event code for kp del, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_DOWNARROW`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_DOWNARROW = 159;
```

Key/event code for kp downarrow, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_END`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_END = 158;
```

Key/event code for kp end, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_ENTER`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_ENTER = 172;
```

Key/event code for kp enter, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_EQUALS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_EQUALS = 173;
```

Key/event code for kp equals, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_HOME`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_HOME = 164;
```

Key/event code for kp home, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_INS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_INS = 157;
```

Key/event code for kp ins, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_LEFTARROW`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_LEFTARROW = 161;
```

Key/event code for kp leftarrow, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_MINUS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_MINUS = 170;
```

Key/event code for kp minus, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_NUMLOCK`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_NUMLOCK = 154;
```

Key/event code for kp numlock, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_PGDN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_PGDN = 160;
```

Key/event code for kp pgdn, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_PGUP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_PGUP = 166;
```

Key/event code for kp pgup, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_PLUS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_PLUS = 171;
```

Key/event code for kp plus, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_RIGHTARROW`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_RIGHTARROW = 163;
```

Key/event code for kp rightarrow, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_SLASH`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_SLASH = 168;
```

Key/event code for kp slash, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_STAR`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_STAR = 169;
```

Key/event code for kp star, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_KP_UPARROW`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_KP_UPARROW = 165;
```

Key/event code for kp uparrow, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_LALT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_LALT = 132;
```

Key/event code for lalt, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_LCTRL`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_LCTRL = 133;
```

Key/event code for lctrl, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_LEFTARROW`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_LEFTARROW = 130;
```

Key/event code for leftarrow, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_LSHIFT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_LSHIFT = 134;
```

Key/event code for lshift, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_LWIN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_LWIN = -274;
```

Key/event code for lwin, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MINUS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MINUS = 45;
```

Key/event code for minus, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MOUSE1`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MOUSE1 = 512;
```

Key/event code for mouse1, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MOUSE10`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MOUSE10 = 523;
```

Key/event code for mouse10, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MOUSE2`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MOUSE2 = 513;
```

Key/event code for mouse2, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MOUSE3`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MOUSE3 = 514;
```

Key/event code for mouse3, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MOUSE4`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MOUSE4 = 517;
```

Key/event code for mouse4, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MOUSE5`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MOUSE5 = 518;
```

Key/event code for mouse5, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MOUSE6`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MOUSE6 = 519;
```

Key/event code for mouse6, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MOUSE7`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MOUSE7 = 520;
```

Key/event code for mouse7, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MOUSE8`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MOUSE8 = 521;
```

Key/event code for mouse8, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MOUSE9`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MOUSE9 = 522;
```

Key/event code for mouse9, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MWHEELDOWN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MWHEELDOWN = 516;
```

Key/event code for mwheeldown, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_MWHEELUP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_MWHEELUP = 515;
```

Key/event code for mwheelup, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_PAUSE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_PAUSE = 153;
```

Key/event code for pause, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_PGDN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_PGDN = 149;
```

Key/event code for pgdn, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_PGUP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_PGUP = 150;
```

Key/event code for pgup, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_PLUS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_PLUS = 43;
```

Key/event code for plus, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_POWER`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_POWER = -130;
```

Key/event code for power, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_PRINTSCREEN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_PRINTSCREEN = 174;
```

Key/event code for printscreen, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_QUOTES`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_QUOTES = 34;
```

Key/event code for quotes, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_RALT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_RALT = -280;
```

Key/event code for ralt, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_RCTRL`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_RCTRL = -281;
```

Key/event code for rctrl, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_RIGHTARROW`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_RIGHTARROW = 131;
```

Key/event code for rightarrow, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_RSHIFT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_RSHIFT = -282;
```

Key/event code for rshift, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_RWIN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_RWIN = -275;
```

Key/event code for rwin, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_SCROLLLOCK`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_SCROLLLOCK = 156;
```

Key/event code for scrolllock, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_SEARCH`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_SEARCH = -277;
```

Key/event code for search, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_SEMICOLON`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_SEMICOLON = 59;
```

Key/event code for semicolon, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_SPACE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_SPACE = 32;
```

Key/event code for space, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_TAB`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_TAB = 9;
```

Key/event code for tab, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_TILDE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_TILDE = 126;
```

Key/event code for tilde, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_TOUCH`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_TOUCH = 600;
```

Key/event code for touch, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_TOUCHLONG`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_TOUCHLONG = 603;
```

Key/event code for touchlong, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_TOUCHSLIDE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_TOUCHSLIDE = 601;
```

Key/event code for touchslide, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_TOUCHTAP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_TOUCHTAP = 602;
```

Key/event code for touchtap, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_UPARROW`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_UPARROW = 128;
```

Key/event code for uparrow, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_VOLDOWN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_VOLDOWN = -279;
```

Key/event code for voldown, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `K_VOLUP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float K_VOLUP = -278;
```

Key/event code for volup, used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.

#### `LFIELD_AMBIENTSCALE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_AMBIENTSCALE = 10;
```

Dynamic-light property selector: ambientscale; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_ANGLES`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_ANGLES = 5;
```

Dynamic-light property selector: angles; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_COLOUR`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_COLOUR = 1;
```

Dynamic-light property selector: colour; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_CORONA`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_CORONA = 7;
```

Dynamic-light property selector: corona; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_CORONASCALE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_CORONASCALE = 8;
```

Dynamic-light property selector: coronascale; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_CUBEMAPNAME`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_CUBEMAPNAME = 9;
```

Dynamic-light property selector: cubemapname; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_DIETIME`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_DIETIME = 14;
```

Dynamic-light property selector: dietime; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_DIFFUSESCALE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_DIFFUSESCALE = 11;
```

Dynamic-light property selector: diffusescale; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_FLAGS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_FLAGS = 3;
```

Dynamic-light property selector: flags; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_FOV`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_FOV = 6;
```

Dynamic-light property selector: fov; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_NEARCLIP`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_NEARCLIP = 18;
```

Dynamic-light property selector: nearclip; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_ORIGIN`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_ORIGIN = 0;
```

Dynamic-light property selector: origin; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_OWNER`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_OWNER = 19;
```

Dynamic-light property selector: owner; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_RADIUS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_RADIUS = 2;
```

Dynamic-light property selector: radius; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_RADIUSDECAY`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_RADIUSDECAY = 16;
```

Dynamic-light property selector: radiusdecay; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_RGBDECAY`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_RGBDECAY = 15;
```

Dynamic-light property selector: rgbdecay; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_ROTATION`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_ROTATION = 13;
```

Dynamic-light property selector: rotation; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_SPECULARSCALE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_SPECULARSCALE = 12;
```

Dynamic-light property selector: specularscale; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_STYLE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_STYLE = 4;
```

Dynamic-light property selector: style; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFIELD_STYLESTRING`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFIELD_STYLESTRING = 17;
```

Dynamic-light property selector: stylestring; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.

#### `LFLAG_CREPUSCULAR`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFLAG_CREPUSCULAR = 1024;
```

Light flag requesting supported crepuscular/shaft effects; renderer support is required.

#### `LFLAG_FLASHBLEND`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFLAG_FLASHBLEND = 8;
```

Light flag requesting the supported flashblend presentation path.

#### `LFLAG_LIGHTMAP`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFLAG_LIGHTMAP = 4;
```

Light flag for contributing through the supported lightmap lighting path.

#### `LFLAG_NORMALMODE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFLAG_NORMALMODE = 1;
```

Light flags selecting visibility in ordinary or real-time lighting modes.

#### `LFLAG_NOSHADOWS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFLAG_NOSHADOWS = 256;
```

Light flag suppressing shadow casting.

#### `LFLAG_ORTHOSUN`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFLAG_ORTHOSUN = 2048;
```

Light flag selecting a directional/orthographic sunlight-style projection.

#### `LFLAG_REALTIMEMODE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFLAG_REALTIMEMODE = 2;
```

Light flags selecting visibility in ordinary or real-time lighting modes.

#### `LFLAG_SHADOWMAP`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float LFLAG_SHADOWMAP = 512;
```

Light flag selecting the supported shadow-map path.

#### `M_PI`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float M_PI = 3.14159;
```

Mathematica Pi constant.

#### `MASK_ENGINE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float MASK_ENGINE = 1;
```

Valid as an argument for addentities. If specified, all non-csqc entities will be added to the scene.

#### `MASK_VIEWMODEL`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float MASK_VIEWMODEL = 2;
```

Valid as an argument for addentities. If specified, the regular engine viewmodel will be added to the scene.

#### `MF_GIB`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MF_GIB = 4;
```

Regular blood trail

#### `MF_GRENADE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MF_GRENADE = 2;
```

Model flag selecting the conventional rocket/grenade particle trail.

#### `MF_ROCKET`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MF_ROCKET = 1;
```

Model flag selecting the conventional rocket/grenade particle trail.

#### `MF_ROTATE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MF_ROTATE = 8;
```

Model flag enabling the conventional automatic item rotation presentation.

#### `MF_TRACER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MF_TRACER = 16;
```

AKA: green scrag trail

#### `MF_TRACER2`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MF_TRACER2 = 64;
```

AKA: hellknight projectile trail

#### `MF_TRACER3`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MF_TRACER3 = 128;
```

AKA: purple vore trail

#### `MF_ZOMGIB`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MF_ZOMGIB = 32;
```

Dark blood trail

#### `MOVE_ENTCHAIN`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVE_ENTCHAIN = 128;
```

Returns a list of entities impacted via the trace_ent.chain field

#### `MOVE_EVERYTHING`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVE_EVERYTHING = 32;
```

This type of trace will hit solids and triggers alike. Even non-solid entities.

#### `MOVE_HITMODEL`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVE_HITMODEL = 4;
```

Traces will impact the actual mesh of the model instead of merely their bounding box. Should generally only be used for tracelines. Note that this flag is unreliable as an object can animate through projectiles. The bounding box MUST be set to completely encompass the entity or those extra areas will be non-solid (leaving a hole for things to go through).

#### `MOVE_LAGGED`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MOVE_LAGGED = 64;
```

Will use antilag based upon the player's latency. Traces will be performed against old positions for entities instead of their current origin.

#### `MOVE_MISSILE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVE_MISSILE = 2;
```

The trace will use a bbox size of +/- 15 against entities with FL_MONSTER set.

#### `MOVE_NOMONSTERS`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVE_NOMONSTERS = 1;
```

The trace will ignore all non-solid_bsp entities.

#### `MOVE_NORMAL`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVE_NORMAL = 0;
```

Ordinary entity/world collision mode for trace operations.

#### `MOVE_OTHERONLY`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVE_OTHERONLY = 256;
```

Traces that use this trace type will collide against *only* the entity specified via the 'other' global, and will ignore all owner/solid_not/dimension etc rules, they will still adhere to contents and bsp/bbox rules though.

#### `MOVE_TRIGGERS`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVE_TRIGGERS = 16;
```

This trace type will impact only triggers. It will ignore non-solid entities.

#### `MOVE_WORLDONLY`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
FTEDEP("use MOVE_OTHERONLY") const float MOVE_WORLDONLY = 3;
```

The trace will ignore everything but the worldmodel. This is useful for to prevent the q3bsp pvs+culling issues that come with spectator modes leaving the world .

#### `MOVETYPE_6DOF`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_6DOF = 30;
```

A glorified MOVETYPE_FLY. Players using this movetype will get some flightsim-like physics, with fully independant rotations (order-dependant transforms).

#### `MOVETYPE_BOUNCE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_BOUNCE = 10;
```

Gravity-driven motion that bounces on collision.

#### `MOVETYPE_BOUNCEMISSILE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_BOUNCEMISSILE = 11;
```

Bouncing projectile variant, requiring the advertised extended movement support.

#### `MOVETYPE_FLY`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_FLY = 5;
```

Flying movement without ordinary gravity; valid player behavior requires the relevant extension/path.

#### `MOVETYPE_FLYMISSILE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_FLYMISSILE = 9;
```

Flying projectile movement with missile collision conventions.

#### `MOVETYPE_FOLLOW`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_FOLLOW = 12;
```

Follows an attachment target using the extension's attachment conventions.

#### `MOVETYPE_NOCLIP`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_NOCLIP = 8;
```

Movement without blocking collision; game code should deliberately control who may use it.

#### `MOVETYPE_NONE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_NONE = 0;
```

No automatic entity movement; game code may reposition it explicitly.

#### `MOVETYPE_PHYSICS`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_PHYSICS = 32;
```

Enable the use of ODE physics upon this entity.

#### `MOVETYPE_PUSH`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_PUSH = 7;
```

Brush pusher movement such as a door or platform, with obstruction handling and its ltime clock.

#### `MOVETYPE_STEP`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_STEP = 4;
```

Step-based ground monster movement; game code supplies AI/movement intents.

#### `MOVETYPE_TOSS`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_TOSS = 6;
```

Gravity-driven thrown entity; normally settles on collision.

#### `MOVETYPE_WALK`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_WALK = 3;
```

Walking/player movement using the standard movement path and collision handling.

#### `MOVETYPE_WALLWALK`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float MOVETYPE_WALLWALK = 31;
```

Players using this movetype will be able to orient themselves to walls, and then run up them.

#### `MSG_ALL`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Use MSG_MULTICAST+multicast(MULTICAST_ALL)") const float MSG_ALL = 2;
```

The byte(s) will be reliably sent to all players.

#### `MSG_ALL`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
const float MSG_ALL = 2;
```

The byte(s) will be reliably sent to all players.

#### `MSG_BROADCAST`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Use MSG_MULTICAST+multicast(MULTICAST_*)") const float MSG_BROADCAST;
```

The byte(s) will be unreliably sent to all players. MSG_ constants are valid arguments to the Write* builtin family.

#### `MSG_BROADCAST`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
const float MSG_BROADCAST = 0;
```

The byte(s) will be unreliably sent to all players. MSG_ constants are valid arguments to the Write* builtin family.

#### `MSG_ENTITY`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MSG_ENTITY = 5;
```

The byte(s) will be written into the entity buffer. This is a special value used only inside 'SendEntity' functions.

#### `MSG_INIT`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MSG_INIT = 3;
```

The byte(s) will be written into the signon buffer. Clients will see these messages when they connect later. This buffer is only flushed on map changes, so spamming it _WILL_ result in overflows.

#### `MSG_MULTICAST`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MSG_MULTICAST = 4;
```

The byte(s) will be written into the multicast buffer for more selective sending. Messages sent this way will never be split across packets, and using this for csqc-only messages will not break protocol translation.

#### `MSG_ONE`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Use MSG_MULTICAST+multicast(MULTICAST_ONE_R)") const float MSG_ONE = 1;
```

The byte(s) will be reliably sent to the player specified in the msg_entity global. WARNING: in quakeworld servers without network preparsing enabled, this can result in illegible server messages (due to individual reliable messages being split between multiple backbuffers/packets). NQ has larger reliable buffers which avoids this issue, but still kicks the client.

#### `MSG_ONE`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
const float MSG_ONE = 1;
```

The byte(s) will be reliably sent to the player specified in the msg_entity global. WARNING: in quakeworld servers without network preparsing enabled, this can result in illegible server messages (due to individual reliable messages being split between multiple backbuffers/packets). NQ has larger reliable buffers which avoids this issue, but still kicks the client.

#### `MULTICAST_ALL`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MULTICAST_ALL = 0;
```

The multicast message is unreliably sent to all players. MULTICAST_ constants are valid arguments for the multicast builtin, which ignores the specified origin when given this constant.

#### `MULTICAST_ALL_R`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MULTICAST_ALL_R = 3;
```

The multicast message is reliably sent to all players. The specified origin is ignored.

#### `MULTICAST_ONE`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MULTICAST_ONE = 6;
```

The multicast message is unreliably sent to the player (AND ALL TRACKING SPECTATORS) specified in the msg_entity global. The specified origin is ignored.

#### `MULTICAST_ONE_NOSPECS`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MULTICAST_ONE_NOSPECS = 9;
```

The multicast message is unreliably sent to the player specified in the msg_entity global. The specified origin is ignored.

#### `MULTICAST_ONE_R`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MULTICAST_ONE_R = 7;
```

The multicast message is reliably sent to the player (AND ALL TRACKING SPECTATORS) specified in the msg_entity global. The specified origin is ignored

#### `MULTICAST_ONE_R_NOSPECS`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MULTICAST_ONE_R_NOSPECS = 10;
```

The multicast message is reliably sent to the player specified in the msg_entity global. The specified origin is ignored

#### `MULTICAST_PHS`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MULTICAST_PHS = 1;
```

The multicast message is unreliably sent to only players that can potentially hear the specified origin. Its quite loose.

#### `MULTICAST_PHS_R`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MULTICAST_PHS_R = 4;
```

The multicast message is reliably sent to only players that can potentially hear the specified origin. Players might still not receive it if they are out of range.

#### `MULTICAST_PVS`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MULTICAST_PVS = 2;
```

The multicast message is unreliably sent to only players that can potentially see the specified origin.

#### `MULTICAST_PVS_R`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float MULTICAST_PVS_R = 5;
```

The multicast message is reliably sent to only players that can potentially see the specified origin. Players might still not receive it if they cannot see the event.

#### `PFLAGS_CORONA`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float PFLAGS_CORONA = 2;
```

Enables support of coronas on the associated rtlights.

#### `PFLAGS_FULLDYNAMIC`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float PFLAGS_FULLDYNAMIC = 128;
```

When set in self.pflags, enables fully-customised dynamic lights. Custom rtlight information is not otherwise used.

#### `PFLAGS_NOSHADOW`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float PFLAGS_NOSHADOW = 1;
```

Associated RT lights attached will not cast shadows, making them significantly faster to draw.

#### `physics_mode`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
__used var float physics_mode = 2;
```

0: original csqc - physics are not run 1: DP-compat. Thinks occur, but not true movetypes. 2: movetypes occur just as they do in ssqc.

#### `PRECACHE_PIC_DOWNLOAD`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float PRECACHE_PIC_DOWNLOAD = 256;
```

If no image could be loaded then attempt to download one from the server. This flag can cause the function to block until completion. (Slow!)

#### `PRECACHE_PIC_FROMWAD`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float PRECACHE_PIC_FROMWAD = 1;
```

Attempt to load it from the legacy gfx.wad file (usually its better to just use a gfx/ prefix instead).

#### `PRECACHE_PIC_NOCLAMP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float PRECACHE_PIC_NOCLAMP = 4;
```

Texture coords for the pic will not be clamped nor padded nor atlased.

#### `PRECACHE_PIC_TEST`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float PRECACHE_PIC_TEST = 512;
```

The precache will block until the image is fully loaded, returning a null string on failure. (Slow!)

#### `PREDRAW_AUTOADD`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float PREDRAW_AUTOADD = 0;
```

Valid as a return value from the predraw function. Returning this will cause the engine to automatically invoke addentity(self) for you.

#### `PREDRAW_NEXT`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float PREDRAW_NEXT = 1;
```

Valid as a return value from the predraw function. Returning this will simply move on to the next entity without the autoadd behaviour, so can be used for particle/invisible/special entites, or entities that were explicitly drawn with addentity.

#### `PRINT_CHAT`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
const float PRINT_CHAT = 3;
```

QuakeWorld text message priority levels, filtered/displayed according to client print settings.

#### `PRINT_HIGH`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
const float PRINT_HIGH = 2;
```

QuakeWorld text message priority levels, filtered/displayed according to client print settings.

#### `PRINT_LOW`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
const float PRINT_LOW = 0;
```

QuakeWorld text message priority levels, filtered/displayed according to client print settings.

#### `PRINT_MEDIUM`

**Scope:** QW SSQC. **Compatibility:** Engine interface.

```qc
const float PRINT_MEDIUM = 1;
```

QuakeWorld text message priority levels, filtered/displayed according to client print settings.

#### `PVSF_IGNOREPVS`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float PVSF_IGNOREPVS = 3;
```

Ignores pvs. This entity is visible whereever you are on the map. Updates will be sent regardless of pvs or phs

#### `PVSF_NOREMOVE`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float PVSF_NOREMOVE = 128;
```

Once visible to a client, this entity will remain visible. This can be useful for csqc and corpses. While this flag is set, no CSQC_Remove events will be sent for the entity, but this does NOT mean that it will still receive further updates while outside of the pvs.

#### `PVSF_NORMALPVS`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float PVSF_NORMALPVS = 0;
```

Filter first by PVS, then filter this entity using tracelines if sv_cullentities is enabled.

#### `PVSF_NOTRACECHECK`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float PVSF_NOTRACECHECK = 1;
```

Filter strictly by PVS.

#### `PVSF_USEPHS`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float PVSF_USEPHS = 2;
```

Send if we're close enough to be able to hear this entity.

#### `RESSTATE_FAILED`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RESSTATE_FAILED = 3;
```

Resources in this state are unusable/could not be loaded. You will get placeholders or dummy results. Queries will not stall the engine. The engine may display placeholder content.

#### `RESSTATE_LOADED`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RESSTATE_LOADED = 4;
```

Resources in this state are finally usable, everything will work okay. Hurrah. Queries will not stall the engine.

#### `RESSTATE_LOADING`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RESSTATE_LOADING = 2;
```

Resources in this this state are queued for loading, and will be loaded at the engine's convienience. If you attempt to query the resource now, the engine will stall until the result is available. sounds in this state may be delayed, while models/pics/shaders may be invisible.

#### `RESSTATE_NOTKNOWN`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RESSTATE_NOTKNOWN = 0;
```

RESSTATE_* constants are return values from the resourcestatus builtin. The engine doesn't know about the resource if it is in this state. This means you will need to precache it. Attempting to use it anyway may result in warnings, errors, or silently succeed, depending on engine version and resource type.

#### `RESSTATE_NOTLOADED`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RESSTATE_NOTLOADED = 1;
```

The resource was precached, but has been flushed and there has not been an attempt to reload it. If you use the resource normally, chances are it'll be loaded but at the cost of a stall.

#### `RESTYPE_MODEL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RESTYPE_MODEL = 0;
```

RESTYPE_* constants are used as arguments with the resourcestatus builtin.

#### `RESTYPE_PARTICLE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RESTYPE_PARTICLE = 2;
```

particleeffectnum

#### `RESTYPE_PIC`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RESTYPE_PIC = 3;
```

precache_pic. Status results are an amalgomation of the textures used by the named shader.

#### `RESTYPE_SKIN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RESTYPE_SKIN = 4;
```

setcustomskin

#### `RESTYPE_SOUND`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RESTYPE_SOUND = 1;
```

precache_sound

#### `RESTYPE_TEXTURE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RESTYPE_TEXTURE = 5;
```

Individual textures within shaders. These are not directly usable, but may be named as part of a skin file, or a shader.

#### `RF_ADDITIVE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RF_ADDITIVE = 8;
```

Shaders from this entity will temporarily be hacked to use an additive blend mode instead of their normal blend mode.

#### `RF_DEPTHHACK`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float RF_DEPTHHACK = 4;
```

Hacks the depth values such that the entity uses depth values as if it were closer to the screen. This is useful when combined with viewmodels to avoid weapons poking in to walls.

#### `RF_EXTERNALMODEL`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float RF_EXTERNALMODEL = 2;
```

Specifies that this entity should be displayed in mirrors (and may still cast shadows), but will not otherwise be visible.

#### `RF_FIRSTPERSON`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float RF_FIRSTPERSON = 1024;
```

This is basically the opposite of RF_EXTERNALMODEL. Don't draw in third-person or mirrors.

#### `RF_FRAMETIMESARESTARTTIMES`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float RF_FRAMETIMESARESTARTTIMES = 64;
```

Specifies that the frame1time, frame2time field are timestamps (denoting the start of the animation) rather than time into the animation.

#### `RF_NOSHADOW`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float RF_NOSHADOW = 32;
```

This entity will not cast shadows. Often useful on view models.

#### `RF_USEAXIS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float RF_USEAXIS = 16;
```

The entity will be oriented according to the current v_forward+v_right+v_up vector values instead of the entity's .angles field.

#### `RF_VIEWMODEL`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float RF_VIEWMODEL = 1;
```

Specifies that the entity is a view model, and that its origin is relative to the current view position. These entities are also subject to viewweapon bob.

#### `SERVERKEY_CONSTATE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string SERVERKEY_CONSTATE = "constate";
```

The current connection state. Will be set to one of: disconnected (menu-only mode), active (gamestate received and loaded), connecting(connecting, downloading, or precaching content, aka: loading screen).

#### `SERVERKEY_DLSTATE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string SERVERKEY_DLSTATE = "dlstate";
```

The progress of any current downloads. Empty string if no download is active, otherwise a tokenizable string containing this info: files-remaining, total-size, unknown-sizes-flag, file-localname, file-remotename, file-percent, file-rate, file-received-bytes, file-total-bytes If the current file info is omitted, then we are waiting for a download to start.

#### `SERVERKEY_IP`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string SERVERKEY_IP = "ip";
```

The address of the server we connected to.

#### `SERVERKEY_LOADSTATE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string SERVERKEY_LOADSTATE = "loadstate";
```

loadstage, loading image name, current step, max steps Stages are: 1=connecting, 2=serverside, 3=clientside Key will be empty if we are not loading.

#### `SERVERKEY_MAXPLAYERS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string SERVERKEY_MAXPLAYERS = "maxplayers";
```

The number of player/spectator slots allocated on the server.

#### `SERVERKEY_PAUSESTATE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string SERVERKEY_PAUSESTATE = "pausestate";
```

1 if the server claimed to be paused. 0 otherwise

#### `SERVERKEY_PROTOCOL`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string SERVERKEY_PROTOCOL = "protocol";
```

The protocol we are connected to the server with.

#### `SERVERKEY_SERVERNAME`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string SERVERKEY_SERVERNAME = "servername";
```

The hostname that was last passed to the connect command.

#### `SERVERKEY_TRANSFERRING`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const string SERVERKEY_TRANSFERRING = "transferring";
```

Set to the hostname of the server that we are attempting to connect or transfer to.

#### `skin`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
__deprecated("Obsoleted by .skin=CONTENTS_LADDER") const float SOLID_LADDER = 20;
```

Obsolete and may be removed at some point. Use skin=CONTENT_LADDER and solid_bsp or solid_trigger instead.

#### `SL_ORG_BL`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP_CSQC const float SL_ORG_BL = 22;
```

Legacy picture origin/alignment code: bl; T/B/M mean top/bottom/middle and L/R/M mean left/right/middle. Prefer CSQC layout for new HUDs.

#### `SL_ORG_BM`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP_CSQC const float SL_ORG_BM = 26;
```

Legacy picture origin/alignment code: bm; T/B/M mean top/bottom/middle and L/R/M mean left/right/middle. Prefer CSQC layout for new HUDs.

#### `SL_ORG_BR`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP_CSQC const float SL_ORG_BR = 23;
```

Legacy picture origin/alignment code: br; T/B/M mean top/bottom/middle and L/R/M mean left/right/middle. Prefer CSQC layout for new HUDs.

#### `SL_ORG_ML`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP_CSQC const float SL_ORG_ML = 27;
```

Legacy picture origin/alignment code: ml; T/B/M mean top/bottom/middle and L/R/M mean left/right/middle. Prefer CSQC layout for new HUDs.

#### `SL_ORG_MM`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP_CSQC const float SL_ORG_MM = 24;
```

Legacy picture origin/alignment code: mm; T/B/M mean top/bottom/middle and L/R/M mean left/right/middle. Prefer CSQC layout for new HUDs.

#### `SL_ORG_MR`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP_CSQC const float SL_ORG_MR = 28;
```

Legacy picture origin/alignment code: mr; T/B/M mean top/bottom/middle and L/R/M mean left/right/middle. Prefer CSQC layout for new HUDs.

#### `SL_ORG_TL`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP_CSQC const float SL_ORG_TL = 20;
```

Used with showpic etc, specifies that the x+y values are relative to the top-left of the screen

#### `SL_ORG_TM`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP_CSQC const float SL_ORG_TM = 25;
```

Legacy picture origin/alignment code: tm; T/B/M mean top/bottom/middle and L/R/M mean left/right/middle. Prefer CSQC layout for new HUDs.

#### `SL_ORG_TR`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
DEP_CSQC const float SL_ORG_TR = 21;
```

Legacy picture origin/alignment code: tr; T/B/M mean top/bottom/middle and L/R/M mean left/right/middle. Prefer CSQC layout for new HUDs.

#### `SLIST_HOSTCACHETOTALCOUNT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_HOSTCACHETOTALCOUNT = 1;
```

Server-browser property/comparison selector: hostcachetotalcount; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_HOSTCACHEVIEWCOUNT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_HOSTCACHEVIEWCOUNT = 0;
```

Server-browser property/comparison selector: hostcacheviewcount; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_MASTERQUERYCOUNT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_MASTERQUERYCOUNT = 2;
```

Server-browser property/comparison selector: masterquerycount; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_MASTERREPLYCOUNT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_MASTERREPLYCOUNT = 3;
```

Server-browser property/comparison selector: masterreplycount; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_SERVERQUERYCOUNT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_SERVERQUERYCOUNT = 4;
```

Server-browser property/comparison selector: serverquerycount; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_SERVERREPLYCOUNT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_SERVERREPLYCOUNT = 5;
```

Server-browser property/comparison selector: serverreplycount; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_SORTDESCENDING`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_SORTDESCENDING = 7;
```

Server-browser property/comparison selector: sortdescending; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_SORTFIELD`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_SORTFIELD = 6;
```

Server-browser property/comparison selector: sortfield; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_TEST_CONTAINS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_TEST_CONTAINS = 0;
```

Server-browser property/comparison selector: test contains; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_TEST_EQUAL`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_TEST_EQUAL = 4;
```

Server-browser property/comparison selector: test equal; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_TEST_GREATER`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_TEST_GREATER = 5;
```

Server-browser property/comparison selector: test greater; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_TEST_GREATEREQUAL`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_TEST_GREATEREQUAL = 6;
```

Server-browser property/comparison selector: test greaterequal; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_TEST_LESS`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_TEST_LESS = 3;
```

Server-browser property/comparison selector: test less; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_TEST_LESSEQUAL`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_TEST_LESSEQUAL = 2;
```

Server-browser property/comparison selector: test lessequal; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_TEST_NOTCONTAIN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_TEST_NOTCONTAIN = 1;
```

Server-browser property/comparison selector: test notcontain; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_TEST_NOTEQUAL`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_TEST_NOTEQUAL = 7;
```

Server-browser property/comparison selector: test notequal; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_TEST_NOTSTARTSWITH`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_TEST_NOTSTARTSWITH = 9;
```

Server-browser property/comparison selector: test notstartswith; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SLIST_TEST_STARTSWITH`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float SLIST_TEST_STARTSWITH = 8;
```

Server-browser property/comparison selector: test startswith; use with the matching gethostcache/sethostcachemask/sorting API.

#### `SOLID_BBOX`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_BBOX = 2;
```

Blocking axis-aligned entity collision box.

#### `SOLID_BSP`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_BSP = 4;
```

Does not collide against other SOLID_BSP entities. Normally paired with MOVETYPE_PUSH.

#### `SOLID_BSPTRIGGER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_BSPTRIGGER = 22;
```

For complex-shaped trigger volumes, instead of being a pure aabb.

#### `SOLID_CORPSE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_CORPSE = 5;
```

Non-solid to SOLID_SLIDEBOX or other SOLID_CORPSE entities. For hitscan weapons to hit corpses, change the player's .hitcontentsmaski value to include CONTENTBIT_CORPSE, perform the traceline, then revert the player's .solid value.

#### `SOLID_NOT`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_NOT = 0;
```

Non-solid entity; no blocking collision.

#### `SOLID_PHYSICS_BOX`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_PHYSICS_BOX = 32;
```

Legacy rigid-body solid selector for box; backend support is required; prefer documented geomtype setup.

#### `SOLID_PHYSICS_CAPSULE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_PHYSICS_CAPSULE = 34;
```

Legacy rigid-body solid selector for capsule; backend support is required; prefer documented geomtype setup.

#### `SOLID_PHYSICS_CYLINDER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_PHYSICS_CYLINDER = 36;
```

Legacy rigid-body solid selector for cylinder; backend support is required; prefer documented geomtype setup.

#### `SOLID_PHYSICS_SPHERE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_PHYSICS_SPHERE = 33;
```

Legacy rigid-body solid selector for sphere; backend support is required; prefer documented geomtype setup.

#### `SOLID_PHYSICS_TRIMESH`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_PHYSICS_TRIMESH = 35;
```

Legacy rigid-body solid selector for trimesh; backend support is required; prefer documented geomtype setup.

#### `SOLID_PORTAL`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_PORTAL = 21;
```

CSG subtraction volume combined with entity transformations on impact.

#### `SOLID_SLIDEBOX`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_SLIDEBOX = 3;
```

Player/monster sliding collision box, with the movement path's step/slide rules.

#### `SOLID_TRIGGER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOLID_TRIGGER = 1;
```

Overlap/touch-only trigger volume; does not block movement.

#### `SOUNDFLAG_ABSVOLUME`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float SOUNDFLAG_ABSVOLUME = 16;
```

The sample's volume is not scaled by the volume cvar. Use with caution

#### `SOUNDFLAG_FOLLOW`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOUNDFLAG_FOLLOW = 64;
```

The sound's origin will updated to follow the emitting entity.

#### `SOUNDFLAG_FORCELOOP`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOUNDFLAG_FORCELOOP = 2;
```

The sound will restart once it reaches the end of the sample.

#### `SOUNDFLAG_INACTIVE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float SOUNDFLAG_INACTIVE = 2048;
```

The sound will ignore the value of the snd_inactive cvar.

#### `SOUNDFLAG_NOREPLACE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOUNDFLAG_NOREPLACE = 128;
```

Sounds started with this flag will be ignored when there's already a sound playing on that same ent-channel. Such sounds can be safely 're-' started every single frame without harming anything. Tends not to make sense with CHAN_AUTO.

#### `SOUNDFLAG_NOREVERB`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOUNDFLAG_NOREVERB = 32;
```

Disables the use of underwater/reverb effects on this sound effect.

#### `SOUNDFLAG_NOSPACIALISE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SOUNDFLAG_NOSPACIALISE = 4;
```

The different audio channels are played at the same volume regardless of which way the player is facing, without needing to use 0 attenuation.

#### `SOUNDFLAG_RELIABLE`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float SOUNDFLAG_RELIABLE = 1;
```

The sound will be sent reliably, and without regard to phs.

#### `SOUNDFLAG_SENDVELOCITY`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float SOUNDFLAG_SENDVELOCITY = 512;
```

The entity's current velocity will be sent to the client, only useful if doppler is enabled.

#### `SOUNDFLAG_UNICAST`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float SOUNDFLAG_UNICAST = 256;
```

The sound will be sent only by the player specified by msg_entity. Spectators and related splitscreen players will also hear the sound.

#### `SPA_LIGHTMAP0_COLOR`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SPA_LIGHTMAP0_COLOR = 6;
```

Surface point's first lightmap coordinates or lighting color attribute.

#### `SPA_LIGHTMAP0_TEXCOORDS`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SPA_LIGHTMAP0_TEXCOORDS = 5;
```

Surface point's first lightmap coordinates or lighting color attribute.

#### `SPA_POSITION`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SPA_POSITION = 0;
```

These SPA_* constants are to specify which attribute is returned by the getsurfacepointattribute builtin

#### `SPA_R_AXIS`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SPA_R_AXIS = 3;
```

aka: SPA_NORMAL

#### `SPA_S_AXIS`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SPA_S_AXIS = 1;
```

Surface texture-coordinate basis vectors for getsurfacepointattribute.

#### `SPA_T_AXIS`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SPA_T_AXIS = 2;
```

Surface texture-coordinate basis vectors for getsurfacepointattribute.

#### `SPA_TEXCOORDS0`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float SPA_TEXCOORDS0 = 4;
```

Surface point's primary texture coordinates for getsurfacepointattribute.

#### `STAT_ACTIVEWEAPON`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_ACTIVEWEAPON = 10;
```

player.weapon

#### `STAT_AMMO`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_AMMO = 3;
```

player.currentammo

#### `STAT_ARMOR`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_ARMOR = 4;
```

Client/server statistic index for armor. Game code maintains custom/conventional game values; transmit/read with matching stat APIs.

#### `STAT_CELLS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_CELLS = 9;
```

Client/server statistic index for cells. Game code maintains custom/conventional game values; transmit/read with matching stat APIs.

#### `STAT_FOUNDSECRETS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_FOUNDSECRETS = 13;
```

Client/server statistic index for foundsecrets. Game code maintains custom/conventional game values; transmit/read with matching stat APIs.

#### `STAT_HEALTH`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_HEALTH = 0;
```

Player's health.

#### `STAT_ITEMS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_ITEMS = 15;
```

self.items | (self.items2<<23). In order to decode this stat properly, you need to use getstatbits(STAT_ITEMS,0,23) to read self.items, and getstatbits(STAT_ITEMS,23,11) to read self.items2 or getstatbits(STAT_ITEMS,28,4) to read the visible part of serverflags, whichever is applicable.

#### `STAT_KILLEDMONSTERS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_KILLEDMONSTERS = 14;
```

Client/server statistic index for killedmonsters. Game code maintains custom/conventional game values; transmit/read with matching stat APIs.

#### `STAT_NAILS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_NAILS = 7;
```

Client/server statistic index for nails. Game code maintains custom/conventional game values; transmit/read with matching stat APIs.

#### `STAT_ROCKETS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_ROCKETS = 8;
```

Client/server statistic index for rockets. Game code maintains custom/conventional game values; transmit/read with matching stat APIs.

#### `STAT_SHELLS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_SHELLS = 6;
```

Client/server statistic index for shells. Game code maintains custom/conventional game values; transmit/read with matching stat APIs.

#### `STAT_TOTALMONSTERS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_TOTALMONSTERS = 12;
```

Client/server statistic index for totalmonsters. Game code maintains custom/conventional game values; transmit/read with matching stat APIs.

#### `STAT_TOTALSECRETS`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_TOTALSECRETS = 11;
```

Client/server statistic index for totalsecrets. Game code maintains custom/conventional game values; transmit/read with matching stat APIs.

#### `STAT_USER`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_USER = 32;
```

Custom user stats start here (lower values are reserved for engine use).

#### `STAT_VIEW2`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_VIEW2 = 20;
```

This stat contains the number of the entity in the server's .view2 field.

#### `STAT_VIEWHEIGHT`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_VIEWHEIGHT = 16;
```

player.view_ofs_z

#### `STAT_VIEWZOOM`

**Scope:** CSQC. **Compatibility:** Shared extension.

```qc
const float STAT_VIEWZOOM = 21;
```

Scales fov and sensitiity. Part of DP_VIEWZOOM.

#### `STAT_WEAPONFRAME`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_WEAPONFRAME = 5;
```

Client/server statistic index for weaponframe. Game code maintains custom/conventional game values; transmit/read with matching stat APIs.

#### `STAT_WEAPONMODELI`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float STAT_WEAPONMODELI = 2;
```

This is the modelindex of the current viewmodel (renamed from the original name 'STAT_WEAPON' due to confusions).

#### `STUFFCMD_BROADCAST`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float STUFFCMD_BROADCAST = 4;
```

The stuffcmd will be broadcast server-wide (according to the mvd filters).

#### `STUFFCMD_DEMOONLY`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float STUFFCMD_DEMOONLY = 2;
```

This stuffcmd will ONLY be written into mvds/qtv streams.

#### `STUFFCMD_IGNOREINDEMO`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float STUFFCMD_IGNOREINDEMO = 1;
```

This stuffcmd will NOT be written to mvds/qtv.

#### `STUFFCMD_UNRELIABLE`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float STUFFCMD_UNRELIABLE = 8;
```

The stuffcmd might not arrive. It might also get there faster than ones sent over the reliable channel.

#### `SVC_CGAMEPACKET`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Engine interface.

```qc
const float SVC_CGAMEPACKET = 83;
```

Direct ssqc->csqc message. Must only be multicast. The data triggers a CSQC_Parse_Event call in the csqc for the csqc to read the contents. The server *may* insert length information for clients connected via proxies which are not able to cope with custom csqc payloads. This should only ever be used in conjunction with the MSG_MULTICAST destination.

#### `TE_BEAM`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
const float TE_BEAM = 13;
```

Temporary network effect identifier: beam. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_BLOOD`

**Scope:** QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_BLOOD = 12;
```

Temporary network effect identifier: blood. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_EXPLOSION`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_EXPLOSION = 3;
```

Temporary network effect identifier: explosion. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_EXPLOSION2`

**Scope:** NQ SSQC. **Compatibility:** Engine interface.

```qc
const float TE_EXPLOSION2 = 12;
```

Temporary network effect identifier: explosion2. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_GUNSHOT`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_GUNSHOT = 2;
```

Temporary network effect identifier: gunshot. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_KNIGHTSPIKE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_KNIGHTSPIKE = 8;
```

Temporary network effect identifier: knightspike. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_LAVASPLASH`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_LAVASPLASH = 10;
```

Temporary network effect identifier: lavasplash. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_LIGHTNING1`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_LIGHTNING1 = 5;
```

Temporary network effect identifier: lightning1. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_LIGHTNING2`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_LIGHTNING2 = 6;
```

Temporary network effect identifier: lightning2. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_LIGHTNING3`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_LIGHTNING3 = 9;
```

Temporary network effect identifier: lightning3. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_LIGHTNINGBLOOD`

**Scope:** QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_LIGHTNINGBLOOD = 13;
```

Temporary network effect identifier: lightningblood. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_SPIKE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_SPIKE = 0;
```

Temporary network effect identifier: spike. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_SUPERSPIKE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_SUPERSPIKE = 1;
```

Temporary network effect identifier: superspike. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_TAREXPLOSION`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_TAREXPLOSION = 4;
```

Temporary network effect identifier: tarexplosion. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_TELEPORT`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_TELEPORT = 11;
```

Temporary network effect identifier: teleport. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TE_WIZSPIKE`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Engine interface.

```qc
const float TE_WIZSPIKE = 7;
```

Temporary network effect identifier: wizspike. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.

#### `TEREDIT_ENT_ADD`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_ENT_ADD = 28;
```

Terrain editing operation: ent add; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_ENT_COUNT`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_ENT_COUNT = 29;
```

Terrain editing operation: ent count; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_ENT_GET`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_ENT_GET = 26;
```

Terrain editing operation: ent get; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_ENT_SET`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_ENT_SET = 27;
```

Terrain editing operation: ent set; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_HEIGHT_FLATTEN`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_HEIGHT_FLATTEN = 18;
```

Terrain editing operation: height flatten; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_HEIGHT_LOWER`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_HEIGHT_LOWER = 7;
```

Terrain editing operation: height lower; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_HEIGHT_RAISE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_HEIGHT_RAISE = 6;
```

Terrain editing operation: height raise; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_HEIGHT_SET`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_HEIGHT_SET = 3;
```

Terrain editing operation: height set; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_HEIGHT_SMOOTH`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_HEIGHT_SMOOTH = 4;
```

Terrain editing operation: height smooth; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_HEIGHT_SPREAD`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_HEIGHT_SPREAD = 5;
```

Terrain editing operation: height spread; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_MESH_ADD`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_MESH_ADD = 15;
```

Terrain editing operation: mesh add; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_MESH_KILL`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_MESH_KILL = 16;
```

Terrain editing operation: mesh kill; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_RELOAD`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_RELOAD = 0;
```

Terrain editing operation: reload; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_RELOAD_SECT`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_RELOAD_SECT = 21;
```

Terrain editing operation: reload sect; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_RESET_SECT`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_RESET_SECT = 20;
```

Terrain editing operation: reset sect; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_SAVE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_SAVE = 1;
```

Terrain editing operation: save; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_SETHOLE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_SETHOLE = 2;
```

Terrain editing operation: sethole; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_TEX_BLEND`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_TEX_BLEND = 10;
```

Terrain editing operation: tex blend; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_TEX_BLUR`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_TEX_BLUR = 13;
```

Terrain editing operation: tex blur; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_TEX_GET`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_TEX_GET = 9;
```

Terrain editing operation: tex get; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_TEX_KILL`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_TEX_KILL = 8;
```

Terrain editing operation: tex kill; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_TEX_NOISE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_TEX_NOISE = 12;
```

Terrain editing operation: tex noise; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_TEX_REPLACE`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_TEX_REPLACE = 19;
```

Terrain editing operation: tex replace; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_TEX_SETMASK`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_TEX_SETMASK = 25;
```

Terrain editing operation: tex setmask; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_TEX_UNIFY`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_TEX_UNIFY = 11;
```

Terrain editing operation: tex unify; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_TINT`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_TINT = 17;
```

Terrain editing operation: tint; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TEREDIT_WATER_SET`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float TEREDIT_WATER_SET = 14;
```

Terrain editing operation: water set; pass to terrain_edit with the matching operation arguments; terrain support is required.

#### `TRUE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float TRUE = 1;
```

Boolean true, represented as one; use FALSE/zero for false.

#### `VF_ACTIVESEAT`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float VF_ACTIVESEAT = 202;
```

The 'seat' number, used when running splitscreen.

#### `VF_AFOV`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_AFOV = 203;
```

Aproximate fov. Matches the 'fov' cvar. The engine handles the aspect ratio for you.

#### `VF_ANGLES`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_ANGLES = 15;
```

The angles the view will be drawn at. Not the angle the client reports to the server.

#### `VF_ANGLES_X`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_ANGLES_X = 16;
```

View property selector: angles x; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_ANGLES_Y`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_ANGLES_Y = 17;
```

View property selector: angles y; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_ANGLES_Z`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_ANGLES_Z = 18;
```

View property selector: angles z; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_CL_VIEWANGLES`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float VF_CL_VIEWANGLES = 33;
```

View property selector: cl viewangles; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_CL_VIEWANGLES_X`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float VF_CL_VIEWANGLES_X = 34;
```

View property selector: cl viewangles x; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_CL_VIEWANGLES_Y`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float VF_CL_VIEWANGLES_Y = 35;
```

View property selector: cl viewangles y; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_CL_VIEWANGLES_Z`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float VF_CL_VIEWANGLES_Z = 36;
```

View property selector: cl viewangles z; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_DRAWCROSSHAIR`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float VF_DRAWCROSSHAIR = 21;
```

boolean. If set to 1, the engine will draw its default crosshair.

#### `VF_DRAWENGINESBAR`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float VF_DRAWENGINESBAR = 20;
```

boolean. If set to 1, the sbar will be drawn, and viewsize will be honoured automatically.

#### `VF_DRAWWORLD`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float VF_DRAWWORLD = 19;
```

boolean. If set to 1, the engine will draw the world and static/persistant rtlights. If 0, the world will be skipped and everything will be fullbright.

#### `VF_ENVMAP`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_ENVMAP = 220;
```

The cubemap name to use as a fallback for $reflectcube, if a shader was unable to load one. Note that this doesn't automatically change shader permutations or anything.

#### `VF_FOV`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_FOV = 8;
```

sets both fovx and fovy. consider using afov instead.

#### `VF_FOV_X`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_FOV_X = 9;
```

horizontal field of view. does not consider aspect at all.

#### `VF_FOV_Y`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_FOV_Y = 10;
```

vertical field of view. does not consider aspect at all.

#### `VF_MAXDIST`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_MAXDIST = 24;
```

The distance of the far clip plane from the view position. If 0, will be considered infinite.

#### `VF_MIN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_MIN = 1;
```

The top-left of the 3d viewport in screenspace. The VF_ values are used via the setviewprop/getviewprop builtins.

#### `VF_MIN_X`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_MIN_X = 2;
```

View property selector: min x; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_MIN_Y`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_MIN_Y = 3;
```

View property selector: min y; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_MINDIST`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_MINDIST = 23;
```

The distance of the near clip plane from the view position. Should generally not be <=0, as this would introduce NANs.

#### `VF_ORIGIN`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_ORIGIN = 11;
```

The origin of the view. Not of the player.

#### `VF_ORIGIN_X`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_ORIGIN_X = 12;
```

View property selector: origin x; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_ORIGIN_Y`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_ORIGIN_Y = 13;
```

View property selector: origin y; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_ORIGIN_Z`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_ORIGIN_Z = 14;
```

View property selector: origin z; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_PERSPECTIVE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_PERSPECTIVE = 200;
```

1: regular rendering. Fov specifies the angle. 0: isometric-style. Fov specifies the number of Quake Units each side of the viewport, and mindist restrictions are removed, pvs culling should be disabled.

#### `VF_PROJECTIONOFFSET`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_PROJECTIONOFFSET = 224;
```

vec2 horizontal+vertical offset for the projection matrix, for weird off-centre rendering.

#### `VF_RT_DEPTH`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_RT_DEPTH = 210;
```

The texture name to use as a depth buffer. Also used for shaders that specify $sourcedepth. 1-based. Additional arguments are: format (IMGFMT_D*), sizexy.

#### `VF_RT_DESTCOLOUR`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_RT_DESTCOLOUR = 212;
```

The texture name to write colour info into, this includes both 3d and 2d drawing. Additional arguments are: format (IMGFMT_*), sizexy. Written to by both 3d and 2d rendering. Note that any rendertarget textures may be destroyed on video mode changes or so. Shaders can name render targets by prefixing texture names with '$rt:', or $sourcecolour.

#### `VF_RT_DESTCOLOUR1`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_RT_DESTCOLOUR1 = 213;
```

Like VF_RT_DESTCOLOUR, for multiple render targets.

#### `VF_RT_DESTCOLOUR2`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_RT_DESTCOLOUR2 = 214;
```

Like VF_RT_DESTCOLOUR, for multiple render targets.

#### `VF_RT_DESTCOLOUR3`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_RT_DESTCOLOUR3 = 215;
```

Like VF_RT_DESTCOLOUR, for multiple render targets.

#### `VF_RT_RIPPLE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_RT_RIPPLE = 211;
```

The texture name to use as a ripplemap (target for shaders with 'sort ripple'). Also used for shaders that specify $ripplemap. 1-based. Additional arguments are: format, sizexy.

#### `VF_RT_SOURCECOLOUR`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_RT_SOURCECOLOUR = 209;
```

The texture name to use with shaders that specify a $sourcecolour map.

#### `VF_SCREENPSIZE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_SCREENPSIZE = 205;
```

Provides a reliable way to retrieve the current physical screen size (cvars need vid_restart for them to take effect).

#### `VF_SCREENVSIZE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_SCREENVSIZE = 204;
```

Provides a reliable way to retrieve the current virtual screen size (even if the screen is automatically scaled to retain aspect).

#### `VF_SIZE`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_SIZE = 4;
```

The width+height of the 3d viewport in screenspace.

#### `VF_SIZE_X`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_SIZE_X = 5;
```

View property selector: size x; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_SIZE_Y`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_SIZE_Y = 6;
```

View property selector: size y; use with getproperty/setproperty and the documented scalar/vector component type.

#### `VF_SKYROOM_CAMERA`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float VF_SKYROOM_CAMERA = 222;
```

Controls the camera position of the skyroom (which will be drawn underneath transparent sky surfaces). This should move slightly with the real camera, but not so much that the skycamera enters walls. Requires a skyshader with a blend mode on the first pass (or no passes).

#### `VF_USERDATA`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_USERDATA = 221;
```

Pointer (and byte size) to an array of vec4s. This data is then globally visible to all glsl via the w_user uniform.

#### `VF_VIEWENTITY`

**Scope:** CSQC. **Compatibility:** Engine interface.

```qc
const float VF_VIEWENTITY = 206;
```

Changes the RF_EXTERNALMODEL flag on entities to match the new selection, and removes entities flaged with RF_VIEWENTITY. Requires cunning use of .entnum and typically requires calling addentities(MASK_VIEWMODEL) too.

#### `VF_VIEWPORT`

**Scope:** CSQC, MenuQC. **Compatibility:** Engine interface.

```qc
const float VF_VIEWPORT = 7;
```

vector+vector. Two argument shortcut for VF_MIN and VF_SIZE

### Types and handle aliases

19 declarations.

#### `brushface_t`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Source convenience.

```qc
typedef struct
{
	string	shadername;
	vector	planenormal;
	float	planedist;
	vector	sdir;
	float	sbias;
	vector	tdir;
	float	tbias;
} brushface_t;
```

Structured argument type used by the corresponding builtins; member comments describe its layout.

#### `field_t`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
typedef .__variant field_t;
```

Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.

#### `filestream`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
#define filestream float
```

Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.

#### `hashtable`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
#define hashtable float
```

Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.

#### `infostring`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
#define infostring string
```

Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.

#### `json_t`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
typedef struct json_s *json_t;
```

Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.

#### `json_type_e`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
enum json_type_e : int { JSON_TYPE_STRING, JSON_TYPE_NUMBER, JSON_TYPE_OBJECT, JSON_TYPE_ARRAY, JSON_TYPE_TRUE, JSON_TYPE_FALSE, JSON_TYPE_NULL };
```

Integer enumeration identifying JSON node types; used by json_get_value_type.

#### `jsonnode`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
accessor jsonnode : json_t
{
	inline get json_type_e type = json_get_value_type;
	inline get string s = json_get_string;
	inline get float f = json_get_float;
	inline get __int i = json_get_integer;
	inline get __int length = json_get_length;
	inline get jsonnode a[__int key] = json_get_child_at_index;
	inline get jsonnode[string key] = json_find_object_child;
	inline get string name = json_get_name;
};
```

Compiler accessor wrapper for the corresponding handle; calls the get/set builtins shown in its declaration.

#### `nodeslist_t`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Source convenience.

```qc
typedef struct
{
	vector dest;
	int linkflags;
	float radius;
} nodeslist_t;
```

Structured argument type used by the corresponding builtins; member comments describe its layout.

#### `patchinfo_t`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Source convenience.

```qc
typedef struct
{
	string shadername;
	int contents;
	int cpwidth;
	int cpheight;
	int tesswidth;
	int tessheight;
	vector texinfo;/*scalex,y,rot*/
} patchinfo_t;
```

Structured argument type used by the corresponding builtins; member comments describe its layout.

#### `patchvert_t`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Source convenience.

```qc
typedef struct
{
	vector xyz;
	vector rgb; float a;
	float s, t;
} patchvert_t;
```

Structured argument type used by the corresponding builtins; member comments describe its layout.

#### `reverbinfo_t`

**Scope:** CSQC. **Compatibility:** Source convenience.

```qc
typedef struct {
	float flDensity;
	float flDiffusion;
	float flGain;
	float flGainHF;
	float flGainLF;
	float flDecayTime;
	float flDecayHFRatio;
	float flDecayLFRatio;
	float flReflectionsGain;
	float flReflectionsDelay;
	vector flReflectionsPan;
	float flLateReverbGain;
	float flLateReverbDelay;
	vector flLateReverbPan;
	float flEchoTime;
	float flEchoDepth;
	float flModulationTime;
	float flModulationDepth;
	float flAirAbsorptionGainHF;
	float flHFReference;
	float flLFReference;
	float flRoomRolloffFactor;
	int   iDecayHFLimit;
} reverbinfo_t;
```

Structured argument type used by the corresponding builtins; member comments describe its layout.

#### `searchhandle`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
#define searchhandle float
```

Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.

#### `skelblend_t`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
typedef struct
{
	int sourcemodelindex; /*frame data will be imported from this model, bones must be compatible*/
	int reserved;
	int firstbone;
	int lastbone;
	float prescale;	/*0 destroys existing data, 1 retains it*/
	float scale[4];	/*you'll need to do lerpfrac manually*/
	int animation[4];
	float animationtime[4];
	/*halflife models*/
	float subblend[2];
	float controllers[5];
} skelblend_t;
```

Structured argument type used by the corresponding builtins; member comments describe its layout.

#### `strbuf`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
#define strbuf float
```

Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.

#### `vec2`

**Scope:** CSQC. **Compatibility:** Source convenience.

```qc
typedef float vec2[2];
```

Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.

#### `vec3`

**Scope:** CSQC. **Compatibility:** Source convenience.

```qc
typedef float vec3[3];
```

Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.

#### `vec4`

**Scope:** CSQC. **Compatibility:** Source convenience.

```qc
typedef float vec4[4];
```

Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.

#### `xyz`

**Scope:** CSQC. **Compatibility:** Source convenience.

```qc
typedef struct trisoup_simple_vert_s {vec3 xyz;
```

Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.

### Convenience macros

14 declarations.

#### `ChangeYaw`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Source convenience.

```qc
#define ChangeYaw changeyaw
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `dotproduct`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
#define dotproduct (v1,v2) ((vector)(v1)*(vector)(v2))
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `draw_getimagesize`

**Scope:** CSQC. **Compatibility:** Source convenience.

```qc
#define draw_getimagesize drawgetimagesize
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `findentity`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Source convenience.

```qc
#define findentity findfloat
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `ftell`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
#define ftell fseek
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `getstatbits`

**Scope:** CSQC. **Compatibility:** Source convenience.

```qc
#define getstatbits getstatf
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `getstati_punf`

**Scope:** CSQC. **Compatibility:** Source convenience.

```qc
#define getstati_punf (stnum) (float)(__variant)getstati(stnum)
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `getviewprop`

**Scope:** CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
#define getviewprop getproperty
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `patch_delete`

**Scope:** NQ SSQC, QW SSQC, CSQC. **Compatibility:** Source convenience.

```qc
#define patch_delete (modelidx,patchidx) brush_delete(modelidx,patchidx)
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `setviewprop`

**Scope:** CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
#define setviewprop setproperty
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `strcmp`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
#define strcmp strncmp
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `unicast`

**Scope:** NQ SSQC, QW SSQC. **Compatibility:** Source convenience.

```qc
#define unicast (pl,reli) do{msg_entity = pl; multicast('0 0 0', reli?MULTICAST_ONE_R:MULTICAST_ONE);}while(0)
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `uri_post`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Source convenience.

```qc
#define uri_post uri_get
```

Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.

#### `VF_LPLAYER`

**Scope:** CSQC. **Compatibility:** Source convenience.

```qc
#define VF_LPLAYER VF_ACTIVESEAT
```

View property selector: lplayer; use with getproperty/setproperty and the documented scalar/vector component type.

### Extension feature labels

222 declarations.

#### `BX_COLOREDTEXT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define BX_COLOREDTEXT
```

Compile-time label for the BX COLOREDTEXT feature/interface. It does not enable that feature at runtime; query checkextension("BX_COLOREDTEXT") and read the related declarations before using it.

#### `DP_CON_SET`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_CON_SET
```

The 'set' console command exists, and can be used to create/set cvars.

#### `DP_CON_SETA`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_CON_SETA
```

The 'seta' console command exists, like the 'set' command, but also marks the cvar for archiving, allowing it to be written into the user's config. Use this command in your default.cfg file.

#### `DP_CSQC_ROTATEMOVES`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_CSQC_ROTATEMOVES
```

Compile-time label for the DP CSQC ROTATEMOVES feature/interface. It does not enable that feature at runtime; query checkextension("DP_CSQC_ROTATEMOVES") and read the related declarations before using it.

#### `DP_EF_ADDITIVE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_EF_ADDITIVE
```

Compile-time label for the DP EF ADDITIVE feature/interface. It does not enable that feature at runtime; query checkextension("DP_EF_ADDITIVE") and read the related declarations before using it.

#### `DP_EF_BLUE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_EF_BLUE
```

Compile-time label for the DP EF BLUE feature/interface. It does not enable that feature at runtime; query checkextension("DP_EF_BLUE") and read the related declarations before using it.

#### `DP_EF_FULLBRIGHT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_EF_FULLBRIGHT
```

Compile-time label for the DP EF FULLBRIGHT feature/interface. It does not enable that feature at runtime; query checkextension("DP_EF_FULLBRIGHT") and read the related declarations before using it.

#### `DP_EF_NODEPTHTEST`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_EF_NODEPTHTEST
```

Compile-time label for the DP EF NODEPTHTEST feature/interface. It does not enable that feature at runtime; query checkextension("DP_EF_NODEPTHTEST") and read the related declarations before using it.

#### `DP_EF_NODRAW`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_EF_NODRAW
```

Compile-time label for the DP EF NODRAW feature/interface. It does not enable that feature at runtime; query checkextension("DP_EF_NODRAW") and read the related declarations before using it.

#### `DP_EF_NOGUNBOB`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_EF_NOGUNBOB
```

Compile-time label for the DP EF NOGUNBOB feature/interface. It does not enable that feature at runtime; query checkextension("DP_EF_NOGUNBOB") and read the related declarations before using it.

#### `DP_EF_NOSHADOW`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_EF_NOSHADOW
```

Compile-time label for the DP EF NOSHADOW feature/interface. It does not enable that feature at runtime; query checkextension("DP_EF_NOSHADOW") and read the related declarations before using it.

#### `DP_EF_RED`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_EF_RED
```

Compile-time label for the DP EF RED feature/interface. It does not enable that feature at runtime; query checkextension("DP_EF_RED") and read the related declarations before using it.

#### `DP_ENT_ALPHA`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_ENT_ALPHA
```

Compile-time label for the DP ENT ALPHA feature/interface. It does not enable that feature at runtime; query checkextension("DP_ENT_ALPHA") and read the related declarations before using it.

#### `DP_ENT_COLORMOD`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_ENT_COLORMOD
```

Compile-time label for the DP ENT COLORMOD feature/interface. It does not enable that feature at runtime; query checkextension("DP_ENT_COLORMOD") and read the related declarations before using it.

#### `DP_ENT_CUSTOMCOLORMAP`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_ENT_CUSTOMCOLORMAP
```

Compile-time label for the DP ENT CUSTOMCOLORMAP feature/interface. It does not enable that feature at runtime; query checkextension("DP_ENT_CUSTOMCOLORMAP") and read the related declarations before using it.

#### `DP_ENT_EXTERIORMODELTOCLIENT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_ENT_EXTERIORMODELTOCLIENT
```

Compile-time label for the DP ENT EXTERIORMODELTOCLIENT feature/interface. It does not enable that feature at runtime; query checkextension("DP_ENT_EXTERIORMODELTOCLIENT") and read the related declarations before using it.

#### `DP_ENT_SCALE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_ENT_SCALE
```

Compile-time label for the DP ENT SCALE feature/interface. It does not enable that feature at runtime; query checkextension("DP_ENT_SCALE") and read the related declarations before using it.

#### `DP_ENT_TRAILEFFECTNUM`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_ENT_TRAILEFFECTNUM
```

self.traileffectnum=particleeffectnum("myeffectname"); can be used to attach a particle trail to the given server entity. This is equivelent to calling trailparticles each frame.

#### `DP_ENT_VIEWMODEL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_ENT_VIEWMODEL
```

Compile-time label for the DP ENT VIEWMODEL feature/interface. It does not enable that feature at runtime; query checkextension("DP_ENT_VIEWMODEL") and read the related declarations before using it.

#### `DP_GECKO_SUPPORT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_GECKO_SUPPORT
```

Compile-time label for the DP GECKO SUPPORT feature/interface. It does not enable that feature at runtime; query checkextension("DP_GECKO_SUPPORT") and read the related declarations before using it.

#### `DP_GFX_FONTS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_GFX_FONTS
```

Compile-time label for the DP GFX FONTS feature/interface. It does not enable that feature at runtime; query checkextension("DP_GFX_FONTS") and read the related declarations before using it.

#### `DP_GFX_QUAKE3MODELTAGS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_GFX_QUAKE3MODELTAGS
```

Compile-time label for the DP GFX QUAKE3MODELTAGS feature/interface. It does not enable that feature at runtime; query checkextension("DP_GFX_QUAKE3MODELTAGS") and read the related declarations before using it.

#### `DP_GFX_SKINFILES`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_GFX_SKINFILES
```

Compile-time label for the DP GFX SKINFILES feature/interface. It does not enable that feature at runtime; query checkextension("DP_GFX_SKINFILES") and read the related declarations before using it.

#### `DP_GFX_SKYBOX`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_GFX_SKYBOX
```

Compile-time label for the DP GFX SKYBOX feature/interface. It does not enable that feature at runtime; query checkextension("DP_GFX_SKYBOX") and read the related declarations before using it.

#### `DP_HALFLIFE_MAP`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_HALFLIFE_MAP
```

Compile-time label for the DP HALFLIFE MAP feature/interface. It does not enable that feature at runtime; query checkextension("DP_HALFLIFE_MAP") and read the related declarations before using it.

#### `DP_HALFLIFE_MAP_CVAR`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_HALFLIFE_MAP_CVAR
```

Compile-time label for the DP HALFLIFE MAP CVAR feature/interface. It does not enable that feature at runtime; query checkextension("DP_HALFLIFE_MAP_CVAR") and read the related declarations before using it.

#### `DP_INPUTBUTTONS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_INPUTBUTTONS
```

Compile-time label for the DP INPUTBUTTONS feature/interface. It does not enable that feature at runtime; query checkextension("DP_INPUTBUTTONS") and read the related declarations before using it.

#### `DP_LIGHTSTYLE_STATICVALUE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_LIGHTSTYLE_STATICVALUE
```

Compile-time label for the DP LIGHTSTYLE STATICVALUE feature/interface. It does not enable that feature at runtime; query checkextension("DP_LIGHTSTYLE_STATICVALUE") and read the related declarations before using it.

#### `DP_LITSUPPORT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_LITSUPPORT
```

Compile-time label for the DP LITSUPPORT feature/interface. It does not enable that feature at runtime; query checkextension("DP_LITSUPPORT") and read the related declarations before using it.

#### `DP_MONSTERWALK`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_MONSTERWALK
```

MOVETYPE_WALK is valid on non-player entities. Note that only players receive acceleration etc in line with none/bounce/fly/noclip movetypes on the player, thus you will have to provide your own accelerations (incluing gravity) yourself.

#### `DP_MOVETYPEBOUNCEMISSILE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_MOVETYPEBOUNCEMISSILE
```

Compile-time label for the DP MOVETYPEBOUNCEMISSILE feature/interface. It does not enable that feature at runtime; query checkextension("DP_MOVETYPEBOUNCEMISSILE") and read the related declarations before using it.

#### `DP_MOVETYPEFOLLOW`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_MOVETYPEFOLLOW
```

Compile-time label for the DP MOVETYPEFOLLOW feature/interface. It does not enable that feature at runtime; query checkextension("DP_MOVETYPEFOLLOW") and read the related declarations before using it.

#### `DP_QC_ASINACOSATANATAN2TAN`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_ASINACOSATANATAN2TAN
```

Compile-time label for the DP QC ASINACOSATANATAN2TAN feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_ASINACOSATANATAN2TAN") and read the related declarations before using it.

#### `DP_QC_CHANGEPITCH`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_CHANGEPITCH
```

Compile-time label for the DP QC CHANGEPITCH feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_CHANGEPITCH") and read the related declarations before using it.

#### `DP_QC_COPYENTITY`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_COPYENTITY
```

Compile-time label for the DP QC COPYENTITY feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_COPYENTITY") and read the related declarations before using it.

#### `DP_QC_CRC16`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_CRC16
```

Compile-time label for the DP QC CRC16 feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_CRC16") and read the related declarations before using it.

#### `DP_QC_CVAR_DEFSTRING`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_CVAR_DEFSTRING
```

Compile-time label for the DP QC CVAR DEFSTRING feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_CVAR_DEFSTRING") and read the related declarations before using it.

#### `DP_QC_CVAR_STRING`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_CVAR_STRING
```

Compile-time label for the DP QC CVAR STRING feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_CVAR_STRING") and read the related declarations before using it.

#### `DP_QC_CVAR_TYPE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_CVAR_TYPE
```

Compile-time label for the DP QC CVAR TYPE feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_CVAR_TYPE") and read the related declarations before using it.

#### `DP_QC_DIGEST_SHA256`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_DIGEST_SHA256
```

Compile-time label for the DP QC DIGEST SHA256 feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_DIGEST_SHA256") and read the related declarations before using it.

#### `DP_QC_EDICT_NUM`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_EDICT_NUM
```

Compile-time label for the DP QC EDICT NUM feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_EDICT_NUM") and read the related declarations before using it.

#### `DP_QC_ENTITYDATA`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_ENTITYDATA
```

Compile-time label for the DP QC ENTITYDATA feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_ENTITYDATA") and read the related declarations before using it.

#### `DP_QC_ETOS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_ETOS
```

Compile-time label for the DP QC ETOS feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_ETOS") and read the related declarations before using it.

#### `DP_QC_FINDCHAIN`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_FINDCHAIN
```

Compile-time label for the DP QC FINDCHAIN feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_FINDCHAIN") and read the related declarations before using it.

#### `DP_QC_FINDCHAINFLAGS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_FINDCHAINFLAGS
```

Compile-time label for the DP QC FINDCHAINFLAGS feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_FINDCHAINFLAGS") and read the related declarations before using it.

#### `DP_QC_FINDCHAINFLOAT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_FINDCHAINFLOAT
```

Compile-time label for the DP QC FINDCHAINFLOAT feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_FINDCHAINFLOAT") and read the related declarations before using it.

#### `DP_QC_FINDFLAGS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_FINDFLAGS
```

Compile-time label for the DP QC FINDFLAGS feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_FINDFLAGS") and read the related declarations before using it.

#### `DP_QC_FINDFLOAT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_FINDFLOAT
```

Compile-time label for the DP QC FINDFLOAT feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_FINDFLOAT") and read the related declarations before using it.

#### `DP_QC_FS_SEARCH`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_FS_SEARCH
```

Compile-time label for the DP QC FS SEARCH feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_FS_SEARCH") and read the related declarations before using it.

#### `DP_QC_FS_SEARCH_PACKFILE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_FS_SEARCH_PACKFILE
```

Compile-time label for the DP QC FS SEARCH PACKFILE feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_FS_SEARCH_PACKFILE") and read the related declarations before using it.

#### `DP_QC_GETLIGHT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_GETLIGHT
```

Compile-time label for the DP QC GETLIGHT feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_GETLIGHT") and read the related declarations before using it.

#### `DP_QC_GETSURFACE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_GETSURFACE
```

Compile-time label for the DP QC GETSURFACE feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_GETSURFACE") and read the related declarations before using it.

#### `DP_QC_GETSURFACEPOINTATTRIBUTE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_GETSURFACEPOINTATTRIBUTE
```

Compile-time label for the DP QC GETSURFACEPOINTATTRIBUTE feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_GETSURFACEPOINTATTRIBUTE") and read the related declarations before using it.

#### `DP_QC_GETTAGINFO`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_GETTAGINFO
```

Compile-time label for the DP QC GETTAGINFO feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_GETTAGINFO") and read the related declarations before using it.

#### `DP_QC_I18N`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_I18N
```

Specifies that the engine uses $MODULE.dat.$LANG.po files that translates the dotranslate_* globals on load - these are usually created via the _("foo") qcc intrinsic.

#### `DP_QC_MINMAXBOUND`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_MINMAXBOUND
```

Compile-time label for the DP QC MINMAXBOUND feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_MINMAXBOUND") and read the related declarations before using it.

#### `DP_QC_MULTIPLETEMPSTRINGS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_MULTIPLETEMPSTRINGS
```

Superseded by DP_QC_UNLIMITEDTEMPSTRINGS. Functions that return a temporary string will not overwrite/destroy previous temporary strings until at least 16 strings are returned (or control returns to the engine).

#### `DP_QC_RANDOMVEC`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_RANDOMVEC
```

Compile-time label for the DP QC RANDOMVEC feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_RANDOMVEC") and read the related declarations before using it.

#### `DP_QC_RENDER_SCENE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_RENDER_SCENE
```

clearscene+addentity+setviewprop+renderscene+setmodel are available to menuqc. WARNING: DP advertises this extension without actually supporting it, FTE does actually support it.

#### `DP_QC_SINCOSSQRTPOW`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_SINCOSSQRTPOW
```

Compile-time label for the DP QC SINCOSSQRTPOW feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_SINCOSSQRTPOW") and read the related declarations before using it.

#### `DP_QC_SPRINTF`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_SPRINTF
```

Provides the sprintf builtin, which allows for rich formatting along the lines of C's function with the same name. Not to be confused with QC's sprint builtin.

#### `DP_QC_STRFTIME`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_STRFTIME
```

Compile-time label for the DP QC STRFTIME feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_STRFTIME") and read the related declarations before using it.

#### `DP_QC_STRING_CASE_FUNCTIONS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_STRING_CASE_FUNCTIONS
```

Compile-time label for the DP QC STRING CASE FUNCTIONS feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_STRING_CASE_FUNCTIONS") and read the related declarations before using it.

#### `DP_QC_STRINGBUFFERS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_STRINGBUFFERS
```

Compile-time label for the DP QC STRINGBUFFERS feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_STRINGBUFFERS") and read the related declarations before using it.

#### `DP_QC_STRINGCOLORFUNCTIONS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_STRINGCOLORFUNCTIONS
```

Compile-time label for the DP QC STRINGCOLORFUNCTIONS feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_STRINGCOLORFUNCTIONS") and read the related declarations before using it.

#### `DP_QC_STRREPLACE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_STRREPLACE
```

Compile-time label for the DP QC STRREPLACE feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_STRREPLACE") and read the related declarations before using it.

#### `DP_QC_TOKENIZEBYSEPARATOR`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_TOKENIZEBYSEPARATOR
```

Compile-time label for the DP QC TOKENIZEBYSEPARATOR feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_TOKENIZEBYSEPARATOR") and read the related declarations before using it.

#### `DP_QC_TRACE_MOVETYPE_HITMODEL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_TRACE_MOVETYPE_HITMODEL
```

Compile-time label for the DP QC TRACE MOVETYPE HITMODEL feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_TRACE_MOVETYPE_HITMODEL") and read the related declarations before using it.

#### `DP_QC_TRACE_MOVETYPE_WORLDONLY`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_TRACE_MOVETYPE_WORLDONLY
```

Compile-time label for the DP QC TRACE MOVETYPE WORLDONLY feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_TRACE_MOVETYPE_WORLDONLY") and read the related declarations before using it.

#### `DP_QC_TRACE_MOVETYPES`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_TRACE_MOVETYPES
```

Compile-time label for the DP QC TRACE MOVETYPES feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_TRACE_MOVETYPES") and read the related declarations before using it.

#### `DP_QC_TRACEBOX`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_TRACEBOX
```

Compile-time label for the DP QC TRACEBOX feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_TRACEBOX") and read the related declarations before using it.

#### `DP_QC_TRACETOSS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_TRACETOSS
```

Compile-time label for the DP QC TRACETOSS feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_TRACETOSS") and read the related declarations before using it.

#### `DP_QC_UNLIMITEDTEMPSTRINGS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_UNLIMITEDTEMPSTRINGS
```

Supersedes DP_QC_MULTIPLETEMPSTRINGS, superseded by FTE_QC_PERSISTENTTEMPSTRINGS. Specifies that all temp strings will be valid at least until the QCVM returns.

#### `DP_QC_URI_ESCAPE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_URI_ESCAPE
```

Compile-time label for the DP QC URI ESCAPE feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_URI_ESCAPE") and read the related declarations before using it.

#### `DP_QC_URI_GET`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_URI_GET
```

Compile-time label for the DP QC URI GET feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_URI_GET") and read the related declarations before using it.

#### `DP_QC_URI_POST`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_URI_POST
```

Compile-time label for the DP QC URI POST feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_URI_POST") and read the related declarations before using it.

#### `DP_QC_VECTOANGLES_WITH_ROLL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_VECTOANGLES_WITH_ROLL
```

Compile-time label for the DP QC VECTOANGLES WITH ROLL feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_VECTOANGLES_WITH_ROLL") and read the related declarations before using it.

#### `DP_QC_VECTORVECTORS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_VECTORVECTORS
```

Compile-time label for the DP QC VECTORVECTORS feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_VECTORVECTORS") and read the related declarations before using it.

#### `DP_QC_WHICHPACK`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QC_WHICHPACK
```

Compile-time label for the DP QC WHICHPACK feature/interface. It does not enable that feature at runtime; query checkextension("DP_QC_WHICHPACK") and read the related declarations before using it.

#### `DP_QUAKE2_MODEL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QUAKE2_MODEL
```

Compile-time label for the DP QUAKE2 MODEL feature/interface. It does not enable that feature at runtime; query checkextension("DP_QUAKE2_MODEL") and read the related declarations before using it.

#### `DP_QUAKE2_SPRITE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QUAKE2_SPRITE
```

Compile-time label for the DP QUAKE2 SPRITE feature/interface. It does not enable that feature at runtime; query checkextension("DP_QUAKE2_SPRITE") and read the related declarations before using it.

#### `DP_QUAKE3_MAP`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QUAKE3_MAP
```

Compile-time label for the DP QUAKE3 MAP feature/interface. It does not enable that feature at runtime; query checkextension("DP_QUAKE3_MAP") and read the related declarations before using it.

#### `DP_QUAKE3_MODEL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_QUAKE3_MODEL
```

Compile-time label for the DP QUAKE3 MODEL feature/interface. It does not enable that feature at runtime; query checkextension("DP_QUAKE3_MODEL") and read the related declarations before using it.

#### `DP_REGISTERCVAR`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_REGISTERCVAR
```

Compile-time label for the DP REGISTERCVAR feature/interface. It does not enable that feature at runtime; query checkextension("DP_REGISTERCVAR") and read the related declarations before using it.

#### `DP_SND_OGGVORBIS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SND_OGGVORBIS
```

Compile-time label for the DP SND OGGVORBIS feature/interface. It does not enable that feature at runtime; query checkextension("DP_SND_OGGVORBIS") and read the related declarations before using it.

#### `DP_SND_SOUND7_WIP2`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SND_SOUND7_WIP2
```

Compile-time label for the DP SND SOUND7 WIP2 feature/interface. It does not enable that feature at runtime; query checkextension("DP_SND_SOUND7_WIP2") and read the related declarations before using it.

#### `DP_SND_STEREOWAV`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SND_STEREOWAV
```

Compile-time label for the DP SND STEREOWAV feature/interface. It does not enable that feature at runtime; query checkextension("DP_SND_STEREOWAV") and read the related declarations before using it.

#### `DP_SOLIDCORPSE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SOLIDCORPSE
```

Compile-time label for the DP SOLIDCORPSE feature/interface. It does not enable that feature at runtime; query checkextension("DP_SOLIDCORPSE") and read the related declarations before using it.

#### `DP_SPRITE32`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SPRITE32
```

Compile-time label for the DP SPRITE32 feature/interface. It does not enable that feature at runtime; query checkextension("DP_SPRITE32") and read the related declarations before using it.

#### `DP_SV_BOTCLIENT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_BOTCLIENT
```

Compile-time label for the DP SV BOTCLIENT feature/interface. It does not enable that feature at runtime; query checkextension("DP_SV_BOTCLIENT") and read the related declarations before using it.

#### `DP_SV_CLIENTCAMERA`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_CLIENTCAMERA
```

Works like svc_setview except also handles pvs.

#### `DP_SV_CLIENTCOLORS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_CLIENTCOLORS
```

Provided only for compatibility with DP.

#### `DP_SV_CLIENTNAME`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_CLIENTNAME
```

Provided only for compatibility with DP.

#### `DP_SV_CUSTOMIZEENTITYFORCLIENT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_CUSTOMIZEENTITYFORCLIENT
```

Deprecated feature for compat with DP. Can be slow, incompatible with splitscreen, usually malfunctions with mvds.

#### `DP_SV_DRAWONLYTOCLIENT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_DRAWONLYTOCLIENT
```

Compile-time label for the DP SV DRAWONLYTOCLIENT feature/interface. It does not enable that feature at runtime; query checkextension("DP_SV_DRAWONLYTOCLIENT") and read the related declarations before using it.

#### `DP_SV_DROPCLIENT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_DROPCLIENT
```

Equivelent to quakeworld's stuffcmd(self,"disconnect\n"); hack

#### `DP_SV_EFFECT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_EFFECT
```

Compile-time label for the DP SV EFFECT feature/interface. It does not enable that feature at runtime; query checkextension("DP_SV_EFFECT") and read the related declarations before using it.

#### `DP_SV_NODRAWTOCLIENT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_NODRAWTOCLIENT
```

Compile-time label for the DP SV NODRAWTOCLIENT feature/interface. It does not enable that feature at runtime; query checkextension("DP_SV_NODRAWTOCLIENT") and read the related declarations before using it.

#### `DP_SV_PLAYERPHYSICS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_PLAYERPHYSICS
```

Allows reworking parts of NQ player physics. USE AT OWN RISK - this necessitates NQ physics and is thus guarenteed to break prediction.

#### `DP_SV_POINTSOUND`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_POINTSOUND
```

Compile-time label for the DP SV POINTSOUND feature/interface. It does not enable that feature at runtime; query checkextension("DP_SV_POINTSOUND") and read the related declarations before using it.

#### `DP_SV_PRECACHEANYTIME`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_PRECACHEANYTIME
```

Specifies that the various precache builtins can be called at any time. WARNING: precaches are sent reliably while sound events, modelindexes, and particle events are not. This can mean sounds and particles might not work the first time around, or models may take a while to appear (after the reliables are received and the model is loaded from disk). Always attempt to precache a little in advance in order to reduce these issues (preferably at the start of the map...)

#### `DP_SV_PRINT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_PRINT
```

Says that the print builtin can be used from nqssqc (as well as just csqc), bypassing the developer cvar issues.

#### `DP_SV_ROTATINGBMODEL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_ROTATINGBMODEL
```

Engines that support this support avelocity on MOVETYPE_PUSH entities, pushing entities out of the way as needed.

#### `DP_SV_SETCOLOR`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_SETCOLOR
```

Compile-time label for the DP SV SETCOLOR feature/interface. It does not enable that feature at runtime; query checkextension("DP_SV_SETCOLOR") and read the related declarations before using it.

#### `DP_SV_SHUTDOWN`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_SHUTDOWN
```

Compile-time label for the DP SV SHUTDOWN feature/interface. It does not enable that feature at runtime; query checkextension("DP_SV_SHUTDOWN") and read the related declarations before using it.

#### `DP_SV_SPAWNFUNC_PREFIX`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_SPAWNFUNC_PREFIX
```

Compile-time label for the DP SV SPAWNFUNC PREFIX feature/interface. It does not enable that feature at runtime; query checkextension("DP_SV_SPAWNFUNC_PREFIX") and read the related declarations before using it.

#### `DP_SV_WRITEPICTURE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_WRITEPICTURE
```

Compile-time label for the DP SV WRITEPICTURE feature/interface. It does not enable that feature at runtime; query checkextension("DP_SV_WRITEPICTURE") and read the related declarations before using it.

#### `DP_SV_WRITEUNTERMINATEDSTRING`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_SV_WRITEUNTERMINATEDSTRING
```

Compile-time label for the DP SV WRITEUNTERMINATEDSTRING feature/interface. It does not enable that feature at runtime; query checkextension("DP_SV_WRITEUNTERMINATEDSTRING") and read the related declarations before using it.

#### `DP_TE_BLOOD`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_TE_BLOOD
```

Compile-time label for the DP TE BLOOD feature/interface. It does not enable that feature at runtime; query checkextension("DP_TE_BLOOD") and read the related declarations before using it.

#### `DP_TE_CUSTOMFLASH`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_TE_CUSTOMFLASH
```

Compile-time label for the DP TE CUSTOMFLASH feature/interface. It does not enable that feature at runtime; query checkextension("DP_TE_CUSTOMFLASH") and read the related declarations before using it.

#### `DP_TE_EXPLOSIONRGB`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_TE_EXPLOSIONRGB
```

Compile-time label for the DP TE EXPLOSIONRGB feature/interface. It does not enable that feature at runtime; query checkextension("DP_TE_EXPLOSIONRGB") and read the related declarations before using it.

#### `DP_TE_PARTICLECUBE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_TE_PARTICLECUBE
```

Compile-time label for the DP TE PARTICLECUBE feature/interface. It does not enable that feature at runtime; query checkextension("DP_TE_PARTICLECUBE") and read the related declarations before using it.

#### `DP_TE_PARTICLERAIN`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_TE_PARTICLERAIN
```

Compile-time label for the DP TE PARTICLERAIN feature/interface. It does not enable that feature at runtime; query checkextension("DP_TE_PARTICLERAIN") and read the related declarations before using it.

#### `DP_TE_PARTICLESNOW`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_TE_PARTICLESNOW
```

Compile-time label for the DP TE PARTICLESNOW feature/interface. It does not enable that feature at runtime; query checkextension("DP_TE_PARTICLESNOW") and read the related declarations before using it.

#### `DP_TE_SMALLFLASH`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_TE_SMALLFLASH
```

Compile-time label for the DP TE SMALLFLASH feature/interface. It does not enable that feature at runtime; query checkextension("DP_TE_SMALLFLASH") and read the related declarations before using it.

#### `DP_TE_SPARK`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_TE_SPARK
```

Compile-time label for the DP TE SPARK feature/interface. It does not enable that feature at runtime; query checkextension("DP_TE_SPARK") and read the related declarations before using it.

#### `DP_TE_STANDARDEFFECTBUILTINS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_TE_STANDARDEFFECTBUILTINS
```

Compile-time label for the DP TE STANDARDEFFECTBUILTINS feature/interface. It does not enable that feature at runtime; query checkextension("DP_TE_STANDARDEFFECTBUILTINS") and read the related declarations before using it.

#### `DP_VIEWZOOM`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define DP_VIEWZOOM
```

Compile-time label for the DP VIEWZOOM feature/interface. It does not enable that feature at runtime; query checkextension("DP_VIEWZOOM") and read the related declarations before using it.

#### `EX_PROMPT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define EX_PROMPT
```

Engine-driven alternative to centerprint menus.

#### `EXT_BITSHIFT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define EXT_BITSHIFT
```

Compile-time label for the EXT BITSHIFT feature/interface. It does not enable that feature at runtime; query checkextension("EXT_BITSHIFT") and read the related declarations before using it.

#### `EXT_CSQC`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define EXT_CSQC
```

Compile-time label for the EXT CSQC feature/interface. It does not enable that feature at runtime; query checkextension("EXT_CSQC") and read the related declarations before using it.

#### `EXT_CSQC_SHARED`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define EXT_CSQC_SHARED
```

Compile-time label for the EXT CSQC SHARED feature/interface. It does not enable that feature at runtime; query checkextension("EXT_CSQC_SHARED") and read the related declarations before using it.

#### `EXT_DIMENSION_GHOST`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define EXT_DIMENSION_GHOST
```

Compile-time label for the EXT DIMENSION GHOST feature/interface. It does not enable that feature at runtime; query checkextension("EXT_DIMENSION_GHOST") and read the related declarations before using it.

#### `EXT_DIMENSION_PHYSICS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define EXT_DIMENSION_PHYSICS
```

Compile-time label for the EXT DIMENSION PHYSICS feature/interface. It does not enable that feature at runtime; query checkextension("EXT_DIMENSION_PHYSICS") and read the related declarations before using it.

#### `EXT_DIMENSION_VISIBILITY`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define EXT_DIMENSION_VISIBILITY
```

Compile-time label for the EXT DIMENSION VISIBILITY feature/interface. It does not enable that feature at runtime; query checkextension("EXT_DIMENSION_VISIBILITY") and read the related declarations before using it.

#### `FRIK_FILE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define FRIK_FILE
```

Compile-time label for the FRIK FILE feature/interface. It does not enable that feature at runtime; query checkextension("FRIK_FILE") and read the related declarations before using it.

#### `FTE_CALLTIMEOFDAY`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_CALLTIMEOFDAY
```

Replication of mvdsv functionality (call calltimeofday to cause 'timeofday' to be called, with arguments that can be saved off to a global). Generally strftime is simpler to use.

#### `FTE_CSQC_ALTCONSOLES`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_CSQC_ALTCONSOLES
```

The engine tracks multiple consoles. These may or may not be directly visible to the user.

#### `FTE_CSQC_BASEFRAME`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_CSQC_BASEFRAME
```

Specifies that .basebone, .baseframe2, .baselerpfrac, baseframe1time, etc exist in csqc. These fields affect all bones in the entity's model with a lower index than the .basebone field, allowing you to give separate control to the legs of a skeletal model, without affecting the torso animations.

#### `FTE_CSQC_HALFLIFE_MODELS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_CSQC_HALFLIFE_MODELS
```

Compile-time label for the FTE CSQC HALFLIFE MODELS feature/interface. It does not enable that feature at runtime; query checkextension("FTE_CSQC_HALFLIFE_MODELS") and read the related declarations before using it.

#### `FTE_CSQC_RAWIMAGES`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_CSQC_RAWIMAGES
```

Provides raw rgba image access to csqc. With this, the csprogs can read textures into qc-accessible memory, modify it, and then upload it to the renderer.

#### `FTE_CSQC_RENDERTARGETS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_CSQC_RENDERTARGETS
```

VF_RT_DESTCOLOUR exists and can be used to redirect any rendering to a texture instead of the screen.

#### `FTE_CSQC_REVERB`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_CSQC_REVERB
```

Specifies that the mod can create custom reverb effects. Whether they will actually be used or not depends upon the sound driver.

#### `FTE_CSQC_SERVERBROWSER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_CSQC_SERVERBROWSER
```

Provides builtins to query the engine's serverbrowser servers list from ssqc. Note that these builtins are always available in menuqc.

#### `FTE_CSQC_SKELETONOBJECTS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_CSQC_SKELETONOBJECTS
```

Provides container objects for skeletal bone data, which can be modified on a per bone basis if needed. This allows you to dynamically generate animations (or just blend them with greater customisation) instead of being limited to a single animation or two.

#### `FTE_CSQC_WINDOWCAPTION`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_CSQC_WINDOWCAPTION
```

Provides csqc with the ability to change the window caption as displayed when running windowed or in the task bar when switched out.

#### `FTE_ENT_SKIN_CONTENTS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_ENT_SKIN_CONTENTS
```

self.skin = CONTENTS_WATER; makes a brush entity into water. use -16 for a ladder.

#### `FTE_ENT_UNIQUESPAWNID`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_ENT_UNIQUESPAWNID
```

Compile-time label for the FTE ENT UNIQUESPAWNID feature/interface. It does not enable that feature at runtime; query checkextension("FTE_ENT_UNIQUESPAWNID") and read the related declarations before using it.

#### `FTE_EXTENDEDTEXTCODES`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_EXTENDEDTEXTCODES
```

Compile-time label for the FTE EXTENDEDTEXTCODES feature/interface. It does not enable that feature at runtime; query checkextension("FTE_EXTENDEDTEXTCODES") and read the related declarations before using it.

#### `FTE_FORCEINFOKEY`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_FORCEINFOKEY
```

Provides an easy way to change a user's userinfo from the server.

#### `FTE_FORCESHADER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_FORCESHADER
```

Allows csqc to override shaders on models with an explicitly named replacement. Also allows you to define shaders with a fallback if it does not exist on disk.

#### `FTE_GFX_IQM_HITMESH`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_GFX_IQM_HITMESH
```

Supports hitmesh iqm extensions. Also supports geomsets and embedded events.

#### `FTE_GFX_MODELEVENTS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_GFX_MODELEVENTS
```

Provides a query for per-animation events in model files, including from progs/foo.mdl.events files.

#### `FTE_GFX_QUAKE3SHADERS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_GFX_QUAKE3SHADERS
```

specifies that the engine has full support for vanilla quake3 shaders

#### `FTE_GFX_REMAPSHADER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_GFX_REMAPSHADER
```

With the raw power of stuffcmds, the r_remapshader console command is exposed! This mystical command can be used to remap any shader to another. Remapped shaders that specify $diffuse etc in some form will inherit the textures implied by the surface.

#### `FTE_HEXEN2`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_HEXEN2
```

Compile-time label for the FTE HEXEN2 feature/interface. It does not enable that feature at runtime; query checkextension("FTE_HEXEN2") and read the related declarations before using it.

#### `FTE_INFOBLOBS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_INFOBLOBS
```

Removes the length limits on user/server/local info strings, and allows embedded nulls and other otherwise-reserved characters. This can be used to network avatar images and the like, or other binary data.

#### `FTE_ISBACKBUFFERED`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_ISBACKBUFFERED
```

Allows you to check if a client has too many reliable messages pending.

#### `FTE_MEDIA_CIN`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_MEDIA_CIN
```

playfilm command supports q2 cin files.

#### `FTE_MEDIA_ROQ`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_MEDIA_ROQ
```

playfilm command supports q3 roq files.

#### `FTE_MEMALLOC`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_MEMALLOC
```

Allows dynamically allocating memory. Use pointers to access this memory. Memory will not be saved into saved games.

#### `FTE_MULTIPROGS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_MULTIPROGS
```

Multiple progs.dat files can be loaded inside the same qcvm. Insert new ones with addprogs inside the 'init' function, and use externvalue+externset to rewrite globals (and hook functions) to link them together. Note that the result is generally not very clean unless you carefully design for it beforehand.

#### `FTE_MULTITHREADED`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_MULTITHREADED
```

Faux multithreading, allowing multiple contexts to run in sequence.

#### `FTE_MVD_PLAYERSTATS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_MVD_PLAYERSTATS
```

In csqc, getplayerstat can be used to query any player's stats when playing back MVDs. isdemo will return 2 in this case.

#### `FTE_PART_NAMESPACE_EFFECTINFO`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PART_NAMESPACE_EFFECTINFO
```

Specifies that effectinfo.bar can load effects from effectinfo.txt for DP compatibility.

#### `FTE_PART_NAMESPACES`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PART_NAMESPACES
```

Specifies that the engine can use foo.bar to load effect foo from particle description bar. When used via ssqc, this should cause the client to download whatever effects as needed.

#### `FTE_PART_SCRIPT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PART_SCRIPT
```

Specifies that the r_particledesc cvar can be used to select a list of particle effects to load from particles/foo.cfg, the format of which is documented elsewhere.

#### `FTE_PEXT_256PACKETENTITIES`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PEXT_256PACKETENTITIES
```

Specifies that the client is not limited to vanilla's limit of only 64 ents visible at once.

#### `FTE_PEXT_CUSTOMTENTS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PEXT_CUSTOMTENTS
```

Compile-time label for the FTE PEXT CUSTOMTENTS feature/interface. It does not enable that feature at runtime; query checkextension("FTE_PEXT_CUSTOMTENTS") and read the related declarations before using it.

#### `FTE_PEXT_FATNESS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PEXT_FATNESS
```

Compile-time label for the FTE PEXT FATNESS feature/interface. It does not enable that feature at runtime; query checkextension("FTE_PEXT_FATNESS") and read the related declarations before using it.

#### `FTE_PEXT_FLOATCOORDS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PEXT_FLOATCOORDS
```

Compile-time label for the FTE PEXT FLOATCOORDS feature/interface. It does not enable that feature at runtime; query checkextension("FTE_PEXT_FLOATCOORDS") and read the related declarations before using it.

#### `FTE_PEXT_LIGHTSTYLECOL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PEXT_LIGHTSTYLECOL
```

Compile-time label for the FTE PEXT LIGHTSTYLECOL feature/interface. It does not enable that feature at runtime; query checkextension("FTE_PEXT_LIGHTSTYLECOL") and read the related declarations before using it.

#### `FTE_PEXT_Q2BSP`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PEXT_Q2BSP
```

Specifies that the client supports q2bsps.

#### `FTE_PEXT_Q3BSP`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PEXT_Q3BSP
```

Specifies that the client supports q3bsps.

#### `FTE_PEXT_SETVIEW`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PEXT_SETVIEW
```

NQ's svc_setview works correctly even in quakeworld

#### `FTE_PEXT_SPAWNSTATIC`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PEXT_SPAWNSTATIC
```

Compile-time label for the FTE PEXT SPAWNSTATIC feature/interface. It does not enable that feature at runtime; query checkextension("FTE_PEXT_SPAWNSTATIC") and read the related declarations before using it.

#### `FTE_PEXT_TE_BULLET`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PEXT_TE_BULLET
```

Compile-time label for the FTE PEXT TE BULLET feature/interface. It does not enable that feature at runtime; query checkextension("FTE_PEXT_TE_BULLET") and read the related declarations before using it.

#### `FTE_PEXT_VIEW2`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_PEXT_VIEW2
```

Compile-time label for the FTE PEXT VIEW2 feature/interface. It does not enable that feature at runtime; query checkextension("FTE_PEXT_VIEW2") and read the related declarations before using it.

#### `FTE_QC_BASEFRAME`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_BASEFRAME
```

Specifies that .basebone and .baseframe exist in ssqc. These fields affect all bones in the entity's model with a lower index than the .basebone field, allowing you to give separate control to the legs of a skeletal model, without affecting the torso animations, from ssqc.

#### `FTE_QC_CHANGELEVEL_HUB`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_CHANGELEVEL_HUB
```

Adds an extra argument to changelevel which is carried over to the next map in the 'startspot' global. Maps will be saved+reloaded until the extra argument is omitted again, purging all saved maps. Saved games will contain a copy of each preserved map. parm1-parm64 globals can be used, giving more space to transfer more player data.

#### `FTE_QC_CHECKCOMMAND`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_CHECKCOMMAND
```

Provides a way to test if a console command exists, and whether its a command/alias/cvar. Does not say anything about the expected meanings of any arguments or values.

#### `FTE_QC_CHECKPVS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_CHECKPVS
```

Compile-time label for the FTE QC CHECKPVS feature/interface. It does not enable that feature at runtime; query checkextension("FTE_QC_CHECKPVS") and read the related declarations before using it.

#### `FTE_QC_CROSSPRODUCT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_CROSSPRODUCT
```

Compile-time label for the FTE QC CROSSPRODUCT feature/interface. It does not enable that feature at runtime; query checkextension("FTE_QC_CROSSPRODUCT") and read the related declarations before using it.

#### `FTE_QC_CUSTOMSKINS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_CUSTOMSKINS
```

The engine supports the use of q3 skins, as well as the use of such skin 'files' to specify rich top+bottom colours, qw skins, geomsets, or texture composition even on non-players..

#### `FTE_QC_DIGEST_SHA1`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_DIGEST_SHA1
```

The digest_hex builtin supports 160-bit sha1 hashes.

#### `FTE_QC_DIGEST_SHA224`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_DIGEST_SHA224
```

The digest_hex builtin supports 224-bit sha2 hashes.

#### `FTE_QC_DIGEST_SHA384`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_DIGEST_SHA384
```

The digest_hex builtin supports 384-bit sha2 hashes.

#### `FTE_QC_DIGEST_SHA512`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_DIGEST_SHA512
```

The digest_hex builtin supports 512-bit sha2 hashes.

#### `FTE_QC_FILE_BINARY`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_FILE_BINARY
```

Extends FRIK_FILE with binary read+write, as well as allowing seeking. Requires pointers.

#### `FTE_QC_FS_SEARCH_SIZEMTIME`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_FS_SEARCH_SIZEMTIME
```

Compile-time label for the FTE QC FS SEARCH SIZEMTIME feature/interface. It does not enable that feature at runtime; query checkextension("FTE_QC_FS_SEARCH_SIZEMTIME") and read the related declarations before using it.

#### `FTE_QC_HARDWARECURSORS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_HARDWARECURSORS
```

setcursormode exists in both csqc+menuqc, and accepts additional arguments to specify a cursor image to use when this module has focus. If the image exceeds hardware limits (or hardware cursors are unsupported), it will be emulated using regular draws - this at least still avoids conflicting cursors as only one will ever be used, even if console+menu+csqc are all overlayed.

#### `FTE_QC_HASHTABLES`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_HASHTABLES
```

Provides efficient string-based lookups.

#### `FTE_QC_INFOKEY`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_INFOKEY
```

QuakeWorld's infokey builtin works, and reports at least name+topcolor+bottomcolor+ping(in ms)+ip(unmasked, but not always ipv4)+team(aka bottomcolor in nq). Does not require actual localinfo/serverinfo/userinfo, but they're _highly_ recommended to any engines with csqc

#### `FTE_QC_INTCONV`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_INTCONV
```

Provides string<>int conversions, including hex representations.

#### `FTE_QC_MATCHCLIENTNAME`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_MATCHCLIENTNAME
```

Compile-time label for the FTE QC MATCHCLIENTNAME feature/interface. It does not enable that feature at runtime; query checkextension("FTE_QC_MATCHCLIENTNAME") and read the related declarations before using it.

#### `FTE_QC_MULTICAST`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_MULTICAST
```

QuakeWorld's multicast builtin works along with MSG_MULTICAST, but also with unicast support.

#### `FTE_QC_PAUSED`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_PAUSED
```

Compile-time label for the FTE QC PAUSED feature/interface. It does not enable that feature at runtime; query checkextension("FTE_QC_PAUSED") and read the related declarations before using it.

#### `FTE_QC_PERSISTENTTEMPSTRINGS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_PERSISTENTTEMPSTRINGS
```

Supersedes DP_QC_MULTIPLETEMPSTRINGS. Temp strings are garbage collected automatically, and do not expire while they're still in use. This makes strzone redundant.

#### `FTE_QC_RAGDOLL_WIP`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_RAGDOLL_WIP
```

Compile-time label for the FTE QC RAGDOLL WIP feature/interface. It does not enable that feature at runtime; query checkextension("FTE_QC_RAGDOLL_WIP") and read the related declarations before using it.

#### `FTE_QC_SENDPACKET`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_SENDPACKET
```

Allows the use of out-of-band udp packets to/from other hosts. Includes the SV_ParseConnectionlessPacket event.

#### `FTE_QC_STUFFCMDFLAGS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_STUFFCMDFLAGS
```

Variation on regular stuffcmd that gives control over how spectators/mvds should be treated.

#### `FTE_QC_TRACETRIGGER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QC_TRACETRIGGER
```

Compile-time label for the FTE QC TRACETRIGGER feature/interface. It does not enable that feature at runtime; query checkextension("FTE_QC_TRACETRIGGER") and read the related declarations before using it.

#### `FTE_QUAKE2_CLIENT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QUAKE2_CLIENT
```

This engine is able to act as a quake2 client

#### `FTE_QUAKE2_SERVER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QUAKE2_SERVER
```

This engine is able to act as a quake2 server

#### `FTE_QUAKE3_CLIENT`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QUAKE3_CLIENT
```

This engine is able to act as a quake3 client

#### `FTE_QUAKE3_SERVER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_QUAKE3_SERVER
```

This engine is able to act as a quake3 server

#### `FTE_RAW_MAP`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_RAW_MAP
```

This engine supports directly loading .map files, as well as realtime editing of the various brushes.

#### `FTE_SOLID_BSPTRIGGER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_SOLID_BSPTRIGGER
```

Allows for mappers to use shaped triggers instead of being limited to axially aligned triggers.

#### `FTE_SOLID_LADDER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_SOLID_LADDER
```

Allows a simple trigger to remove effects of gravity (solid 20). obsolete. will prolly be removed at some point as it is not networked properly. Use FTE_ENT_SKIN_CONTENTS

#### `FTE_SPLITSCREEN`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_SPLITSCREEN
```

Client supports splitscreen, controlled via cl_splitscreen. Servers require allow_splitscreen 1 if splitscreen is to be used over the internet. Mods that use csqc will need to be aware for this to work properly. per-client networking may be problematic.

#### `FTE_SQL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_SQL
```

Provides sql* builtins which can be used for sql database access

#### `FTE_SQL_SQLITE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_SQL_SQLITE
```

SQL functionality is able to utilise sqlite databases

#### `FTE_STRINGS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_STRINGS
```

Extra builtins (and additional behaviour) to make string manipulation easier

#### `FTE_SV_POINTPARTICLES`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_SV_POINTPARTICLES
```

Specifies that particleeffectnum, pointparticles, and trailparticles exist in ssqc as well as csqc. particleeffectnum acts as a precache, allowing ssqc values to be networked up with csqc for use. Use in combination with FTE_PART_SCRIPT+FTE_PART_NAMESPACES to use custom effects. This extension is functionally identical to the DP version, but avoids any misplaced assumptions about the format of the client's particle descriptions.

#### `FTE_SV_REENTER`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_SV_REENTER
```

Compile-time label for the FTE SV REENTER feature/interface. It does not enable that feature at runtime; query checkextension("FTE_SV_REENTER") and read the related declarations before using it.

#### `FTE_TE_STANDARDEFFECTBUILTINS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_TE_STANDARDEFFECTBUILTINS
```

Provides builtins to replace writebytes, with a QW compatible twist.

#### `FTE_TERRAIN_MAP`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_TERRAIN_MAP
```

This engine supports .hmp files, as well as terrain embedded within bsp files.

#### `FTE_VRINPUTS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** FTE extension.

```qc
#define FTE_VRINPUTS
```

input_weapon, input_left_*, input_right_*, input_head_* work, both in csqc (as inputs with suitable plugin/hardware support) and ssqc (available in PlayerPreThink).

#### `KRIMZON_SV_PARSECLIENTCOMMAND`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define KRIMZON_SV_PARSECLIENTCOMMAND
```

SSQC's SV_ParseClientCommand function is able to handle client 'cmd' commands. The tokenizing parts also work in csqc.

#### `NEH_CMD_PLAY2`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define NEH_CMD_PLAY2
```

Compile-time label for the NEH CMD PLAY2 feature/interface. It does not enable that feature at runtime; query checkextension("NEH_CMD_PLAY2") and read the related declarations before using it.

#### `NEH_RESTOREGAME`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define NEH_RESTOREGAME
```

Compile-time label for the NEH RESTOREGAME feature/interface. It does not enable that feature at runtime; query checkextension("NEH_RESTOREGAME") and read the related declarations before using it.

#### `QSG_CVARSTRING`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define QSG_CVARSTRING
```

Compile-time label for the QSG CVARSTRING feature/interface. It does not enable that feature at runtime; query checkextension("QSG_CVARSTRING") and read the related declarations before using it.

#### `QW_ENGINE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define QW_ENGINE
```

Compile-time label for the QW ENGINE feature/interface. It does not enable that feature at runtime; query checkextension("QW_ENGINE") and read the related declarations before using it.

#### `QWE_MVD_RECORD`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define QWE_MVD_RECORD
```

You can use the easyrecord command to record MVD demos serverside.

#### `TEI_MD3_MODEL`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define TEI_MD3_MODEL
```

Compile-time label for the TEI MD3 MODEL feature/interface. It does not enable that feature at runtime; query checkextension("TEI_MD3_MODEL") and read the related declarations before using it.

#### `TEI_SHOWLMP2`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define TEI_SHOWLMP2
```

Compile-time label for the TEI SHOWLMP2 feature/interface. It does not enable that feature at runtime; query checkextension("TEI_SHOWLMP2") and read the related declarations before using it.

#### `TENEBRAE_GFX_DLIGHTS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define TENEBRAE_GFX_DLIGHTS
```

Allows ssqc to attach rtlights to entities with various special properties.

#### `ZQ_MOVETYPE_FLY`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define ZQ_MOVETYPE_FLY
```

MOVETYPE_FLY works on players.

#### `ZQ_MOVETYPE_NOCLIP`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define ZQ_MOVETYPE_NOCLIP
```

MOVETYPE_NOCLIP works on players.

#### `ZQ_MOVETYPE_NONE`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define ZQ_MOVETYPE_NONE
```

MOVETYPE_NONE works on players.

#### `ZQ_QC_STRINGS`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define ZQ_QC_STRINGS
```

The strings-only subset of FRIK_FILE is supported.

#### `ZQ_VWEP`

**Scope:** NQ SSQC, QW SSQC, CSQC, MenuQC. **Compatibility:** Shared extension.

```qc
#define ZQ_VWEP
```

Compile-time label for the ZQ VWEP feature/interface. It does not enable that feature at runtime; query checkextension("ZQ_VWEP") and read the related declarations before using it.

<!-- END GENERATED QUAKEC API -->
