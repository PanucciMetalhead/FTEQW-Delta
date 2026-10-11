# Console Commands

This reference lists **600 built-in command names** registered by this fork's
macOS SDL3 GPU client and dedicated server: 598 in the graphical client and
198 in the dedicated server, with overlapping names counted once. It includes
compatibility aliases, held input actions, developer tools, and protocol helpers.
Definitions come from the engine's registered help text and command handlers.
The inventory was checked with `cmdlist` and `apropos ""` on 12 October 2026.

## Using the console

Open the console with its assigned key (normally the backtick/tilde key), or
use `toggleconsole` from a binding. Type a command followed by its arguments.
Quote arguments containing spaces; a semicolon separates commands. Use Tab
completion to find names and supported completions.

```text
map e1m1
bind w +forward
bind F12 screenshot
echo "Loading my settings"; exec myconfig.cfg
cfg_save
```

In the reference, `<argument>` is a placeholder to replace, and `[argument]`
means optional; do not type the brackets. Game paths normally refer to the
engine's virtual filesystem and selected game directory, including mounted
packages. Output paths and permitted writes depend on the command.

**Commands perform actions; console variables (cvars) store settings.** For
example, `screenshot` is a command, while `r_fullbright 0` sets a cvar. This
document lists commands, including commands that manipulate cvars; it does
not attempt to enumerate the much larger cvar registry.

```text
cmdlist
cmdlist *demo*
apropos screenshot
find lighting
cvarlist
```

`cmdlist` accepts a wildcard name filter. `apropos` and its alias `find` search
both names and registered help text for a substring. Some older commands have
no runtime help text; their definitions are supplied here from their handlers.
The parenthesized number in `cmdlist` output is a command restriction level.
Aliases created with `alias` can be inspected separately using `aliaslist`.

Commands starting with `+` activate an action, and their matching `-` commands
release it. A binding such as `bind w +forward` supplies the release when the
key is released. Typing `+forward` manually keeps that input active until
`-forward` is executed. The mod and movement mode determine the gameplay
meaning of inputs such as attack, use, and auxiliary buttons.

## Availability and scope

| Where | Meaning |
| --- | --- |
| Client | Registered by the graphical client executable. This also contains a listen server; some entries are hosted-server administration commands. |
| Server | Registered by the dedicated server executable. |
| Both | Registered by both executables. A name can have different client and server behavior, as noted in its definition. |

Presence in the registry does not guarantee that an operation is meaningful
at every moment. A server command may need an active hosted game; QuakeC tools
need the relevant VM; media, plugins, and databases need their optional decoders
or libraries. Cheat requests require server authorization. Client commands can
forward requests to a remote server, whose protocol, permissions, and gamecode
determine the result. In a graphical listen server, `serverinfo` delegates to
the hosted server's handler; when connected only to a remote server, its
no-argument form inspects that server's information.

The list describes this fork's desktop build, not every upstream platform or
compile-time configuration. A different build, loaded menu program, mod, or
plugin can add or remove commands. Init-only commands that are removed before
normal console use, user aliases, cvars, and unregistered wire-protocol tokens
are outside this inventory. The native GPU renderer is `sdlgpu`; material tools
with shader names remain useful, but removed OpenGL renderers are unavailable.

## Quick tasks

| Task | Commands/settings |
| --- | --- |
| Find a command or setting | `cmdlist *name*`, `apropos keyword`, `cvarlist` |
| Load/save configuration | `exec myconfig.cfg`, `cfg_save` |
| Start or restore a game | `map e1m1`, `save quick`, `load quick` |
| Connect to a server | `connect address:port`, `disconnect`, `reconnect` |
| Record/play a client demo | `record mydemo`, `stop`, `playdemo mydemo` |
| Inspect rendering/performance | `gpu_info`, `gpu_benchmark`, `timedemo` |
| Change a graphics preset | `fps_preset normal` (faithful), `fps_preset vanilla`, or the Options menu |
| Inspect a file lookup | `path`, `which filename`, `dir pattern` |

## Command reference

Names are listed individually, including aliases, so the reference can be
searched using the exact spelling shown by the engine. Input actions and
menu commands are grouped first; the remaining commands are alphabetical.

Jump to [input actions](#held-input-actions), [menus](#built-in-menu-entry-points),
[0–C](#0c), [D–H](#dh), [I–M](#im), [N–R](#nr), or [S–Z](#sz).

### Held input actions

| Command | Where | Definition |
| --- | --- | --- |
| `+attack` | Client | Activate attack/fire input while held. |
| `+back` | Client | Activate backward movement input while held. |
| `+button0` | Client | Press auxiliary input button 0; meaning is defined by the mod and may be unused. |
| `+button1` | Client | Press auxiliary input button 1; meaning is defined by the mod and may be unused. |
| `+button10` | Client | Press auxiliary input button 10; meaning is defined by the mod and may be unused. |
| `+button11` | Client | Press auxiliary input button 11; meaning is defined by the mod and may be unused. |
| `+button12` | Client | Press auxiliary input button 12; meaning is defined by the mod and may be unused. |
| `+button13` | Client | Press auxiliary input button 13; meaning is defined by the mod and may be unused. |
| `+button14` | Client | Press auxiliary input button 14; meaning is defined by the mod and may be unused. |
| `+button15` | Client | Press auxiliary input button 15; meaning is defined by the mod and may be unused. |
| `+button16` | Client | Press auxiliary input button 16; meaning is defined by the mod and may be unused. |
| `+button17` | Client | Press auxiliary input button 17; meaning is defined by the mod and may be unused. |
| `+button18` | Client | Press auxiliary input button 18; meaning is defined by the mod and may be unused. |
| `+button19` | Client | Press auxiliary input button 19; meaning is defined by the mod and may be unused. |
| `+button2` | Client | Press auxiliary input button 2; meaning is defined by the mod and may be unused. |
| `+button3` | Client | Press auxiliary input button 3; meaning is defined by the mod and may be unused. |
| `+button4` | Client | Press auxiliary input button 4; meaning is defined by the mod and may be unused. |
| `+button5` | Client | Press auxiliary input button 5; meaning is defined by the mod and may be unused. |
| `+button6` | Client | Press auxiliary input button 6; meaning is defined by the mod and may be unused. |
| `+button7` | Client | Press auxiliary input button 7; meaning is defined by the mod and may be unused. |
| `+button8` | Client | Press auxiliary input button 8; meaning is defined by the mod and may be unused. |
| `+button9` | Client | Press auxiliary input button 9; meaning is defined by the mod and may be unused. |
| `+forward` | Client | Activate forward movement input while held. |
| `+infoplaque` | Client | Activate the Hexen II information plaque while held. |
| `+jump` | Client | Activate jump input while held. |
| `+klook` | Client | Activate keyboard-look mode (forward/back keys control pitch) while held. |
| `+left` | Client | Activate left yaw-turn input while held. |
| `+lookdown` | Client | Activate downward pitch input while held. |
| `+lookup` | Client | Activate upward pitch input while held. |
| `+mlook` | Client | Activate mouse-look mode while held. |
| `+movedown` | Client | Activate downward movement input while held. |
| `+moveleft` | Client | Activate left strafe input while held. |
| `+moveright` | Client | Activate right strafe input while held. |
| `+moveup` | Client | Activate upward movement input while held. |
| `+p` | Client | Execute a press action for a selected split-screen seat: `+p <seat-number> <action>`; prepends `+` to the action name. |
| `+p1` | Client | Execute a press command for split-screen seat 1: `+p1 <action>`; prepends `+` to the action name. |
| `+p2` | Client | Execute a press command for split-screen seat 2: `+p2 <action>`; prepends `+` to the action name. |
| `+p3` | Client | Execute a press command for split-screen seat 3: `+p3 <action>`; prepends `+` to the action name. |
| `+p4` | Client | Execute a press command for split-screen seat 4: `+p4 <action>`; prepends `+` to the action name. |
| `+right` | Client | Activate right yaw-turn input while held. |
| `+rollleft` | Client | Activate left roll input while held. |
| `+rollright` | Client | Activate right roll input while held. |
| `+showdm` | Client | Activate the scoreboard (Hexen II compatibility name) while held. |
| `+showinfo` | Client | Activate the extra Hexen II status information while held. |
| `+showscores` | Client | Activate the scoreboard while held. |
| `+showteamscores` | Client | Activate the team scoreboard while held. |
| `+speed` | Client | Activate speed-modifier input while held. |
| `+strafe` | Client | Activate strafe-modifier mode (turn keys control sideways movement) while held. |
| `+use` | Client | Activate use/interact input (game-dependent) while held. |
| `+voip` | Client | Activate voice capture for push-to-talk while held. |
| `+weaponwheel` | Client | Activate the weapon-selection wheel while held. |
| `-attack` | Client | Release/deactivate attack/fire input. |
| `-back` | Client | Release/deactivate backward movement input. |
| `-button0` | Client | Release auxiliary input button 0. |
| `-button1` | Client | Release auxiliary input button 1. |
| `-button10` | Client | Release auxiliary input button 10. |
| `-button11` | Client | Release auxiliary input button 11. |
| `-button12` | Client | Release auxiliary input button 12. |
| `-button13` | Client | Release auxiliary input button 13. |
| `-button14` | Client | Release auxiliary input button 14. |
| `-button15` | Client | Release auxiliary input button 15. |
| `-button16` | Client | Release auxiliary input button 16. |
| `-button17` | Client | Release auxiliary input button 17. |
| `-button18` | Client | Release auxiliary input button 18. |
| `-button19` | Client | Release auxiliary input button 19. |
| `-button2` | Client | Release auxiliary input button 2. |
| `-button3` | Client | Release auxiliary input button 3. |
| `-button4` | Client | Release auxiliary input button 4. |
| `-button5` | Client | Release auxiliary input button 5. |
| `-button6` | Client | Release auxiliary input button 6. |
| `-button7` | Client | Release auxiliary input button 7. |
| `-button8` | Client | Release auxiliary input button 8. |
| `-button9` | Client | Release auxiliary input button 9. |
| `-forward` | Client | Release/deactivate forward movement input. |
| `-infoplaque` | Client | Release/deactivate the Hexen II information plaque. |
| `-jump` | Client | Release/deactivate jump input. |
| `-klook` | Client | Release/deactivate keyboard-look mode (forward/back keys control pitch). |
| `-left` | Client | Release/deactivate left yaw-turn input. |
| `-lookdown` | Client | Release/deactivate downward pitch input. |
| `-lookup` | Client | Release/deactivate upward pitch input. |
| `-mlook` | Client | Release/deactivate mouse-look mode. |
| `-movedown` | Client | Release/deactivate downward movement input. |
| `-moveleft` | Client | Release/deactivate left strafe input. |
| `-moveright` | Client | Release/deactivate right strafe input. |
| `-moveup` | Client | Release/deactivate upward movement input. |
| `-p` | Client | Execute the matching release action for a selected split-screen seat; prepends `-` to the action name. |
| `-p1` | Client | Execute the matching release command for split-screen seat 1; prepends `-` to the action name. |
| `-p2` | Client | Execute the matching release command for split-screen seat 2; prepends `-` to the action name. |
| `-p3` | Client | Execute the matching release command for split-screen seat 3; prepends `-` to the action name. |
| `-p4` | Client | Execute the matching release command for split-screen seat 4; prepends `-` to the action name. |
| `-right` | Client | Release/deactivate right yaw-turn input. |
| `-rollleft` | Client | Release/deactivate left roll input. |
| `-rollright` | Client | Release/deactivate right roll input. |
| `-showdm` | Client | Release/deactivate the scoreboard (Hexen II compatibility name). |
| `-showinfo` | Client | Release/deactivate the extra Hexen II status information. |
| `-showscores` | Client | Release/deactivate the scoreboard. |
| `-showteamscores` | Client | Release/deactivate the team scoreboard. |
| `-speed` | Client | Release/deactivate speed-modifier input. |
| `-strafe` | Client | Release/deactivate strafe-modifier mode (turn keys control sideways movement). |
| `-use` | Client | Release/deactivate use/interact input (game-dependent). |
| `-voip` | Client | Release/deactivate voice capture for push-to-talk. |
| `-weaponwheel` | Client | Release/deactivate the weapon-selection wheel. |

### Built-in menu entry points

| Command | Where | Definition |
| --- | --- | --- |
| `menu_audio` | Client | Open the built-in audio options menu; a mod-provided menu may replace the visible interface. |
| `menu_cmd` | Client | Pass command text to the menu QuakeC VM's `GameCommand` handler; subcommands are defined by the menu program. |
| `menu_credits` | Client | Open the built-in credits menu; a mod-provided menu may replace the visible interface. |
| `menu_demo` | Client | Open the built-in demo browser menu; a mod-provided menu may replace the visible interface. |
| `menu_download` | Client | Open the built-in package/download manager menu; a mod-provided menu may replace the visible interface. |
| `menu_fps` | Client | Open the built-in performance/graphics preset options menu; a mod-provided menu may replace the visible interface. |
| `menu_keys` | Client | Open the built-in key bindings menu; a mod-provided menu may replace the visible interface. |
| `menu_lighting` | Client | Open the built-in lighting options menu; a mod-provided menu may replace the visible interface. |
| `menu_load` | Client | Open the built-in load-game menu; a mod-provided menu may replace the visible interface. |
| `menu_loadgame` | Client | Open the built-in load-game menu; a mod-provided menu may replace the visible interface. |
| `menu_main` | Client | Open the built-in main menu; a mod-provided menu may replace the visible interface. |
| `menu_media` | Client | Open the built-in media playlist menu; a mod-provided menu may replace the visible interface. |
| `menu_mediafiles` | Client | Open the built-in media file browser menu; a mod-provided menu may replace the visible interface. |
| `menu_mods` | Client | Open the built-in game/mod selection menu; a mod-provided menu may replace the visible interface. |
| `menu_multi` | Client | Open the built-in multiplayer menu; a mod-provided menu may replace the visible interface. |
| `menu_network` | Client | Open the built-in network options menu; a mod-provided menu may replace the visible interface. |
| `menu_newmulti` | Client | Open the built-in multiplayer game setup/hosting menu; a mod-provided menu may replace the visible interface. |
| `menu_options` | Client | Open the built-in options menu; a mod-provided menu may replace the visible interface. |
| `menu_particles` | Client | Open the built-in particle options menu; a mod-provided menu may replace the visible interface. |
| `menu_quit` | Client | Open the built-in quit confirmation menu; a mod-provided menu may replace the visible interface. |
| `menu_render` | Client | Open the built-in rendering options menu; a mod-provided menu may replace the visible interface. |
| `menu_restart` | Client | Restart the menu subsystem and reload the menu program/resources. |
| `menu_save` | Client | Open the built-in save-game menu; a mod-provided menu may replace the visible interface. |
| `menu_servers` | Client | Open the built-in server browser menu; a mod-provided menu may replace the visible interface. |
| `menu_setup` | Client | Open the built-in player setup menu; a mod-provided menu may replace the visible interface. |
| `menu_single` | Client | Open the built-in single-player menu; a mod-provided menu may replace the visible interface. |
| `menu_slist` | Client | Open the built-in server browser menu; a mod-provided menu may replace the visible interface. |
| `menu_spcheats` | Client | Open the built-in single-player cheat options menu; a mod-provided menu may replace the visible interface. |
| `menu_speakers` | Client | Open the built-in speaker configuration menu; a mod-provided menu may replace the visible interface. |
| `menu_teamplay` | Client | Open the built-in QuakeWorld teamplay options menu; a mod-provided menu may replace the visible interface. |
| `menu_teamplay_ammo_health` | Client | Open the built-in teamplay ammo/health naming menu; a mod-provided menu may replace the visible interface. |
| `menu_teamplay_armor` | Client | Open the built-in teamplay armor naming menu; a mod-provided menu may replace the visible interface. |
| `menu_teamplay_items` | Client | Open the built-in teamplay item naming menu; a mod-provided menu may replace the visible interface. |
| `menu_teamplay_locations` | Client | Open the built-in teamplay location options menu; a mod-provided menu may replace the visible interface. |
| `menu_teamplay_needs` | Client | Open the built-in teamplay needs thresholds menu; a mod-provided menu may replace the visible interface. |
| `menu_teamplay_powerups` | Client | Open the built-in teamplay powerup naming menu; a mod-provided menu may replace the visible interface. |
| `menu_teamplay_status_location_misc` | Client | Open the built-in teamplay status/location/miscellaneous naming menu; a mod-provided menu may replace the visible interface. |
| `menu_teamplay_team_fortress` | Client | Open the built-in Team Fortress teamplay naming menu; a mod-provided menu may replace the visible interface. |
| `menu_teamplay_weapons` | Client | Open the built-in teamplay weapon naming menu; a mod-provided menu may replace the visible interface. |
| `menu_textures` | Client | Open the built-in texture options menu; a mod-provided menu may replace the visible interface. |
| `menu_video` | Client | Open the built-in video/display options menu; a mod-provided menu may replace the visible interface. |

### 0–C

| Command | Where | Definition |
| --- | --- | --- |
| `6dof` | Client | Toggle six-degree-of-freedom movement as a server-authorized cheat; requires compatible gamecode. |
| `addip` | Both | Add an address or address range to the server's IP penalty/filter list. |
| `adduser` | Both | Create a persistent ranking/account entry: `rankadd <name> [numeric-password] [rights]`. |
| `alias` | Both | Define a named console command sequence, or inspect aliases. Example: `alias greet "say Hello"`. `newalias` is a compatibility name for the same handler. |
| `aliasedit` | Client | Put an existing alias definition into the console input line for editing. |
| `aliaslevel` | Both | Set or inspect the execution level used by a console alias. |
| `aliaslist` | Both | List defined console aliases, optionally filtered by name. |
| `allskins` | Client | Force QuakeWorld players to use a specified skin instead of their individual choices. |
| `applycompile` | Both | Reload server QuakeC after compilation while saving and restoring the active world's entity state; intended for development on a running server. |
| `apropos` | Both | Search commands and cvars for a substring in their name or help text: `apropos screenshot`. `find` is an alias. |
| `autotrack` | Client | Reset spectator automatic player tracking, allowing the tracking logic to choose a target again. |
| `ban` | Both | Block the player's IP, preventing them from connecting. Also kicks them. |
| `banlist` | Both | Displays a list of every banned player on the server. |
| `banname` | Both | Legacy compat, please use ban. |
| `bestweapon` | Client | Works like 'impulse', for compat with other engines. |
| `bf` | Client | Trigger the bonus/pickup screen-color flash. |
| `bind` | Client | Changes the action associated with each keyboard button. Use eg "bind ctrl+shift+alt+k kill" for special modifiers (should be used only after more basic modifiers). |
| `bindlevel` | Client | Create or inspect a key binding with an explicit execution level: `bindlevel <key> <level> [command]`. |
| `breakpoint` | Both | Toggle a server QuakeC breakpoint at a source filename and line number. |
| `breakpoint_csqc` | Client | Toggle a client QuakeC breakpoint at a source filename and line number. |
| `breakpoint_menuqc` | Client | Toggle a menu QuakeC breakpoint at a source filename and line number. |
| `cancel` | Both | Cancel the active hosted-server multiview-demo recording and discard its output; compatibility name for `sv_democancel`. |
| `capture` | Client | Captures realtime action to a named video file. Check the capture* cvars to control driver/codecs/rates. |
| `capturedemo` | Client | capturedemo foo.dem foo.avi - Captures a named demo to a named video file. Demo capturing is performed offscreen when possible, allowing arbitrary video sizes or smooth captures on underpowered hardware. |
| `capturepause` | Client | Pauses the video capture, allowing you to avoid capturing uninteresting parts. This is a toggle, so reuse the same command to resume capturing again. |
| `capturestop` | Client | Stop the current video capture and close its output. |
| `cd` | Client | Control numbered CD/music tracks. Subcommands include `play`, `loop`, `stop`, `pause`, `resume`, `info`, and `remap`; physical CD support depends on the platform/backend. |
| `centerview` | Client | Return the player's view pitch toward its centered position. |
| `cfg_load` | Both | Execute console commands from a configuration file: `exec myconfig.cfg`. |
| `cfg_reset` | Both | Reset cvar values to their defaults by executing `cvarreset *`; does not reset bindings or aliases. |
| `cfg_save` | Both | Write settings, bindings, and aliases to a configuration file. No filename uses the normal config; an explicit name writes under `configs/`. |
| `cfg_save_ifmodified` | Both | Save the configuration only if an archived cvar has been modified. |
| `changelevel` | Both | Continues the game on a different map. The current map can be reentered later when a starting position for the new map is specified as a second argument. |
| `changing` | Client | Part of network protocols. This command should not be used manually. |
| `check_maps` | Both | Obsolete ktpro compatibility command; modern QuakeC can enumerate maps using `search_begin` instead. |
| `cinematic` | Client | Play a cinematic from `video/`, using the movie playback handler. |
| `cl_addport` | Client | Print the client's active network ports/addresses. Despite its name, this handler does not add a port. |
| `cl_cmd` | Client | Calls the csqc's GameCommand function |
| `cl_status` | Client | Print client-side network socket/connection information. |
| `cl_transfer` | Client | Connect to a different server, disconnecting from the current server only when the new server replies. |
| `cl_voip_mute` | Client | Tell the server to stop sending voice from a selected player to this client. |
| `cl_voip_unmute` | Client | Tell the server to resume sending voice from a selected player to this client. |
| `clear` | Client | Clear text from the current console. |
| `clientkick` | Both | Kick a server player by client slot number, rather than by name. |
| `closemenu` | Client | Close the active menu. |
| `cmd` | Client | Forward the remaining command text to the connected server. The server or mod determines which forwarded commands it accepts. |
| `cmdlist` | Both | List registered commands, optionally matching a wildcard: `cmdlist *demo*`. The number beside each name is its restriction level. |
| `color` | Client | Set the player's top and bottom clothing colors: `color <top> [bottom]`. |
| `colorize` | Client | Override the displayed clothing colors of a selected player; uses QuakeWorld teamplay color overrides. |
| `colourise` | Client | Override the displayed clothing colors of a selected player; uses QuakeWorld teamplay color overrides. |
| `com_reloadfilter` | Both | Reload the engine's message filtering/replacement rules from `filter.txt`. |
| `compile` | Both | Run the embedded QuakeC compiler on the mod's source; an optional source-list filename or explicit compiler arguments can be supplied. |
| `conactivate` | Client | Brings focus to the named console. Will not do anything if the named console is not created yet (so be sure to do any echos before using this command) |
| `conclear` | Client | Clears a named console (instead of just the main one) |
| `conclose` | Client | Destroys a named console |
| `condump` | Client | Write the console's text history to a file. |
| `conecho` | Client | Print text to a named console: `conecho <consolename> <text>`. |
| `conecho_center` | Client | Print centered text to a named console: `conecho_center <consolename> <text>`; an empty console name selects the main console. |
| `conmenu` | Client | Create a built-in scripted menu with `conmenu <callback>`; following menu-item commands populate it, and closing it invokes the callback with `cancel`. |
| `connect` | Client | Connect to a server: `connect [scheme://]address[:port]`. Default ports are QuakeWorld 27500, NetQuake 26000, Quake II 27910, and Quake III 27960. Supported transport schemes include `tcp://`, `tls://`, and `dtls://`; the server must support the chosen transport. |
| `connectbr` | Client | connect address:port Connect to a qw server using the best route we can detect. |
| `connectnq` | Client | Connects to the specified server, defaulting to port 26000. Also disables QW/Q2/Q3/DP handshakes preventing them from being favoured, so should only be used when you actually want NQ protocols specifically. |
| `connectq2e` | Client | Connects to the specified server, defaulting to port 5069. |
| `connectqe` | Client | Connects to the specified server, defaulting to port 26000. Also forces the use of DTLS and QE-specific handshakes. You will also need to ensure the dtls_psk_* cvars are set properly or the server will refuse the connection. |
| `connecttcp` | Client | Connect to a server using the tcp:// prefix |
| `coredump_csqc` | Client | Write a diagnostic state dump of the client QuakeC virtual machine. |
| `coredump_menuqc` | Client | Write a diagnostic state dump of the menu QuakeC virtual machine. |
| `coredump_ssqc` | Both | Write a diagnostic state dump of the server QuakeC virtual machine. |
| `cprint` | Client | Display text through the center-print mechanism. |
| `cripple` | Both | Block the player's ability to move. |
| `ctfscores` | Client | Apply Capture the Flag team-score updates sent by a server; a protocol helper rather than a user scoreboard toggle. |
| `cuff` | Both | Slap handcuffs on the player, preventing them from being able to attack. |
| `curl` | Client | Compatibility command for Xonotic URL-transfer requests; behavior depends on its download subcommands and web-client support. |
| `cvar_lockdefaults` | Both | Make current eligible cvar values the mod's reset defaults; skips read-only and cheat variables. |
| `cvar_purgedefaults` | Both | Resets all cvar defaults to back to the engine's default. Does not change their active value. |
| `cvar_resettodefaults_all` | Both | Resets all cvars to the mod's default value. |
| `cvar_resettodefaults_nosaveonly` | Both | Resets all unsaved cvars to the mod's default value. |
| `cvar_resettodefaults_saveonly` | Both | Resets all saved cvars to the mod's default value (without changing unsaved cvars). |
| `cvarlist` | Both | Lists all cvars. eg, 'cvarlist -cvd *' can be used to list all cvars with a value other than the mod's default. |
| `cvarreset` | Both | Resets the named cvar to its default value. |
| `cvarwatch` | Both | Prints a notification when the named cvar is changed. Also displays the start/end of configs. Alternatively, use '-watch foo' on the commandline. |

### D–H

| Command | Where | Definition |
| --- | --- | --- |
| `decompile` | Both | Decompile a QuakeC program file with the embedded decompiler; defaults to `qwprogs.dat` when no filename is supplied. |
| `defer` | Both | Issues the given command after a time delay. Disabled if ruleset_allow_in is 0. |
| `demo_jump` | Client | Jump to a specified time in a demo. Prefix with a + or - for a relative offset. Seeking backwards will restart the demo and the fast forward, which can take some time in long demos. |
| `demo_jump_end` | Client | Jump to the next intermission message. |
| `demo_jump_mark` | Client | Jump to the next '//demomark' marker. |
| `demo_nudge` | Client | Nudge the demo by one frame. Argument should be +1 or -1. Nudging backwards is limited. |
| `demo_setspeed` | Client | Set demo playback speed. |
| `demolist` | Both | List the hosted server's multiview-demo recordings; compatibility name for `sv_demolist`. |
| `demos` | Client | Starts playing the demo reel. |
| `devmap` | Both | Loads a map in developer mode (sv_cheats 1), for Quake III compat. |
| `df` | Client | Trigger the dark screen-color flash. |
| `dir` | Both | Displays filesystem listings. Accepts wildcards. |
| `disconnect` | Client | Disconnect from the current server and end the client session. |
| `dlsize` | Client | Process a server download-size/error/redirection response; an internal file-transfer helper, not a user download-size query. |
| `download` | Both | Client: request a file from the connected server, or download a supported URL. Dedicated server: download a supported URL to a local file with `download <url> [localname]`. |
| `download_map_url` | Both | Legacy map-download compatibility command; normal per-file map downloading handles transfers instead. |
| `dtls_importtrust` | Client | Import the saved server-certificate trust/pinning database, optionally from a specified file. |
| `dtls_untrustall` | Client | Clear remembered server-certificate trust/pinning entries. |
| `easyrecord` | Both | Start a hosted-server multiview-demo recording with an automatically generated name, or an optional explicit name; compatibility name for `sv_demoeasyrecord`. |
| `echo` | Both | Print text to the console: `echo "Hello"`. |
| `edit` | Client | Open the engine's built-in text editor for a file. |
| `envmap` | Client | Legacy name for the screenshot_cubemap command. |
| `exec` | Both | Execute console commands from a configuration file: `exec myconfig.cfg`. |
| `extensionlist_csqc` | Client | List the extensions exposed by the client QuakeC virtual machine. |
| `extensionlist_ssqc` | Both | List the extensions exposed by the server QuakeC virtual machine. |
| `filter` | Client | Configure QuakeWorld team-message filters using `#`-prefixed tags; no arguments lists filters, and `filter clear` clears them. |
| `find` | Both | Search commands and cvars for a substring in their name or help text: `apropos screenshot`. `find` is an alias. |
| `finishdl` | Client | Stop the current client download and treat it as completed, then proceed to the next queued file; intended for download troubleshooting. |
| `flocate` | Both | Searches for a named file, and displays where it can be found in the OS's filesystem |
| `floodprot` | Both | Inspect or configure server chat-flood protection: message count, interval, and silence duration. |
| `flush` | Both | Flush purgeable cached resources from the engine's memory cache. |
| `fly` | Client | Toggle flying movement as a server-authorized cheat; gamecode/protocol support is required. |
| `fog` | Client | Set the main view fog: `fog <density> <red> <green> <blue> <alpha> <depthbias>`. |
| `fps_preset` | Client | Apply a graphics/performance preset: `fps_preset <name>`. Menu names include `286`, `fast`, `spasm`, `vanilla`, `normal` (faithful), `nice`, and `realtime`; changes multiple settings together. |
| `fraglogfile` | Both | Toggle the server's legacy frag-log output. |
| `fs_changegame` | Both | Switch between different manifests (or registered games) |
| `fs_changemod` | Both | Provides the backend functionality of a transient online installer. Eg, for quaddicted's map/mod database. |
| `fs_flush` | Both | Flush the filesystem lookup cache so newly added or removed files can be discovered. |
| `fs_hash` | Both | Computes a hash of the specified file. |
| `fs_restart` | Both | Rebuild the engine's filesystem search paths and package state. |
| `fs_showmanifest` | Both | Print the active game's filesystem manifest. |
| `fullinfo` | Client | Set userinfo keys from a complete backslash-separated info string; principally a compatibility/protocol helper. |
| `gamedir` | Both | Change the current gamedir. |
| `gamemap` | Both | Load a map through the server map command's Quake II compatibility path. |
| `give` | Both | Give a player items, ammunition, or health as a server-authorized cheat. Arguments and supported items depend on the game; dedicated-server use also identifies the player. |
| `globalservers` | Client | Refresh the server browser and request Quake III master listings: `globalservers <master-number> <protocol> [keywords]`; master number 0 queries all configured masters. |
| `god` | Both | Toggle immunity to damage as a server-authorized cheat. The client forwards the request; dedicated-server use selects a player. |
| `gpu_batching` | Client | Compare optimized/reference submission: gpu_batching 1/0. Enabled by default; resets on video restart. Does not change graphical features. |
| `gpu_benchmark` | Client | Measure frame throughput and CPU stages: gpu_benchmark [seconds] [label]. Warm up first; disable vsync and raise cl_maxfps for throughput. |
| `gpu_info` | Client | Report SDL GPU device, frame geometry, material, and light counts. |
| `heartbeat` | Both | Sends an update or ping to the master server so the current server can remain listed. |
| `help` | Client | Open the built-in help menu. Use `apropos <text>` for command/cvar descriptions. |
| `hunkprint` | Both | Print hunk-memory allocation information. |

### I–M

| Command | Where | Definition |
| --- | --- | --- |
| `identify` | Both | Looks up a player's ip to see if they're using a different name |
| `if` | Both | Execute console commands conditionally using an expression and command body, with optional `else`; used in config scripting. |
| `ignore` | Client | Ignore chat from a named player. |
| `ignore_id` | Client | Ignore chat from a player identified by user ID. |
| `ignore_team` | Client | Ignore chat from members of a named team. |
| `ignorelist` | Client | List ignored players and teams. |
| `impulse` | Client | Sends an impulse number to the server (read: weapon change). |
| `in` | Both | Issues the given command after a time delay. Disabled if ruleset_allow_in is 0. |
| `in_bind` | Client | Create or inspect a binding in an alternate bind map: `in_bind <bindmap> <key> [command]`; uses the binding handler. |
| `in_deviceids` | Client | List detected input-device identifiers for device-specific bindings and split-screen configuration. |
| `in_restart` | Client | Restart the input subsystem and rediscover devices. |
| `inc` | Both | Adds a value to the named cvar. Use a negative value if you wish to decrease the cvar's value. |
| `invleft` | Client | Select the previous inventory item in games with engine-supported inventory controls, such as Hexen II. |
| `invnext` | Client | Select the next inventory item in games with engine-supported inventory controls, such as Hexen II. |
| `invprev` | Client | Select the previous inventory item in games with engine-supported inventory controls, such as Hexen II. |
| `invright` | Client | Select the next inventory item in games with engine-supported inventory controls, such as Hexen II. |
| `invuse` | Client | Use the selected inventory item in games with engine-supported inventory controls. |
| `ipdump` | Both | Merge existing entries and write remembered player-name/IP history to the specified IP-log file. |
| `ipmerge` | Both | Merge a player-name/IP history file into the engine's IP log. |
| `join` | Client | Switches away from spectator mode, optionally connecting to a different server. |
| `kick` | Both | Removes a player from the server, provide the name or IP of the desired player. |
| `kill` | Client | Send a request for the local player to commit suicide; the gamecode decides the result. |
| `killserver` | Both | Shut down the hosted server without exiting the engine. |
| `listconfigs` | Client | Displays a list of every installed config file. |
| `listfonts` | Client | Displays a list of every installed font. |
| `listip` | Both | List the addresses in the server's penalty/filter list, including penalties other than bans. |
| `listmaps` | Both | Displays a list of installed maps. |
| `listskins` | Client | Displays a list of every installed or downloaded QuakeWorld player skin. |
| `listskyboxes` | Client | Displays a list of available custom skyboxes that can be set with r_skybox, type or click an entry. |
| `load` | Both | Loads an existing saved game. |
| `loadfont` | Client | Load/register a font for scripted menus or HUD use; accepts a font identifier and font parameters. |
| `loadgame` | Both | Loads an existing saved game. |
| `loadloc` | Client | Load a `.loc` location-name file for QuakeWorld teamplay callouts. |
| `loadsky` | Client | Select a skybox through a compatibility command; prefer the `r_skybox` cvar. |
| `localinfo` | Both | Inspect or change the server's local key/value information, which is distinct from public serverinfo. |
| `localservers` | Client | Refresh server discovery, including a Quake III LAN broadcast query where supported. |
| `logfile` | Both | Toggle console logging to a file. |
| `logplayers` | Both | Toggle server player-event logging. |
| `logrcon` | Both | Toggle remote-console command/result logging. |
| `ls` | Both | Displays filesystem listings. Accepts wildcards. |
| `macrolist` | Both | Lists all available $macro expansions. |
| `map` | Both | Begins a new game on the specified map. |
| `map_restart` | Both | Restarts the server and reloads the map while flushing level cache, for general use and Quake III compat. |
| `mapcluster` | Client | Sets this server up as a cluster-server gateway. Additional processes will be used to host individual maps. If an argument is given then that will be the name of the map that new clients will initially be directed to. This can also be used for single-player to off-load nearly all server functions - use the 'ssv' command to direct each subserver. |
| `mapedit` | Both | Loads the named map without any gamecode active. |
| `maplist` | Both | Displays a list of installed maps. |
| `maps` | Both | Displays a list of installed maps. |
| `me` | Client | Send an action-style chat message using the player's name. |
| `media_add` | Client | Add a track to the media playlist and save `sound/media.m3u`. |
| `media_next` | Client | Advance to the next music/media playlist track. |
| `media_remove` | Client | Remove a track from the media playlist and save `sound/media.m3u`. |
| `menubind` | Client | Add a key-binding control to the current scripted menu: `menubind <x> <y> <caption> <bindcommand>`. |
| `menubox` | Client | Add a decorative box to the current scripted menu: `menubox <x> <y> <width> <height>`. |
| `menucallback` | Client | Explicitly invoke the active script menu's callback function with the given option set. |
| `menucheck` | Client | Add a cvar checkbox/bit toggle: `menucheck <x> <y> <caption> <cvarname> <bitmask>`. |
| `menuclear` | Client | Pop the currently scripted menu. |
| `menucomboi` | Client | Add a combo control storing a selected option index: `menucomboi <x> <y> <caption> <cvarname> <caption0> ...`. |
| `menucombos` | Client | Add a combo control storing a selected string value: `menucombos <x> <y> <caption> <cvarname> <caption0> <value0> ...`. |
| `menuedit` | Client | Add a cvar text-entry field: `menuedit <x> <y> <caption> <cvarname>`. |
| `menueditpriv` | Client | Add a private/password-style text-entry field to a scripted menu: `menueditpriv <x> <y> <caption> <def>`. |
| `menupic` | Client | Add an image to the current scripted menu: `menupic <x> <y> <picname>`. |
| `menupop` | Client | Pop the current menu from the menu stack. |
| `menuslider` | Client | Add a cvar slider: `menuslider <x> <y> <caption> <cvarname> <min> <max>`. |
| `menutext` | Client | Add a text menu option invoking a callback command: `menutext <x> <y> <caption> <cbcommand>`. |
| `menutextbig` | Client | Add a large-font text menu option: `menutextbig <x> <y> <caption> <cbcommand>`. |
| `messagemode` | Client | Start entering a chat message for everyone. |
| `messagemode2` | Client | Start entering a team-only chat message. |
| `mod` | Both | Pass the remaining command text to the server gamecode's console-command handler. Available subcommands are defined by the mod. |
| `mod_batchlist` | Client | List loaded brush-model render batches and their material/lightmap information. |
| `mod_bspx_injest` | Client | Ingests supplemental files (like .lits) into a new bspx file. REMEMBER TO BACK-UP FIRST! |
| `mod_bspx_list` | Client | Lists all lumps (and their sizes) in the specified bsp. |
| `mod_bspx_strip` | Client | Strips a named extension lump from a bsp file. |
| `mod_buildcubemaps` | Client | Automatically builds cubemaps for each env_cubemap present in the map. |
| `mod_findcubemaps` | Client | Scans the entities of a map to find reflection env_cubemap sites and determines the nearest one to each surface. |
| `mod_memlist` | Both | List loaded models and their memory usage. |
| `mod_realign` | Client | Reads the named bsp and writes it back out with only alignment changes. |
| `mod_showent` | Both | Allows you to quickly search through a map's entities. |
| `mod_terrain_convert` | Client | mod_terrain_convert [mapname] [texkill] Convert a terrain to the current format. If texkill is specified, only tiles with the named texture will be converted, and tiles with that texture will be stripped. This is a slow operation. |
| `mod_terrain_create` | Client | Create an editable terrain map through the built-in terrain subsystem. |
| `mod_terrain_reload` | Both | Reload terrain data from disk. |
| `mod_terrain_save` | Both | Save edited terrain data to disk. |
| `mod_texturelist` | Client | List textures associated with loaded models. |
| `mod_usetexture` | Client | Replace a named loaded brush texture with a solid RGB color for debugging: `mod_usetexture <texture> <red> <green> <blue>`. |
| `modelviewer` | Client | Open the model viewer to inspect a model and its animations/materials. |
| `msg_trigger` | Client | Define or inspect an alias-based trigger for incoming message text: `msg_trigger <name> "string" [-l <level>]`. |
| `music` | Client | Play a named music track, optionally with a separate looping track: `music <initial-track> [loop-track]`; requires a suitable audio decoder. |
| `music_fforward` | Client | Seek forward in the current media track by a number of seconds, defaulting to 15. |
| `music_next` | Client | Advance to the next music/media playlist track. |
| `music_rewind` | Client | Seek backward in the current media track by a number of seconds, defaulting to 15. |
| `mute` | Both | Mutes the player (no voice or chat), shaming them. |
| `mvdcancel` | Both | Cancel the active server multiview-demo recording and discard its output. |
| `mvdlist` | Both | List the server's multiview-demo recordings. |
| `mvdrecord` | Both | Start recording a server multiview demo. |
| `mvdstop` | Both | Finish the current server multiview-demo recording. |

### N–R

| Command | Where | Definition |
| --- | --- | --- |
| `net_ice_show` | Both | Print ICE session, candidate, and connectivity state for network diagnostics. |
| `newalias` | Both | Define a named console command sequence, or inspect aliases. Example: `alias greet "say Hello"`. `newalias` is a compatibility name for the same handler. |
| `nextul` | Client | Send the next chunk of an active client upload; an internal file-transfer helper. |
| `noclip` | Both | Toggle movement through walls as a server-authorized cheat. The client forwards the request; dedicated-server use selects a player. |
| `notarget` | Client | Toggle whether monsters target the player as a server-authorized cheat; gamecode support is required. |
| `observe` | Client | Switches to spectator mode, optionally connecting to a different server. |
| `openroute` | Both | Send an out-of-band `hello` packet to an address from the server socket, useful for establishing a network route/NAT mapping. |
| `p` | Client | Execute a command for a selected split-screen seat: `p <seat-number> <command>`. |
| `p1` | Client | Execute the remaining command for split-screen seat 1: `p1 <command>`; requires that seat to be active for normal gameplay. |
| `p2` | Client | Execute the remaining command for split-screen seat 2: `p2 <command>`; requires that seat to be active for normal gameplay. |
| `p3` | Client | Execute the remaining command for split-screen seat 3: `p3 <command>`; requires that seat to be active for normal gameplay. |
| `p4` | Client | Execute the remaining command for split-screen seat 4: `p4 <command>`; requires that seat to be active for normal gameplay. |
| `packet` | Client | Send an explicit connectionless packet to a specified address; a low-level network diagnostic command. |
| `path` | Both | prints a list of current search paths. |
| `pause` | Client | Request a pause/unpause of the current game, subject to server and game rules. |
| `pin_add` | Both | Add a persistent server notice: `pin_add "author" "message"`. |
| `pin_delete` | Both | Delete the oldest pinned server notice. |
| `pin_reload` | Both | Reload pinned server notices from disk. |
| `pin_save` | Both | Save pinned server notices to disk. |
| `pkg` | Both | Provides a way to install / list / disable / purge packages via the console. |
| `play` | Client | Play one or more local sound files without spatial attenuation. This fork uses the same playback behavior for both names; omitted extensions default to `.wav`. |
| `play2` | Client | Play one or more local sound files without spatial attenuation. This fork uses the same playback behavior for both names; omitted extensions default to `.wav`. |
| `playclip` | Client | Play a video inside an engine console window; requires a suitable video decoder. |
| `playdemo` | Client | Play a recorded demo file. |
| `playfilm` | Client | Play a video/cinematic, optionally followed by additional queued files; requires suitable video decoders. |
| `playvideo` | Client | Play a video/cinematic, optionally followed by additional queued files; requires suitable video decoders. |
| `playvol` | Client | Play a local sound at an explicit volume: `playvol <sound> <volume>`; omitted extensions default to `.wav`. |
| `plug_close` | Both | Unload a named engine plugin. |
| `plug_closeall` | Both | Unload all loaded engine plugins. |
| `plug_list` | Both | List loaded engine plugins. |
| `plug_load` | Both | Load a named engine plugin, if supported and available. |
| `pointfile` | Client | Load a map compiler's leak-point file and display its path in the level. |
| `poke_csqc` | Client | Inspect or evaluate a debug expression in the client QuakeC virtual machine. |
| `poke_menuqc` | Client | Inspect or evaluate a debug expression in the menu QuakeC virtual machine. |
| `poke_ssqc` | Both | Inspect or evaluate a debug expression in the server QuakeC virtual machine. |
| `pr_dumpplatform` | Both | Export the engine's QuakeC platform definitions for mod development; the graphical client implements the export, while a dedicated-only build reports that it is unavailable. |
| `precaches` | Both | Displays a list of current server precaches. |
| `profile_csqc` | Client | Print per-function client QuakeC profiling information; requires an active VM and enabled profiling. |
| `profile_menuqc` | Client | Print per-function menu QuakeC profiling information; requires an active VM and enabled profiling. |
| `profile_ssqc` | Both | Displays how much time has been spent in various QC functions since this command was last used. If pr_enable_profiling is set, profiling will be enabled automatically, and can be used to list spawn functions. Add an arg with value 1 if you wish to avoid purging timing information. |
| `qtvdemos` | Client | Request the demo list from a QTV proxy. |
| `qtvlist` | Client | Request the source-stream list from a QTV proxy. |
| `qtvplay` | Client | Connect to and play a QTV multiview-demo stream. |
| `qtvreverse` | Both | Establish a reverse QTV stream connection for multiview demo streaming. |
| `quickconnect` | Client | Use the built-in server browser's quick-connect selection. |
| `quit` | Both | Exit the engine. Use `cfg_save` first to explicitly save settings; quitting through the menu offers the normal confirmation flow. |
| `qwurl` | Client | Handle a QuakeWorld URL/connection request; compatibility with ezQuake URL handling. |
| `r_beaminfo` | Client | Print diagnostic information about scripted particle beams. |
| `r_converteffectinfo` | Client | Attempt to convert particle effects made for a certain other engine. |
| `r_dumpshaders` | Client | Export embedded upstream GPU program sources supported by the legacy shader registry. This handler does not export the SDL3 GPU renderer's shaders and may produce no files in this build. |
| `r_editlights_clone` | Client | Duplicate the current light (with a new origin) |
| `r_editlights_copyinfo` | Client | store a copy of all properties (except origin) of the selected light |
| `r_editlights_edit` | Client | Changes named properties on the current light. |
| `r_editlights_editall` | Client | Like r_editlights_edit, but affects all lights instead of just the selected one. |
| `r_editlights_lock` | Client | Blocks changing the current light according the crosshair. |
| `r_editlights_pasteinfo` | Client | apply the stored properties onto the selected light (making it exactly identical except for origin) |
| `r_editlights_reload` | Client | Reload static rtlights. Argument can be rtlights&#124;statics&#124;bsp&#124;none to override the source. |
| `r_editlights_remove` | Client | Removes the current light. |
| `r_editlights_save` | Client | Saves rtlights to maps/FOO.rtlights |
| `r_editlights_spawn` | Client | Spawn a new light with default properties |
| `r_editlights_togglecorona` | Client | Toggles the current light's corona field. |
| `r_editlights_toggleshadow` | Client | Toggles the shadow flag on the current light. |
| `r_effect` | Client | Associate a particle emitter effect with a model, or list associations; optional flags include `replace` and `forwards`. |
| `r_exportalleffects` | Client | Export loaded scripted particle-effect definitions to `particles/export.cfg`. |
| `r_exportbuiltinparticles` | Client | Export a named built-in particle set to `particles/<name>.cfg`, for example `r_exportbuiltinparticles faithful`. |
| `r_imageformats` | Client | Prints out a list of the usable hardware pixel formats. |
| `r_imagelist` | Client | Prints out a list of the currently-known textures. |
| `r_imagelist_wad` | Client | displays the available wad images. |
| `r_part` | Client | Define a named scripted particle effect using a following `{ ... }` block in a config file; also supports particle namespaces. |
| `r_partinfo` | Client | Print diagnostic information about scripted particle effects and allocations. |
| `r_partredirect` | Client | Inspect or change particle-effect aliases: `r_partredirect <from> <to>`; an empty target removes a mapping. |
| `r_remapshader` | Client | Remap one material/shader name to another. |
| `r_shaderlist` | Client | Prints out a list of the currently-loaded shaders. |
| `r_showshader` | Client | Print the definition and source filename of a named loaded material/shader: `r_showshader <name>`. |
| `r_trail` | Client | Associate a particle trail effect with a model, or list associations. |
| `rankadd` | Both | Create a persistent ranking/account entry: `rankadd <name> [numeric-password] [rights]`. |
| `rankfind` | Both | Find persistent ranking/account entries by a wildcard name. |
| `ranklist` | Both | Write the full persistent ranking list, with kills/deaths, to `list.txt` in the game directory. |
| `rankrconlevel` | Both | Change a ranking account's remote-console trust level; the caller cannot grant a level equal to or above their own. |
| `rankrefresh` | Both | Commit connected players' current statistics to the persistent ranking database. |
| `rankremove` | Both | Remove a ranking entry by its position in the ranking list. |
| `ranktopten` | Both | Print the leading ten entries in the persistent server rankings. |
| `rcon` | Client | Send a remote-console command to the selected server, using the configured RCON credentials. |
| `reconnect` | Client | Reconnect to the current or previously selected server. |
| `record` | Both | Client: start a client-perspective demo recording. Dedicated server: start a server multiview-demo recording. |
| `register_bestweapon` | Client | Register the weapon/impulse, ownership mask, ammunition requirements, and optional presentation data used for best-weapon selection; usually set by the mod's `default.cfg`. |
| `removeip` | Both | Removes an IP Address from the penalty list. |
| `renameclient` | Both | Change a hosted server player's name from the server console. |
| `rerecord` | Client | Reconnects to the previous/current server, but starts recording a clean demo. |
| `resetcfg` | Both | Reset cvar values to their defaults by executing `cvarreset *`; does not reset bindings or aliases. |
| `restrict` | Both | Inspect or change the permission/restriction level needed to execute a command or change a cvar. |
| `rmdemo` | Both | Delete a named server multiview-demo recording. |
| `rmdemonum` | Both | Delete a server multiview-demo recording by its numbered list entry. |
| `rotate` | Client | Queue a yaw rotation of the player's view by a specified number of degrees. |
| `route_reload` | Both | Clear cached waypoint/pathfinding networks in the client QuakeC and server worlds so they can be rebuilt. |
| `route_visualise` | Client | Calculate and visualize a waypoint/pathfinding route from the current view to coordinates: `route_visualise <x> <y> <z>`; requires a waypoint network. |

### S–Z

| Command | Where | Definition |
| --- | --- | --- |
| `save` | Both | Saves the game to the named location. |
| `saveconfig` | Both | Save configuration using the DarkPlaces-compatible output-path rules; an explicit filename is relative to the base game directory. |
| `savegame` | Both | Saves the game to the named location. |
| `savegame_legacy` | Both | Saves the game in a format compatible with vanilla Quake. Anything not supported by that format will be lost. |
| `say` | Both | Send chat to everyone on the server; dedicated-server use speaks from the server console. |
| `say_team` | Client | Send chat to teammates. |
| `sayone` | Both | Client: forward a private-chat request through the mod's chat handler. Dedicated server: send a console private message to an identified player. |
| `screenshot` | Client | Save a screenshot of the current view, optionally with a specified filename. |
| `screenshot_360` | Client | screenshot_360 &lt;name&gt; [width] [height] Takes an equirectangular screenshot. |
| `screenshot_cubemap` | Client | screenshot_cubemap &lt;name&gt; [size] Takes 6 screenshots forming a single cubemap. |
| `screenshot_mega` | Client | screenshot_mega &lt;name&gt; [width] [height] Takes a screenshot with explicit sizes that are not tied to the size of your monitor, allowing for true monstrosities. |
| `screenshot_stereo` | Client | screenshot_stereo &lt;name&gt; [width] [height] Takes a simple stereo screenshot. |
| `screenshot_vr` | Client | screenshot_vr &lt;name&gt; [width] Takes a spherical stereoscopic panorama image, for viewing with VR displays. |
| `sendcvar` | Client | Send a named cvar's permitted value to the server as a `sentcvar` message; protected variables are sent as empty values. |
| `serverinfo` | Both | Display the connected server's public key/value information. In a graphical listen server or dedicated server, inspect or set the hosted server's information: `serverinfo [key value]`. |
| `serverinfoblob` | Server | Set a hosted serverinfo key to binary data loaded from a file; a protocol/developer facility. |
| `set` | Both | Changes the current value of the named cvar, creating it if it doesn't yet exist. |
| `set_calc` | Both | Sets the named cvar to the result of a (complex) expression. |
| `set_tp` | Both | Identical to set. Included for compatibility with ezquake where ruleset restrictions on macros are on commands rather than cvars. |
| `seta` | Both | Changes the current value of the named cvar, creating it if it doesn't yet exist. Also forces the archive flag so that the cvar will always be written into any saved configs. |
| `seta_calc` | Both | Sets the named cvar to the result of a (complex) expression. Also forces the archive flag so that the cvar will always be written into any saved configs. |
| `setfl` | Both | Changes the current value of the named cvar, creating it if it doesn't yet exist. The third arg allows setting cvar flags and should be u, s, or a. This command should normally be used only inside default.cfg. |
| `setinfo` | Client | Inspect or set player userinfo: `setinfo <key> <value>`; no arguments prints the current userinfo. |
| `setinfoblob` | Client | Set a player userinfo key to binary data loaded from a file; uses the userinfo handler's blob path. |
| `setmaster` | Both | Configure the server's master-server endpoints for public listing. |
| `setpass` | Both | Change a persistent ranking/account's numeric password: `setpass <name> <newpass>`. |
| `setpos` | Client | Request that the player's position/view be set explicitly; a server-authorized cheat whose arguments depend on the protocol/game handler. |
| `setrenderer` | Client | Select/restart a video renderer. This fork's desktop GPU renderer is `sdlgpu`; removed upstream renderer names are unavailable. |
| `showalias` | Both | Print the body of a named alias. |
| `showpic` | Client | showpic &lt;imagename&gt; &lt;placename&gt; &lt;x&gt; &lt;y&gt; &lt;zone&gt; [width] [height] [touchcommand] Displays an image onscreen, that potentially has a key binding attached to it when clicked/touched. zone should be one of: TL, TR, BL, BR, MM, TM, BM, ML, MR. This serves as an extra offset to move the image around the screen without any foreknowledge of the screen resolution. |
| `showpic_removeall` | Client | removes any pictures inserted with the showpic command. |
| `sizedown` | Client | Decrease viewsize |
| `sizeup` | Client | Increase viewsize |
| `skel_generateragdoll` | Both | Generate a starting ragdoll definition from a skeletal model: `skel_generateragdoll <model>` writes `<model>.doll`, which requires further hand editing. |
| `skel_info` | Both | Print information about loaded skeletal-model objects. |
| `skins` | Client | Reload or refresh QuakeWorld player skins. |
| `skipdl` | Client | Abort the current download as failed and continue with the next queued file. |
| `sky` | Client | Select a skybox through a compatibility command; prefer the `r_skybox` cvar. |
| `skygroup` | Client | Provides a way to associate a skybox name with a series of maps, so that the requested skybox will override on a per-map basis. |
| `skyroomfog` | Client | Set skyroom fog: `skyroomfog <density> <red> <green> <blue> <alpha> <depthbias>`. |
| `snap` | Both | Ask an identified server client to upload a screenshot; delivery depends on that client's support and permissions. |
| `snapall` | Both | Ask all eligible server clients to upload screenshots. |
| `snd_restart` | Client | Restart audio devices and reload the sound subsystem. |
| `soundcontrol` | Client | Control the sound subsystem and individual output devices; supports disabling audio, changing sample rate, and speaker-layout options. |
| `soundinfo` | Client | Print audio-device and sound-system configuration information. |
| `soundlist` | Client | List known sound resources and their memory use. |
| `spdevmap` | Both | Loads a map in single-player developer mode (sv_cheats 1), for Quake III compat. |
| `spiderpig` | Client | Toggle wall-walking movement as a server-authorized cheat; requires compatible gamecode. |
| `spmap` | Both | Loads a map in single-player mode, for Quake III compat. |
| `sqlkill` | Both | Close a numbered engine SQL connection: `sqlkill <serverid>`. |
| `sqlkillall` | Both | Close all engine SQL connections. |
| `sqlstatus` | Both | List SQL connections, queued work, and database-driver availability; database features require their optional dependencies. |
| `ssv` | Client | Send a command to a selected cluster subserver. |
| `ssv_all` | Client | Send a command to every cluster subserver. |
| `startdemos` | Client | Sets the demoreel list, but does not start playing them (use the 'demos' command for that) |
| `status` | Both | Prints info about the current server. |
| `stealthmute` | Both | Mutes the player, without telling them, while pretending that their messages are still being broadcast. For use against people that would escalate on expiry or externally. |
| `stop` | Both | Client: stop demo recording (also handles hosted MVD recording). Dedicated server: finish server multiview-demo recording. |
| `stopdemo` | Client | Stop demo playback. |
| `stopmusic` | Client | Stop named music-track playback. |
| `stopsound` | Client | Stop currently playing sound effects. |
| `stopul` | Client | Stop an active client upload; an internal file-transfer helper. |
| `stuffcmd` | Both | Send console-command text to a hosted server client; execution is subject to the receiving client's restrictions. |
| `sv` | Both | Pass the remaining command text to the server gamecode's console-command handler. Available subcommands are defined by the mod. |
| `sv_addport` | Both | Add a hosted server network listening endpoint. |
| `sv_democancel` | Both | Cancel the active server multiview-demo recording and discard its output. |
| `sv_demoeasyrecord` | Both | Start a server multiview-demo recording with an automatically generated name. |
| `sv_demoinfo` | Both | Show information associated with a server demo recording. |
| `sv_demoinfoadd` | Both | Add descriptive information to a server demo recording. |
| `sv_demoinforemove` | Both | Remove descriptive information from a server demo recording. |
| `sv_demolist` | Both | List the server's multiview-demo recordings. |
| `sv_demonumremove` | Both | Delete a server multiview-demo recording by its numbered list entry. |
| `sv_demorecord` | Both | Start recording a server multiview demo. |
| `sv_demoremove` | Both | Delete a named server multiview-demo recording. |
| `sv_demostop` | Both | Finish the current server multiview-demo recording. |
| `sv_downloadchunksperframe` | Both | Legacy compatibility setting command; downloads are paced by the client download/rate settings instead. |
| `sv_enableprofile` | Both | Legacy compatibility command for an unimplemented server profiling setting; use the QuakeC profiling commands instead. |
| `sv_gamedir` | Both | Change the gamedir reported to clients, without changing any actual paths on the server. |
| `sv_impulse` | Both | Run a QuakeC impulse from the server console by creating a temporary console player; requires a free player slot and active QuakeC gamecode. |
| `sv_lightstyle` | Both | Overrides lightstyles from the server's console, mostly for debug use. |
| `sv_meminfo` | Both | Print server memory-use information. |
| `sv_progsname` | Both | Legacy compatibility command; use the `sv_progs` cvar to choose the server game program. |
| `sv_progtype` | Both | Legacy compatibility command; use the `sv_progs` cvar to choose the server game program. |
| `sv_saveentfile` | Both | Dumps a copy of the map's entities to disk, so that it can be edited and used as a replacement for slightly customised maps. |
| `sv_settimer` | Both | Schedule a server-console command: `sv_settimer <count> <interval-seconds> <command>`; count -1 repeats indefinitely, and `sv_settimer 0` cancels it. |
| `sv_speedcheck` | Both | Legacy compatibility setting command; the engine instead meters client movement time over time. |
| `sys_openfile` | Client | Select a file to open/install/etc. |
| `sys_select_timeout` | Both | Legacy compatibility setting command; server throttling follows the engine tick-rate settings. |
| `tcpconnect` | Client | Connect to a server using the tcp:// prefix |
| `tell` | Server | Send a server-console private message to an identified player: `tell <player> <message>`. |
| `timedemo` | Client | Play a demo as a performance benchmark and report timing/frame-rate results. |
| `toggle` | Both | Toggles a cvar between two values toggle CVARNAME [newval [altval]] |
| `toggleconsole` | Client | Open or close the console. |
| `togglemenu` | Client | Open or close the main menu. |
| `topten` | Client | Print the leading ten entries in the persistent server rankings. |
| `tp_pickup` | Client | Inspect or set the item categories that trigger QuakeWorld teamplay pickup handling. |
| `tp_point` | Client | Inspect or set the item/player categories recognized by QuakeWorld teamplay pointing macros. |
| `tp_took` | Client | Inspect or set the item categories recognized by QuakeWorld teamplay 'took' macros. |
| `track` | Client | Choose spectator tracking targets by user ID/name, or `off`; additional arguments can select targets for additional split-screen views. |
| `track1` | Client | Select a spectator tracking target for split-screen view 1: `track1 <userid-or-name-or-off>`. |
| `track2` | Client | Select a spectator tracking target for split-screen view 2: `track2 <userid-or-name-or-off>`. |
| `track3` | Client | Select a spectator tracking target for split-screen view 3: `track3 <userid-or-name-or-off>`. |
| `track4` | Client | Select a spectator tracking target for split-screen view 4: `track4 <userid-or-name-or-off>`. |
| `unban` | Both | Unbans or removes an IP Address from the penalty list, alias to removeip. |
| `unbind` | Client | Remove a key binding: `unbind <key>`. |
| `unbindall` | Client | Remove every key binding, including movement and console bindings; normally used when loading a complete default binding set. |
| `unignore` | Client | Stop ignoring chat from a named player. |
| `unignoreAll` | Client | Clear all ignored-player entries. |
| `unignoreAll_team` | Client | Clear all ignored-team entries. |
| `unignore_id` | Client | Stop ignoring a player identified by user ID. |
| `unignore_team` | Client | Stop ignoring members of a named team. |
| `unsavegame` | Both | Wipes an existing saved game from disk. |
| `user` | Both | Display userinfo for an identified player; client use queries the connected player list, and server use queries hosted players. |
| `users` | Client | List players and their user IDs. |
| `v_cshift` | Client | Set an explicit screen-color shift using red, green, blue, and intensity arguments; a compatibility/protocol effect. |
| `version` | Both | Reports engine revision and optional compile-time settings. |
| `version_modelformats` | Both | List the registered model-format loaders. |
| `vid_reload` | Client | Reload renderer resources through the renderer reload path. |
| `vid_restart` | Client | Restart the video/renderer subsystem and apply latched video settings. |
| `vid_toggle` | Client | Toggle windowed/fullscreen display mode. |
| `voip` | Client | Adjust the Speex voice preprocessor's automatic-gain maximum with `voip maxgain <value>` when Speex is available; use `+voip` for push-to-talk. |
| `vstr` | Both | Executes the string value of the cvar, much like if it were an alias. For compatibility with q3. |
| `wait` | Both | Pause execution of the current command buffer until a later frame. |
| `watchpoint` | Both | Set or clear a server QuakeC expression watchpoint used by the debugger. |
| `watchpoint_csqc` | Client | Set or clear a client QuakeC expression watchpoint used by the debugger. |
| `watchpoint_menuqc` | Client | Set or clear a menu QuakeC expression watchpoint used by the debugger. |
| `watchpoint_ssqc` | Both | Set or clear a server QuakeC expression watchpoint used by the debugger. |
| `waterfog` | Client | Set underwater fog: `waterfog <density> <red> <green> <blue> <alpha> <depthbias>`. |
| `wf` | Client | Trigger the white screen-color flash. |
| `which` | Both | Searches for a named file, and displays where it can be found in the OS's filesystem |
| `worker_status` | Both | Print background-worker/thread job information. |
| `worker_test` | Both | Queue a background-worker ping and report its completion latency. |
| `writeip` | Both | Write the server's IP penalty/filter configuration to disk. |

## Related documentation and source

See [SDL3 GPU renderer controls](SDL3-GPU.md) for graphics settings,
[Writing Your progs.dat](writing_progs.md) for the QuakeC API and gamecode,
and the [FTEQCC manual](FTEQCC.md) for compiler commands/options. QuakeC builtin
functions and FTEQCC command-line options are separate from engine console commands.

The command registry, permission handling, config commands, and help search
live in [cmd.c](../engine/common/cmd.c); cvar commands are in
[cvar.c](../engine/common/cvar.c). Client registration is in
[cl_main.c](../engine/client/cl_main.c), input actions in
[cl_input.c](../engine/client/cl_input.c), menu entry points in
[menu.c](../engine/client/menu.c), and server administration in
[sv_ccmds.c](../engine/server/sv_ccmds.c). Demo commands are implemented in
[cl_demo.c](../engine/client/cl_demo.c) and
[sv_mvd.c](../engine/server/sv_mvd.c). Additional subsystems register their
own commands during initialization.

To audit this reference after an engine update, collect `cmdlist` and
`apropos ""` from both executables after initialization, compare the names
with this table, and inspect each changed registration/handler. Use the
console of the build you are running as the authority for availability.
