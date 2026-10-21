# Console Commands and Cvars — Quake 1 and Quake 3

This is a combined reference for this fork's **Quake 1 (NetQuake/QuakeWorld)**
and **Quake 3/OpenArena** console interfaces. Shared names are listed once;
unmarked entries apply to both games. Game-specific entries and behavior
differences are marked in their definitions. **Q1 target** means the operation
needs Quake 1 protocols or QuakeC; **Q3** means it needs the Quake 3 plugin or
its cgame/qagame/UI modules. These labels describe useful behavior, not merely
whether a name happens to be present in the engine registry.

The inventory combines the macOS SDL3 GPU client and dedicated-server
registries with the command dispatchers and cvar lookups of **OpenArena 0.8.8**.
It includes every command/cvar captured after initializing Quake 1 and
OpenArena, plus gamecode commands and settings created/read on demand. It was
checked with `cmdlist`, `apropos ""`, `cvarlist -adF`, the fork's source and the
loaded QVMs on **13 October 2026**. Definitions use registered help where
available, otherwise the corresponding handlers/gamecode. The subsequent
`vid_revert` command and crisp filtering defaults are included below.

Quake 3 mods provide their own VMs, and Quake 1 mods can add commands through
QuakeC; there cannot be a fixed exhaustive list for every possible mod.
Quake 3 Arena/baseq3 and other mods may differ from the OpenArena entries
shown here. Build options and optional plugins can also change availability.
The experimental Jolt plugin adds the optional `physics_jolt_enabled` and
`physics_jolt_pointanchor2` settings listed below; they are not present until `plug_load jolt`. Client-side QuakeC can
also opt into an independent local Jolt world; this does not enable physics on a
remote server. `vid_renderer headless` now continues CSQC callbacks and local
physics without rendering or requiring a system tray; it is useful for automated
client tests. See [the CSQC physics guide](JoltPhysics.md#independent-csqc-physics).
The tables combine registered engine settings with on-demand gamecode
interfaces. User-created aliases and arbitrary user-created cvars are outside
the fixed inventory.

## Using the console

Open the console with its assigned key (normally backtick/tilde), or bind a key
to `toggleconsole`. Use Tab for completion. Quote arguments containing spaces;
a semicolon separates commands. `<argument>` is a placeholder and `[argument]`
is optional; do not type the brackets. File names normally refer to the active
game's virtual filesystem, including mounted PAK/PK3 packages.

**Commands perform actions; cvars store values.** Enter a cvar name alone to
inspect it, or follow it with a value to change it. Save archived settings with
`cfg_save`. Server-owned rules must be changed on the hosted server or through
authorized RCON; setting a same-named client cvar cannot override a remote server.

```text
menu_options
fov 120
r_fullbright 0
bind w +forward
bind F12 screenshot
exec myconfig.cfg
cfg_save
```

```text
cmdlist *demo*
cvarlist -adF
apropos lighting
find fov
```

`cmdlist` accepts a wildcard filter; its parenthesized number is a command
restriction level. `apropos`/`find` search command and cvar names/help.
`cvarlist -a` includes aliases, `-d` defaults and `-F` flags. Loaded gamecode
can handle names that do not appear in `cmdlist`; see the gamecode entries
below. `aliaslist` lists user-defined aliases separately.

Commands starting with `+` activate a held action; the matching `-` releases it.
Binding the `+` form sends the release automatically. Typing `+forward` manually
keeps moving until `-forward` is executed. The gamecode defines attack, use,
gesture and weapon behavior; shared input names do not imply identical physics.

## Availability and scope

| Where | Meaning |
| --- | --- |
| Client | Graphical-client command, or a client/gamecode request. The client also includes a listen server. |
| Server | Dedicated-server command. |
| Both | Present in both executables, or implemented by hosted qagame in both server forms; game-specific requirements still apply. |

Commands need the appropriate active connection, hosted game or VM. Optional
media, plugin and database commands need their corresponding libraries.
Cheats require server/ruleset authorization. Quake 1 tools for QuakeC, QTV/MVD,
classic teamplay macros and palette skins do not become Quake 3 features simply
because their names are registered. Other-game compatibility helpers retained
in the registry are identified by their definitions.

Quake 3 local rendering/HUD commands go to **cgame**, hosted server-game commands
to **qagame**, and bundled menu commands to **UI**. `cmd <text>` explicitly
forwards a request to the connected server; `sv <text>`/`mod <text>` explicitly
invoke local server gamecode. Prefer that prefix if an engine command or cvar
has the same name as a qagame command. On a remote server, use authorized
`rcon sv <text>` to reach its gamecode handler when appropriate.

## Common differences and quick tasks

| Task | Shared interface / game-specific behavior |
| --- | --- |
| Open this fork's settings | `menu_options`: native Options in either game. OpenArena's Escape menu belongs to its UI VM; it need not expose the native FOV slider. |
| Change field of view | `fov 120` or `cg_fov 120`: one saved setting. Native Options slider: 70–170, immediately below FOV Mode. Console values can exceed 170; Regular perspective renders up to 179. OpenArena zoom/server limits remain respected. |
| Use normal world lighting | `r_fullbright 0`: Quake 1 BSP lightmaps and Quake 3 baked lightmaps/vertex lighting. |
| Choose graphics preset | `fps_preset normal` (faithful) or `fps_preset vanilla`; presets can change multiple cvars in either game. |
| Start a local map | `map e1m1` for Quake 1, `map oa_dm1` for OpenArena, with the corresponding game data selected. |
| Connect/disconnect | `connect address:port`, `disconnect`, `reconnect`; protocol and assets must match the server. |
| Select weapons | Quake 1: impulses or priority-based engine `weapon`; Quake 3: cgame `weapon`, `weapnext`, `weapprev`. |
| Show the scoreboard | Quake 1: `+showscores`; Quake 3/OpenArena: `+scores`. |
| Choose rules/difficulty | Quake 1: QuakeC `deathmatch`, `coop`, `teamplay`, `skill`; Quake 3: `g_gametype` and qagame settings/bot skill. |
| Change movement rules | Quake 1: engine `sv_*` physics plus QuakeC; Quake 3: qagame `g_speed`, `g_gravity`, `pmove_*`. These are server/gamecode choices, not graphics controls. |
| Find a file | `path`, `which filename`, `dir pattern`. |
| Record/play a demo | `record mydemo`, `stop`, `playdemo mydemo`; protocol and gamecode/assets determine format and compatibility. |
| Inspect performance | `gpu_info`, `gpu_benchmark`, `timedemo`. |

## Command reference

Each command name appears once. Shared commands have no game label; **Q1 target**
means its documented operation needs Quake 1 protocols/gamecode, **Q3** means
it needs Quake 3 gamecode/plugin support, and **Different** explains a shared
name with different behavior. Availability in an executable is separate from
usefulness in the current game.

Jump to [input actions](#held-input-actions), [menus](#built-in-menu-entry-points),
[0–C](#0c), [D–H](#dh), [I–M](#im), [N–R](#nr), or [S–Z](#sz).

### Held input actions

| Command | Where | Definition / game notes |
| --- | --- | --- |
| `+acc` | Client | **Q3:** Show/hide OpenArena weapon-accuracy statistics while held. |
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
| `+fire` | Client | '+fire 8 7' will fire lg if you have it and fall back on rl if you don't, and just fire your current weapon if neither are held. Releasing fire will then switch away to exploit a bug in most mods to deny your weapon upgrades to your killer. **Q1 only:** priority-based weapon firing; Quake 3 uses `+attack` and its gamecode weapon selection. |
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
| `+scores` | Client | **Q3:** Show/hide the cgame scoreboard while held. Bind the `+` form to a key for automatic release. |
| `+showdm` | Client | Activate the scoreboard (Hexen II compatibility name) while held. |
| `+showinfo` | Client | Activate the extra Hexen II status information while held. |
| `+showscores` | Client | **Q1 target:** Activate the scoreboard while held. |
| `+showteamscores` | Client | **Q1 target:** Activate the team scoreboard while held. |
| `+speed` | Client | Activate speed-modifier input while held. |
| `+strafe` | Client | Activate strafe-modifier mode (turn keys control sideways movement) while held. |
| `+use` | Client | Activate use/interact input (game-dependent) while held. |
| `+voip` | Client | Activate voice capture for push-to-talk while held. |
| `+weaponwheel` | Client | **Q1 target:** Activate the weapon-selection wheel while held. |
| `+zoom` | Client | **Q3:** Start/end gamecode zoom, using `cg_zoomfov`; releasing zoom restores the shared `fov`/`cg_fov` setting. |
| `-acc` | Client | **Q3:** Show/hide OpenArena weapon-accuracy statistics while held. |
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
| `-fire` | Client | **Q1 only:** release priority-based firing started by `+fire`. |
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
| `-scores` | Client | **Q3:** Show/hide the cgame scoreboard while held. Bind the `+` form to a key for automatic release. |
| `-showdm` | Client | Release/deactivate the scoreboard (Hexen II compatibility name). |
| `-showinfo` | Client | Release/deactivate the extra Hexen II status information. |
| `-showscores` | Client | **Q1 target:** Release/deactivate the scoreboard. |
| `-showteamscores` | Client | **Q1 target:** Release/deactivate the team scoreboard. |
| `-speed` | Client | Release/deactivate speed-modifier input. |
| `-strafe` | Client | Release/deactivate strafe-modifier mode (turn keys control sideways movement). |
| `-use` | Client | Release/deactivate use/interact input (game-dependent). |
| `-voip` | Client | Release/deactivate voice capture for push-to-talk. |
| `-weaponwheel` | Client | **Q1 target:** Release/deactivate the weapon-selection wheel. |
| `-zoom` | Client | **Q3:** Start/end gamecode zoom, using `cg_zoomfov`; releasing zoom restores the shared `fov`/`cg_fov` setting. |

### Built-in menu entry points

| Command | Where | Definition / game notes |
| --- | --- | --- |
| `menu_audio` | Client | Open the built-in audio options menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_cmd` | Client | Pass command text to the menu QuakeC VM's `GameCommand` handler; subcommands are defined by the menu program. Requires a QuakeC menu program; Quake 3’s bundled UI VM is a separate interface. |
| `menu_credits` | Client | Open the built-in credits menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_demo` | Client | Open the built-in demo browser menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_download` | Client | Open the built-in package/download manager menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_fps` | Client | Open the built-in performance/graphics preset options menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_keys` | Client | Open the built-in key bindings menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_lighting` | Client | Open the built-in lighting options menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_load` | Client | **Q1 target:** Open the built-in load-game menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. Quake 3/OpenArena qagame does not implement this engine saved-game state format. |
| `menu_loadgame` | Client | **Q1 target:** Open the built-in load-game menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. Quake 3/OpenArena qagame does not implement this engine saved-game state format. |
| `menu_main` | Client | Open the built-in main menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_media` | Client | Open the built-in media playlist menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_mediafiles` | Client | Open the built-in media file browser menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_mods` | Client | Open the built-in game/mod selection menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_multi` | Client | Open the built-in multiplayer menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_network` | Client | Open the built-in network options menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_newmulti` | Client | Open the built-in multiplayer game setup/hosting menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_options` | Client | Open FTEQW’s native **Options** menu. In Quake 1 this is the engine Options page. In Quake 3/OpenArena it opens the same engine page even though Escape normally opens the game’s bundled UI. **Panini Resolution**, **Panini Filter** and **Panini Sharpening** are directly below **View Projection**. **Field of View** is directly below **FOV Mode**, with a 70–170 slider; use the console for wider values. It does not mean “open the OpenArena gamecode options page.” |
| `menu_particles` | Client | Open the built-in particle options menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_quit` | Client | Open the built-in quit confirmation menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_render` | Client | Open the built-in rendering options menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_restart` | Client | Restart the menu subsystem and reload the menu program/resources. Requires a QuakeC menu program; Quake 3’s bundled UI VM is a separate interface. |
| `menu_save` | Client | **Q1 target:** Open the built-in save-game menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. Quake 3/OpenArena qagame does not implement this engine saved-game state format. |
| `menu_servers` | Client | Open the built-in server browser menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_setup` | Client | Open the built-in player setup menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_single` | Client | Open the built-in single-player menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_slist` | Client | Open the built-in server browser menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_spcheats` | Client | Open the built-in single-player cheat options menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_speakers` | Client | Open the built-in speaker configuration menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_teamplay` | Client | **Q1 target:** Open the built-in QuakeWorld teamplay options menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_teamplay_ammo_health` | Client | **Q1 target:** Open the built-in teamplay ammo/health naming menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_teamplay_armor` | Client | **Q1 target:** Open the built-in teamplay armor naming menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_teamplay_items` | Client | **Q1 target:** Open the built-in teamplay item naming menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_teamplay_locations` | Client | **Q1 target:** Open the built-in teamplay location options menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_teamplay_needs` | Client | **Q1 target:** Open the built-in teamplay needs thresholds menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_teamplay_powerups` | Client | **Q1 target:** Open the built-in teamplay powerup naming menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_teamplay_status_location_misc` | Client | **Q1 target:** Open the built-in teamplay status/location/miscellaneous naming menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_teamplay_team_fortress` | Client | **Q1 target:** Open the built-in Team Fortress teamplay naming menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_teamplay_weapons` | Client | **Q1 target:** Open the built-in teamplay weapon naming menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_textures` | Client | Open the built-in texture options menu. In Quake 3 this is the engine menu, separate from the bundled gamecode UI. |
| `menu_video` | Client | Open the engine video/display menu in either game. Apply Settings starts a 15-second Keep/Revert test; Escape or timeout restores the previous display/interface values. F1 shows setting help. In Quake 3 this is separate from the bundled gamecode UI. |

### 0–C

| Command | Where | Definition / game notes |
| --- | --- | --- |
| `6dof` | Client | **Q1 target:** Toggle six-degree-of-freedom movement as a server-authorized cheat; requires compatible gamecode. OpenArena does not implement this Quake 1 cheat command; its position cheat is setviewpos. |
| `abort_podium` | Both | **Q3:** Skip/abort the single-player end-game podium sequence. |
| `acc` | Client | **Q3:** Request weapon-accuracy data from OpenArena qagame; `+acc` displays it. |
| `addbot` | Both | **Q3:** `addbot <bot-name> [skill] [team] [delay] [alternate-name]`: add a bot to a hosted game. Bot assets and bot-library support are required. |
| `addip` | Both | Add an address or address range to the server's IP penalty/filter list. **Different:** OpenArena qagame also has a command with this name; use `sv addip` to explicitly select its handler. Gamecode IP filters and the engine IP penalty list are separate. |
| `adduser` | Both | Create a persistent ranking/account entry: `rankadd <name> [numeric-password] [rights]`. |
| `alias` | Both | Define a named console command sequence, or inspect aliases. Example: `alias greet "say Hello"`. `newalias` is a compatibility name for the same handler. |
| `aliasedit` | Client | Put an existing alias definition into the console input line for editing. |
| `aliaslevel` | Both | Set or inspect the execution level used by a console alias. |
| `aliaslist` | Both | List defined console aliases, optionally filtered by name. |
| `allskins` | Client | **Q1 target:** Force QuakeWorld players to use a specified skin instead of their individual choices. |
| `applycompile` | Both | **Q1 target:** Reload server QuakeC after compilation while saving and restoring the active world's entity state; intended for development on a running server. |
| `apropos` | Both | Search commands and cvars for a substring in their name or help text: `apropos screenshot`. `find` is an alias. |
| `autotrack` | Client | Reset spectator automatic player tracking, allowing the tracking logic to choose a target again. |
| `ban` | Both | Block the player's IP, preventing them from connecting. Also kicks them. |
| `banlist` | Both | Displays a list of every banned player on the server. |
| `banname` | Both | Legacy compat, please use ban. |
| `bestweapon` | Client | Works like 'impulse', for compat with other engines. |
| `bf` | Client | Trigger the bonus/pickup screen-color flash. |
| `bind` | Client | Changes the action associated with each keyboard button. Use eg "bind ctrl+shift+alt+k kill" for special modifiers (should be used only after more basic modifiers). |
| `bindlevel` | Client | Create or inspect a key binding with an explicit execution level: `bindlevel <key> <level> [command]`. |
| `botlist` | Both | **Q3:** List bot definitions available to qagame. |
| `breakpoint` | Both | **Q1 target:** Toggle a server QuakeC breakpoint at a source filename and line number. |
| `breakpoint_csqc` | Client | **Q1 target:** Toggle a client QuakeC breakpoint at a source filename and line number. |
| `breakpoint_menuqc` | Client | Toggle a menu QuakeC breakpoint at a source filename and line number. Requires a QuakeC menu program; Quake 3’s bundled UI VM is a separate interface. |
| `callteamvote` | Client | **Q3:** Propose a team vote, for example a team-leader change; requires team membership and permission. |
| `callvote` | Client | **Q3:** `callvote <proposal> [arguments]`: propose a vote; permitted proposals and limits come from server gamecode. |
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
| `chat` | Both | **Q3:** Dedicated qagame console: send a chat-style announcement. |
| `check_maps` | Both | **Q1 target:** Obsolete ktpro compatibility command; modern QuakeC can enumerate maps using `search_begin` instead. |
| `cinematic` | Client | Play a cinematic from `video/`, using the movie playback handler. |
| `cl_addport` | Client | Print the client's active network ports/addresses. Despite its name, this handler does not add a port. |
| `cl_cmd` | Client | Calls the csqc's GameCommand function **Q1:** requires a loaded CSQC VM; this is not a command sent to Quake 3 cgame. |
| `cl_status` | Client | Print client-side network socket/connection information. |
| `cl_transfer` | Client | Connect to a different server, disconnecting from the current server only when the new server replies. |
| `cl_voip_mute` | Client | Tell the server to stop sending voice from a selected player to this client. |
| `cl_voip_unmute` | Client | Tell the server to resume sending voice from a selected player to this client. |
| `clear` | Client | Clear text from the current console. |
| `clientkick` | Both | Kick a server player by client slot number, rather than by name. |
| `clientkick_game` | Both | **Q3:** Kick a client using its qagame client-slot number. |
| `clients` | Client | **Q3:** Print client/gamecode information for connected players. |
| `closemenu` | Client | Close the active menu. |
| `cmd` | Client | Forward the remaining command text to the connected server. The server or mod determines which forwarded commands it accepts. |
| `cmdlist` | Both | List registered commands, optionally matching a wildcard: `cmdlist *demo*`. The number beside each name is its restriction level. |
| `color` | Client | **Q1 only:** set shirt/trouser palette colors, `color <top> [bottom]`. Quake 3 uses model skins and `color1`/`color2`; this engine handler is removed when using the Quake 3 protocol. |
| `colorize` | Client | **Q1 target:** Override the displayed clothing colors of a selected player; uses QuakeWorld teamplay color overrides. |
| `colourise` | Client | **Q1 target:** Override the displayed clothing colors of a selected player; uses QuakeWorld teamplay color overrides. |
| `com_reloadfilter` | Both | Reload the engine's message filtering/replacement rules from `filter.txt`. |
| `compile` | Both | **Q1 target:** Run the embedded QuakeC compiler on the mod's source; an optional source-list filename or explicit compiler arguments can be supplied. |
| `conactivate` | Client | Brings focus to the named console. Will not do anything if the named console is not created yet (so be sure to do any echos before using this command) |
| `conclear` | Client | Clears a named console (instead of just the main one) |
| `conclose` | Client | Destroys a named console |
| `condump` | Client | Write the console's text history to a file. |
| `conecho` | Client | Print text to a named console: `conecho <consolename> <text>`. |
| `conecho_center` | Client | Print centered text to a named console: `conecho_center <consolename> <text>`; an empty console name selects the main console. |
| `conmenu` | Client | Create a built-in scripted menu with `conmenu <callback>`; following menu-item commands populate it, and closing it invokes the callback with `cancel`. |
| `connect` | Client | **Q1 target:** Connect to a server: `connect [scheme://]address[:port]`. Default ports are QuakeWorld 27500, NetQuake 26000, Quake II 27910, and Quake III 27960. Supported transport schemes include `tcp://`, `tls://`, and `dtls://`; the server must support the chosen transport. |
| `connectbr` | Client | connect address:port Connect to a qw server using the best route we can detect. |
| `connectnq` | Client | Connects to the specified server, defaulting to port 26000. Also disables QW/Q2/Q3/DP handshakes preventing them from being favoured, so should only be used when you actually want NQ protocols specifically. |
| `connectq2e` | Client | Connects to the specified server, defaulting to port 5069. |
| `connectqe` | Client | Connects to the specified server, defaulting to port 26000. Also forces the use of DTLS and QE-specific handshakes. You will also need to ensure the dtls_psk_* cvars are set properly or the server will refuse the connection. |
| `connecttcp` | Client | Connect to a server using the tcp:// prefix |
| `coredump_csqc` | Client | **Q1 target:** Write a diagnostic state dump of the client QuakeC virtual machine. |
| `coredump_menuqc` | Client | Write a diagnostic state dump of the menu QuakeC virtual machine. Requires a QuakeC menu program; Quake 3’s bundled UI VM is a separate interface. |
| `coredump_ssqc` | Both | **Q1 target:** Write a diagnostic state dump of the server QuakeC virtual machine. |
| `cp` | Both | **Q3:** Dedicated qagame console: broadcast a center-screen message. |
| `cprint` | Client | Display text through the center-print mechanism. |
| `cripple` | Both | Block the player's ability to move. |
| `ctfscores` | Client | Apply Capture the Flag team-score updates sent by a server; a protocol helper rather than a user scoreboard toggle. |
| `cuff` | Both | Slap handcuffs on the player, preventing them from being able to attack. |
| `curl` | Client | Compatibility command for Xonotic URL-transfer requests; behavior depends on its download subcommands and native HTTP transfer support. |
| `cvar_lockdefaults` | Both | Make current eligible cvar values the mod's reset defaults; skips read-only and cheat variables. |
| `cvar_purgedefaults` | Both | Resets all cvar defaults to back to the engine's default. Does not change their active value. |
| `cvar_resettodefaults_all` | Both | Resets all cvars to the mod's default value. |
| `cvar_resettodefaults_nosaveonly` | Both | Resets all unsaved cvars to the mod's default value. |
| `cvar_resettodefaults_saveonly` | Both | Resets all saved cvars to the mod's default value (without changing unsaved cvars). |
| `cvarlist` | Both | Lists all cvars. eg, 'cvarlist -cvd *' can be used to list all cvars with a value other than the mod's default. |
| `cvarreset` | Both | Resets the named cvar to its default value. |
| `cvarwatch` | Both | Prints a notification when the named cvar is changed. Also displays the start/end of configs. Alternatively, use '-watch foo' on the commandline. |

### D–H

| Command | Where | Definition / game notes |
| --- | --- | --- |
| `decompile` | Both | **Q1 target:** Decompile a QuakeC program file with the embedded decompiler; defaults to `qwprogs.dat` when no filename is supplied. |
| `defer` | Both | Issues the given command after a time delay. Disabled if ruleset_allow_in is 0. |
| `demo_jump` | Client | Jump to a specified time in a demo. Prefix with a + or - for a relative offset. Seeking backwards will restart the demo and the fast forward, which can take some time in long demos. |
| `demo_jump_end` | Client | Jump to the next intermission message. |
| `demo_jump_mark` | Client | Jump to the next '//demomark' marker. |
| `demo_nudge` | Client | Nudge the demo by one frame. Argument should be +1 or -1. Nudging backwards is limited. |
| `demo_setspeed` | Client | **Q1 target:** Set demo playback speed. |
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
| `dumpuser` | Both | **Q3:** Print a client’s userinfo through the hosted qagame console. |
| `easyrecord` | Both | Start a hosted-server multiview-demo recording with an automatically generated name, or an optional explicit name; compatibility name for `sv_demoeasyrecord`. |
| `echo` | Both | Print text to the console: `echo "Hello"`. |
| `edit` | Client | Open the engine's built-in text editor for a file. |
| `eject` | Both | **Q3:** Eject a client from the hosted OpenArena server using the gamecode handler. |
| `endgamenow` | Both | **Q3:** End the current match immediately through qagame. |
| `entityList` | Both | **Q3:** Print the qagame entity list; run on the hosted server, optionally through `sv entityList`. |
| `envmap` | Client | Legacy name for the screenshot_cubemap command. |
| `exec` | Both | Execute console commands from a configuration file: `exec myconfig.cfg`. |
| `extensionlist_csqc` | Client | **Q1 target:** List the extensions exposed by the client QuakeC virtual machine. |
| `extensionlist_ssqc` | Both | **Q1 target:** List the extensions exposed by the server QuakeC virtual machine. |
| `filter` | Client | **Q1 target:** Configure QuakeWorld team-message filters using `#`-prefixed tags; no arguments lists filters, and `filter clear` clears them. |
| `find` | Both | Search commands and cvars for a substring in their name or help text: `apropos screenshot`. `find` is an alias. |
| `finishdl` | Client | Stop the current client download and treat it as completed, then proceed to the next queued file; intended for download troubleshooting. |
| `flocate` | Both | Searches for a named file, and displays where it can be found in the OS's filesystem |
| `floodprot` | Both | Inspect or configure server chat-flood protection: message count, interval, and silence duration. |
| `flush` | Both | Flush purgeable cached resources from the engine's memory cache. |
| `fly` | Client | **Q1 target:** Toggle flying movement as a server-authorized cheat; gamecode/protocol support is required. OpenArena does not implement this Quake 1 cheat command; its position cheat is setviewpos. |
| `fog` | Client | **Q1 target:** Set the main view fog: `fog <density> <red> <green> <blue> <alpha> <depthbias>`. |
| `follow` | Client | **Q3:** `follow <client-number\|name>`: spectate a selected player; no argument can stop following. |
| `follownext` | Client | **Q3:** Cycle spectator following to the next/previous eligible player. |
| `followprev` | Client | **Q3:** Cycle spectator following to the next/previous eligible player. |
| `forceTeam` | Both | **Q3:** Force a client onto a team from the server gamecode console. |
| `fps_preset` | Client | Apply a graphics/performance preset: `fps_preset <name>`. Menu names include `286`, `fast`, `spasm`, `vanilla`, `normal` (faithful), `nice`, and `realtime`; changes multiple settings together. |
| `fraglogfile` | Both | Toggle the server's legacy frag-log output. |
| `freespectator` | Client | **Q3:** Leave player-following mode and return to a free spectator camera. |
| `fs_changegame` | Both | Switch between different manifests (or registered games) |
| `fs_changemod` | Both | Provides the backend functionality of a transient online installer. Eg, for quaddicted's map/mod database. |
| `fs_flush` | Both | Flush the filesystem lookup cache so newly added or removed files can be discovered. |
| `fs_hash` | Both | Computes a hash of the specified file. |
| `fs_restart` | Both | Rebuild the engine's filesystem search paths and package state. |
| `fs_showmanifest` | Both | Print the active game's filesystem manifest. |
| `fullinfo` | Client | Set userinfo keys from a complete backslash-separated info string; principally a compatibility/protocol helper. |
| `game_memory` | Both | **Q3:** Report qagame memory usage. |
| `gamedir` | Both | Change the current gamedir. |
| `gamemap` | Both | Load a map through the server map command's Quake II compatibility path. |
| `gc` | Client | **Q3:** Send a numbered canned game/team message to a target client, as implemented by qagame. |
| `getmappage` | Client | **Q3:** Request a numbered page of map names from OpenArena qagame for the UI. |
| `give` | Both | Request items/ammunition/health as a server-authorized cheat. **Different syntax:** Quake 1 uses traditional item letters/counts or mod-defined requests; OpenArena supports requests such as `give all`, `give health`, `give weapons`, `give ammo`, `give armor`, or a pickup name. |
| `globalservers` | Client | Refresh the server browser and request Quake III master listings: `globalservers <master-number> <protocol> [keywords]`; master number 0 queries all configured masters. |
| `god` | Both | Toggle immunity to damage as a server-authorized cheat. The client forwards the request; dedicated-server use selects a player. |
| `gpu_batching` | Client | Compare optimized/reference submission: gpu_batching 1/0. Enabled by default; resets on video restart. Does not change graphical features. |
| `gpu_benchmark` | Client | Measure frame throughput and CPU stages: gpu_benchmark [seconds] [label]. Warm up first; disable vsync and raise cl_maxfps for throughput. |
| `gpu_info` | Client | Report SDL GPU device, frame geometry, material and light counts, plus actual Panini source/output dimensions, filter and sharpening. |
| `heartbeat` | Both | Sends an update or ping to the master server so the current server can remain listed. |
| `help` | Client | Open the built-in help menu. Use `apropos <text>` for command/cvar descriptions. |
| `hunkprint` | Both | Print hunk-memory allocation information. |

### I–M

| Command | Where | Definition / game notes |
| --- | --- | --- |
| `iamacheater` | Client | **Q3:** Unlock the single-player arenas in the gamecode’s saved progress. |
| `iamamonkey` | Client | **Q3:** Unlock single-player medals/awards in saved progress. |
| `identify` | Both | Looks up a player's ip to see if they're using a different name |
| `if` | Both | Execute console commands conditionally using an expression and command body, with optional `else`; used in config scripting. |
| `ignore` | Client | Ignore chat from a named player. |
| `ignore_id` | Client | Ignore chat from a player identified by user ID. |
| `ignore_team` | Client | Ignore chat from members of a named team. |
| `ignorelist` | Client | List ignored players and teams. |
| `impulse` | Client | Sends an impulse number to the server (read: weapon change). **Q1:** QuakeC interprets the numeric impulse. Quake 3/OpenArena uses weapon/gamecode commands instead; arbitrary Quake 1 impulses have no equivalent effect. |
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
| `join` | Client | **Q1 target:** Switches away from spectator mode, optionally connecting to a different server. Quake 3/OpenArena uses `cmd team free` or `cmd team spectator` instead. |
| `kick` | Both | Removes a player from the server, provide the name or IP of the desired player. |
| `kill` | Client | Send a request for the local player to commit suicide; the gamecode decides the result. |
| `killserver` | Both | Shut down the hosted server without exiting the engine. |
| `levelselect` | Client | **Q3:** Open the gamecode single-player level-selection UI. |
| `levelshot` | Client | **Q3:** Generate the gamecode level-preview screenshot; requires authorized cheats/local development. |
| `listconfigs` | Client | Displays a list of every installed config file. |
| `listfonts` | Client | Displays a list of every installed font. |
| `listip` | Both | List the addresses in the server's penalty/filter list, including penalties other than bans. **Different:** OpenArena qagame also has a command with this name; use `sv listip` to explicitly select its handler. Gamecode IP filters and the engine IP penalty list are separate. |
| `listmaps` | Both | Displays a list of installed maps. |
| `listskins` | Client | **Q1 target:** Displays a list of every installed or downloaded QuakeWorld player skin. |
| `listskyboxes` | Client | Displays a list of available custom skyboxes that can be set with r_skybox, type or click an entry. |
| `load` | Both | **Q1 target:** Loads an existing saved game. Jolt entity checkpoints require the matching enabled plugin, point-anchor mode and collision assets; malformed/mismatched physics state rejects loading. See [entity saves](JoltPhysics.md#saving-entity-scenes). Quake 3/OpenArena qagame does not implement this engine saved-game state format. |
| `loaddeferred` | Client | **Q3:** Load player models whose loading was deferred by cgame. |
| `loadfont` | Client | Load/register a font for scripted menus or HUD use; accepts a font identifier and font parameters. |
| `loadgame` | Both | **Q1 target:** Loads an existing saved game. Quake 3/OpenArena qagame does not implement this engine saved-game state format. |
| `loadloc` | Client | **Q1 target:** Load a `.loc` location-name file for QuakeWorld teamplay callouts. |
| `loadsky` | Client | Select a skybox through a compatibility command; prefer the `r_skybox` cvar. |
| `localinfo` | Both | Inspect or change the server's local key/value information, which is distinct from public serverinfo. |
| `localservers` | Client | Refresh server discovery, including a Quake III LAN broadcast query where supported. |
| `logfile` | Both | Toggle console logging to a file. Quake 3 bot setup also queries a cvar spelled logfile, but this name belongs to an engine command here; it is not a separate working bot-log cvar. Use the documented engine/gamecode logging controls. |
| `logplayers` | Both | Toggle server player-event logging. |
| `logrcon` | Both | Toggle remote-console command/result logging. |
| `ls` | Both | Displays filesystem listings. Accepts wildcards. |
| `macrolist` | Both | Lists all available $macro expansions. |
| `map` | Both | Load a map on the local server: `map <mapname>`. **Different:** Quake 1 initializes QuakeC; Quake 3 requires the Quake 3 plugin, qagame and Quake 3 assets. `map e1m1` and `map oa_dm1` therefore need their respective game directories. |
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
| `mod` | Both | Pass the remaining arguments to the active server gamecode’s console-command handler. **Different:** Quake 1 uses QuakeC `ConsoleCmd`; Quake 3 uses qagame `GAME_CONSOLE_COMMAND`. Useful when an engine name shadows a gamecode command, for example `sv addip <address>` for OpenArena’s gamecode IP filters. |
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
| `mvdcancel` | Both | **Q1 target:** Cancel the active server multiview-demo recording and discard its output. |
| `mvdlist` | Both | **Q1 target:** List the server's multiview-demo recordings. |
| `mvdrecord` | Both | **Q1 target:** Start recording a server multiview demo. |
| `mvdstop` | Both | **Q1 target:** Finish the current server multiview-demo recording. |

### N–R

| Command | Where | Definition / game notes |
| --- | --- | --- |
| `net_ice_show` | Both | Print ICE session, candidate, and connectivity state for network diagnostics. |
| `newalias` | Both | Define a named console command sequence, or inspect aliases. Example: `alias greet "say Hello"`. `newalias` is a compatibility name for the same handler. |
| `nextframe` | Client | **Q3:** Advance/rewind the developer test model animation frame. |
| `nextskin` | Client | **Q3:** Advance/rewind the developer test model skin. |
| `nextul` | Client | Send the next chunk of an active client upload; an internal file-transfer helper. |
| `noclip` | Both | Toggle movement through walls as a server-authorized cheat. The client forwards the request; dedicated-server use selects a player. |
| `notarget` | Client | Toggle whether monsters target the player as a server-authorized cheat; gamecode support is required. |
| `observe` | Client | **Q1 target:** Switches to spectator mode, optionally connecting to a different server. Quake 3/OpenArena uses `cmd team free` or `cmd team spectator` instead. |
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
| `pkg` | Both | Install / list / disable / purge packages via the console. See [package trust rules](PackageTrust.md) for signatures and unsigned data downloads. |
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
| `poke_csqc` | Client | **Q1 target:** Inspect or evaluate a debug expression in the client QuakeC virtual machine. |
| `poke_menuqc` | Client | Inspect or evaluate a debug expression in the menu QuakeC virtual machine. Requires a QuakeC menu program; Quake 3’s bundled UI VM is a separate interface. |
| `poke_ssqc` | Both | **Q1 target:** Inspect or evaluate a debug expression in the server QuakeC virtual machine. |
| `postgame` | Client | **Q3:** Open the gamecode single-player results UI; arguments normally come from gamecode. |
| `pr_dumpplatform` | Both | **Q1 target:** Export the engine's QuakeC platform definitions for mod development; the graphical client implements the export, while a dedicated-only build reports that it is unavailable. |
| `precaches` | Both | Displays a list of current server precaches. |
| `prevframe` | Client | **Q3:** Advance/rewind the developer test model animation frame. |
| `prevskin` | Client | **Q3:** Advance/rewind the developer test model skin. |
| `profile_csqc` | Client | **Q1 target:** Print per-function client QuakeC profiling information; requires an active VM and enabled profiling. |
| `profile_menuqc` | Client | Print per-function menu QuakeC profiling information; requires an active VM and enabled profiling. Requires a QuakeC menu program; Quake 3’s bundled UI VM is a separate interface. |
| `profile_ssqc` | Both | Displays how much time has been spent in various QC functions since this command was last used. If pr_enable_profiling is set, profiling will be enabled automatically, and can be used to list spawn functions. Add an arg with value 1 if you wish to avoid purging timing information. |
| `qtvdemos` | Client | **Q1 target:** Request the demo list from a QTV proxy. |
| `qtvlist` | Client | **Q1 target:** Request the source-stream list from a QTV proxy. |
| `qtvplay` | Client | **Q1 target:** Connect to and play a QTV multiview-demo stream. |
| `qtvreverse` | Both | **Q1 target:** Establish a reverse QTV stream connection for multiview demo streaming. |
| `quickconnect` | Client | Use the built-in server browser's quick-connect selection. |
| `quit` | Both | Exit the engine. Use `cfg_save` first to explicitly save settings; quitting through the menu offers the normal confirmation flow. |
| `qwurl` | Client | **Q1 target:** Handle a QuakeWorld URL/connection request; compatibility with ezQuake URL handling. |
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
| `removeip` | Both | Removes an IP Address from the penalty list. **Different:** OpenArena qagame also has a command with this name; use `sv removeip` to explicitly select its handler. Gamecode IP filters and the engine IP penalty list are separate. |
| `renameclient` | Both | Change a hosted server player's name from the server console. |
| `rerecord` | Client | Reconnects to the previous/current server, but starts recording a clean demo. |
| `resetcfg` | Both | Reset cvar values to their defaults by executing `cvarreset *`; does not reset bindings or aliases. |
| `restrict` | Both | Inspect or change the permission/restriction level needed to execute a command or change a cvar. |
| `rmdemo` | Both | **Q1 target:** Delete a named server multiview-demo recording. |
| `rmdemonum` | Both | **Q1 target:** Delete a server multiview-demo recording by its numbered list entry. |
| `rotate` | Client | Queue a yaw rotation of the player's view by a specified number of degrees. |
| `route_reload` | Both | **Q1 target:** Clear cached waypoint/pathfinding networks in the client QuakeC and server worlds so they can be rebuilt. |
| `route_visualise` | Client | Calculate and visualize a waypoint/pathfinding route from the current view to coordinates: `route_visualise <x> <y> <z>`; requires a waypoint network. |

### S–Z

| Command | Where | Definition / game notes |
| --- | --- | --- |
| `save` | Both | **Q1 target:** Saves the game to the named location. Native FTE and extended legacy saves retain Jolt props, live rigs, skeleton handles/poses and animation controls; invalid/unbound skeletal resources or pending physics work refuse ordinary saves. See [entity saves](JoltPhysics.md#saving-entity-scenes). Quake 3/OpenArena qagame does not implement this engine saved-game state format. |
| `saveconfig` | Both | Save configuration using the DarkPlaces-compatible output-path rules; an explicit filename is relative to the base game directory. |
| `savegame` | Both | **Q1 target:** Saves the game to the named location. Quake 3/OpenArena qagame does not implement this engine saved-game state format. |
| `savegame_legacy` | Both | **Q1 target:** Saves the game in a format compatible with vanilla Quake. FTE extensions retain Jolt prop and skeletal checkpoints when loaded by this engine; vanilla engines do not restore that physics state. Invalid/unbound skeletal resources refuse saving. Other unsupported state may be lost. Quake 3/OpenArena qagame does not implement this engine saved-game state format. |
| `say` | Both | Send global chat. **Different:** Quake 1 uses the engine’s chat formatting/macros; Quake 3 forwards the message to qagame, which applies its chat rules. |
| `say_team` | Client | Send team chat. **Different:** Quake 1 can expand engine teamplay macros; Quake 3 qagame routes team chat according to actual team membership. In a dedicated OpenArena server, qagame also supports team-directed announcements. |
| `sayone` | Both | Client: forward a private-chat request through the mod's chat handler. Dedicated server: send a console private message to an identified player. |
| `score` | Client | **Q3:** Request the scoreboard data from qagame; `+scores` displays the cgame scoreboard. |
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
| `setpos` | Client | **Q1 target:** Request that the player's position/view be set explicitly; a server-authorized cheat whose arguments depend on the protocol/game handler. OpenArena does not implement this Quake 1 cheat command; its position cheat is setviewpos. |
| `setrenderer` | Client | Select/restart a video renderer. This fork's desktop GPU renderer is `sdlgpu`; removed upstream renderer names are unavailable. |
| `setviewpos` | Client | **Q3:** `setviewpos <x> <y> <z> <yaw>`: move the player/view for development; requires server-authorized cheats. |
| `showalias` | Both | Print the body of a named alias. |
| `showpic` | Client | showpic &lt;imagename&gt; &lt;placename&gt; &lt;x&gt; &lt;y&gt; &lt;zone&gt; [width] [height] [touchcommand] Displays an image onscreen, that potentially has a key binding attached to it when clicked/touched. zone should be one of: TL, TR, BL, BR, MM, TM, BM, ML, MR. This serves as an extra offset to move the image around the screen without any foreknowledge of the screen resolution. |
| `showpic_removeall` | Client | removes any pictures inserted with the showpic command. |
| `sizedown` | Client | **Different:** increase/decrease the visible view size. Quake 1 changes `viewsize` and the native status-bar layout; Quake 3/OpenArena changes `cg_viewsize` through cgame. |
| `sizeup` | Client | **Different:** increase/decrease the visible view size. Quake 1 changes `viewsize` and the native status-bar layout; Quake 3/OpenArena changes `cg_viewsize` through cgame. |
| `skel_generateragdoll` | Both | Generate a starting ragdoll definition from a skeletal model: `skel_generateragdoll <model>` writes `<model>.doll`, which requires further hand editing. |
| `skel_info` | Both | Print information about loaded skeletal-model objects. |
| `skins` | Client | **Q1 target:** Reload or refresh QuakeWorld player skins. |
| `skipdl` | Client | Abort the current download as failed and continue with the next queued file. |
| `sky` | Client | Select a skybox through a compatibility command; prefer the `r_skybox` cvar. |
| `skygroup` | Client | Provides a way to associate a skybox name with a series of maps, so that the requested skybox will override on a per-map basis. |
| `skyroomfog` | Client | **Q1 target:** Set skyroom fog: `skyroomfog <density> <red> <green> <blue> <alpha> <depthbias>`. |
| `snap` | Both | Ask an identified server client to upload a screenshot; delivery depends on that client's support and permissions. |
| `snapall` | Both | Ask all eligible server clients to upload screenshots. |
| `snd_restart` | Client | Restart audio devices and reload the sound subsystem. |
| `soundcontrol` | Client | Control the sound subsystem and individual output devices; supports disabling audio, changing sample rate, and speaker-layout options. |
| `soundinfo` | Client | Print audio-device and sound-system configuration information. |
| `soundlist` | Client | List known sound resources and their memory use. |
| `spdevmap` | Both | Loads a map in single-player developer mode (sv_cheats 1), for Quake III compat. |
| `spiderpig` | Client | **Q1 target:** Toggle wall-walking movement as a server-authorized cheat; requires compatible gamecode. OpenArena does not implement this Quake 1 cheat command; its position cheat is setviewpos. |
| `spmap` | Both | Loads a map in single-player mode, for Quake III compat. |
| `sqlkill` | Both | Close a numbered engine SQL connection: `sqlkill <serverid>`. |
| `sqlkillall` | Both | Close all engine SQL connections. |
| `sqlstatus` | Both | List SQL connections, queued work, and database-driver availability; database features require their optional dependencies. |
| `ssv` | Client | Send a command to a selected cluster subserver. |
| `ssv_all` | Client | Send a command to every cluster subserver. |
| `startdemos` | Client | Sets the demoreel list, but does not start playing them (use the 'demos' command for that) |
| `startOrbit` | Client | **Q3:** Toggle the developer orbit camera; requires developer mode. |
| `status` | Both | Prints info about the current server. **Q3:** `sv status` explicitly selects the hosted OpenArena qagame status report rather than the engine report. |
| `stealthmute` | Both | Mutes the player, without telling them, while pretending that their messages are still being broadcast. For use against people that would escalate on expiry or externally. |
| `stop` | Both | Client: stop demo recording (also handles hosted MVD recording). Dedicated server: finish server multiview-demo recording. |
| `stopdemo` | Client | Stop demo playback. |
| `stopmusic` | Client | Stop named music-track playback. |
| `stopsound` | Client | Stop currently playing sound effects. |
| `stopul` | Client | Stop an active client upload; an internal file-transfer helper. |
| `stuffcmd` | Both | Send console-command text to a hosted server client; execution is subject to the receiving client's restrictions. |
| `sv` | Both | Pass the remaining arguments to the active server gamecode’s console-command handler. **Different:** Quake 1 uses QuakeC `ConsoleCmd`; Quake 3 uses qagame `GAME_CONSOLE_COMMAND`. Useful when an engine name shadows a gamecode command, for example `sv addip <address>` for OpenArena’s gamecode IP filters. |
| `sv_addport` | Both | Add a hosted server network listening endpoint. |
| `sv_democancel` | Both | **Q1 target:** Cancel the active server multiview-demo recording and discard its output. |
| `sv_demoeasyrecord` | Both | **Q1 target:** Start a server multiview-demo recording with an automatically generated name. |
| `sv_demoinfo` | Both | **Q1 target:** Show information associated with a server demo recording. |
| `sv_demoinfoadd` | Both | **Q1 target:** Add descriptive information to a server demo recording. |
| `sv_demoinforemove` | Both | **Q1 target:** Remove descriptive information from a server demo recording. |
| `sv_demolist` | Both | **Q1 target:** List the server's multiview-demo recordings. |
| `sv_demonumremove` | Both | **Q1 target:** Delete a server multiview-demo recording by its numbered list entry. |
| `sv_demorecord` | Both | **Q1 target:** Start recording a server multiview demo. |
| `sv_demoremove` | Both | **Q1 target:** Delete a named server multiview-demo recording. |
| `sv_demostop` | Both | **Q1 target:** Finish the current server multiview-demo recording. |
| `sv_downloadchunksperframe` | Both | Legacy compatibility setting command; downloads are paced by the client download/rate settings instead. |
| `sv_enableprofile` | Both | **Q1 target:** Legacy compatibility command for an unimplemented server profiling setting; use the QuakeC profiling commands instead. |
| `sv_gamedir` | Both | Change the gamedir reported to clients, without changing any actual paths on the server. |
| `sv_impulse` | Both | **Q1 target:** Run a QuakeC impulse from the server console by creating a temporary console player; requires a free player slot and active QuakeC gamecode. |
| `sv_lightstyle` | Both | Overrides lightstyles from the server's console, mostly for debug use. |
| `sv_meminfo` | Both | Print server memory-use information. |
| `sv_progsname` | Both | Legacy compatibility command; use the `sv_progs` cvar to choose the server game program. |
| `sv_progtype` | Both | Legacy compatibility command; use the `sv_progs` cvar to choose the server game program. |
| `sv_saveentfile` | Both | Dumps a copy of the map's entities to disk, so that it can be edited and used as a replacement for slightly customised maps. |
| `sv_settimer` | Both | Schedule a server-console command: `sv_settimer <count> <interval-seconds> <command>`; count -1 repeats indefinitely, and `sv_settimer 0` cancels it. |
| `sv_speedcheck` | Both | Legacy compatibility setting command; the engine instead meters client movement time over time. |
| `sys_openfile` | Client | Select a file to open/install/etc. |
| `sys_select_timeout` | Both | Legacy compatibility setting command; server throttling follows the engine tick-rate settings. |
| `tcmd` | Client | **Q3:** Developer target command: send a numeric game command to the player under the crosshair. |
| `tcpconnect` | Client | Connect to a server using the tcp:// prefix |
| `team` | Client | **Different:** **Combined command/cvar entry.** Quake 1: team-name string cvar/userinfo, default `""`. Quake 3/OpenArena: qagame command requesting team/playing-state selection or reporting the current team. Use `cmd team <red\|blue\|free\|spectator>` to reach the remote gamecode handler because the engine team cvar shadows the bare name; for example `cmd team spectator`. |
| `teamtask` | Client | **Q3:** Set the player’s team-task/status value used by teammate displays and team orders. |
| `teamvote` | Client | **Q3:** Cast yes/no on the active team vote. |
| `tell` | Server | Send a server-console private message to an identified player: `tell <player> <message>`. |
| `tell_attacker` | Client | **Q3:** Send private text to the player under the crosshair / the most recent attacker respectively. |
| `tell_target` | Client | **Q3:** Send private text to the player under the crosshair / the most recent attacker respectively. |
| `testgun` | Client | **Q3:** Display a developer test model as a first-person gun: `testgun <model>`. Cheat/developer restrictions apply. |
| `testmodel` | Client | **Q3:** Load a developer test model in front of the player: `testmodel <model>`. Cheat/developer restrictions apply. |
| `timedemo` | Client | Play a demo as a performance benchmark and report timing/frame-rate results. |
| `toggle` | Both | Toggles a cvar between two values toggle CVARNAME [newval [altval]] |
| `toggleconsole` | Client | Open or close the console. |
| `togglemenu` | Client | Open or close the main menu. |
| `topten` | Client | Print the leading ten entries in the persistent server rankings. |
| `tp_pickup` | Client | **Q1 target:** Inspect or set the item categories that trigger QuakeWorld teamplay pickup handling. |
| `tp_point` | Client | **Q1 target:** Inspect or set the item/player categories recognized by QuakeWorld teamplay pointing macros. |
| `tp_took` | Client | **Q1 target:** Inspect or set the item categories recognized by QuakeWorld teamplay 'took' macros. |
| `track` | Client | Choose spectator tracking targets by user ID/name, or `off`; additional arguments can select targets for additional split-screen views. |
| `track1` | Client | Select a spectator tracking target for split-screen view 1: `track1 <userid-or-name-or-off>`. |
| `track2` | Client | Select a spectator tracking target for split-screen view 2: `track2 <userid-or-name-or-off>`. |
| `track3` | Client | Select a spectator tracking target for split-screen view 3: `track3 <userid-or-name-or-off>`. |
| `track4` | Client | Select a spectator tracking target for split-screen view 4: `track4 <userid-or-name-or-off>`. |
| `ui_cache` | Client | **Q3:** Precache resources used by the bundled Quake 3 UI. |
| `ui_cdkey` | Client | **Q3:** Open the legacy Quake 3 key-entry UI, if supplied by the game; OpenArena does not require a commercial Quake 3 key. |
| `ui_cinematics` | Client | **Q3:** Open the bundled UI’s cinematics menu. |
| `ui_mappage` | Client | **Q3:** Internal UI map-page response handler; arguments are normally supplied by the map-list protocol. |
| `ui_restart` | Client | **Q3:** Reload the Quake 3 UI VM. `ui_restart off` stops that UI; this does not unload cgame/qagame or open the native Options menu. Requires the Quake 3 plugin. |
| `ui_teamOrders` | Client | **Q3:** Open the bundled UI’s team-orders menu. |
| `unban` | Both | Unbans or removes an IP Address from the penalty list, alias to removeip. |
| `unbind` | Client | Remove a key binding: `unbind <key>`. |
| `unbindall` | Client | Remove every key binding, including movement and console bindings; normally used when loading a complete default binding set. |
| `unignore` | Client | Stop ignoring chat from a named player. |
| `unignore_id` | Client | Stop ignoring a player identified by user ID. |
| `unignore_team` | Client | Stop ignoring members of a named team. |
| `unignoreAll` | Client | Clear all ignored-player entries. |
| `unignoreAll_team` | Client | Clear all ignored-team entries. |
| `unsavegame` | Both | **Q1 target:** Wipes an existing saved game from disk. Quake 3/OpenArena qagame does not implement this engine saved-game state format. |
| `user` | Both | Display userinfo for an identified player; client use queries the connected player list, and server use queries hosted players. |
| `users` | Client | List players and their user IDs. |
| `v_cshift` | Client | Set an explicit screen-color shift using red, green, blue, and intensity arguments; a compatibility/protocol effect. |
| `version` | Both | Reports engine revision and optional compile-time settings. |
| `version_modelformats` | Both | List the registered model-format loaders. |
| `vid_reload` | Client | Reload renderer resources through the renderer reload path. |
| `vid_restart` | Client | Restart the video/renderer subsystem and apply latched video settings. |
| `vid_revert` | Client | Revert an unconfirmed display/interface-size change made with the engine Video menu. No effect without a pending test. The menu also offers Keep/Revert and automatically restores unanswered changes after 15 seconds; console `vid_restart` does not start a test. |
| `vid_toggle` | Client | Toggle windowed/fullscreen display mode. |
| `viewpos` | Client | **Q3:** Print the current cgame camera origin and yaw. |
| `voip` | Client | Adjust the Speex voice preprocessor's automatic-gain maximum with `voip maxgain <value>` when Speex is available; use `+voip` for push-to-talk. |
| `vosay` | Client | **Q3:** Voice-only versions of the scripted voice-message commands, with text display suppressed. |
| `vosay_local` | Client | **Q3:** Voice-only versions of the scripted voice-message commands, with text display suppressed. |
| `vosay_team` | Client | **Q3:** Voice-only versions of the scripted voice-message commands, with text display suppressed. |
| `vote` | Client | **Q3:** `vote <yes\|no>`: cast a vote on the active global vote. |
| `votell` | Client | **Q3:** Send a scripted voice message privately to a client; `votell` requests voice-only display. |
| `vsay` | Client | **Q3:** Play a scripted voice message globally, for teammates, or locally as allowed by qagame; use a voice-message identifier, not recorded microphone audio. |
| `vsay_local` | Client | **Q3:** Play a scripted voice message globally, for teammates, or locally as allowed by qagame; use a voice-message identifier, not recorded microphone audio. |
| `vsay_team` | Client | **Q3:** Play a scripted voice message globally, for teammates, or locally as allowed by qagame; use a voice-message identifier, not recorded microphone audio. |
| `vstr` | Both | Executes the string value of the cvar, much like if it were an alias. For compatibility with q3. |
| `vtaunt` | Client | **Q3:** Play the context-appropriate gamecode voice taunt. |
| `vtell` | Client | **Q3:** Send a scripted voice message privately to a client; `votell` requests voice-only display. |
| `vtell_attacker` | Client | **Q3:** Send a scripted voice-chat message to the player under the crosshair / the most recent attacker respectively. |
| `vtell_target` | Client | **Q3:** Send a scripted voice-chat message to the player under the crosshair / the most recent attacker respectively. |
| `wait` | Both | Pause execution of the current command buffer until a later frame. |
| `watchpoint` | Both | **Q1 target:** Set or clear a server QuakeC expression watchpoint used by the debugger. |
| `watchpoint_csqc` | Client | **Q1 target:** Set or clear a client QuakeC expression watchpoint used by the debugger. |
| `watchpoint_menuqc` | Client | Set or clear a menu QuakeC expression watchpoint used by the debugger. Requires a QuakeC menu program; Quake 3’s bundled UI VM is a separate interface. |
| `watchpoint_ssqc` | Both | **Q1 target:** Set or clear a server QuakeC expression watchpoint used by the debugger. |
| `waterfog` | Client | **Q1 target:** Set underwater fog: `waterfog <density> <red> <green> <blue> <alpha> <depthbias>`. |
| `weapnext` | Client | **Q3:** Cycle to the next/previous available weapon according to cgame rules. |
| `weapon` | Client | **Different:** Quake 1 sets weapon priorities for the next `+attack`, as an alternative to `impulse`. Quake 3/OpenArena selects a weapon by its gamecode weapon number: `weapon <number>`. |
| `weapprev` | Client | **Q3:** Cycle to the next/previous available weapon according to cgame rules. |
| `wf` | Client | Trigger the white screen-color flash. |
| `where` | Client | **Q3:** Print the player’s server-gamecode origin. |
| `which` | Both | Searches for a named file, and displays where it can be found in the OS's filesystem |
| `worker_status` | Both | Print background-worker/thread job information. |
| `worker_test` | Both | Queue a background-worker ping and report its completion latency. |
| `writeip` | Both | Write the server's IP penalty/filter configuration to disk. |

## Cvar reference

Each setting is listed once, with real engine aliases on the same row. Names
are case-insensitive. An unmarked entry is shared; **Q1 target** or **Q3** marks
its game/protocol-specific effect, even if the engine registers its name while
another game is running. Some Quake 1 compatibility settings can be used by
custom Quake 3 mods, and vice versa, if their gamecode explicitly reads them.
The team cvar and logfile compatibility lookup share names with commands;
their complete combined entries are in the command table rather than repeated here.

Defaults below are initialization/profile defaults, not recommendations or the value in
your saved config. Game profiles, graphical presets, gamecode and platform
selection can change them. `""` means an empty string; **on demand** means a
menu, gamecode path or config establishes the value later. Informational values
are engine/game managed. Cvars marked for restart may remain latched until the
appropriate restart; cheat/ruleset restrictions can prevent client changes.
An on-demand compatibility lookup may return an empty/zero value until gamecode
or a config creates it; listing it does not claim a legacy ioquake3 renderer or
sound feature is implemented by SDL3 GPU.
Where the two initialized game profiles use different defaults, both appear
in the same row. Platform/device/session values are summarized rather than
publishing a particular machine's data.

Legacy `gl_*` names do not imply that OpenGL is present: many control the shared
material system or SDL3 GPU renderer. Others are retained compatibility values.
Likewise, an OpenArena UI lookup for an ioquake3 `r_*` setting does not implement
that setting in this engine. Use the native `menu_options`, `menu_video`,
`menu_render` and `menu_lighting` pages for this fork's graphics configuration.

Jump to [symbols and A–C](#cvars-symbols-and-ac), [D–H](#cvars-dh),
[I–M](#cvars-im), [N–R](#cvars-nr), or [S–Z](#cvars-sz).

### Cvars: Symbols and A–C

| Cvar (aliases) | Default | Definition / game notes |
| --- | --- | --- |
| `_cl_disconnectreason` (alias: `com_errorMessage`) | `""` | This cvar contains the reason for the last disconnection, so that mod menus can know why things failed. |
| `_cl_voip_capturedevice_opts` | dynamic / platform dependent | The possible audio capture devices, in "value" "description" pairs, for gamecode to read. |
| `_pext_infoblobs` | `"0"` | RENAME ME WHEN STABLE. Enables the use of very large infokeys containing potentially invalid chars. Note that the userinfo is still limited by sv_userinfo_bytelimit and sv_userinfo_keylimit. |
| `_pext_lerptime` | `"0"` | RENAME ME WHEN STABLE. Sends timing hints for interpolation. |
| `_pext_vrinputs` | `"0"` | RENAME ME WHEN STABLE. Networks player inputs slightly differently, allowing for greater capabilities, particuarly vr controller info. |
| `_q3bsp_bihtraces` | `"0"` | Uses runtime-generated bih collision culling for faster traces. |
| `_s_device_opts` | `""` | The possible audio output devices, in "value" "description" pairs, for gamecode to read. |
| `_vid_renderer_opts` | `""` | The possible video renderer apis, in "value" "description" pairs, for gamecode to read. |
| `addon0` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon1` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon10` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon11` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon12` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon13` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon14` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon15` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon2` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon3` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon4` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon5` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon6` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon7` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon8` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `addon9` | `""` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `allow_download` (alias: `sv_allowDownload`) | `"1"` | If 1, permits downloading. Set to 0 to unconditionally block *ALL* downloads from this server. You may wish to set sv_dlURL if you wish clients to still be able to download content. |
| `allow_download_configs` | `"0"` | 1 allows downloading of config files, either with the extension .cfg or in the subdir configs/.<br>THIS IS DANGEROUS AS IT CAN ALLOW PEOPLE TO READ YOUR RCON PASSWORD ETC. |
| `allow_download_copyrighted` | `"0"` | 0 blocks download of packages that are considered copyrighted. Specifically, this means packages with a leading 'pak' prefix on the filename.<br>If you take your copyrights seriously, you should also set allow_download_pakmaps 0 and allow_download_pakcontents 0. |
| `allow_download_demos` | `"1"` | 0 blocks downloading of any file in the demos/ directory |
| `allow_download_locs` | `"1"` | 0 blocks downloading of any file in the locs/ directory |
| `allow_download_logs` | `"0"` | 1 permits downloading files with the extension .log<br>THIS IS DANGEROUS AS IT POTENTIALLY ALLOWS PEOPLE TO SEE PASSWORDS OR OTHER PRIVATE INFORMATION.<br>Note that it can be switch on/off via rcon. |
| `allow_download_maps` | `"1"` | 0 blocks downloading of any file in the maps/ directory |
| `allow_download_models` | `"1"` | 0 blocks downloading of any file in the progs/ or models/ directory |
| `allow_download_other` | `"0"` | 0 blocks downloading of any file that was not covered by any of the directory download blocks. |
| `allow_download_packages` | `"1"` | if 1, permits downloading files (from root directory or elsewhere) with known package extensions (eg: pak+pk3). Packages with a name starting 'pak' are covered by allow_download_copyrighted as well. |
| `allow_download_pakcontents` | Q1: `"2"`; Q3: `"0"` | 0: clients may not download non-map files within the server's packages.<br>1: clients may download such files so long as the package is not deemed copyrighted, for compat with old clients that do not support package downloads.<br>2: client may download such files regardless of copyright state (WARNING! ONLY for consistency with vanilla QuakeWorld!). |
| `allow_download_pakmaps` | `"0"` | 0: clients may not download map files within the server's packages.<br>1: clients may download such files so long as the package is not deemed copyrighted, for compat with old clients that do not support package downloads.<br>2: client may download such files regardless of copyright state (WARNING! Should never be used!). |
| `allow_download_particles` | `"1"` | 0 blocks downloading of any file in the particles/ directory |
| `allow_download_refpackages` | Q1: `"0"`; Q3: `"1"` | If set to 1, packages that contain files needed during spawn functions will be become 'referenced' and automatically downloaded to clients.<br>This cvar should probably not be set if you have large packages that provide replacement pickup models on public servers.<br>The path command will show a '(ref)' tag next to packages which clients will automatically attempt to download. |
| `allow_download_root` | `"0"` | If set, enables downloading from the root of the gamedir (not the basedir). This setting has a lower priority than extension-based checks. |
| `allow_download_skins` | `"1"` | 0 blocks downloading of any file in the skins/ directory |
| `allow_download_sounds` | `"1"` | 0 blocks downloading of any file in the sound/ directory |
| `allow_download_textures` | `"1"` | 0 blocks downloading of any file in the textures/ directory |
| `allow_download_wads` | `"1"` | 0 blocks downloading of any file in the wads/ directory, or is in the root directory with the extension .wad |
| `allow_f_cmdline` | `"0"` | **Q1 target:** permit replies to the corresponding f_cmdline chat query disclosing launch command line. |
| `allow_f_fakeshaft` | `"1"` | **Q1 target:** permit replies to the corresponding f_fakeshaft chat query disclosing lightning prediction setting. |
| `allow_f_modified` | `"1"` | **Q1 target:** permit replies to the corresponding f_modified chat query disclosing modified-file status. |
| `allow_f_ruleset` | `"1"` | **Q1 target:** permit replies to the corresponding f_ruleset chat query disclosing ruleset. |
| `allow_f_scripts` | `"1"` | **Q1 target:** permit replies to the corresponding f_scripts chat query disclosing script usage. |
| `allow_f_server` | `"1"` | **Q1 target:** permit replies to the corresponding f_server chat query disclosing server address. |
| `allow_f_skins` | `"1"` | **Q1 target:** permit replies to the corresponding f_skins chat query disclosing skin settings. |
| `allow_f_system` | `"0"` | **Q1 target:** permit replies to the corresponding f_system chat query disclosing system information. |
| `allow_f_version` | `"1"` | **Q1 target:** permit replies to the corresponding f_version chat query disclosing engine version. |
| `allow_skybox` | `""` | This setting says whether clients should skip writing skybox depth when rendering skyboxes/skydomes. Skipping depth writes is required for halflife, quake2, and quake3 compat, but q1 content generally requires depth masking. Empty uses format-specific defaults. |
| `allow_splitscreen` | `""` | Specifies whether clients can use splitscreen extensions to dynamically add additional clients. This only affects remote clients and not the built-in client.<br>Clients may need to reconnect in order to add seats when this is changed. |
| `b_switch` | `""` | **Q1 target:** weapon-switch preference userinfo, interpreted by supporting gamecode when picking up weapons/backpacks. |
| `baseskin` | `""` | **Q1 target:** The name of the player skin to use as a fallback. |
| `bot_aasoptimize` | on demand | **Q3:** AAS optimization preference passed to bot-library setup. |
| `bot_challenge` | `"0"` | **Q3:** Bot challenge/difficulty behavior toggle. |
| `bot_developer` | `"0"` | **Q3:** Enable bot developer diagnostics. |
| `bot_enable` | `"1"` | Controls whether bots may be used by the gamecode or not. |
| `bot_fastchat` | `"0"` | **Q3:** Reduce bot chat delays. |
| `bot_forceclustering` | on demand | **Q3:** force AAS cluster recalculation during bot-library setup. |
| `bot_forcereachability` | on demand | **Q3:** force AAS reachability recalculation during bot-library setup. |
| `bot_forcewrite` | on demand | **Q3:** force the bot library to write recalculated AAS data. |
| `bot_grapple` | `"0"` | **Q3:** Allow bots to use grappling hooks. |
| `bot_interbreedbots` | `"10"` | **Q3:** Number of bots used by bot genetic/interbreeding tests. |
| `bot_interbreedchar` | `""` | **Q3:** Bot character file used by interbreeding tests. |
| `bot_interbreedcycle` | `"20"` | **Q3:** Cycles between bot interbreeding operations. |
| `bot_interbreedwrite` | `""` | **Q3:** Output filename for bot interbreeding results. |
| `bot_memorydump` | `"0"` | **Q3:** Request a bot-library memory dump. |
| `bot_minplayers` | `"0"` | **Q3:** Minimum player count maintained by adding/removing bots. |
| `bot_nochat` | `"0"` | **Q3:** Disable bot chat. |
| `bot_pause` | `"0"` | **Q3:** Pause bot thinking for diagnosis. |
| `bot_predictobstacles` | `"1"` | **Q3:** Predict navigation obstacles when planning bot movement. |
| `bot_reloadcharacters` | on demand | **Q3:** reload bot character files rather than reusing cached data. |
| `bot_report` | `"0"` | **Q3:** Print bot state/report diagnostics. |
| `bot_rocketjump` | `"1"` | **Q3:** Allow bots to rocket-jump. |
| `bot_saveroutingcache` | `"0"` | **Q3:** Request saving of the bot routing cache. |
| `bot_testclusters` | `"0"` | **Q3:** Visualize/test bot navigation clusters. |
| `bot_testichat` | on demand | **Q3:** test initial bot chat behavior. |
| `bot_testrchat` | `"0"` | **Q3:** Test random bot chat generation. |
| `bot_testsolid` | `"0"` | **Q3:** Test solid areas in bot navigation data. |
| `bot_thinktime` | `"100"` | **Q3:** Milliseconds between bot thinking updates. |
| `bot_visualizejumppads` | on demand | **Q3:** bot-library jump-pad visualization preference. |
| `bottomcolor` | `"12"` | **Different:** Quake 1 player-shirt/trouser palette colors; Quake 3 uses model skins and color1/color2 instead. |
| `brightness` (alias: `v_brightness`) | `"0.0"` | Brightness is how much 'white' to add to each and every pixel on the screen. |
| `capturecodec` | `"tga"` | the compression/encoding codec to use.<br>With raw capturing, this should be one of tga,png,jpg,pcx (ie: screenshot extensions). |
| `capturedemoheight` | `"0"` | When using capturedemo, this specifies the height of the FBO image used. |
| `capturedemowidth` | `"0"` | When using capturedemo, this specifies the width of the FBO image used. Can be larger than your physical monitor. |
| `capturedriver` | `""` | The driver to use to capture the demo. Use the capture command with no arguments for a list of drivers. |
| `capturelimit` | `"8"` | **Q3:** Capture/round-score limit in supported Quake 3 team game types. |
| `capturemessage` | `""` | Message text used by the video-capture interface. |
| `capturerate` | `"30"` | The framerate of the video to capture |
| `capturesound` | `"1"` | Enables the capturing of game voice. If not using capturedemo, this can be combined with cl_voip_test to capture your own voice. |
| `capturesoundbits` | `"16"` | Audio bit depth for captured video/audio output. |
| `capturesoundchannels` | `"2"` | Number of audio channels for captured video/audio output. |
| `capturethrottlesize` | `"0"` | If set, capturing will slow down significantly if there is less disk space available than this cvar (in mb). This is useful when recording a stream to (ram)disk while another program is simultaneously consuming frames. |
| `cfg_reload_on_gamedir` | `"1"` | Reload configuration when switching the active game directory. |
| `cfg_save_aliases` | `"1"` | If 1, saves aliases to configs. Note that aliases sent from servers are assumed to be server-specific and are never saved (and are forgotten on map changes, too). |
| `cfg_save_all` | `""` | If 1, cfg_save ALWAYS saves all cvars. If 0, cfg_save only ever saves archived cvars. If empty, cfg_save saves all cvars only when an explicit filename was given (ie: when not used internally via quit menu options). |
| `cfg_save_auto` | `"0"` | If 1, the config will automatically be saved and without prompts. If 0, you'll have to save your config manually (possibly via prompts from the quit menu). |
| `cfg_save_binds` | `"1"` | If 1, saves all key bindings to configs. |
| `cfg_save_buttons` | `"0"` | If 1, saves the state of things such as +mlook or +forward to configs. |
| `cfg_save_infos` | `"1"` | If 1, saves userinfo and serverinfo to configs. |
| `cfg_save_name` | `"fte"` | This is the config name that is saved by default when no argument is specified. |
| `cg_alwaysWeaponBar` | `"0"` | **Q3:** Keep the weapon bar visible outside normal display conditions. |
| `cg_animspeed` | `"1"` | **Q3:** Animation-speed diagnostic switch used by client gamecode. |
| `cg_autoswitch` | `"1"` | **Q3:** Automatically switch to newly picked-up weapons. |
| `cg_autovertex` | `"0"` | **Q3:** OpenArena automatic vertex-light preference; actual lighting behavior depends on renderer support. |
| `cg_bobpitch` | `"0.002"` | **Q3:** Camera pitch/roll amplitude during movement bobbing. |
| `cg_bobroll` | `"0.002"` | **Q3:** Camera pitch/roll amplitude during movement bobbing. |
| `cg_bobup` | `"0.005"` | **Q3:** Vertical camera bob amplitude. |
| `cg_brassTime` | `"2500"` | **Q3:** Lifetime of ejected shell casings, in milliseconds; 0 disables them. |
| `cg_cameraOrbit` | `"0"` | **Q3:** Camera orbit angular increment used by developer orbit mode. |
| `cg_cameraOrbitDelay` | `"50"` | **Q3:** Time interval between developer camera-orbit increments. |
| `cg_centertime` | `"3"` | **Q3:** Duration of centered messages, in seconds. |
| `cg_ch1` | `"1"` | **Q3:** Weapon 1 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch10` | `"1"` | **Q3:** Weapon 10 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch10size` | `"24"` | **Q3:** Weapon 10 crosshair size. |
| `cg_ch11` | `"1"` | **Q3:** Weapon 11 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch11size` | `"24"` | **Q3:** Weapon 11 crosshair size. |
| `cg_ch12` | `"1"` | **Q3:** Weapon 12 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch12size` | `"24"` | **Q3:** Weapon 12 crosshair size. |
| `cg_ch13` | `"1"` | **Q3:** Weapon 13 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch13size` | `"24"` | **Q3:** Weapon 13 crosshair size. |
| `cg_ch1size` | `"24"` | **Q3:** Weapon 1 crosshair size. |
| `cg_ch2` | `"1"` | **Q3:** Weapon 2 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch2size` | `"24"` | **Q3:** Weapon 2 crosshair size. |
| `cg_ch3` | `"1"` | **Q3:** Weapon 3 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch3size` | `"24"` | **Q3:** Weapon 3 crosshair size. |
| `cg_ch4` | `"1"` | **Q3:** Weapon 4 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch4size` | `"24"` | **Q3:** Weapon 4 crosshair size. |
| `cg_ch5` | `"1"` | **Q3:** Weapon 5 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch5size` | `"24"` | **Q3:** Weapon 5 crosshair size. |
| `cg_ch6` | `"1"` | **Q3:** Weapon 6 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch6size` | `"24"` | **Q3:** Weapon 6 crosshair size. |
| `cg_ch7` | `"1"` | **Q3:** Weapon 7 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch7size` | `"24"` | **Q3:** Weapon 7 crosshair size. |
| `cg_ch8` | `"1"` | **Q3:** Weapon 8 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch8size` | `"24"` | **Q3:** Weapon 8 crosshair size. |
| `cg_ch9` | `"1"` | **Q3:** Weapon 9 crosshair selection when per-weapon crosshairs are enabled. |
| `cg_ch9size` | `"24"` | **Q3:** Weapon 9 crosshair size. |
| `cg_chatBeep` | `"1"` | **Q3:** Play notification sounds for global/team chat respectively. |
| `cg_cmdTimeNudge` | `"0"` | **Q3:** User-command timing adjustment, in milliseconds, for supporting anti-lag code. |
| `cg_crosshairColorBlue` | `"1.0"` | **Q3:** Red, green or blue component of the custom crosshair color. |
| `cg_crosshairColorGreen` | `"1.0"` | **Q3:** Red, green or blue component of the custom crosshair color. |
| `cg_crosshairColorRed` | `"1.0"` | **Q3:** Red, green or blue component of the custom crosshair color. |
| `cg_crosshairHealth` | `"1"` | **Q3:** Color the crosshair according to player health. |
| `cg_crosshairPulse` | `"1"` | **Q3:** Pulse the crosshair when picking up items. |
| `cg_crosshairSize` | `"24"` | **Q3:** Gamecode crosshair size. |
| `cg_crosshairX` | `"0"` | **Q3:** Gamecode crosshair horizontal/vertical offset. |
| `cg_crosshairY` | `"0"` | **Q3:** Gamecode crosshair horizontal/vertical offset. |
| `cg_cyclegrapple` | `"1"` | **Q3:** Include the grappling hook in weapon cycling. |
| `cg_debuganim` | `"0"` | **Q3:** Print animation debugging information. |
| `cg_debugevents` | `"0"` | **Q3:** Print game-event debugging information. |
| `cg_debugposition` | `"0"` | **Q3:** Print position debugging information. |
| `cg_deferPlayers` | `"1"` | **Q3:** Defer loading player models during active play to reduce stalls. |
| `cg_delag` | `"1"` | **Q3:** Client preference for server-assisted hit-scan lag compensation. |
| `cg_differentCrosshairs` | `"0"` | **Q3:** Use separate crosshair choices for individual weapons. |
| `cg_draw2D` | `"1"` | **Q3:** Draw gamecode HUD/2D overlays. |
| `cg_draw3dIcons` | `"1"` | **Q3:** Use 3D rather than flat HUD icons. |
| `cg_drawAmmoWarning` | `"1"` | **Q3:** Display low-ammunition warnings. |
| `cg_drawAttacker` | `"1"` | **Q3:** Display the last-attacker indicator. |
| `cg_drawCrosshair` | `"4"` | **Q3:** Select the gamecode crosshair; 0 disables it. |
| `cg_drawCrosshairNames` | `"1"` | **Q3:** Display names of players under the crosshair. |
| `cg_drawFPS` | `"0"` | **Q3:** Display the gamecode FPS counter. |
| `cg_drawFriend` | `"1"` | **Q3:** Display the friendly-player marker. |
| `cg_drawGun` | `"1"` | **Q3:** Draw the first-person weapon model. |
| `cg_drawIcons` | `"1"` | **Q3:** Draw HUD item icons. |
| `cg_drawRewards` | `"1"` | **Q3:** Display award/reward indicators. |
| `cg_drawSnapshot` | `"0"` | **Q3:** Display snapshot diagnostics. |
| `cg_drawSpeed` | `"0"` | **Q3:** Display the gamecode speed indicator. |
| `cg_drawStatus` | `"1"` | **Q3:** Draw the status/health/ammo HUD. |
| `cg_drawTeamOverlay` | `"0"` | **Q3:** Display the teammate information overlay. |
| `cg_drawTimer` | `"0"` | **Q3:** Display the game timer. |
| `cg_errordecay` | `"100"` | **Q3:** Milliseconds over which prediction errors are smoothed away. |
| `cg_footsteps` | `"1"` | **Q3:** Enable footstep sounds. |
| `cg_forceModel` | `"0"` | **Q3:** Force other players to use the local player's model selection. |
| `cg_fragmsgsize` | `"1.0"` | **Q3:** Size of frag/kill notification text. |
| `cg_gibs` | `"1"` | **Q3:** Spawn/display gib effects. |
| `cg_gunX` | `"0"` | **Q3:** First-person weapon offset on the x axis. |
| `cg_gunY` | `"0"` | **Q3:** First-person weapon offset on the y axis. |
| `cg_gunZ` | `"0"` | **Q3:** First-person weapon offset on the z axis. |
| `cg_hitsound` | `"0"` | **Q3:** Enable the hit-confirmation sound. |
| `cg_ignore` | `"0"` | **Q3:** Client-gamecode debugging value; not a user-facing gameplay option. |
| `cg_lagometer` | `"1"` | **Q3:** Display the gamecode latency/connection graph. |
| `cg_leiBrassNoise` | `"0"` | **Q3:** OpenArena shell-casing impact sounds. |
| `cg_leiEnhancement` | `"0"` | **Q3:** OpenArena enhanced effect-style selection. |
| `cg_leiGoreNoise` | `"0"` | **Q3:** OpenArena gore-effect sounds. |
| `cg_leiSuperGoreyAwesome` | `"0"` | **Q3:** OpenArena extra gore effects. |
| `cg_marks` | `"1"` | **Q3:** Enable impact marks/decals generated by gamecode. |
| `cg_music` | `""` | **Q3:** Enable gamecode-controlled map music playback. |
| `cg_noplayeranims` | `"0"` | **Q3:** Disable player-model animation for diagnosis. |
| `cg_nopredict` | `"0"` | **Q3:** Disable gamecode client movement prediction for diagnosis. |
| `cg_noProjectileTrail` | `"0"` | **Q3:** Disable gamecode projectile trails. |
| `cg_noTaunt` | `"0"` | **Q3:** Suppress scripted taunt sounds. |
| `cg_noVoiceChats` | `"0"` | **Q3:** Disable scripted voice-chat audio. |
| `cg_noVoiceText` | `"0"` | **Q3:** Disable text captions for scripted voice chats. |
| `cg_oldPlasma` | `"1"` | **Q3:** Use the older plasma/rail/rocket trail effect respectively. |
| `cg_oldRail` | `"0"` | **Q3:** Use the older plasma/rail/rocket trail effect respectively. |
| `cg_oldRocket` | `"1"` | **Q3:** Use the older plasma/rail/rocket trail effect respectively. |
| `cg_optimizePrediction` | `"1"` | **Q3:** Reuse valid previous prediction results to reduce prediction work. |
| `cg_predictItems` | `"1"` | **Q3:** Predict local item pickups before server confirmation. |
| `cg_projectileNudge` | `"0"` | **Q3:** Visual projectile timing adjustment, in milliseconds. |
| `cg_railTrailTime` | `"600"` | **Q3:** Lifetime of railgun trails in milliseconds. |
| `cg_runpitch` | `"0.002"` | **Q3:** Camera pitch/roll response to running. |
| `cg_runroll` | `"0.005"` | **Q3:** Camera pitch/roll response to running. |
| `cg_scorePlums` | `"1"` | **Q3:** Display floating score-change numbers. |
| `cg_shadows` | `"1"` | **Q3:** Gamecode entity-shadow mode; this differs from the engine's real-time light/shadow controls. |
| `cg_showmiss` | `"0"` | **Q3:** Print prediction-mismatch diagnostics. |
| `cg_simpleItems` | `"0"` | **Q3:** Use sprite icons instead of 3D pickup models. |
| `cg_stats` | `"0"` | **Q3:** Print gamecode frame/statistics diagnostics. |
| `cg_swingSpeed` | `"0.3"` | **Q3:** Player-model torso/leg turning interpolation speed. |
| `cg_teamChatBeep` | `"1"` | **Q3:** Play notification sounds for global/team chat respectively. |
| `cg_teamChatHeight` | `"0"` | **Q3:** Maximum visible team-chat lines. |
| `cg_teamChatsOnly` | `"0"` | **Q3:** Suppress non-team chat in the gamecode chat display. |
| `cg_teamChatTime` | `"3000"` | **Q3:** Lifetime of team-chat overlay messages, in milliseconds. |
| `cg_thirdPerson` | `"0"` | **Q3:** Enable the gamecode third-person camera. |
| `cg_thirdPersonAngle` | `"0"` | **Q3:** Orbit angle of the third-person camera. |
| `cg_thirdPersonRange` | `"40"` | **Q3:** Distance of the third-person camera from the player. |
| `cg_timescaleFadeEnd` | `"1"` | **Q3:** Target value for the gamecode time-scale fade. |
| `cg_timescaleFadeSpeed` | `"0"` | **Q3:** Rate at which gamecode fades timescale toward its target. |
| `cg_tracerchance` | `"0.4"` | **Q3:** Probability of rendering a bullet tracer. |
| `cg_tracerlength` | `"100"` | **Q3:** Length of bullet tracers. |
| `cg_tracerwidth` | `"1"` | **Q3:** Width of bullet tracers. |
| `cg_trueLightning` | `"0.0"` | **Q3:** Blend between predicted and server-reported lightning-gun beam direction. |
| `cg_viewsize` | `"100"` | **Q3:** Size of the gamecode-rendered viewport, as a percentage. |
| `cg_voipTeamOnly` | `"1"` | **Q3:** Limit voice-chat targeting to teammates when supported by gamecode. |
| `cg_vote_custom_commands` | `""` | **Q3:** Server-supplied list of allowed custom vote commands; informational. |
| `cg_voteflags` | `"*"` | **Q3:** Server-supplied vote permissions used by the client UI; informational. |
| `cg_weaponBarStyle` | `"0"` | **Q3:** Weapon-bar layout/style selection. |
| `cg_weaponOrder` | `"/1/2/4/3/6/7/8/9/5/"` | **Q3:** Weapon priority string used for best-weapon selection. |
| `cg_zoomfov` | `"22.5"` | **Q3:** Target field of view while holding +zoom; cg_fov/fov is the unzoomed setting. |
| `chase_active` | `"0"` | **Q1 target:** enable the engine's third-person chase camera. Quake 3 uses cg_thirdPerson. |
| `chase_back` | `"48"` | **Q1 target:** chase-camera offsets behind, to the right of, and above the player respectively. |
| `chase_right` | `"0"` | **Q1 target:** chase-camera offsets behind, to the right of, and above the player respectively. |
| `chase_up` | `"24"` | **Q1 target:** chase-camera offsets behind, to the right of, and above the player respectively. |
| `cl_aliasoverlap` | `"1"` | Rename new aliases if they would override cvar names. |
| `cl_anglespeedkey` | `"1.5"` | Keyboard turning/look-speed multiplier while the speed modifier is held. |
| `cl_autotrack` | `"auto"` | **Q1 target:** Specifies the default tracking mode at the start of the map. Use the 'autotrack' command to reset/apply an auto-tracking mode without changing the default.<br>Valid values are: high, killer, mod, user. Other values are treated as weighting scripts for mvd playback, where available. |
| `cl_autotrack_team` | `""` | **Q1 target:** Specifies a team name that should be auto-tracked (players on other teams will not be candidates for autotracking). Accepts * and ? wildcards for awkward chars. |
| `cl_backspeed` | `""` | The base speed that you move backwards at. If empty, uses the value of cl_forwardspeed instead. |
| `cl_beam_alpha` (alias: `r_shaftalpha`) | `"1"` | Specifies the translucency of the lightning beam segments. |
| `cl_beam_trace` | `"0"` | Clips the length of any beams according to any walls they may have impacted. |
| `cl_bob` | `"0.02"` | Controls how much the camera position should bob up down as the player runs around. |
| `cl_bobcycle` | `"0.6"` | **Q1 target:** duration of the view-bobbing cycle. |
| `cl_bobmodel` | `"0"` | Controls whether the viewmodel should bob up and down as the player runs around. |
| `cl_bobmodel_side` | `"0.15"` | **Q1 target:** sideways/vertical weapon-model bob amplitude. |
| `cl_bobmodel_speed` | `"7"` | **Q1 target:** speed of weapon-model bobbing. |
| `cl_bobmodel_up` | `"0.06"` | **Q1 target:** sideways/vertical weapon-model bob amplitude. |
| `cl_bobup` | `"0.5"` | **Q1 target:** fraction of the bob cycle spent rising. |
| `cl_c2sdupe` | `"0"` | Send duplicate copies of packets to the server. This avoids extra latency caused by packetloss, but could also make the problem worse. |
| `cl_c2sImpulseBackup` | `"3"` | Prevents the cl_c2spps setting from dropping redundant packets that contain impulses, in an attempt to keep impulses more reliable. |
| `cl_c2sMaxRedundancy` | `"5"` | This is the maximum number of input frames to send in each input packet. Values greater than 1 provide redundancy and avoid prediction misses, though you might find cl_c2sdupe provides equivelent result and at lower latency. It is locked at 3 for vanilla quakeworld, and locked at 1 for vanilla netquake. |
| `cl_c2spps` | `"0"` | Reduces outgoing packet rates by dropping up to a third of outgoing packets. |
| `cl_chasecam` | `"1"` | **Q1 target:** spectator chase-camera preference. |
| `cl_chatmode` | `"2"` | 0(nq) - everything is assumed to be a console command. prefix with 'say', or just use a messagemode bind<br>1(q3) - everything is assumed to be chat, unless its prefixed with a /<br>2(qw) - anything explicitly recognised as a command will be used as a command, anything unrecognised will be a chat message.<br>/ prefix is supported in all cases.<br>ctrl held when pressing enter always makes any implicit chat into team chat instead. |
| `cl_chatsound` | `"1"` | **Q1 target:** chat-notification sound policy: 0 disables sounds, 1 allows them, 2 limits them to team chat. The actual sound files are cl_teamchatsound/cl_enemychatsound. |
| `cl_clock` | `"0"` | Show the engine's real-time clock overlay. |
| `cl_clock_x` | `"0"` | Screen position of the cl clock overlay on the x axis. |
| `cl_clock_y` | `"-1"` | Screen position of the cl clock overlay on the y axis. |
| `cl_countpendingpl` | `"0"` | If set to 1, packet loss percentages will show packets still in transit as lost, even if they might still be received. |
| `cl_crossx` | `"0"` | Horizontal/vertical offset of the engine crosshair. |
| `cl_crossy` | `"0"` | Horizontal/vertical offset of the engine crosshair. |
| `cl_crypt_rcon` | `"1"` | Controls whether to send a hash instead of sending your rcon password as plain-text. Set to 1 for security, or 0 for backwards compatibility.<br>Your command and any responses will still be sent as plain text.<br>Instead, it is recommended to use rcon ONLY via dtls/tls/wss connections. |
| `cl_csqc_nodeprecate` (alias: `dpcompat_nocsqcwarnings`) | `"0"` | **Q1 target:** When set, disables deprecation warnings. |
| `cl_csqcdebug` | `"0"` | **Q1 target:** enable client-side QuakeC debugging. |
| `cl_currentServerAddress` | on demand | **Q3:** Legacy Quake 3 UI lookup for the connected server address; informational compatibility value. |
| `cl_cursor` | `""` | Texture/image used for the engine mouse cursor. |
| `cl_cursor_bias_x` | Q1: `"7.5"`; Q3: `"0.0"` | Horizontal/vertical offset of the cursor image relative to the pointer hotspot. |
| `cl_cursor_bias_y` | Q1: `"0.8"`; Q3: `"0.0"` | Horizontal/vertical offset of the cursor image relative to the pointer hotspot. |
| `cl_cursor_scale` | Q1: `"0.2"`; Q3: `"1.0"` | Scale of the engine mouse cursor. |
| `cl_deadbodyfilter` | `"0"` | **Q1 target:** filter dead player bodies from the client view. |
| `cl_defaultport` (alias: `port`) | Q1: `"27500"`; Q3: `"27960"` | The default port used to connect to servers.<br>QW: 27500, NQ: 26000, Q2: 27910, Q3: 27960. |
| `cl_delay_packets` | `"0"` | Extra latency, in milliseconds. |
| `cl_demoreel` | `"0"` | When enabled, the engine will begin playing a demo loop on startup. |
| `cl_demospeed` | `"1"` | Client demo playback speed multiplier. |
| `cl_dlemptyterminate` | `"1"` | Terminate downloads when reciving an empty download packet. This should help work around buggy mvdsv servers. |
| `cl_download_csprogs` | `"1"` | Download updated client gamecode if available. Warning: If you clear this to avoid downloading vm code, you should also clear cl_download_packages. |
| `cl_download_mapsrc` | Q1: `"https://maps.quakeworld.nu/all/"`; Q3: `""` | Specifies an http location prefix for map downloads. EG: "http://example.com/path/gamemaps/" |
| `cl_download_packages` | `"1"` | 0=Do not download packages simply because the server is using them. 1=Download and load packages as needed (does not affect games which do not use this package). 2=Do download and install permanently (use with caution!) |
| `cl_download_redirection` | `"2"` | Follow download redirection to download packages instead of individual files. Also allows the server to send nearly arbitary download commands.<br>2: allows redirection only to named packages files (and demos/*.mvd), which is a bit safer. |
| `cl_download_wait` (alias: `requiredownloads`) | `"1"` | 0=join the game before downloads have even finished (might be laggy). 1=wait for all downloads to complete before joining. |
| `cl_downloadCount` | on demand | **Q3:** Quake 3 UI lookup for downloaded bytes; informational compatibility value. |
| `cl_downloadName` | on demand | **Q3:** Quake 3 UI lookup for the file being downloaded; informational compatibility value. |
| `cl_downloads` (alias: `cl_allowDownload`) | `"1"` | Allows you to block all automatic downloads. |
| `cl_downloadSize` | on demand | **Q3:** Quake 3 UI lookup for total download bytes; this fork's download UI support determines whether it is populated. |
| `cl_downloadTime` | on demand | **Q3:** Quake 3 UI lookup for download start time; informational compatibility value. |
| `cl_enemychatsound` | `"misc/talk.wav"` | Alternative sound for incoming chat from opponents. |
| `cl_enemyskin` (alias: `enemyskin`) | `""` | **Q1 target:** override enemy/team player skins respectively. |
| `cl_expsprite` | `"1"` | Display a central sprite in explosion effects. QuakeWorld typically does so, NQ mods should not (which is problematic when played with the qw protocol). |
| `cl_fakeframes` | `"0"` | Slow GPU? Want to see higher framerates get reported! Unleash the power of the lie to see much higher framerates! Many people said it couldn't be done, that the people wouldn't accept it, but to hell with the neighsayers and non-believers! WE WANT BIGGER NUMBERS AND WE'RE DAMN WELL GONNA GET THEM!... For best results, combine with an external tool like fluid motion frames... |
| `cl_fakename` | `""` | **Q1 target:** alternative name used by team-chat formatting. |
| `cl_fastaccel` | `"1"` | Begin moving at full speed instantly, instead of waiting a frame or so. |
| `cl_forwardspeed` | `"400"` | **Different:** Forward/back movement-command speed in the engine input layer. Quake 3 gamecode caps actual movement using its own movement rules. |
| `cl_fullpitch` (alias: `pq_fullpitch`) | `"0"` | When set, attempts to unlimit the default view pitch. Note that some servers will screw over your angles if you use this, resulting in terrible gameplay, while some may merely clamp your angle serverside. This is also considered a cheat in quakeworld, so this will not function there. For the equivelent in quakeworld, use serverinfo minpitch+maxpitch instead, which applies to all players fairly. [cheat/ruleset restricted.] |
| `cl_gameclock` | `"0"` | Show the engine's game-time clock overlay. |
| `cl_gameclock_x` | `"0"` | Screen position of the cl gameclock overlay on the x axis. |
| `cl_gameclock_y` | `"-1"` | Screen position of the cl gameclock overlay on the y axis. |
| `cl_gibfilter` | `"0"` | **Q1 target:** filter gib entities from the client view. |
| `cl_gunanglex` | `"0"` | **Q1 target:** first-person weapon angle offset on the x axis. Quake 3 uses cg_gunX/Y/Z. |
| `cl_gunangley` | `"0"` | **Q1 target:** first-person weapon angle offset on the y axis. Quake 3 uses cg_gunX/Y/Z. |
| `cl_gunanglez` | `"0"` | **Q1 target:** first-person weapon angle offset on the z axis. Quake 3 uses cg_gunX/Y/Z. |
| `cl_gunx` | `"0"` | **Q1 target:** first-person weapon position offset on the x axis. Quake 3 uses cg_gunX/Y/Z. |
| `cl_guny` | `"0"` | **Q1 target:** first-person weapon position offset on the y axis. Quake 3 uses cg_gunX/Y/Z. |
| `cl_gunz` | `"0"` | **Q1 target:** first-person weapon position offset on the z axis. Quake 3 uses cg_gunX/Y/Z. |
| `cl_hightrack` | `"0"` | **Q1 target:** Obsolete. If you want hightrack, use '[cl_]autotrack high' instead. |
| `cl_hudswap` | `"0"` | **Q1 target:** swap the sides of the classic status bar's weapon/ammo display. |
| `cl_idlefps` (alias: `cl_maxidlefps`) | `"60"` | This is the maximum framerate to attain while idle/paused/unfocused. |
| `cl_iDrive` | `"1"` | Effectively releases movement keys when the opposing key is pressed. This avoids dead-time when both keys are pressed. This can be emulated with various scripts, but that's messy. [cheat/ruleset restricted.] |
| `cl_instantrotate` | `"1"` | **Q1 target:** apply predicted angle changes immediately, instead of interpolating them. [cheat/ruleset restricted.] |
| `cl_legacystains` | `"1"` | WARNING: this cvar will default to 0 and later removed at some point |
| `cl_lerp_driftbias` | `"0"` | **Q1 target:** Additional bias, can be set to a negative value to hold interpolation in the past. |
| `cl_lerp_driftfrac` | `"0"` | **Q1 target:** Proportion of the latest time vs the older time to favour drifting towards. |
| `cl_lerp_maxdistance` | `"200"` | Maximum distance that an entity may move between snapshots without being considered as having teleported. |
| `cl_lerp_maxinterval` | `"0.3"` | Maximum interval between keyframes, in seconds. Larger values can result in entities drifting very slowly when they move sporadically. |
| `cl_lerp_players` | `"0"` | Set this to make other players smoother, though it may increase effective latency. Affects only QuakeWorld. |
| `cl_lerp_smooth` | `"2"` | **Q1 target:** If 2, will act as 1 when playing demos/singleplayer and otherwise act as if set to 0 (ie: deathmatch).<br>If 1, interpolation will run in the past, resulting in really smooth movement at the cost of latency (even on bunchy german ISDNs).<br>If 0, interpolation will be based upon packet arrival times and may judder due to packet loss. |
| `cl_loopbackprotocol` | `"qw"` | Which protocol to use for single-player/the internal client. Should be one of: qw, qwid, nqid, nq, fitz, bjp3, dp6, dp7, auto. If 'auto', will use qw protocols for qw mods, and nq protocols for nq mods. |
| `cl_maxfps` | `"250"` | Sets the maximum allowed framerate. If you're using vsync or want to uncap framerates entirely then you should probably set this to 0. Set cl_yieldcpu 0 if you're trying to benchmark. |
| `cl_maxfps_slop` | `"3"` | If a frame is delayed (eg because of poor system timer precision), this is how much sooner to pretend the frame happened (in milliseconds). If it is set too low then the average framerate will drop below the target, while too high may result in excessively fast frames. |
| `cl_maxPing` | on demand | **Q3:** OpenArena UI maximum-ping filter for server searches; distinct from the native sb_* browser filters. |
| `cl_model_bobbing` | `"0"` | Makes rotating pickup items bob too. |
| `cl_movement` | `"1"` | Specifies whether to send movement sequence info over DPP7 protocols (other protocols are unaffected). Unlike cl_nopred, this can result in different serverside behaviour. |
| `cl_movespeedkey` | `"2.0"` | Movement-speed multiplier while the speed modifier is held. |
| `cl_muzzleflash` | `"1"` | **Q1 target:** control rendering of weapon muzzle-flash effects. |
| `cl_netfps` | `"150"` | Send up to this many packets to the server per second. The rate used is also limited by the server which usually forces a cap to this setting of 77. Low packet rates can result in extra extrapolation to try to hide the resulting latencies. |
| `cl_noblink` | `"0"` | Disable the ^b text blinking feature. |
| `cl_nocsqc` | `"0"` | **Q1 target:** disable client-side QuakeC execution. |
| `cl_nodelta` | `"0"` | **Q1 target:** request full rather than delta-compressed entity updates, for diagnosis. |
| `cl_nofake` | `"2"` | value 0: permits \r chars in chat messages<br>value 1: blocks all \r chars<br>value 2: allows \r chars, but only from teammates |
| `cl_nolerp` | `"0"` | Disables interpolation. If set, missiles/monsters will be show exactly what was last received, which will be jerky. Does not affect players. A value of 2 means 'interpolate only in single-player/coop'. |
| `cl_nolerp_netquake` | `"0"` | Disables interpolation when connected to an NQ server. Does affect players, even the local player. You probably don't want to set this. |
| `cl_nopext` | `"0"` | **Q1 target:** disable FTE protocol-extension negotiation for compatibility. |
| `cl_nopred` | `"0"` | **Q1 target:** Disables clientside movement prediction. |
| `cl_parseFunChars` | `"1"` | **Q1 target:** expand special/fun-character escapes in chat. |
| `cl_parseSay` | `"1"` | **Q1 target:** expand engine teamplay macros in chat text. |
| `cl_parsewhitetext` | `"1"` | When parsing chat messages, enable support for messages like: red{white}red |
| `cl_part_density_fade` | `"1024"` | Specifies the distance over which ssqc pointparticles density fades from all to none. If this is set to 0 then particles will spawn at their normal density regardless of location on the map. |
| `cl_part_density_fade_start` | `"1024"` | Specifies the distance at which ssqc's pointparticles will start to get less dense. |
| `cl_paused` | dynamic / platform dependent | **Q3:** Pause state exposed to Quake 3 gamecode; normally engine-managed. |
| `cl_pext_mask` | `"0xffffffff"` | **Q1 target:** bitmask of protocol extensions the client permits negotiating. |
| `cl_pitchspeed` | `"150"` | Keyboard look-up/down rate in degrees per second. |
| `cl_playerclass` | `""` | Class selection userinfo for class-based Quake 1/Hexen II gamecode. |
| `cl_predict_extrapolate` | `""` | **Q1 target:** If 1, enables prediction based upon partial input frames which can change over time resulting in a swimmy feel but does not need to interpolate. If 0, prediction will stay in the past and thus use only completed frames. Interpolation will then be used to smooth movement.<br>This cvar only applies when video and input frames are independant (ie: cl_netfps is set). |
| `cl_predict_players` | `"1"` | **Q1 target:** Clear this cvar to see ents exactly how they are on the server. |
| `cl_predict_players_frac` | `"0.9"` | **Q1 target:** How much of other players to predict. Values less than 1 will help minimize overruns. |
| `cl_predict_players_latency` | `"1.0"` | **Q1 target:** Push the player back according to your latency, to give a smooth consistent simulation of the server. |
| `cl_predict_players_nudge` | `"0.02"` | **Q1 target:** An extra nudge of time, to cover video latency. |
| `cl_predict_timenudge` | `"0"` | **Q1 target:** A debug feature. You should normally leave this as 0. Nudges local player prediction into the future if positive (resulting in extrapolation), or into the past if negative (resulting in laggy interpolation). Value is in seconds, so small decimals are required. This cvar applies even if input frames are tied to video frames. |
| `cl_proxyaddr` | `""` | Proxy address used by the client's connection/proxy support. |
| `cl_prydoncursor` | `""` | **Q1 target:** cursor protocol for mods using Prydon-style point-and-click input. |
| `cl_pure` | `"0"` | 0=standard quake rules.<br>1=clients should prefer files within packages present on the server.<br>2=clients should use *only* files within packages present on the server.<br>Due to quake 1.01/1.06 differences, a setting of 2 is only reliable with total conversions.<br>If sv_pure is set, the client will prefer the highest value set. |
| `cl_queueimpulses` | `"0"` | Queues unsent impulses instead of replacing them. This avoids the need for extra wait commands (and the timing issues of such commands), but potentially increases latency and can cause scripts to be desynced with regard to buttons and impulses. |
| `cl_r2g` | `"0"` | Uses progs/grenade.mdl instead of progs/missile.mdl when 1. |
| `cl_rollalpha` | `"20"` | Controls the speed at which the view rolls according to sideways movement. |
| `cl_rollangle` | `"2.0"` | Controls the maximum view should tilt while strafing. |
| `cl_rollspeed` | `"200"` | Controls the speed required to reach cl_rollangle's tilt. |
| `cl_run` | `"0"` | Enables autorun, inverting the state of the +speed key. |
| `cl_running` | on demand | **Q3:** Quake 3 gamecode lookup for whether a client is running locally; engine/plugin-maintained where supported. |
| `cl_sbar` | `"0"` | **Q1 target:** force the classic status bar layout rather than the heads-up variant. |
| `cl_sbaralpha` (alias: `scr_sbaralpha`) | `"0.75"` | Specifies the transparency of the status bar. Only Takes effect when cl_sbar is set to 2. |
| `cl_sendchatstate` | `"1"` | Announce your chat state to the server in a privacy-violating kind of way. This allows other players to see your afk/at-console status. |
| `cl_sendguid` | `""` | Send a randomly generated 'globally unique' id to servers, which can be used by servers for score rankings and stuff. Different servers will see different guids. Delete the 'qkey' file in order to appear as a different user.<br>If set to 2, all servers will see the same guid. Be warned that this can show other people the guid that you're using. |
| `cl_serveraddress` | `"none"` | The address of the last server you connected to |
| `cl_servername` | `""` | The hostname of the last server you connected to |
| `cl_shownet` | `"0"` | Debugging var. 0 shows nothing. 1 shows incoming packet sizes. 2 shows individual messages. 3 shows entities too. |
| `cl_sidespeed` | `"400"` | Strafing movement-command speed in the engine input layer. |
| `cl_smartjump` | `"1"` | Makes the jump button act as +moveup when in water. This is typically quieter and faster. |
| `cl_solid_players` | `"1"` | **Q1 target:** Consider other players as solid for player prediction. |
| `cl_splitscreen` | `"0"` | Enables splitscreen support. See also: allow_splitscreen, in_rawinput*, the "p" command. |
| `cl_standardchat` | `"0"` | Disables auto colour coding in chat messages. |
| `cl_standardmsg` | `"0"` | Disables auto colour coding in console prints. |
| `cl_staticsounds` | `"1"` | Enable map static/looping sound emitters. |
| `cl_teamchatsound` | `"misc/talk.wav"` | Sound played for incoming team chat. |
| `cl_teamskin` (alias: `teamskin`) | `""` | **Q1 target:** override enemy/team player skins respectively. |
| `cl_threadedphysics` | `"0"` | When set, client input frames are generated and sent on a worker thread |
| `cl_timeNudge` | `"0"` | **Q3:** Quake 3 gamecode snapshot timing offset in milliseconds; different from Quake 1 prediction offsets. |
| `cl_timeout` | `"60"` | Seconds without server traffic before declaring the connection timed out. |
| `cl_triggers` | `"1"` | **Q1 target:** enable engine chat/game-event triggers. |
| `cl_truelightning` | `"0"` | Manipulate the end position of the player's own beams, to hide lag. Can be set to fractional values to reduce the effect. [cheat/ruleset restricted.] |
| `cl_upspeed` | `"400"` | Vertical movement-command speed in the engine input layer. |
| `cl_verify_urischeme` (alias: `cl_verify_qwprotocol`) | `"0"` | 0: Do nothing.<br>1: Check whether our protocol scheme is registered and prompt the user to register associations.<br>2: Always re-register on every startup, without prompting. Sledgehammer style. |
| `cl_voip_autogain` | `"0"` | Attempts to normalize your voice levels to a standard level. Useful for lazy people, but interferes with voice activation levels. |
| `cl_voip_bitrate` | `"3000"` | For codecs with non-specific bitrates, this specifies the target bitrate to use. |
| `cl_voip_capturedevice` | `""` | Voice-chat input device selection; empty chooses the default device. |
| `cl_voip_capturingvol` | `"0.5"` | Volume multiplier applied while capturing, to avoid your audio from being heard by others. Does not affect game volume when others speak (minimum of cl_voip_capturingvol and cl_voip_ducking is used). |
| `cl_voip_codec` | `""` | 0: speex(@11khz). 1: raw. 2: opus. 3: speex(@8khz). 4: speex(@16). 5:speex(@32). 6: pcma. 7: pcmu. |
| `cl_voip_ducking` | `"0.5"` | Scales game audio by this much when someone is talking to you. Does not affect your speaker volume when you speak (minimum of cl_voip_capturingvol and cl_voip_ducking is used). |
| `cl_voip_micamp` | `"2"` | Amplifies your microphone when using voip. |
| `cl_voip_noisefilter` | `"1"` | Enable the use of the noise cancelation filter. |
| `cl_voip_play` | `"1"` | Enables voip playback. Value is a volume scaler. |
| `cl_voip_send` | `"0"` | Sends voice-over-ip data to the server whenever it is set.<br>0: only send voice if +voip is pressed.<br>1: voice activation.<br>2: constantly send.<br>+4: Do not send to game, only to rtp sessions. |
| `cl_voip_showmeter` | `"1"` | Shows your speech volume above the standard hud. 0=hide, 1=show when transmitting, 2=ignore voice-activation disable |
| `cl_voip_test` | `"0"` | If 1, enables you to hear your own voice directly, bypassing the server and thus without networking latency, but is fine for checking audio levels. Note that sv_voip_echo can be set if you want to include latency and packetloss considerations, but setting that cvar requires server admin access and is thus much harder to use. |
| `cl_voip_vad_delay` | `"0.3"` | Keeps sending voice data for this many seconds after voice activation would normally stop |
| `cl_voip_vad_threshhold` | `"15"` | This is the threshhold for voice-activation-detection when sending voip data |
| `cl_voipSendTarget` | on demand | **Q3:** OpenArena gamecode voice-recipient preference. This spelling is not connected to the fork's engine voice sender; cl_voip_send controls activation, not recipient selection. |
| `cl_vrui_force` | `"0"` | Force the use of VR UIs, even with no VR headset active. |
| `cl_vrui_lock` | `"1"` | Controls how the UI is positioned when using VR/XR. 0: Repositioned infront of the head when the console/menus are toggled. 1: Locked infront of the reference position (which may require the user to turn around to find it). |
| `cl_warncmd` | `"0"` | Print warnings for commands the engine cannot resolve. |
| `cl_weaponforgetorder` | `"0"` | The 'weapon' command will lock in its weapon choice, instead of choosing a different weapon between select+fire. |
| `cl_weaponhide` | `"0"` | HACK: Attempt to switch weapon to another in order to cheat your killer out of any possible weapon upgrades.<br>0: original behaviour<br>1: switch away when +attack/+fire is released<br>2: switch away only in deathmatch |
| `cl_weaponhide_preference` (alias: `cl_weaponhide_axe`) | `"2 1"` | The weapon you would like to try to switch to when cl_weaponhide is active |
| `cl_weaponpreselect` | `"0"` | HACK: Controls the interaction between the weapon and +attack commands (does not affect impulse).<br>0: weapon switch happens instantly<br>1: weapon switch happens on next attack<br>2: instant only when already firing, otherwise delayed<br>3: delay until new attack only in deathmatch 1<br>4: delay until any attack only in deathmatch 1 |
| `cl_yawspeed` | `"140"` | Keyboard turning rate in degrees per second. |
| `cl_yieldcpu` | `"1"` | Attempt to yield between frames. This can resolve issues with certain drivers and background software, but can mean less consistant frame times. Will reduce power consumption/heat generation so should be set on laptops or similar (over-hot/battery powered) devices. |
| `cmd_allowaccess` | `"0"` | Permit command access/restriction changes through the command permission system. |
| `cmd_gamecodelevel` | `"(29)"` | Permission level assigned to commands issued by gamecode. |
| `color1` | on demand | **Q3:** primary/secondary railgun and player color selection, sent in userinfo; created by the player-settings UI/config. |
| `color2` | on demand | **Q3:** primary/secondary railgun and player color selection, sent in userinfo; created by the player-settings UI/config. |
| `com_ansiColor` | on demand | **Q3:** gamecode console output color-escape preference; distinct from the engine's sys_colorconsole. |
| `com_blood` | `"1"` | **Q3:** Enable blood effects generated by Quake 3 gamecode. |
| `com_buildScript` | `"0"` | **Q3:** Asset-build/developer mode that forces gamecode to load assets and report missing files. |
| `com_cameraMode` | `"0"` | **Q3:** Gamecode camera/demo mode indicator. |
| `com_fullgamename` (alias: `fs_gamename`) | Q1: `"Quake"`; Q3: `"Quake III Arena"` | The filesystem is trying to run this game |
| `com_gamedirnativecode` | Q1: `"0"`; Q3: `"1"` | FTE QW blocks all downloads of files with a .dll or .so extension, however other engines (eg: ezquake and fodquake) do not - this omission can be used to trigger delayed eremote exploits in any engine (including FTE) which is later run from the same gamedir.<br>Quake2, Quake3(when debugging), and KTX typically run native gamecode from within gamedirs, so if you wish to run any of these games you will need to ensure this cvar is changed to 1, as well as ensure that you don't run unsafe clients. |
| `com_highlightcolor` | `"12"` | ANSI colour to be used for highlighted text, used when com_parseutf8 is active. |
| `com_parseutf8` | `"0"` | Interpret console messages/playernames/etc as UTF-8. Requires special fonts. -1=iso 8859-1. 0=quakeascii(chat uses high chars). 1=utf8, revert to ascii on decode errors. 2=utf8 ignoring errors. The [Quake rerelease profile](QuakeRemaster.md#launch) defaults to `1`. |
| `com_protocolname` (alias: `com_gamename`) | Q1: `"FTE-Quake DarkPlaces-Quake"`; Q3: `"Quake3"` | The protocol game name used for dpmaster queries. For compatibility with DP, you can set this to 'DarkPlaces-Quake' in order to be listed in DP's master server, and to list DP servers. |
| `com_protocolversion` | Q1: `"3"`; Q3: `"68"` | **Different:** The protocol version used for dpmaster queries. Quake 3 UI VM requests for protocol are translated to this setting; protocol is not an engine console alias. |
| `con_centernotify` | `"0"` | Center engine notification text. |
| `con_displaypossibilities` | `"1"` | Show possible console completions. |
| `con_echochat` | `"0"` | Echo chat into the engine console. |
| `con_logcenterprint` | `"1"` | Specifies whether to print centerprints on the console.<br>0: never<br>1: single-player or coop only.<br>2: always. |
| `con_maxlines` | `"1024"` | Maximum console scrollback lines. |
| `con_notify_w` | `"1"` | Width of the engine's on-screen notification area. |
| `con_notify_x` | `"0"` | Position of the engine's on-screen notification area. |
| `con_notify_y` | `"0"` | Position of the engine's on-screen notification area. |
| `con_notifylines` | `"4"` | Number of visible notification lines. |
| `con_notifytime` | `"3"` | Seconds for ordinary console notifications to remain visible. |
| `con_notifytime_chat` | `"8"` | Seconds for chat notifications to remain visible. |
| `con_numnotifylines_chat` | `"8"` | Number of visible chat notification lines. |
| `con_ocranaleds` | `"2"` | Replace decorative LED glyphs with text suitable for copying/OCR. |
| `con_savehistory` | `"1"` | Write/update conhistory.txt |
| `con_separatechat` | `"0"` | Maintain a separate chat notification area. |
| `con_showcompletion` | `"1"` | Console completion-preview preference. |
| `con_stayhidden` | Q1: `"0"`; Q3: `"1"` | 0: allow console to pounce on the user<br>1: console stays hidden unless explicitly invoked<br>2:toggleconsole command no longer works<br>3: shift+escape key no longer works |
| `con_textfont` (alias: `gl_consolefont`) | `""` | Specifies the font file to use. a value such as FONT:ALTFONT specifies an alternative font to be used when ^a is used.<br>When using TTF fonts, you will likely need to scale text to at least 150% - vid_conautoscale 1.5 will do this.<br>TTF fonts may be loaded from your windows directory. 'gl_font cour?col=1,1,1:couri?col=0,1,0' loads eg: c:\windows\fonts\cour.ttf, and uses the italic version of courier for alternative text, with specific colour tints. |
| `con_textsize` | `"8"` | Resize the console text to be a different height, scaled separately from the hud. The value is the height in (virtual) pixels. |
| `con_timeformat` | `"(%H:%M:%S) "` | Format of console timestamps. |
| `con_timestamps` | `"0"` | Enable console timestamps. |
| `con_window` | `"0"` | States whether the console should be a floating window as in source engine games, or a top-of-the-screen-only thing. |
| `contrast` (alias: `v_contrast`) | `"1.0"` | Scales colour values linearly to make your screen easier to see. Setting this to anything but 1 without hardware gamma will reduce your framerates a little. |
| `coop` | `""` | **Q1 target:** cooperative-game setting interpreted by QuakeC; Quake 3 game types are selected with g_gametype. |
| `crosshair` | `"1"` | **Different:** Engine crosshair selection. Quake 1 uses this; Quake 3/OpenArena draws its gamecode crosshair with cg_drawCrosshair. |
| `crosshairalpha` | `"1"` | Opacity of the engine crosshair. |
| `crosshaircolor` | `"255 255 255"` | Color of the engine crosshair. |
| `crosshaircorrect` | `"0"` | Moves the crosshair around to represent the impact point of a weapon positioned below the actual view position. [cheat/ruleset restricted.] |
| `crosshairimage` | `""` | Enables the use of an external/custom crosshair image |
| `crosshairsize` | `"8"` | Scale of the engine crosshair. |

### Cvars: D–H

| Cvar (aliases) | Default | Definition / game notes |
| --- | --- | --- |
| `d1` | on demand | **Q3:** optional demo/map-cycle script value read during end-of-match handling. |
| `d_lodbias` (alias: `gl_texture_lodbias`) | `"0"` | **Different:** Biases choice of mipmap levels. Positive values will give more blury textures, while negative values will give crisper images (but will also give some mid-surface aliasing artifacts). Quake 3 UI VM requests for r_lodBias are translated to this setting; r_lodBias is not an engine console alias. |
| `d_mipcap` (alias: `gl_miptexLevel`) | `"0 1000"` | Specifies the range of mipmap levels to use. |
| `deathmatch` | `""` | **Q1 target:** deathmatch setting interpreted by QuakeC; Quake 3 uses g_gametype instead. |
| `debug_protocol` | on demand | **Q3:** OpenArena server-browser protocol override used for development. |
| `dedicated` | `"0"` | **Q3:** Dedicated-server mode reported to gamecode; normally established at startup. |
| `developer` | `"0"` | Enables the spewing of additional developer/debugging messages. 2 will give even more spam, much of it unwanted. |
| `disable_ammo_bullets` | `"0"` | **Q3:** Suppress spawning of ammo bullets pickups in OpenArena gamecode. |
| `disable_ammo_cells` | `"0"` | **Q3:** Suppress spawning of ammo cells pickups in OpenArena gamecode. |
| `disable_ammo_grenades` | `"0"` | **Q3:** Suppress spawning of ammo grenades pickups in OpenArena gamecode. |
| `disable_ammo_rockets` | `"0"` | **Q3:** Suppress spawning of ammo rockets pickups in OpenArena gamecode. |
| `disable_ammo_shells` | `"0"` | **Q3:** Suppress spawning of ammo shells pickups in OpenArena gamecode. |
| `disable_item_armor_combat` | `"0"` | **Q3:** Suppress spawning of item armor combat pickups in OpenArena gamecode. |
| `disable_item_armor_shard` | `"0"` | **Q3:** Suppress spawning of item armor shard pickups in OpenArena gamecode. |
| `disable_item_health` | `"0"` | **Q3:** Suppress spawning of item health pickups in OpenArena gamecode. |
| `disable_item_health_large` | `"0"` | **Q3:** Suppress spawning of item health large pickups in OpenArena gamecode. |
| `disable_item_health_mega` | `"0"` | **Q3:** Suppress spawning of item health mega pickups in OpenArena gamecode. |
| `disable_item_invis` | `"0"` | **Q3:** Suppress spawning of item invis pickups in OpenArena gamecode. |
| `disable_item_quad` | `"0"` | **Q3:** Suppress spawning of item quad pickups in OpenArena gamecode. |
| `disable_weapon_grenadelauncher` | `"0"` | **Q3:** Suppress spawning of weapon grenadelauncher pickups in OpenArena gamecode. |
| `disable_weapon_lightning` | `"0"` | **Q3:** Suppress spawning of weapon lightning pickups in OpenArena gamecode. |
| `disable_weapon_plasmagun` | `"0"` | **Q3:** Suppress spawning of weapon plasmagun pickups in OpenArena gamecode. |
| `disable_weapon_rocketlauncher` | `"0"` | **Q3:** Suppress spawning of weapon rocketlauncher pickups in OpenArena gamecode. |
| `disable_weapon_shotgun` | `"0"` | **Q3:** Suppress spawning of weapon shotgun pickups in OpenArena gamecode. |
| `dmflags` | `"0"` | **Q3:** Quake 3 deathmatch rule bitmask; bits and enforcement belong to qagame. In Quake 1 only supporting mods use it. |
| `dpcompat_console` | `"0"` | **Q1 target:** Enables hacks to emulate DP's console. |
| `dpcompat_csqcinputeventtypes` | `"999999"` | **Q1 target:** Specifies the first csqc input event that the mod does not recognise. This should never have been a thing, but some mods are simply too buggy. |
| `dpcompat_findradiusarealinks` | `"0"` | **Q1 target:** Use the world collision info to accelerate findradius instead of looping through every single entity. May actually be slower for large radiuses, or fail to find entities which have not been linked properly with setorigin. |
| `dpcompat_nofloodfill` | `"0"` | Disables the q1mdl floodfill. Setting this to 1 may result in blue seams on the vanilla quake models. |
| `dpcompat_nopremulpics` | `"0"` | By default FTE uses premultiplied alpha for hud/2d images, while DP does not (which results in halos with low-res content). Unfortunately DDS files would need to be recompressed, resulting in visible issues. |
| `dpcompat_nopreparse` | `"0"` | **Q1 target:** Xonotic uses svc_tempentity with unknowable lengths mixed with other data that needs to be translated. This cvar disables any attempt to translate or pre-parse network messages, including disabling nq/qw cross compatibility. NOTE: because preparsing will be disabled, messages might not get backbuffered correctly if too much reliable data is written. |
| `dpcompat_noretouchground` | `"0"` | Prevents entities that are already standing on an entity from touching the same entity again. |
| `dpcompat_precachesoundhack` | `"0"` | **Q1 target:** Changes the behaviour of only the ssqc's precache_sound to return a precache index. precache_model and csqc's precaches are unchanged. |
| `dpcompat_set` | `"0"` | **Q1 target:** DarkPlaces-compatible console set-command behavior. |
| `dpcompat_skinfiles` | `"0"` | When set, uses a nodraw shader for any unmentioned surfaces. |
| `dpcompat_smallerfonts` | `"0"` | Mimics DP's behaviour of using a smaller font size than was actually requested. |
| `dpcompat_stats` | `"0"` | **Q1 target:** DarkPlaces-compatible client statistic handling. |
| `dpcompat_strcat_limit` | `""` | **Q1 target:** When set, cripples strcat (and related function) string lengths to the value specified.<br>Set to 16383 to replicate DP's limit, otherwise leave as 0 to avoid limits. |
| `drate` | `"3000000"` | A rough measure of the bandwidth to try to use while downloading (in bytes per second). |
| `dtls_psk_hint` | not exposed | For DTLS-PSK handshakes. This specifies the public server identity. |
| `dtls_psk_key` | not exposed | For DTLS-PSK handshakes. This specifies the hexadecimal key which must match between client+server. Will only be used when client+server's hint settings match. |
| `dtls_psk_user` | not exposed | For DTLS-PSK handshakes. This specifies the username to use when the client+server's hints match. |
| `edit_addcr` | `""` | make sure that each line ends with a \r (on save). Empty will be assumed to be 1 on windows and 0 otherwise. |
| `edit_stripcr` | `"1"` | remove \r from eols (on load) |
| `elimflags` | `"0"` | **Q3:** OpenArena elimination-rule bitmask. |
| `elimination_activewarmup` | `"5"` | **Q3:** Active warmup interval before an elimination round. |
| `elimination_bfg` | `"0"` | **Q3:** Starting ammunition/availability for bfg in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_chain` | `"0"` | **Q3:** Starting ammunition/availability for chain in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_ctf_oneway` | `"0"` | **Q3:** Use attack/defend one-way CTF elimination rules. |
| `elimination_grapple` | `"0"` | **Q3:** Starting ammunition/availability for grapple in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_grenade` | `"100"` | **Q3:** Starting ammunition/availability for grenade in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_lightning` | `"300"` | **Q3:** Starting ammunition/availability for lightning in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_lockspectator` | `"0"` | **Q3:** Restrict eliminated players' spectator views. |
| `elimination_machinegun` | `"500"` | **Q3:** Starting ammunition/availability for machinegun in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_mine` | `"0"` | **Q3:** Starting ammunition/availability for mine in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_nail` | `"0"` | **Q3:** Starting ammunition/availability for nail in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_plasmagun` | `"200"` | **Q3:** Starting ammunition/availability for plasmagun in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_railgun` | `"20"` | **Q3:** Starting ammunition/availability for railgun in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_rocket` | `"50"` | **Q3:** Starting ammunition/availability for rocket in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_roundtime` | `"120"` | **Q3:** Elimination round duration. |
| `elimination_selfdamage` | `"0"` | **Q3:** Allow self-damage during elimination rounds. |
| `elimination_shotgun` | `"500"` | **Q3:** Starting ammunition/availability for shotgun in OpenArena elimination rounds; the gamecode defines weapon-specific interpretation. |
| `elimination_startArmor` | `"150"` | **Q3:** Starting armor/health for elimination rounds. |
| `elimination_startHealth` | `"200"` | **Q3:** Starting armor/health for elimination rounds. |
| `elimination_warmup` | `"7"` | **Q3:** Warmup duration before an elimination round. |
| `enemycolor` | `"off"` | **Q1 target:** forced enemy/team player colors respectively. |
| `ezcompat_markup` | `"1"` | Attempt compatibility with ezquake's text markup.0: disabled.<br>1: Handle markup ampersand markup.<br>2: Handle chevron markup (only in echo commands, for config compat, because its just too unreliable otherwise). |
| `fbskins` | `""` | **Q1 target:** server allowance for fullbright player skins. |
| `ffov` | `""` | Allows you to set a specific field of view for when a custom projection is specified. If empty, will use regular fov cvar, which might get messy. |
| `filterban` | `"1"` | If 0, players will be kicked by default unless there is a rule that allows them. Also affects the default action of addip. |
| `forceqmenu` | `"0"` | Select/force the Quake-style native menu instead of automatic game-specific menu selection. |
| `fov` (alias: `cg_fov`) | `"90"` | Shared saved field of view; `cg_fov` is a real console alias. Native **Options → Field of View** offers 70–170 in steps of 5. Console values above 170 work; Regular perspective renders up to 179. **Different:** Quake 1 uses engine view calculation; Quake 3/OpenArena uses cgame with this fork’s wide-FOV adjustment. Gamecode zoom, intermission cameras and server-enforced FOV limits are preserved. |
| `fraglimit` | `""` | **Different:** Frag/score limit. Quake 1 interpretation depends on QuakeC; Quake 3 qagame owns match scoring and completion. |
| `fraglog_public` | `"1"` | Enables support for connectionless fraglog requests |
| `fs_basepath` | not exposed | Provided for Q2/Q3 compat. System path of the base directory. |
| `fs_cache` | `"1"` | 0: Do individual lookups.<br>1: Scan all files for accelerated lookups. This provides a performance boost on windows and avoids case sensitivity issues on linux.<br>2: like 1, but don't bother checking for external changes (avoiding the cost of rebuild the cache). |
| `fs_game` (alias: `game`) | `""` | Provided for Q2 compat. Contains the subdir of the current mod. |
| `fs_gamepath` | not exposed | Provided for Q2/Q3 compat. System path of the active gamedir. |
| `fs_hidesyspaths` | `"1"` | 0: Show system paths in console prints that might appear in screenshots or video capture.<br>1: Replace the start of filenames in console prints with generic prefixes that cannot leak private info like your operating system's user name. |
| `fs_homepath` | not exposed | Provided for Q2/Q3 compat. System path of the base directory. |
| `fs_packageprioritisation` | not exposed | Favours the package that is:<br>0: Most recently modified<br>1: Is alphabetically last (favour z over a, 9 over 0). |
| `g_admin` | `"admin.dat"` | **Q3:** OpenArena administration configuration filename. |
| `g_adminLog` | `"admin.log"` | **Q3:** OpenArena administrator-action log filename. |
| `g_adminMaxBan` | `"2w"` | **Q3:** Maximum ban duration allowed to administrators without permanent-ban permission. |
| `g_adminNameProtect` | `"1"` | **Q3:** Protect registered administrator names from impersonation. |
| `g_adminParseSay` | `"1"` | **Q3:** Parse administrator commands embedded in chat. |
| `g_adminTempBan` | `"2m"` | **Q3:** Default duration for temporary administrator bans. |
| `g_allowVote` | `"1"` | **Q3:** Permit players to call votes. |
| `g_altExcellent` | `"0"` | **Q3:** OpenArena alternate criteria for the Excellent award. |
| `g_arenasFile` | `""` | **Q3:** Override filename for arena/map metadata. |
| `g_autonextmap` | `"0"` | **Q3:** Automatically select the next map from configured map pools. |
| `g_awardpushing` | `"1"` | **Q3:** Award kills for pushing opponents into environmental hazards. |
| `g_banIPs` | `""` | **Q3:** Gamecode IP-filter list; distinct from the engine's address-penalty list. |
| `g_blueTeam` | `""` | **Q3:** Names of the blue/red team respectively. |
| `g_blueTeamClientNumbers` | `"0"` | **Q3:** Gamecode-maintained team member lists used by client voice/team features. |
| `g_botsFile` | `""` | **Q3:** Override filename for bot metadata. |
| `g_catchup` | `"0"` | **Q3:** Enable OpenArena catch-up assistance for weaker players. |
| `g_cubeTimeout` | `"30"` | **Q3:** Lifetime of dropped skull/cube items in Harvester. |
| `g_damageModifier` | `"0"` | **Q3:** Global gamecode damage multiplier. |
| `g_debugAlloc` | `"0"` | **Q3:** Print gamecode memory-allocation diagnostics. |
| `g_debugDamage` | `"0"` | **Q3:** Print damage calculation diagnostics. |
| `g_debugMove` | `"0"` | **Q3:** Print gamecode movement diagnostics. |
| `g_delagHitscan` | `"0"` | **Q3:** Enable server hit-scan lag compensation. |
| `g_doWarmup` | `"0"` | **Q3:** Enable pre-match warmup. |
| `g_elimination` | `"0"` | **Q3:** Enable elimination-style behavior outside the dedicated elimination game type where supported. |
| `g_enableBreath` | `"0"` | **Q3:** Generate player breath effects. |
| `g_enableDust` | `"0"` | **Q3:** Generate dust effects on supported map surfaces. |
| `g_filterBan` | `"1"` | **Q3:** Treat the IP filter list as a deny list when enabled, or an allow list when disabled. |
| `g_floodMaxDemerits` | `"5000"` | **Q3:** OpenArena gamecode flood-protection penalty threshold. |
| `g_floodMinTime` | `"2000"` | **Q3:** Minimum message interval used by OpenArena flood protection. |
| `g_forcerespawn` | `"20"` | **Q3:** Maximum delay before forcing a dead player to respawn. |
| `g_friendlyFire` | `"0"` | **Q3:** Allow teammates to damage each other. |
| `g_gametype` | Q1: `""`; Q3: `"0"` | **Q3:** Quake 3 game type. OpenArena: 0 FFA, 1 tournament, 2 single-player, 3 team deathmatch, 4 CTF, 5 one-flag, 6 overload, 7 harvester, 8 elimination, 9 CTF elimination, 10 LMS, 11 double domination, 12 domination. Other mods can define different values. |
| `g_gravity` | `"800"` | **Q3:** Quake 3 gamecode gravity; distinct from Quake 1's sv_gravity. |
| `g_gravityModifier` | `"1"` | **Q3:** OpenArena gravity multiplier applied to the base gamecode gravity. |
| `g_humanplayers` | `"0"` | **Q3:** Human-player count reported by gamecode; informational. |
| `g_inactivity` | `"0"` | **Q3:** Time before an inactive player is removed/moved according to gamecode policy. |
| `g_instantgib` | `"0"` | **Q3:** Enable instant-kill/instagib mode. |
| `g_knockback` | `"1000"` | **Q3:** Knockback scale used by damage calculations. |
| `g_lagLightning` | `"1"` | **Q3:** Lightning-gun lag-compensation policy. |
| `g_listEntity` | `"0"` | **Q3:** Dump the game entity list for diagnosis. |
| `g_lms_lives` | `"1"` | **Q3:** Starting lives for Last Man Standing. |
| `g_lms_mode` | `"0"` | **Q3:** Last Man Standing scoring/round mode selection. |
| `g_log` | `"games.log"` | **Q3:** Gamecode match-log filename. |
| `g_logsync` | `"0"` | **Q3:** Flush gamecode log writes synchronously. |
| `g_mappools` | game-type map-pool file mapping | **Q3:** Game-type-to-map-pool configuration file mapping used for automatic map selection. |
| `g_maxGameClients` | `"0"` | **Q3:** Maximum active players; excess clients are spectators when enforced by qagame. |
| `g_maxNameChanges` | `"50"` | **Q3:** Maximum name changes permitted by gamecode. |
| `g_maxVotes` | `"3"` | **Q3:** Maximum votes a player can initiate during a match. |
| `g_maxWarnings` | `"3"` | **Q3:** Warning-count threshold for automatic punishment. |
| `g_minNameChangePeriod` | `"10"` | **Q3:** Minimum time between name changes. |
| `g_motd` | `""` | **Q3:** Gamecode message of the day. |
| `g_motdfile` | `"motd.cfg"` | **Q3:** Filename for an extended message of the day. |
| `g_music` | `""` | **Q3:** Override map music track via gamecode. |
| `g_needpass` | `"0"` | **Q3:** Whether the gamecode server requires a password; informational. |
| `g_obeliskHealth` | `"2500"` | **Q3:** Initial obelisk health in Overload. |
| `g_obeliskRegenAmount` | `"15"` | **Q3:** Health restored per obelisk regeneration step. |
| `g_obeliskRegenPeriod` | `"1"` | **Q3:** Time between obelisk regeneration steps. |
| `g_obeliskRespawnDelay` | `"10"` | **Q3:** Delay before a destroyed obelisk respawns. |
| `g_password` | `""` | **Q3:** Gamecode server-join password. Keep it private; engine password/userinfo handling also applies. |
| `g_podiumDist` | `"80"` | **Q3:** Distance used to position the winner on the end-game podium. |
| `g_podiumDrop` | `"70"` | **Q3:** Vertical offset used for the end-game podium. |
| `g_proxMineTimeout` | `"20000"` | **Q3:** Lifetime of an armed proximity mine. |
| `g_publicAdminMessages` | `"1"` | **Q3:** Broadcast administrator action messages. |
| `g_quadfactor` | `"3"` | **Q3:** Damage multiplier for the Quad powerup. |
| `g_rankings` | `"0"` | **Q3:** Gamecode ranking-service preference; requires supporting infrastructure. |
| `g_redTeam` | `""` | **Q3:** Names of the blue/red team respectively. |
| `g_redTeamClientNumbers` | `"0"` | **Q3:** Gamecode-maintained team member lists used by client voice/team features. |
| `g_regen` | `"0"` | **Q3:** OpenArena regeneration-mode setting. |
| `g_respawntime` | `"0"` | **Q3:** Gamecode respawn timing setting. |
| `g_restarted` | `"0"` | **Q3:** Internal gamecode marker for a map restart. |
| `g_rockets` | `"0"` | **Q3:** Enable the OpenArena rockets-only mode. |
| `g_runes` | `"0"` | **Q3:** Enable persistent rune/powerup items in supported OpenArena modes. |
| `g_smoothClients` | `"1"` | **Q3:** Enable gamecode smoothing/extrapolation of other clients. |
| `g_spAwards` | `""` | **Q3:** Saved single-player award progress. |
| `g_spawnprotect` | `"500"` | **Q3:** Post-spawn protection duration in supporting OpenArena gamecode. |
| `g_specChat` | `"1"` | **Q3:** Permit spectator chat to reach active players. |
| `g_speed` | `"320"` | **Q3:** Quake 3 player movement speed; distinct from Quake 1's sv_maxspeed. |
| `g_spreeDiv` | `"5"` | **Q3:** Interval/divisor used by OpenArena killing-spree messages. |
| `g_sprees` | `"sprees.dat"` | **Q3:** Enable OpenArena killing-spree messages/effects. |
| `g_spScores1` | `""` | **Q3:** Saved single-player scores at skill level 1. |
| `g_spScores2` | `""` | **Q3:** Saved single-player scores at skill level 2. |
| `g_spScores3` | `""` | **Q3:** Saved single-player scores at skill level 3. |
| `g_spScores4` | `""` | **Q3:** Saved single-player scores at skill level 4. |
| `g_spScores5` | `""` | **Q3:** Saved single-player scores at skill level 5. |
| `g_spSkill` | `"2"` | **Q3:** Single-player bot difficulty selected by the game UI. |
| `g_spVideos` | `""` | **Q3:** Saved single-player cinematic unlock/progress state. |
| `g_synchronousClients` | `"0"` | **Q3:** Run client commands synchronously for gamecode movement debugging. |
| `g_teamAutoJoin` | `"0"` | **Q3:** Automatically assign joining players to a team. |
| `g_teamForceBalance` | `"0"` | **Q3:** Reject team changes that would unbalance teams. |
| `g_timestamp` | dynamic / platform dependent | **Q3:** Gamecode match timestamp; engine/game managed. |
| `g_truePing` | `"0"` | **Q3:** Use OpenArena's gamecode ping calculation. |
| `g_vampire` | `"0.0"` | **Q3:** Health gained from damage dealt in OpenArena vampire mode. |
| `g_vampire_max_health` | `"500"` | **Q3:** Maximum health in OpenArena vampire mode. |
| `g_voteBan` | `"0"` | **Q3:** Temporary-ban duration/policy for kick votes. |
| `g_votecustomfile` | `"votecustom.cfg"` | **Q3:** Filename containing configured custom vote commands. |
| `g_voteGametypes` | `"/0/1/3/4/5/6/7/8/9/10/11/12/"` | **Q3:** Game types allowed in votes. |
| `g_votemapsfile` | `"votemaps.cfg"` | **Q3:** Filename containing maps allowed in map votes. |
| `g_voteMaxFraglimit` | `"0"` | **Q3:** Minimum/maximum frag limit permitted in votes. |
| `g_voteMaxTimelimit` | `"1000"` | **Q3:** Minimum/maximum time limit permitted in votes. |
| `g_voteMinFraglimit` | `"0"` | **Q3:** Minimum/maximum frag limit permitted in votes. |
| `g_voteMinTimelimit` | `"0"` | **Q3:** Minimum/maximum time limit permitted in votes. |
| `g_voteNames` | `"/map_restart/nextmap/map/g_gametype/kick/clientkick/g_doWarmup/timelimit/fraglimit/shuffle/"` | **Q3:** Allow vote targets to be specified by player name. |
| `g_warmup` | `"20"` | **Q3:** Warmup duration, in seconds. |
| `g_warningExpire` | `"3600"` | **Q3:** Expiry time for accumulated administrator warnings. |
| `g_weaponrespawn` | `"5"` | **Q3:** Respawn delay for weapon pickups. |
| `g_weaponTeamRespawn` | `"30"` | **Q3:** Weapon-pickup respawn delay in team games. |
| `gamecfg` | `"0"` | **Q1 target:** gamecode-owned configuration value; meaning is defined by the mod. |
| `gamedate` | dynamic / platform dependent | **Q3:** Build date of the loaded gamecode; informational. |
| `gamename` | dynamic / platform dependent | **Q3:** Name of the loaded gamecode; informational. |
| `gameversion` | `""` | gamecode version for server browsers |
| `gameversion_max` | `""` | gamecode version for server browsers |
| `gameversion_min` | `""` | gamecode version for server browsers |
| `gamma` (alias: `v_gamma`) | `"1.0"` | Display gamma curve, applied by the SDL3 GPU presentation shader. Direction follows v_gammainverted; use the native brightness slider to match the active game profile. vid_sceneonlygamma selects scene-only correction. Quake 3 UI requests for r_gamma map to this setting; r_gamma is not a console alias. |
| `gl_affinemodels` | `""` | Legacy affine model-texture-mapping preference; retained for compatibility with reference renderer code. |
| `gl_blendsprites` | `"0"` | Specifies how sprites are blended.<br>0: Alpha tested.<br>1: Premultiplied blend.<br>2: Additive blend. |
| `gl_conback` | `""` | Specifies which conback shader/image to use. The Quake fallback is gfx/conback.lmp |
| `gl_cshiftpercent` | `"100"` | **Q1 target:** strength of engine color shifts from damage, contents and powerups. |
| `gl_driver` | `""` | Specifies the graphics driver name to load. This is typically a filename. Blank for default. [applies on video restart.] |
| `gl_flashblend` | `"0"` | **Q1 target:** draw dynamic-light flash/glow geometry instead of normal surface-light effects where supported. |
| `gl_flashblendscale` | `"0.35"` | Scale of flash-blend dynamic-light glow geometry. |
| `gl_font` | `""` | Specifies the font file to use. a value such as FONT:ALTFONT specifies an alternative font to be used when ^a is used.<br>When using TTF fonts, you will likely need to scale text to at least 150% - vid_conautoscale 1.5 will do this.<br>TTF fonts may be loaded from your windows directory. 'gl_font cour?col=1,1,1:couri?col=0,1,0' loads eg: c:\windows\fonts\cour.ttf, and uses the italic version of courier for alternative text, with specific colour tints. |
| `gl_load24bit` | `"1"` | Enable loading external true-color texture replacements. |
| `gl_max_size` | `"8192"` | Specifies the maximum texture size that the engine may use. Textures larger than this will be downsized. Clamped by the value the driver supports. |
| `gl_maxdist` (alias: `gl_farclip`) | `"0"` | The distance of the far clip plane. If set to 0, some fancy maths will be used to place it at an infinite distance. |
| `gl_menutint_shader` | `""` | Material/shader name used for engine menu tinting. |
| `gl_mindist` (alias: `r_nearclip`) | `"1"` | Distance to the near clip plane. Smaller values may damage depth precision, high values can potentialy be used to see through walls... |
| `gl_outline` | `"0"` | Draw some stylised outlines. |
| `gl_outline_width` | `"2"` | The width of those outlines. |
| `gl_overbright` | Q1: `"1"`; Q3: `"2"` | **Different:** Allow Quake-style overbright lighting; also controls baked Quake 3 lightmap decoding in this fork. Presets and game profiles can change it. |
| `gl_overbright_all` | `"0"` | Apply overbright handling beyond the usual world-lightmap surfaces. |
| `gl_overbright_models` | `"0"` | Doubles the brightness of models, to match QuakeSpasm's misfeature of the same name. [cheat/ruleset restricted.] |
| `gl_part_flame` | `"1"` | Enable particle emitting from models. Mainly used for torch and flame effects. |
| `gl_polyblend` | `"1"` | Controls whether temporary whole-screen colour changes should be honoured or not. Change gl_cshiftpercent if you want to adjust the intensity.<br>This does not affect v_cshift commands sent from the server. |
| `gl_polyblend_edgesize` | `"128"` | This constrains colour shifts to the edge of the screen, with the value specifying the size of those borders. |
| `gl_shadeq1_name` | `"*"` | Rename all surfaces from quake1 bsps using this pattern for the purposes of shader names. |
| `gl_shaftlight` | `"0.8"` | **Q1 target:** lightning-gun beam light strength. |
| `gl_simpleitems` | `"0"` | Replace models with simpler sprites. |
| `gl_skyboxdist` | `"0"` | The distance of the skybox. If 0, the engine will determine it based upon the far clip plane distance. |
| `gl_specular` | `"0.3"` | Multiplier for specular effects. |
| `gl_specular_fallback` | `"0.05"` | Fallback specular strength for materials without a specular map. |
| `gl_specular_fallbackexp` | `"1"` | Fallback shininess exponent for materials without a specular map. |
| `gl_specular_power` | `"32"` | Global material specular strength multiplier. |
| `gl_texture_anisotropy` (alias: `gl_texture_anisotropic_filtering`) | `"4"` | Allows for higher quality textures on surfaces that slope away from the camera (like the floor). Set to 16 or something. Only supported with trilinear filtering. |
| `gl_texturemode` (alias: `r_texturemode`) | `"nll"` | Filter world/model textures. Every built-in graphics tier defaults to Crisp Pixels (`nll`); changing filtering afterward is allowed. Typically 3 letters eg nll or lll.<br>First letter can be l(inear) or n(earest) and says how to upscale low-res textures (n for the classic look - often favoured for embedded textures, l for blurry - best for high-res textures).<br>The middle letter can be set to '.' to disable mipmaps, or n for ugly banding with distance, or l for smooth mipmap transitions.<br>The third letter says what to do when the texture is too high resolution, and should generally be set to 'l' to reduce sparkles including when aiming for the classic lego look. |
| `gl_texturemode2d` | `"GL_NEAREST"` | Filter HUD/menu images. Defaults to Crisp Pixels in every built-in graphics tier; `GL_LINEAR` selects smooth filtering. Font smoothing is controlled separately. |
| `hand` | `""` | For gamecode to know which hand to fire from.<br>0: Right<br>1: Left<br>2: Chest |
| `handicap` | `"100"` | **Q3:** player handicap percentage, reducing starting health and damage potential according to gamecode; other mods may also read this userinfo. |
| `headmodel` | `"sarge"` | **Q3:** separate head model/skin selection. |
| `host_speeds` | `"0"` | Print engine frame-time breakdowns for profiling. |
| `hostname` | `"unnamed"` | Name advertised by the hosted server. |
| `hud_miniscores_show` | `"1"` | **Q1 target:** show the engine mini-scoreboard HUD element. |
| `hud_tracking_show` | `"1"` | **Q1 target:** show the spectator-tracking HUD element. |

### Cvars: I–M

| Cvar (aliases) | Default | Definition / game notes |
| --- | --- | --- |
| `ignore_flood` | `"0"` | Provides a way to reduce inbound spam from flooding out your chat (dupe messages are ignored).<br>0: No inbound flood protection.<br>1: Duplicate non-team messages will be filtered.<br>2: ALL duplicate messages will be filtered |
| `ignore_flood_duration` | `"4"` | Time limit for inbound messages to be considered duplicates. |
| `ignore_mode` | `"0"` | **Q1 target:** mode used by the engine player/chat ignore system. |
| `ignore_opponents` | `"0"` | 0: Don't ignore chat from enemies.<br>1: Always ignore chat from opponents (note: can also ignore f_ruleset checks).<br>2: Ignore chat from opponents only during a match (requires servers that actually reports match state). |
| `ignore_qizmo_spec` | `"0"` | **Q1 target:** ignore spectator chat relayed by Qizmo where applicable. |
| `ignore_spec` | `"0"` | 0: Never ignore spectators.<br>1: Ignore spectators only when playing.<br>2: Always ignore spectators even when spectating. |
| `in_forceseat` (alias: `in_forcesplitclient`) | `"0"` | Overrides the device identifiers to control a specific client from any device. This can be used for debugging mods, where you only have one keyboard/mouse. |
| `in_skipplayerone` | `"1"` | Do not auto-assign joysticks/game-controllers to the first player. Requires in_restart to take effect. |
| `in_vraim` | `"1"` | When set to 1, the 'view' angle sent to the server is controlled by your vr headset instead of separately. This is for fallback behaviour and blocks mouse+joy+gamepad aiming. |
| `in_windowed_mouse` | `"1"` | Capture the mouse while playing in a window. |
| `in_xflip` | `"0"` | Flip the horizontal input direction for mirrored views. |
| `ipautodump` | `"0"` | Enables dumping the 'iplog.txt' file, which contains a log of usernames seen for a given IP, which is useful for detecting fake-nicks. |
| `joyadvaxisr` | `"turnright"` | Provides a way to remap each joystick/controller axis.<br>Should be set to one of: moveforward, moveback, lookup, lookdown, turnleft, turnright, moveleft, moveright, moveup, movedown, rollleft, rollright |
| `joyadvaxisr_scale` | `"1.0"` | Because joyadvaxisx etc can be added together, this provides a way to rescale or invert an individual axis without affecting another with the same action. |
| `joyadvaxisu` | `"lookup"` | Provides a way to remap each joystick/controller axis.<br>Should be set to one of: moveforward, moveback, lookup, lookdown, turnleft, turnright, moveleft, moveright, moveup, movedown, rollleft, rollright |
| `joyadvaxisu_scale` | `"1.0"` | Because joyadvaxisx etc can be added together, this provides a way to rescale or invert an individual axis without affecting another with the same action. |
| `joyadvaxisv` | `""` | Provides a way to remap each joystick/controller axis.<br>Should be set to one of: moveforward, moveback, lookup, lookdown, turnleft, turnright, moveleft, moveright, moveup, movedown, rollleft, rollright |
| `joyadvaxisv_scale` | `"1.0"` | Because joyadvaxisx etc can be added together, this provides a way to rescale or invert an individual axis without affecting another with the same action. |
| `joyadvaxisx` | `"moveright"` | Provides a way to remap each joystick/controller axis.<br>Should be set to one of: moveforward, moveback, lookup, lookdown, turnleft, turnright, moveleft, moveright, moveup, movedown, rollleft, rollright |
| `joyadvaxisx_scale` | `"1.0"` | Because joyadvaxisx etc can be added together, this provides a way to rescale or invert an individual axis without affecting another with the same action. |
| `joyadvaxisy` | `"moveback"` | Provides a way to remap each joystick/controller axis.<br>Should be set to one of: moveforward, moveback, lookup, lookdown, turnleft, turnright, moveleft, moveright, moveup, movedown, rollleft, rollright |
| `joyadvaxisy_scale` | `"1.0"` | Because joyadvaxisx etc can be added together, this provides a way to rescale or invert an individual axis without affecting another with the same action. |
| `joyadvaxisz` | `""` | Provides a way to remap each joystick/controller axis.<br>Should be set to one of: moveforward, moveback, lookup, lookdown, turnleft, turnright, moveleft, moveright, moveup, movedown, rollleft, rollright |
| `joyadvaxisz_scale` | `"1.0"` | Because joyadvaxisx etc can be added together, this provides a way to rescale or invert an individual axis without affecting another with the same action. |
| `joyexponent` | `"1"` | Scales joystick/controller sensitivity non-linearly to increase precision in the center.<br>A value of 1 is linear. |
| `joyforwardsensitivity` | `"1.0"` | Joystick-axis movement sensitivity for forward, sideways, or vertical input respectively. |
| `joyforwardthreshold` | `"0.17"` | Dead-zone threshold for the corresponding joystick movement axis. |
| `joyonly` | `"0"` | If true, treats "game controllers" as regular joysticks.<br>Must be set at startup. |
| `joypitchsensitivity` | `"0.5"` | Scaler value for the controller when it is at its most extreme value |
| `joypitchthreshold` | `"0.19"` | Values reported near the center of the analog joystick/controller are often erroneous and undesired.<br>The joystick threshholds are how much of the total values to ignore. |
| `joyradialdeadzone` | `""` | Treat controller dead zones as a pair, rather than per-axis.<br>0: treat joystick axis independantly (square).<br>1: treat the axis together, radially.<br>2: do not handle deadzones (prefiltered). |
| `joyrollsensitivity` | `"1.0"` | Scaler value for the controller when it is at its most extreme value |
| `joyrollthreshold` | `"0.118"` | Values reported near the center of the analog joystick/controller are often erroneous and undesired.<br>The joystick threshholds are how much of the total values to ignore. |
| `joysidesensitivity` | `"1.0"` | Joystick-axis movement sensitivity for forward, sideways, or vertical input respectively. |
| `joysidethreshold` | `"0.17"` | Dead-zone threshold for the corresponding joystick movement axis. |
| `joyupsensitivity` | `"1.0"` | Joystick-axis movement sensitivity for forward, sideways, or vertical input respectively. |
| `joyupthreshold` | `"0.118"` | Dead-zone threshold for the corresponding joystick movement axis. |
| `joyyawsensitivity` | `"1.0"` | Scaler value for the controller when it is at its most extreme value |
| `joyyawthreshold` | `"0.19"` | Values reported near the center of the analog joystick/controller are often erroneous and undesired.<br>The joystick threshholds are how much of the total values to ignore. |
| `lang` (alias: `prvm_language`) | `"C"` | This cvar contains the language_dialect code of your language, used to find localisation strings. |
| `leftisright` | `"0"` | Mirror the view horizontally where the renderer supports it. |
| `loc_name_ga` | `"ga"` | **Q1 target:** location-label text for green armor in generated/teamplay location descriptions. |
| `loc_name_gl` | `"gl"` | **Q1 target:** location-label text for grenade launcher in generated/teamplay location descriptions. |
| `loc_name_lg` | `"lg"` | **Q1 target:** location-label text for lightning gun in generated/teamplay location descriptions. |
| `loc_name_mh` | `"mh"` | **Q1 target:** location-label text for megahealth in generated/teamplay location descriptions. |
| `loc_name_ng` | `"ng"` | **Q1 target:** location-label text for nailgun in generated/teamplay location descriptions. |
| `loc_name_pent` | `"pent"` | **Q1 target:** location-label text for Pentagram of Protection in generated/teamplay location descriptions. |
| `loc_name_quad` | `"quad"` | **Q1 target:** location-label text for Quad Damage in generated/teamplay location descriptions. |
| `loc_name_ra` | `"ra"` | **Q1 target:** location-label text for red armor in generated/teamplay location descriptions. |
| `loc_name_ring` | `"ring"` | **Q1 target:** location-label text for Ring of Shadows in generated/teamplay location descriptions. |
| `loc_name_rl` | `"rl"` | **Q1 target:** location-label text for rocket launcher in generated/teamplay location descriptions. |
| `loc_name_separator` | `"-"` | **Q1 target:** location-label text for list separator in generated/teamplay location descriptions. |
| `loc_name_sng` | `"sng"` | **Q1 target:** location-label text for super nailgun in generated/teamplay location descriptions. |
| `loc_name_ssg` | `"ssg"` | **Q1 target:** location-label text for super shotgun in generated/teamplay location descriptions. |
| `loc_name_suit` | `"suit"` | **Q1 target:** location-label text for biosuit in generated/teamplay location descriptions. |
| `loc_name_ya` | `"ya"` | **Q1 target:** location-label text for yellow armor in generated/teamplay location descriptions. |
| `log_developer` | `"0"` | Enables logging of console prints when set to 1. Otherwise unimportant messages will not fill up your log files. |
| `log_dir` (alias: `sv_logdir`) | `""` | Directory for engine-generated console/player/RCON logs. |
| `log_dosformat` | `"0"` | Write log line endings in CRLF form. |
| `log_enable` | `"0"` | Enable the general console log. |
| `log_enable_players` | `"0"` | Enable the player-event log. |
| `log_enable_rcon` | `"1"` | Enable the remote-console activity log. |
| `log_name` | `"qconsole"` | Filename for the general console log. |
| `log_name_players` | `"players"` | Filename for the player-event log. |
| `log_name_rcon` | `"rcon"` | Filename for the remote-console log. |
| `log_readable` | `"7"` | Bitfield describing what to convert/strip. If 0, exact byte representation will be used.<br>&1: Dequakify text.<br>&2: Strip special markup.<br>&4: Strip ansi control codes. |
| `log_rotate_files` | `"0"` | Number of rotated log files to retain. |
| `log_rotate_size` | `"131072"` | Size threshold for rotating a log file. |
| `log_timestamps` | `"1"` | Timestamp format/enablement for log entries. |
| `lookspring` | `"0"` | Recentre the camera when the mouse-look is released. |
| `lookstrafe` | `"0"` | Mouselook enables mouse strafing. |
| `m_accel` (alias: `cl_mouseAccel`) | `"0"` | Values >0 will amplify mouse movement proportional to velocity. Small values have great effect. A lot of good Quake Live players use around the 0.1-0.2 mark, but this depends on your mouse CPI and polling rate. |
| `m_accel_offset` (alias: `cl_mouseAccelOffset`) | `"0"` | Used when m_accel_style is 1.<br>Acceleration will not be active until the mouse movement exceeds this speed (counts per millisecond). Negative values are supported, which has the effect of causing higher rates of acceleration to happen at lower velocities. |
| `m_accel_power` (alias: `cl_mouseAccelPower`) | `"2"` | Used when m_accel_style is 1.<br>Values 1 or below are dumb. 2 is linear and the default. 99% of accel users use this. Above 2 begins to amplify exponentially and you will get more acceleration at higher velocities. Great if you want low accel for slow movements, and high accel for fast movements. Good in combination with a sensitivity cap. |
| `m_accel_senscap` (alias: `cl_mouseSensCap`) | `"0"` | Used when m_accel_style is 1.<br>Sets an upper limit on the amplified mouse movement. Great for tuning acceleration around lower velocities while still remaining in control of fast motion such as flicking. |
| `m_accel_style` (alias: `cl_mouseAccelStyle`) | `"1"` | 1 = Quake Live mouse acceleration, 0 = Old style accelertion. |
| `m_filter` | `"0"` | Smooth mouse motion by averaging successive samples. |
| `m_forcewheel` | `"1"` | 0: ignore mousewheels in apis where it is abiguous.<br>1: Use mousewheel when it is treated as a third axis. Motion above a threshold is ignored, to avoid issues with an unknown threshold.<br>2: Like 1, but excess motion is retained. The threshold specifies exact z-axis distance per notice. |
| `m_forcewheel_threshold` | `"32"` | Mousewheel graduations smaller than this will not trigger mousewheel deltas. |
| `m_forward` | `"1"` | Mouse vertical-motion multiplier for forward movement when mouse look is disabled. |
| `m_helpismedia` | `"0"` | Use media/image help screens in the native menu. |
| `m_longpressthreshold` | `"1"` | How long to press for it to register as a long press (touchscreens). |
| `m_pitch` | `"0.022"` | Mouse vertical-motion multiplier; a negative value inverts mouse look. |
| `m_preset_chosen` | `"0"` | Remember that an initial graphics preset has been selected; avoids the first-run preset prompt. |
| `m_side` | `"0.8"` | Mouse horizontal-motion multiplier for strafing. |
| `m_slidethreshold` | `"10"` | How far your finger needs to move to be considered a slide event (touchscreens). |
| `m_touchmajoraxis` | `"1"` | When using a touchscreen, use only the major axis for strafing. |
| `m_touchstrafe` (alias: `m_strafeonright`) | `"0"` | 0: entire screen changes angles only.<br>1: right hand side controls strafing.<br>2:left hand side strafes. |
| `m_yaw` | `"0.022"` | Mouse horizontal-motion multiplier for turning. |
| `map_autoopenportals` | Q1: `"1"`; Q3: `"0"` | When set to 1, force-opens all area portals. Normally these start closed and are opened by doors when they move, but this requires the gamecode to signal this. |
| `map_noareas` | `"0"` | Ignore BSP area/portal visibility restrictions for diagnosis. |
| `map_noCurves` | `"0"` | **Different:** Quake 3 BSP: disable curved patch surfaces for diagnosis. [cheat/ruleset restricted.] |
| `mapname` (alias: `host_mapname`) | `""` | Cvar that holds the short name of the current map, for scripting type stuff |
| `mastervolume` | `"1"` | Additional multiplier for all other sounds. |
| `max_aaslinks` | on demand | **Q3:** optional maximum AAS navigation links passed to bot-library setup. |
| `max_levelitems` | on demand | **Q3:** optional maximum tracked level items passed to bot-library setup. |
| `maxclients` (alias: `sv_maxclients`) | `"8"` | Specify the maximum number of players allowed on the server at once. Can be changed mid-map. |
| `maxpitch` (alias: `sv_maxpitch`) | `""` | Assumed to be 80 |
| `maxspectators` | `"8"` | Specify the maximum number of spectators allowed on the server at once. Can be changed mid-map. |
| `media_repeat` | `"1"` | Repeat playback in the engine media playlist. |
| `media_shuffle` | `"1"` | Shuffle the engine media playlist. |
| `minpitch` (alias: `sv_minpitch`) | `""` | Assumed to be -70 |
| `mod_external_vis` | `"1"` | Attempt to load .vis patches for quake maps, allowing transparent water to work properly. |
| `mod_gltf_fixbuggyanims` | `"1"` | Work around buggy exporters by merging animations that affect only a single bone. |
| `mod_gltf_ignoretechniques` | `"1"` | Ignore the GLTF-1 model-specific glsl. This is enabled by default because it just doesn't work very well. |
| `mod_gltf_loop` | `"1"` | This forces all gltf models to loop, instead of only when the last frame matches the first. |
| `mod_gltf_privatematerials` | `"1"` | Add the model path to material names, to isolate them between different models. |
| `mod_gltf_scale` | `"30"` | This defines the number of units per metre, in order to correctly load standard-scale gltf models. |
| `mod_gltf_standardorientation` | `"1"` | Transform GLTF files from standard y-up to Quake's z-up orientation. Set to 0 to violate the GLTF specification. |
| `mod_h2holey_bugged` | `"1"` | Hexen II model-transparency compatibility setting; retained for that game format, not a Quake 1/3 texture option. |
| `mod_lightpoint_distance` | `"8192"` | This is the maximum distance to trace when searching for a ground surface for lighting info on map formats without light more fancy lighting info. Use 2048 for full compat with Quake. |
| `mod_lightscale_broken` | `"0"` | When active, replicates a bug from vanilla - the radius of r_dynamic lights is scaled by per-surface texture scale rather than using actual distance. |
| `mod_litsprites_force` | `"0"` | If set to 1, sprites will be lit according to world lighting (including rtlights), like Tenebrae. Ideally use EF_ADDITIVE or EF_FULLBRIGHT to make emissive sprites instead. |
| `mod_loadmappackages` | `"1"` | Load additional content embedded within bsp files. |
| `mod_map_lights` | `"0"` | Calculates lighting on brushes/patches. |
| `mod_map_texscale` | `"1"` | Defines the scale of texture texels. Use 1 for quake+quake2 maps, and 0.5 for quake3 maps. |
| `mod_md3flags` | `"1"` | The flags field of md3s was never officially defined. If this is set to 1, the flags will be treated identically to mdl files. Otherwise they will be ignored. Naturally, this is required to provide rotating pickups in quake. |
| `mod_md5_singleanimation` | `"1"` | When loading an md5mesh file, also attempt to load an .md5anim file, and unpack it into individual poses. Use 0 for mods that will be precaching their own md5anims for use with skeletal objects. |
| `mod_obj_orientation` | `"1"` | Controls how the model's axis are interpreted.<br>0: x=forward, z=up (Quake)<br>1: x=forward, y=up<br>2: z=forward, y=up |
| `mod_precache` | `"1"` | Controls when models are loaded.<br>0: Load them only when they're actually needed.<br>1: Load them upfront.<br>2: Lazily load them to shorten load times at the risk of brief stuttering during only the start of the map. |
| `mod_terrain_ambient` | `"0.5"` | Proportion of ambient light. |
| `mod_terrain_defaulttexture` | `""` | Newly created terrain tiles will use this texture. This should generally be updated by the terrain editor. |
| `mod_terrain_networked` | `"0"` | Terrain edits are networked. Clients will download sections on demand, and servers will notify clients of changes. |
| `mod_terrain_savever` | `""` | Which terrain section version to write if terrain was edited. |
| `mod_terrain_shadow_dist` | `"2048"` | How far rays should be cast in order to look for occlusing geometry. |
| `mod_terrain_shadows` | `"0"` | Cast rays to determine whether parts of the terrain should be in shadow. |
| `mod_terrain_sundir` | `"0.4 0.7 2"` | The direction of the sun (vector will be normalised). |
| `mod_warnmodels` | `"1"` | Warn if any models failed to load. Set to 0 if your mod is likely to lack optional models (like its in development). |
| `model` (alias: `_cl_playermodel`) | Q1: `""`; Q3: `"sarge"` | **Different:** Player model selection. Quake 3 accepts model/skin names; Quake 1 support depends on the mod and protocol. |
| `msg` | `"1"` | Filter console prints/messages. Only functions on QuakeWorld servers. 0=pickup messages. 1=death messages. 2=critical messages. 3=chat. |
| `msg_filter` | `"0"` | Filter out chat messages: 0=neither. 1=broadcast chat. 2=team chat. 3=all chat. |
| `msg_filter_frags` | `"0"` | Prevents frag messages from appearing on the console. |
| `music_fade` | `"1"` | Fade duration used by the engine music player. |
| `music_playlist_index` | `"-1"` | Selected legacy music playlist slot. |
| `music_playlist_list0` | `""` | Track list for legacy music playlist slot 0. |
| `music_playlist_list1` | `""` | Track list for legacy music playlist slot 1. |
| `music_playlist_list2` | `""` | Track list for legacy music playlist slot 2. |
| `music_playlist_list3` | `""` | Track list for legacy music playlist slot 3. |
| `music_playlist_list4` | `""` | Track list for legacy music playlist slot 4. |
| `music_playlist_list5` | `""` | Track list for legacy music playlist slot 5. |
| `music_playlist_sampleposition0` | `"-1"` | Saved playback/sample position for legacy music playlist slot 0. |
| `music_playlist_sampleposition1` | `"-1"` | Saved playback/sample position for legacy music playlist slot 1. |
| `music_playlist_sampleposition2` | `"-1"` | Saved playback/sample position for legacy music playlist slot 2. |
| `music_playlist_sampleposition3` | `"-1"` | Saved playback/sample position for legacy music playlist slot 3. |
| `music_playlist_sampleposition4` | `"-1"` | Saved playback/sample position for legacy music playlist slot 4. |
| `music_playlist_sampleposition5` | `"-1"` | Saved playback/sample position for legacy music playlist slot 5. |
| `musicvolume` (alias: `bgmvolume`) | `"0.3"` | **Different:** Volume level for background music. Quake 3 UI VM requests for s_musicvolume are translated to this setting; s_musicvolume is not an engine console alias. |

### Cvars: N–R

| Cvar (aliases) | Default | Definition / game notes |
| --- | --- | --- |
| `name` | `"Player"` | Player's name, sent to the server. |
| `net_compress` | `"0"` | Enables huffman compression of network packets. |
| `net_dns_ipv4` | `"1"` | If 0, disables dns resolution of names to ipv4 addresses (removing any associated error messages). Also hides ipv4 addresses in address:port listings. |
| `net_dns_ipv6` | `"1"` | If 0, disables dns resolution of names to ipv6 addresses (removing any associated error messages). Also hides ipv6 addresses in address:port listings. |
| `net_enable_dtls` (alias: `sv_listen_dtls`) | `""` | Controls serverside dtls support.<br>0: dtls blocked, not advertised.<br>1: clientside choice.<br>2: used where possible (recommended setting).<br>3: disallow non-dtls clients (sv_port_tcp should be eg tls://[::]:27500 to also disallow unencrypted tcp connections). |
| `net_enable_http` | `"0"` | Accept incoming HTTP file requests on a configured TCP listener. Server download permissions still apply. Does not affect outgoing HTTP/HTTPS downloads. The standalone master-server build defaults to `1`. |
| `net_enable_q2e` | `"0"` | If enabled, accept connection requests from the quake2-remaster engine on our listening udp ports.<br>Note that this defaults to disabled due to it being highly vulnerable to applification attacks. |
| `net_enable_qizmo` | `"1"` | Enables serverside support for 'connect tcp://foo' or 'connect tls://foo' (with net_enable_tls), as well as qizmo's tcp connections and compatibles. |
| `net_enable_qtv` | `"2"` | Controls whether inbound qtv requests will be honoured (both proxies and clients using qtvplay).<br>0: Don't accept qtv connections.<br>1: Accept connections.<br>2: Accept qtv connections only from host-scopped addresses (read: 127.*.*.*, ::1, or unix sockets). |
| `net_enable_rtcbroker` | `"0"` | If 1, tcp ports will accept websocket connections from clients trying to broker direct webrtc connections. This should be low traffic, but might involve a lot of mostly-idle connections. |
| `net_enable_tls` | `"1"` | If enabled, binary data sent to a non-tls tcp port will be interpretted as a tls handshake (enabling https or wss over the same tcp port. |
| `net_enable_websockets` | `"1"` | Accept WebSocket game connections on a configured TCP listener; use `sv_port_tcp` to select the listener. The standalone master-server build defaults to `0`. |
| `net_enabled` | `"1"` | If 0, disables all network access, including name resolution and socket creation. Does not affect loopback/internal connections. |
| `net_fakeloss` | `"0"` | Simulates packetloss in both receiving and sending, on a scale from 0 to 1. [cheat/ruleset restricted.] |
| `net_fakemtu` | `"0"` | Cripples packet reception sizes. [cheat/ruleset restricted.] |
| `net_hybriddualstack` | `"1"` | Uses hybrid ipv4+ipv6 sockets where possible. Not supported on xp or below. |
| `net_ice_allowmdns` | `"1"` | Boolean. When set to 0, prevents the use of multicast-dns to obtain candidates using random numbers instead of revealing private network info. |
| `net_ice_allowstun` | `"1"` | Boolean. When set to 0, prevents the use of stun to determine our public address (does not prevent connecting to our peer's server-reflexive candidates). |
| `net_ice_allowturn` | `"1"` | Boolean. When set to 0, prevents registration of turn connections (does not prevent connecting to our peer's relay candidates). |
| `net_ice_broker` | `"tls://master.frag-net.com:27950"` | This is the default broker we attempt to connect through when using 'sv_public /foo' or 'connect /foo'. |
| `net_ice_debug` | `"0"` | 0: Hide messy details.<br>1: Show new candidates.<br>2: Show each connectivity test. |
| `net_ice_exchangeprivateips` | `"0"` | Boolean. When set to 0, hides private IP addresses from your peers - only addresses determined from the other side of your router will be shared. You should only need to set this to 1 if mdns is unavailable. |
| `net_ice_relayonly` | `"0"` | Boolean. When set to 1, blocks reporting non-relay local candidates, does not attempt to connect to remote candidates other than via a relay. |
| `net_ice_servers` | `""` | A space-separated list of ICE servers, eg stun:host.example:3478 or turn:host.example:3478?user=foo?auth=blah |
| `net_ice_usewebrtc` | `""` | Use webrtc's extra overheads rather than simple ICE. This makes packets larger and is slower to connect, for compatibility with WebRTC peers. |
| `net_master1` (alias: `sv_master1`) | `""` | default protocol master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_master2` (alias: `sv_master2`) | `""` | default protocol master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_master3` (alias: `sv_master3`) | `""` | default protocol master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_master4` (alias: `sv_master4`) | `""` | default protocol master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_master5` (alias: `sv_master5`) | `""` | default protocol master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_master6` (alias: `sv_master6`) | `""` | default protocol master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_master7` (alias: `sv_master7`) | `""` | default protocol master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_master8` (alias: `sv_master8`) | `""` | default protocol master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_masterextra1` | `"master.frag-net.com:27950"` | default protocol master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_masterextra2` | `"dpmaster.deathmask.net:27950"` | default protocol master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_masterextra3` | `"dpmaster.tchr.no:27950"` | default protocol master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_masterextra4` | `""` | default protocol master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_masterextra5` | `""` | default protocol master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_masterextra6` | `""` | default protocol master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_masterextra7` | `""` | default protocol master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_masterextra8` | `""` | default protocol master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_mtu` | `"1440"` | Specifies a maximum udp payload size, above which packets will be fragmented. If routers all worked properly this could be some massive value, and some massive value may work really nicely for lans. Use smaller values than the default if you're connecting through nested tunnels through routers that fail with IP fragmentation. |
| `net_q2master1` | `""` | Quake 2 master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_q2master2` | `""` | Quake 2 master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_q2master3` | `""` | Quake 2 master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_q2master4` | `""` | Quake 2 master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_q2masterextra1` | `"netdome.biz:27900"` | Quake 2 master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_q2masterextra2` | `"master.quakeservers.net:27900"` | Quake 2 master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_q2masterextra3` | `"master.q2servers.com:27900"` | Quake 2 master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_q3master1` | `""` | **Q3:** Quake 3 master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_q3master2` | `""` | **Q3:** Quake 3 master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_q3master3` | `""` | **Q3:** Quake 3 master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_q3master4` | `""` | **Q3:** Quake 3 master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_q3masterextra1` | `"master.quake3arena.com:27950"` | **Different:** Quake 3 master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_q3masterextra2` | `"master0.excessiveplus.net:27950"` | **Different:** Quake 3 master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_q3masterextra3` | `"master.ioquake3.org:27950"` | **Different:** Quake 3 master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_q3masterextra4` | `"master.huxxer.de:27950"` | **Different:** Quake 3 master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_q3masterextra5` | `"master.maverickservers.com:27950"` | **Different:** Quake 3 master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_q3masterextra8` | `"master3.idsoftware.com:27950"` | **Different:** Quake 3 master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_qwmaster1` | `""` | QuakeWorld master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_qwmaster2` | `""` | QuakeWorld master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_qwmaster3` | `""` | QuakeWorld master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_qwmaster4` | `""` | QuakeWorld master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_qwmaster5` | `""` | QuakeWorld master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_qwmaster6` | `""` | QuakeWorld master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_qwmaster7` | `""` | QuakeWorld master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_qwmaster8` | `""` | QuakeWorld master-server address in the built-in master list; used for server discovery/advertising as appropriate. |
| `net_qwmasterextra1` | `""` | QuakeWorld master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_qwmasterextra2` | `""` | QuakeWorld master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_qwmasterextra4` | `"master.quakeservers.net:27000"` | QuakeWorld master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `net_qwmasterextra5` | `"qwmaster.fodquake.net:27000"` | QuakeWorld master-server address in the additional/custom master list; used for server discovery/advertising as appropriate. |
| `nextmap` | `""` | **Q3:** command/script used by qagame to select the next map. |
| `nextserver` | `""` | Command/script to execute when the current server level ends, when used by gamecode. |
| `noaim` | `""` | **Q1 target:** disable server-assisted aiming when supported by gamecode. |
| `noexit` | `"0"` | **Q1 target:** gamecode setting preventing exits in supporting deathmatch mods. |
| `nomonsters` | `"0"` | **Q1 target:** gamecode setting suppressing monster spawning in supporting mods. |
| `noskins` | `"0"` | **Q1 target:** disable external player skins. |
| `nosound` | `"0"` | Disable all sound from the engine. Cannot be overriden by configs or anything if set via the -nosound commandline argument. |
| `password` | not exposed | Password supplied when joining a password-protected server. |
| `pausable` | `""` | Permit players to pause a hosted game; gamecode/protocol may add its own restrictions. |
| `pext_ezquake_nochunks` | `"0"` | Prevents ezquake clients from being able to use the chunked download extension. This sidesteps numerous ezquake issues, and will make downloads slower but more robust. |
| `pext_ezquake_verfortrans` | `"7088"` | ezQuake does not implement PEXT_TRANS properly. This is the version of ezquake required for PEXT_TRANS to be allowed. This was still broken when I wrote this description, hence the large value. |
| `pext_predinfo` | `"1"` | Enables some extra things to support prediction over NQ protocols. |
| `pext_replacementdeltas` | `"1"` | Enables the use of alternative nack-based entity deltas |
| `pkg_autoupdate` | `"1"` | Controls autoupdates, can only be changed via the downloads menu.<br>0: off.<br>1: enabled (stable only).<br>2: enabled (unstable).<br>Note that autoupdate will still prompt the user to actually apply the changes. |
| `physics_jolt_enabled` | `"0"` | **Q1 target; optional Jolt plugin.** Opt into rigid props before loading a map. Does not convert classic movement or Q3 qagame physics. Set after `plug_load jolt`; non-archived. Changing it does not stop an existing world immediately. See [JoltPhysics.md](JoltPhysics.md) for restart commands and limits. |
| `physics_jolt_pointanchor2` | `"0"` | **Q1 target; optional Jolt plugin.** Use FTE/ODE entity point-joint `.velocity` second anchors when enabled. Two-body offsets are local to body 2's physics center of mass; world joints use a world-space anchor, including ODE's reversed world-first convention. Set after `plug_load jolt` and before map load; non-archived. Default 0 retains shared `.origin` pivots. Applies to SSQC/CSQC entity point joints, not `.doll` rigs or Q3 qagame physics. Live changes rebuild point joints. See [legacy point anchors](JoltPhysics.md#legacy-point-anchors-and-joint-groups). |
| `plug_loaddefault` | `"1"` | 0: Load plugins only via explicit plug_load commands<br>1: Load built-in plugins and those selected via the package manager<br>2: Scan for misc plugins, loading all that can be found, but not built-ins.<br>3: Scan for plugins, and then load any built-ins |
| `plug_sbar` | `"3"` | Controls whether plugins are allowed to draw the hud, rather than the engine (when allowed by csqc). This is typically used to permit the ezhud plugin without needing to bother unloading it.<br>=0: never use hud plugins.<br>&1: Use hud plugins in deathmatch.<br>&2: Use hud plugins in singleplayer/coop.<br>=3: Always use hud plugins (when loaded). |
| `pm_airstep` (alias: `sv_jumpstep`) | `""` | Allows players to step up while jumping. This makes stairs more graceful but also increases potential jump heights. |
| `pm_autobunny` | `""` | Players will continue jumping without needing to release the jump button. |
| `pm_bunnyfriction` | `""` | Replicates NQ behaviour, ensuring that there's at least a frame of friction while jumping - friction is proportional to tick rate. |
| `pm_bunnyspeedcap` | `""` | 0 or 1, ish. If the player is traveling faster than this speed while turning, their velocity will be gracefully reduced to match their current maxspeed. You can still rocket-jump to gain high velocity, but turning will reduce your speed back to the max. This can be used to disable bunny hopping. |
| `pm_edgefriction` (alias: `edgefriction`) | `""` | Increases friction when about to walk over a cliff, so you're less likely to plummet by mistake. When empty defaults to 2, but uses a tracebox instead of a traceline to detect the drop. |
| `pm_flyfriction` | `""` | Amount of friction that applies in fly or 6dof mode. Empty means 4. |
| `pm_ktjump` | `""` | **Q1 target:** movement compatibility switch for KT-style jump behavior. |
| `pm_noround` | `"0"` | Disables player prediction snapping, in a way that cannot be reliably predicted but may be needed to avoid map bugs. |
| `pm_pground` | `""` | Use persisten onground state instead of recalculating every frame.Do NOT use with nq mods, as most nq mods will interfere with onground state, resulting in glitches. |
| `pm_slidefix` | `""` | Fixes an issue when walking down slopes (ie: so they act more like slopes and not a series of steps) |
| `pm_slidyslopes` | `""` | Replicates NQ behaviour, where players will slowly slide down ramps. Generally requires 'pm_noround 1' too, otherwise the effect rounds to nothing. |
| `pm_stepdown` | `""` | Causes physics to stick to the ground, instead of constantly losing traction while going down steps. |
| `pm_stepheight` (alias: `sv_stepheight`) | `""` | If empty, the value 18 will be used instead. This is the size of the step you can step up or down. |
| `pm_walljump` | `""` | Allows the player to bounce off walls while arborne. |
| `pm_watersinkspeed` | `""` | This is the speed that players will sink at while inactive in water. Empty means 60. |
| `pmove_fixed` | `"0"` | **Q3:** Use fixed-step Quake 3 gamecode movement integration. |
| `pmove_float` | `"1"` | **Q3:** OpenArena floating-point movement preference; affects gamecode movement arithmetic. |
| `pmove_msec` | `"11"` | **Q3:** Step duration in milliseconds for fixed-step Quake 3 gamecode movement. |
| `pr_allowbutton1` | `"1"` | The button1 field is believed to have been intended to work with the +use command, but it was never hooked up. In NetQuake, this field was often repurposed for other things as it was not otherwise used (and cannot be removed without breaking the crc), while third-party QuakeWorld engines did decide to implement it as believed was intended. As a result, this cvar only applies to QuakeWorld mods and a value of 1 is only likely to cause issues with NQ mods that were ported to QW. |
| `pr_autocreatecvars` | `"1"` | **Q1 target:** Implicitly create any cvars that don't exist when read. |
| `pr_brokenfloatconvert` | `"0"` | **Q1 target:** compatibility behavior for historical QuakeC floating-point conversion bugs. |
| `pr_compatabilitytest` | `"0"` | **Q1 target:** Only enables builtins if the extension they are part of was queried. |
| `pr_coreonerror` | `"1"` | **Q1 target:** dump server QuakeC VM state on a runtime error. |
| `pr_csqc_coreonerror` | `"1"` | **Q1 target:** write a CSQC VM diagnostic/core dump on errors. |
| `pr_csqc_formenus` | `"0"` | **Q1 target:** allow CSQC menu integration. |
| `pr_csqc_maxedicts` | `"65536"` | **Q1 target:** maximum entity slots allocated for CSQC. |
| `pr_csqc_memsize` | `"-1"` | **Q1 target:** memory allocation limit/size for the CSQC VM. |
| `pr_debugger` (alias: `debugger`) | `"0"` | When enabled, QC errors and debug events will enable step-by-step tracing. |
| `pr_droptofloorunits` | `"256"` | **Q1 target:** Distance that droptofloor is allowed to drop to be considered successul. |
| `pr_enable_profiling` | `"0"` | **Q1 target:** Enables profiling support. Will run more slowly. Change the map and then use the profile_ssqc/profile_csqc commands to see the results. |
| `pr_enable_uriget` | `"1"` | **Q1 target:** Allows gamecode to make direct http requests |
| `pr_engine` | `"FTE 6751-git-b886f7101"` | This cvar exists so that the menuqc is able to determine which engine-specific settings/values to list/suggest. It must not be used to detect formal QC extensions/builtins. Use checkextension/checkbuiltin/checkcommand for that. |
| `pr_ext_dp_qc_getsurface` | `""` | Set to 0 to block detection of the extension |
| `pr_fixbrokenqccarrays` | `"0"` | **Q1 target:** As part of its nq/qw/h2/csqc support, FTE remaps QC fields to match an internal order. This is a faster way to handle extended fields. However, some QCCs are buggy and don't report all field defs.<br>0: do nothing. QCC must be well behaved.<br>1: Duplicate engine fields, remap the ones we can to known offsets. This is sufficient for QCCX/FrikQCC mods that use hardcoded or even occasional calculated offsets (fixes ktpro).<br>2: Scan the mod for field accessing instructions, and assume those are the fields (and that they don't alias non-fields). This can be used to work around gmqcc's WTFs (fixes xonotic). |
| `pr_gc_threaded` | `"1"` | **Q1 target:** Says whether to use a separate thread for tempstring garbage collections. This avoids main-thread stalls but at the expense of more memory usage. |
| `pr_imitatemvdsv` | `"0"` | **Q1 target:** Enables mvdsv-specific builtins, and fakes identifiers so that mods made for mvdsv can run properly and with the full feature set. |
| `pr_maxedicts` (alias: `max_edicts`) | `"131072"` | **Q1 target:** Maximum number of entities spawnable on the map at once. Low values will crash the server on some maps/mods. High values will result in excessive memory useage (see pr_ssqc_memsize). Illegible server messages may occur with old/other clients above 32k. FTE's network protocols have a maximum at a little over 4 million. Please don't ever make a mod that actually uses that many... |
| `pr_menu_coreonerror` | `"1"` | QuakeC menu VM: write a diagnostic/core dump on errors. |
| `pr_menu_memsize` | `"64m"` | Memory allocation size for the QuakeC menu VM. |
| `pr_no_parsecommand` | `"0"` | **Q1 target:** Provides a way around invalid mod usage of SV_ParseClientCommand, eg xonotic. |
| `pr_no_playerphysics` | `"0"` | **Q1 target:** Prevents support of the 'SV_PlayerPhysics' QC function. This allows servers to prevent needless breakage of player prediction. |
| `pr_nonetaccess` | `"0"` | **Q1 target:** Block all direct access to network buffers (the writebyte builtin and friends will ignore the call). |
| `pr_overridebuiltins` | `"1"` | **Q1 target:** permit gamecode to override builtin functions where supported. |
| `pr_precachepic_slow` | `"0"` | **Q1 target:** Legacy setting. Should be set to 0 where supported. |
| `pr_sourcedir` | `"src"` | **Q1 target:** Subdirectory where your qc source is located. Used by the internal compiler and qc debugging functionality. |
| `pr_ssqc_memsize` | `"-1"` | **Q1 target:** The ammount of memory available to the QC vm. This has a theoretical maximum of 1gb, but that value can only really be used in 64bit builds. -1 will attempt to use some conservative default, but you may need to increase it. Consider also clearing pr_fixbrokenqccarrays if you need to change this cvar. |
| `pr_ssqc_progs` | `""` | **Q3:** Quake 1: server QuakeC program filename. Quake 3 uses its qagame VM rather than progs.dat. |
| `pr_tempstringcount` | `""` | **Q1 target:** Obsolete. Set to 16 if you want to recycle+reuse the same 16 tempstring references and break lots of mods. |
| `pr_tempstringsize` | `"4096"` | **Q1 target:** Obsolete |
| `prox_inmenu` | `"0"` | Proxy/menu compatibility indicator used by the input and connection code. |
| `pushlatency` | `"-999"` | **Q1 target:** legacy prediction time offset in milliseconds. |
| `q3bsp_ignorestyles` | `"0"` | Ignores multiple lightstyles in Raven's q3bsp variant(and derivatives) for better batch/rendering performance. |
| `q3bsp_mergelightmaps` | `"1"` | Specifies whether to merge lightmaps into atlases in order to boost performance. Unfortunately this breaks tcgen on lightmap passes - if you care, set this to 0. |
| `q3bsp_surf_meshcollision_flag` | `"0x80000000"` | The surfaceparm flag(s) that enables q3bsp trisoup collision |
| `q3bsp_surf_meshcollision_force` | `"0"` | Force mesh-based collisions on all q3bsp trisoup surfaces. |
| `qport_` | dynamic / platform dependent | Connection-identification port token used by the network channel; normally managed by the engine. |
| `qtv_maxstreams` (alias: `mvd_maxstreams`) | `"0"` | **Q1 target:** This is the maximum number of QTV clients/proxies that may be directly connected to the server. If empty then there is no limit. 0 disallows any streaming. |
| `qtv_password` | `""` | **Q1 target:** Quake 1/QTV: password for a protected multiview stream. |
| `qtv_streamport` (alias: `mvd_streamport`) | `""` | **Q1 target:** Legacy cvar. Use sv_port_tcp instead. |
| `qtvcl_eztvextensions` | `"1"` | **Q1 target:** Quake 1/QTV: enable ezQuake-compatible stream extensions. |
| `qtvcl_forceversion1` | `"0"` | **Q1 target:** Quake 1/QTV: force the older stream handshake version. |
| `qws_builddate` | dynamic / platform dependent | Engine/server build metadata: builddate. Informational, not a game-rule setting. |
| `qws_buildnum` | dynamic / platform dependent | Engine/server build metadata: buildnum. Informational, not a game-rule setting. |
| `qws_fullname` | dynamic / platform dependent | Engine/server build metadata: fullname. Informational, not a game-rule setting. |
| `qws_homepage` | `"https://fte.triptohell.info"` | Engine/server build metadata: homepage. Informational, not a game-rule setting. |
| `qws_name` | `"FTE"` | Engine/server build metadata: name. Informational, not a game-rule setting. |
| `qws_platform` | dynamic / platform dependent | Engine/server build metadata: platform. Informational, not a game-rule setting. |
| `qws_version` | dynamic / platform dependent | Engine/server build metadata: version. Informational, not a game-rule setting. |
| `r_allowExtensions` | on demand | **Q3:** Legacy Quake 3 OpenGL-extension preference; has no SDL3 GPU OpenGL-extension implementation. |
| `r_availableModes` | on demand | **Q3:** Legacy Quake 3 UI video-mode list lookup; not the SDL3 GPU mode-selection interface. |
| `r_bloodstains` | `"1"` | Enable blood-generated surface stains. |
| `r_bloom` (alias: `gl_bloom`) | `"0"` | Amount of bloom around bright surfaces; zero disables bloom. |
| `r_bluelight_colour` | `"0.5 0.5 3.0 200"` | The red, green, blue, radius values for EF_BLUE effects (typically used for quad-damage in quakeworld) |
| `r_bouncysparks` | `"1"` | Enables particle interaction with world surfaces, allowing for bouncy particles, stains, and decals. |
| `r_brightlight_colour` | `"2.0 1.0 0.5 400"` | The red, green, blue, radius values for EF_BRIGHTLIGHT effects (unused in vanilla quake). |
| `r_clear` (alias: `gl_clear`) | Q1: `"0"`; Q3: `"1"` | Renderer color-buffer clearing preference for debugging. Quake 3 startup/gamecode may establish a different default; the dedicated-server gl_clear lookup has no rendering effect. |
| `r_clearcolour` (alias: `r_clearcolor`) | Q1: `"0.12 0.12 0.12"`; Q3: `"0 0 0"` | Color used for diagnostic background clearing. |
| `r_clutter_density` | `"0"` | Scaler for clutter counts. 0 disables clutter completely.<br>Clutter requires shaders with 'fte_clutter MODEL SPACING SCALEMIN SCALEMAX ZOFS ANGLEMIN ANGLEMAX' terms |
| `r_clutter_distance` | `"1024"` | Distance at which clutter will become invisible. |
| `r_coronas` | `"0"` | Draw coronas on realtime lights. Overrides glquake-esque flashblends. |
| `r_coronas_fadedist` | `"256"` | Coronas will fade out over this distance. |
| `r_coronas_intensity` | `"1"` | Alternative intensity multiplier for coronas. |
| `r_coronas_mindist` | `"128"` | Coronas closer than this will be invisible, preventing near clip plane issues. |
| `r_coronas_occlusion` | `""` | Specifies that coronas should be occluded more carefully.<br>0: No occlusion, at all.<br>1: BSP occlusion only (simple tracelines).<br>2: non-bsp occlusion also (complex tracelines).<br>3: Depthbuffer reads (forces synchronisation).<br>4: occlusion queries. |
| `r_customheight` | on demand | **Q3:** Legacy Quake 3 UI custom-resolution preferences; use vid_width/vid_height through the native video menu for this engine. |
| `r_customwidth` | on demand | **Q3:** Legacy Quake 3 UI custom-resolution preferences; use vid_width/vid_height through the native video menu for this engine. |
| `r_decal_noperpendicular` | `"1"` | When enabled, decals will not be generated on planes at a steep angle from clipped decal orientation. |
| `r_deluxemapping` (alias: `r_glsl_deluxemapping`) | `"1"` | Enables bumpmapping based upon precomputed light directions.<br>0=off<br>1=use if available<br>2=auto-generate (if possible) |
| `r_dimlight_colour` | `"2.0 1.0 0.5 200"` | The red, green, blue, radius values for EF_DIMLIGHT effects (used for quad+pent in vanilla quake). |
| `r_dodgymiptex` | `"1"` | When enabled, this will force regeneration of mipmaps, discarding mips1-4 like glquake did. This may eg solve fullbright issues with some maps, but may reduce distant detail levels. |
| `r_dodgypcxfiles` | `"0"` | When enabled, this will ignore the palette stored within pcx files, for compatibility with quake2. |
| `r_dodgytgafiles` | `"0"` | Many old glquake engines had a buggy tga loader that ignored bottom-up flags. Naturally people worked around this and the world was plagued with buggy images. Most engines have now fixed the bug, but you can reenable it if you have bugged tga files. |
| `r_drawentities` | `"1"` | Controls whether to draw entities or not.<br>0: Draw no entities.<br>1: Draw everything as normal.<br>2: Draw everything but bmodels.<br>3: Draw bmodels only. [cheat/ruleset restricted.] |
| `r_drawflame` | `"1"` | Set to -1 to disable ALL static entities. Set to 0 to disable only wall torches and standing flame. Set to 1 for everything drawn as normal. |
| `r_drawflat` (alias: `gl_textureless`) | `"0"` | Replace normal surface texturing with diagnostic flat shading where supported. [cheat/ruleset restricted.] |
| `r_drawviewmodel` | `"1"` | **Different:** Draw the first-person weapon model when the engine owns its rendering. Quake 3 gamecode uses cg_drawGun. |
| `r_drawviewmodelinvis` | `"0"` | **Q1 target:** draw the weapon model while invisible. |
| `r_dynamic` | `"1"` | **Different:** 0: no standard dlights at all.<br>1: coloured dlights will be used, they may show through walls. These are not realtime things.<br>2: The dlights will be forced to monochrome (this does not affect coronas/flashblends/rtlights attached to the same light). Quake 3 UI VM requests for r_dynamiclight are translated to this setting; r_dynamiclight is not an engine console alias. |
| `r_editlights` | `"0"` | enables .rtlights file editing mode. Consider using csaddon/equivelent instead. |
| `r_editlights_cursordistance` | `"1024"` | maximum distance of cursor from eye |
| `r_editlights_cursorgrid` | `"1"` | snaps cursor to this grid size |
| `r_editlights_cursorpushback` | `"0"` | how far to pull the cursor back toward the eye, for some reason |
| `r_editlights_cursorpushoff` | `"4"` | how far to push the cursor off the impacted surface |
| `r_editlights_import_ambient` | `"0"` | ambient light scaler for imported lights |
| `r_editlights_import_diffuse` | `"1"` | diffuse light scaler for imported lights |
| `r_editlights_import_radius` (alias: `r_editlights_quakelightsizescale`) | `"1"` | changes size of light entities loaded from a map |
| `r_editlights_import_specular` | `"1"` | specular light scaler for imported lights |
| `r_explosionlight` | `"1"` | **Q1 target:** strength of dynamic lights created by explosions. |
| `r_explosionlight_colour` | `"4.0 2.0 0.5"` | This controls the initial RGB values of EF_EXPLOSION effects. |
| `r_explosionlight_fade` | `"0.784 0.92 0.48"` | This controls the per-second RGB decay values of EF_EXPLOSION effects. |
| `r_ext_max_anisotropy` | on demand | **Q3:** Legacy Quake 3 UI anisotropic-filtering preferences; use gl_texture_anisotropy for this renderer. |
| `r_ext_texture_filter_anisotropic` | on demand | **Q3:** Legacy Quake 3 UI anisotropic-filtering preferences; use gl_texture_anisotropy for this renderer. |
| `r_fastsky` | `"0"` | Use a solid-color sky rather than the normal sky material. |
| `r_fastskycolour` | `"0"` | Color used by the simplified solid-color sky. |
| `r_fastturb` | `"0"` | Use the simplified liquid-surface path where supported. |
| `r_fastturbcolour` | `"0.1 0.2 0.3"` | The colour to use for water surfaces draw with r_waterstyle 0. |
| `r_fb_bmodels` (alias: `gl_fb_bmodels`) | `"1"` | Enables loading lumas on the map, as well as any external bsp models. [cheat/ruleset restricted.] |
| `r_fb_models` (alias: `gl_fb_models`) | `"1"` | Enables the use of lumas on models. Note that if ruleset_allow_fbmodels is enabled, then all models are unconditionally fullbright in deathmatch, because cheaters would set up their models like that anyway, hurrah for beating them at their own game. QuakeWorld players suck. [cheat/ruleset restricted.] |
| `r_finish` | on demand | **Q3:** Quake 3 UI request translated to legacy gl_finish by the plugin; not an SDL3 GPU synchronization setting. |
| `r_flares` | on demand | **Q3:** Legacy Quake 3 UI flare-rendering preference; not an SDL3 GPU flare switch. |
| `r_font_linear` | `"1"` | Use linear filtering for font textures. |
| `r_font_postprocess_mono` | `"0"` | Disables anti-aliasing on fonts. |
| `r_font_postprocess_outline` | `"0"` | Controls the number of pixels of dark borders to use around fonts. |
| `r_forceprogramify` (alias: `dpcompat_makeshitup`) | `"0"` | Reduce the shader to a single texture, and then make stuff up about its mother. The resulting fist fight results in more colour when you shine a light upon its face.<br>Set to 2 to ignore 'depthfunc equal' and 'tcmod scale' in order to tolerate bizzare shaders made for a bizzare engine.<br>Because most shaders made for DP are by people who _clearly_ have no idea what the heck they're doing, you'll typically need the '2' setting. |
| `r_fullbright` | `"0"` | 0 uses baked world lighting; 1 forces the fullbright/debug lighting path, subject to cheat/ruleset permissions. Works for Quake 1 BSP lightmaps and Quake 3 lightmapped or vertex-lit world surfaces in SDL3 GPU. Model/material behavior can also depend on gamecode. [cheat/ruleset restricted.] |
| `r_fullbrightSkins` | `"0.8"` | Force the use of fullbright skins on other players. No more hiding in the dark. [cheat/ruleset restricted.] |
| `r_fxaa` | `"0"` | Runs a post-procesing pass to strip the jaggies. |
| `r_globalskin_count` | `"10"` | Specifies how many globalskins there are. |
| `r_globalskin_first` | `"100"` | Specifies the first .skin value that is a global skin. Entities within this range will use the shader/image called 'gfx/skinSKIN.lmp' instead of their regular skin. See also: r_globalskin_count. |
| `r_glsl_emissive` | `"1"` | When set, specifies that the _luma or _glow textures are emissive... When 0 they are taken as a mask for the proportion of the lightmap that will apply (for q2e compat, has issues with overbrights). |
| `r_glsl_offsetmapping` | `"0"` | Enables the use of paralax mapping, adding fake depth to textures. |
| `r_glsl_offsetmapping_reliefmapping` | `"0"` | Changes the paralax sampling mode to be a bit nicer, but noticably more expensive at high resolutions. r_glsl_offsetmapping must be set. |
| `r_glsl_offsetmapping_scale` | `"0.04"` | Height/displacement scale for offset/parallax mapping in supporting materials, including the SDL3 GPU renderer. |
| `r_glsl_pbr` | `"0"` | Force PBR shading. |
| `r_glsl_precache` | `"0"` | Force all relevant glsl permutations to load upfront. |
| `r_glsl_skybox_autorotate` | `"1"` | Defines the axis around which skyboxes will rotate (the first three values). The fourth value defines the speed the skybox rotates at, in degrees per second. |
| `r_glsl_skybox_orientation` | `"0 0 0 0"` | Defines the axis around which skyboxes will rotate (the first three values). The fourth value defines the speed the skybox rotates at, in degrees per second. |
| `r_glsl_turbscale_reflect` | `"1"` | Controls the strength of the water reflection ripples (used by the altwater glsl code). |
| `r_glsl_turbscale_refract` | `"1"` | Controls the strength of the underwater ripples (used by the altwater glsl code). |
| `r_graphics` | `"1"` | Turning this off will result in ascii-style rendering. |
| `r_greenlight_colour` | `"0.5 3.0 0.5 200"` | The red, green, blue, radius values for EF_GREEN effects (rarely used). |
| `r_grenadetrail` | `"1"` | **Q1 target:** engine trail-effect selection for rockets/grenades respectively. [cheat/ruleset restricted.] |
| `r_halfrate` | `"0"` | Use half-rate shading (where supported by gpu). |
| `r_hdr_framebuffer` | `"0"` | If enabled, the map will be rendered into a high-precision image framebuffer. This avoids issues with shaders that contribute more than 1 in any single pass (like overbrights). Can also be set to the name of an image format, to force rendering to that format first - interesting formats are L8, RGB565, B10G11R11F, and others. |
| `r_hdr_irisadaptation` | `"0"` | Automatic exposure/iris adaptation based on sampled world lighting; the resulting exposure factor is used by SDL3 GPU scene materials. |
| `r_hdr_irisadaptation_fade_down` | `"0.5"` | Automatic exposure adjustment rates when darkening/brightening. |
| `r_hdr_irisadaptation_fade_up` | `"0.1"` | Automatic exposure adjustment rates when darkening/brightening. |
| `r_hdr_irisadaptation_maxvalue` | `"4"` | Lower/upper bounds on automatic exposure. |
| `r_hdr_irisadaptation_minvalue` | `"0.5"` | Lower/upper bounds on automatic exposure. |
| `r_hdr_irisadaptation_multiplier` | `"2"` | Automatic exposure target-brightness multiplier. |
| `r_ignoreentpvs` | `"1"` | Disables pvs culling of entities that have been submitted to the renderer. |
| `r_ignoremapprefixes` | `"0"` | Ignores when textures were loaded from map-specific paths.<br>0: textures/foo/tex.tga will not be confused with textures/foo/tex.tga.<br>1: The same texture might be loaded multiple times over. |
| `r_image_downloadsizelimit` | `"131072"` | The maximum allowed file size of images loaded from a web-based url. 0 disables completely, while empty imposes no limit. |
| `r_imageextensions` | `"dds ktx tga png jpg pcx"` | The list of image file extensions which might exist on disk (note that this does not list all supported formats, only the extensions that should be searched for). |
| `r_intensity` | `"1"` | **Q3:** Legacy Quake 3 renderer intensity preference created by the UI; not an SDL3 GPU brightness control. Use gamma/contrast and the documented renderer settings instead. |
| `r_keepimages` | `"0"` | Retain unused images in memory for slightly faster map loading.<br>0: Redundant images will be purged after each map change.<br>1: Images will be retained until vid_reload (potentially consuming a lot of ram). |
| `r_lavaalpha` | `""` | **Q1 target:** lava opacity; an unset/negative override follows the general liquid setting. |
| `r_lavastyle` | `"1"` | See r_waterstyle, but affects only lava. If empty, defers to r_waterstyle. |
| `r_lightflicker` | `"1"` | Allow flickering dynamic lights. |
| `r_lightmap` | `"0"` | Display lightmaps for lighting diagnosis instead of normal material texturing. [cheat/ruleset restricted.] |
| `r_lightmap_format` | `""` | Overrides the default texture format used for lightmaps. rgb9e5 is a good choice for HDR. |
| `r_lightmap_saturation` | `"1"` | Color saturation of baked lightmaps. |
| `r_lightprepass` | `"0"` | Experimental. Attempt to use a different lighting mechanism (aka: deferred lighting). Requires vid_reload to take effect. |
| `r_lightstylescale` | `"1"` | Global multiplier for animated lightstyle brightness. |
| `r_lightstylesmooth` | `"0"` | Interpolate between animated lightstyle values. |
| `r_lightstylesmooth_limit` | `"2"` | Limit on smoothing animated lightstyle transitions. |
| `r_lightstylespeed` | `"10"` | Playback speed of animated lightstyles. |
| `r_loadlit` | `"1"` | Whether to load lit files.<br>0: Do not load external rgb lightmap data.<br>1: Load but don't generate.<br>2: Generate ldr lighting (if none found).<br>3: Generate hdr lighting (if none found).<br>Note that regeneration of lightmap data may be unreliable if the map was made for more advanced lighting tools.<br>Deluxemap information will also be generated, as appropriate. |
| `r_mapoverbrightbits` | on demand | **Q3:** OpenArena fairness/compatibility lookup for legacy Quake 3 overbright bits; use gl_overbright for this fork's actual lightmap handling. |
| `r_max_gpu_bones` | `""` | Specifies the maximum number of bones that can be handled on the GPU. If empty, will guess. |
| `r_menutint` | `"0.68 0.4 0.13"` | Color tint for the engine's menu background material. |
| `r_meshpitch` | `"-1"` | Specifies the direction of the pitch angle on mesh models formats, Quake compatibility requires -1. |
| `r_meshroll` | `"1"` | Specifies the direction of the roll angle on mesh models formats, also affects gamecode, so do not change from its default. |
| `r_mirroralpha` | `"1"` | Specifies how the default shader is generated for the 'window02_1' texture. Values less than 1 will turn it into a mirror. [cheat/ruleset restricted.] |
| `r_mode` | on demand | **Q3:** Legacy Quake 3 UI numbered video-mode preference; use the native video menu in this fork. |
| `r_muzzleflash_colour` | `"1.5 1.3 1.0 200"` | This controls the initial RGB+radius of EF_MUZZLEFLASH/svc_muzzleflash effects. |
| `r_muzzleflash_fade` | `"1.5 0.75 0.375 1000"` | This controls the per-second RGB+radius decay of EF_MUZZLEFLASH/svc_muzzleflash effects. |
| `r_netgraph` | `"0"` | Displays a graph of packet latency. A value of 2 will give additional info about what sort of data is being received from the server. |
| `r_nolerp` | `"0"` | Disable model animation interpolation in the renderer. |
| `r_nolerp_list` | `""` | Models in this list will not interpolate. Any models included here should be considered bad. |
| `r_nolightdir` | `"0"` | Disable use of directional lighting information where available. |
| `r_norefresh` | `"0"` | Disable normal world refresh/rendering for diagnosis. |
| `r_normalmapping` | `"1"` | Enable normal maps on lit materials. Changes apply immediately. |
| `r_noshadow_list` (alias: `progs/missile.mdl,progs/flame.mdl,progs/flame2.mdl,progs/lavaball.mdl,progs/grenade.mdl,progs/spike.mdl,progs/s_spike.mdl,progs/laser.mdl,progs/lspike.mdl,progs/candle.mdl`) | `"r_noEntityCastShadowList"` | Models in this list will not cast shadows. |
| `r_novis` | `"0"` | Ignore map visibility/PVS filtering for diagnosis; can substantially increase rendering work. |
| `r_overbrightbits` | on demand | **Q3:** OpenArena fairness/compatibility lookup for legacy Quake 3 overbright bits; use gl_overbright for this fork's actual lightmap handling. |
| `r_part_beams` | `"1"` | Enable particle beam effects. |
| `r_part_classic_expgrav` | `"10"` | Scaler for how fast classic explosion particles should accelerate due to gravity. 1 for like vanilla, 10 for like zquake. |
| `r_part_classic_opaque` | `"0"` | Disables transparency on classic particles, for the oldskool look. |
| `r_part_classic_square` | `"0"` | Enables square particles, for the oldskool look. |
| `r_part_contentswitch` | `"1"` | Enable particle effects to change based on content (ex. water). |
| `r_part_density` | `"1"` | Engine particle-density multiplier. |
| `r_part_maxdecals` | `"8192"` | Maximum number of engine particle decals. |
| `r_part_maxparticles` | `"65536"` | Maximum number of engine particles. |
| `r_panini_filter` | `"1"` | Panini reconstruction in either game: 0 selects original bilinear filtering; 1 selects bicubic filtering for magnified areas and anisotropic mip filtering for reduced areas. Applies immediately to SDL3 GPU Panini (`r_projection 6`), including offscreen/split views; does not affect Regular projection or HUD/menu rendering. Archived; **Options → Panini Filter**. |
| `r_panini_scale` | `"1.25"` | Panini source width/height multiplier, clamped to 1–2. Larger sources preserve more detail but cost GPU time and memory; 2 requests four times the native pixel count. Limited to 16 million level-zero pixels and 8192 pixels per axis, without reducing an already larger native view. Allocation failure falls back to native size. `gpu_info` reports actual dimensions. Applies immediately, archived; **Options → Panini Resolution** in either game. |
| `r_panini_sharpen` | `"0.2"` | Panini local-contrast sharpening, clamped to 0–1. Zero disables it. Limits overshoot, fades out in reduced areas and preserves HDR highlights. Cannot recover missing source detail at extreme FOVs. Applies immediately, archived; **Options → Panini Sharpening** in either game. |
| `r_part_rain` | `"0"` | Enable particle effects to emit off of surfaces. Mainly used for weather or lava/slime effects. |
| `r_part_rain_quantity` | `"1"` | Quantity of particles generated for weather/rain effects. |
| `r_part_sparks` | `"1"` | Enable particle spark effects. |
| `r_part_sparks_textured` | `"1"` | Use textured spark effects. |
| `r_part_sparks_trifan` | `"1"` | Use triangle-fan spark effects where supported. |
| `r_particle_tracelimit` | `"0x7fffffff"` | Number of traces to allow per frame for particle physics. |
| `r_particledesc` (alias: `r_particlesdesc`) | `"classic"` | Particle effect-description set to load. [cheat/ruleset restricted.] |
| `r_particlesystem` | `"script"` | **Different:** Select the engine particle implementation. Quake 3 gamecode can also submit its own shader-based effects. [cheat/ruleset restricted.] |
| `r_picmip` | on demand | **Q3:** Quake 3 UI requests are translated to gl_picmip, which is not registered in the audited SDL3 GPU build. This is a compatibility lookup, not a working console alias. |
| `r_polygonoffset_shadowmap_factor` | `"0.05"` | Slope/constant depth-bias settings for shadow-map geometry. |
| `r_polygonoffset_shadowmap_offset` | `"0"` | Slope/constant depth-bias settings for shadow-map geometry. |
| `r_polygonoffset_stencil_factor` | `"0.01"` | Legacy slope/constant polygon-offset settings for stencil-shadow rendering. |
| `r_polygonoffset_stencil_offset` | `"1"` | Legacy slope/constant polygon-offset settings for stencil-shadow rendering. |
| `r_polygonoffset_submodel_factor` | Q1: `"0.05"`; Q3: `"0"` | z-fighting avoidance. Pushes submodel depth values slightly towards the camera depending on the slope of the surface. |
| `r_polygonoffset_submodel_map` | `"e?m? r?m? hip?m?"` | List of maps on which z-fighting reduction should be used. wildcards accepted. |
| `r_polygonoffset_submodel_offset` | Q1: `"25"`; Q3: `"0"` | z-fighting avoidance. Pushes submodel depth values slightly towards the camera by a consistent distance. |
| `r_postprocshader` | `""` | Specifies a custom shader to use as a post-processing shader |
| `r_powerupglow` | `"1"` | **Q1 target:** enable dynamic-light glow around powerup-bearing entities. |
| `r_projection` | `"0"` | 0: regular perspective.<br>1: stereographic.<br>2: fisheye.<br>3: panoramic.<br>4: lambert azimuthal equal-area.<br>5: equirectangular.<br>6: panini. The native menu exposes only Regular (0) and Panini (6). Other legacy projection code remains for reference; the SDL3 GPU implementation is described in SDL3-GPU.md. |
| `r_redlight_colour` | `"3.0 0.5 0.5 200"` | The red, green, blue, radius values for EF_RED effects (typically used for pentagram in quakeworld). |
| `r_refract_fbo` | `"1"` | Use an fbo for refraction. If 0, just renders as a portal and uses a copy of the current framebuffer. |
| `r_refractreflect_scale` | `"0.5"` | Use a different scale for refraction and reflection texturemaps. Because $reasons. |
| `r_renderscale` | `"1"` | Retained legacy renderer subsampling/supersampling setting; unused by the SDL3 GPU renderer. Use `r_panini_scale` to change the Panini source resolution. |
| `r_replacemodels` | `"md3 md2 md5mesh"` | Filename extensions to try instead of MDL. This enables remaster MD5 replacements; an empty list selects original models. Set before loading/reloading a map. See [remaster model preferences](QuakeRemaster.md#enhanced-and-classic-models). |
| `r_rocketlight` | `"1"` | **Q1 target:** strength of dynamic lights attached to rockets. |
| `r_rocketlight_colour` | `"2.0 1.0 0.25 200"` | This controls the RGB+radius values of MF_ROCKET effects. |
| `r_rockettrail` | `"1"` | **Q1 target:** engine trail-effect selection for rockets/grenades respectively. [cheat/ruleset restricted.] |
| `r_shadow_bumpscale_basetexture` | `"0"` | bumpyness scaler for generation of fallback normalmap textures from models |
| `r_shadow_bumpscale_bumpmap` | `"4"` | bumpyness scaler for _bump textures |
| `r_shadow_heightscale_basetexture` | `"0"` | scaler for generation of height maps from legacy paletted content. |
| `r_shadow_heightscale_bumpmap` | `"1"` | height scaler for 8bit _bump textures |
| `r_shadow_playershadows` | `"1"` | Controls the presence of shadows on the local player. |
| `r_shadow_raytrace` | `"0"` | Enables use of hardware raytracing for shadows. Consider also using with r_halfrate. |
| `r_shadow_realtime_dlight` (alias: `r_shadow_realtime_dynamic`) | `"1"` | Enables the use of dynamic realtime lights, allowing explosions to use bumpmaps etc properly. |
| `r_shadow_realtime_dlight_ambient` | `"0"` | Ambient contribution of real-time dynamic lights. |
| `r_shadow_realtime_dlight_diffuse` | `"1"` | Diffuse contribution of real-time dynamic lights. |
| `r_shadow_realtime_dlight_shadows` | `"1"` | Allows dynamic realtime lights to cast shadows as they move. |
| `r_shadow_realtime_dlight_specular` | `"4"` | Specular contribution of real-time dynamic lights. |
| `r_shadow_realtime_nonworld_lightmaps` | `"1"` | Scaler for lightmaps used when not using realtime world lighting. Probably broken. |
| `r_shadow_realtime_world` | `"0"` | Enables the use of static/world realtime lights. |
| `r_shadow_realtime_world_importlightentitiesfrommap` | `"0"` | Controls default loading of map-based realtime lights.<br>0: Load explicit .rtlight files only.<br>1: Load explicit lights then try fallback to parsing the entities lump.<br>2: Load only the entities lump. |
| `r_shadow_realtime_world_lightmaps` | `"0"` | Specifies how much of the map's normal lightmap to retain when using world realtime lights. 0 completely replaces lighting. |
| `r_shadow_realtime_world_shadows` | `"1"` | Enable shadows for real-time world lights. |
| `r_shadow_scissor` | `"1"` | constrains stencil shadows to the onscreen box that contains the maxmium extents of the light. This avoids unnecessary work. |
| `r_shadow_shadowmapping` | `"1"` | Enables soft shadows instead of stencil shadows. |
| `r_shadow_shadowmapping_bias` | `"0.03"` | Depth bias used to reduce shadow-map self-shadowing artifacts. |
| `r_shadow_shadowmapping_depthbits` | `"16"` | Shadowmap depth bits. 16, 24, or 32. |
| `r_shadow_shadowmapping_nearclip` | `"1"` | Near clipping distance used for shadow-map rendering. |
| `r_shadow_shadowmapping_precision` | `"1"` | Scales the shadowmap detail level up or down. |
| `r_shadows` | `"0"` | Draw basic blob shadows underneath entities without using realtime lighting. |
| `r_shadows_fakedistance` | `"1024"` | The radius to use for fake shadows. |
| `r_shadows_focus` | `"0 0 0"` | Offset for the center of the fake-shadows volume. |
| `r_shadows_throwdirection` | `"0 0 -1"` | The direction to throw the fake shadows in. Should ideally be opposite to r_sun_dir, but that just shows how fake these things actually are. |
| `r_showbboxes` | `"0"` | Debugging. Shows bounding boxes. 1=ssqc, 2=csqc. Red=solid, Green=stepping/toss/bounce, Blue=onground. [cheat/ruleset restricted.] |
| `r_showfields` | `"0"` | Debugging. Shows entity fields boxes (entity closest to crosshair). 1=ssqc, 2=csqc, 3=snapshots. |
| `r_showshaders` | `"0"` | Debugging. Shows the name of the (worldmodel) shader being pointed to. |
| `r_showtris` (alias: `r_wireframe`) | `"0"` | Developer feature where everything is drawn with wireframe over the top. Only active where cheats are permitted. [cheat/ruleset restricted.] |
| `r_skybox` | `""` | Override skybox name; empty allows map/gamecode sky selection. |
| `r_skycloudalpha` | `"1"` | Controls how opaque the front layer of legacy scrolling skies should be. |
| `r_skyfog` | `"0.5"` | This controls an alpha-blend value for fog on skyboxes, cumulative with regular fog alpha. |
| `r_slimealpha` | `""` | **Q1 target:** slime opacity; an unset/negative override follows the general liquid setting. |
| `r_slimestyle` | `""` | See r_waterstyle, but affects only slime. If empty, defers to r_waterstyle. |
| `r_softwarebanding` | `"0"` | Utilise the Quake colormap in order to emulate 8bit software rendering. This results in banding as well as other artifacts that some believe adds character. Also forces nearest sampling on affected surfaces (palette indicies do not interpolate well). |
| `r_speeds` | `"0"` | Display rendering workload/timing counters. |
| `r_sprite_backfacing` | Q1: `"1"`; Q3: `"0"` | Make oriented sprites face backwards relative to their orientation, for compat with q1. |
| `r_stainfadeammount` | `"1"` | Amount removed from stains per fade step; spelling is retained for compatibility. |
| `r_stainfadetime` | `"1"` | Time interval used to fade surface stains. |
| `r_stains` | `"0"` | Enable engine surface stains. |
| `r_stereo_convergence` | `"0"` | Nudges the angle of each eye inwards when using stereoscopic rendering. |
| `r_stereo_method` | `"0"` | Value 0 = Off.<br>Value 1 = Attempt hardware acceleration. Requires vid_restart.<br>Value 2 = red/cyan.<br>Value 3 = red/blue.<br>Value 4=red/green.<br>Value 5=eye strain. |
| `r_stereo_separation` | `"4"` | How far apart your eyes are, in quake units. A non-zero value will enable stereoscoping rendering. You might need some of them retro 3d glasses. Hardware support is recommended, see r_stereo_context. |
| `r_stereoSeparation` | on demand | **Q3:** Legacy Quake 3 gamecode stereo separation lookup; renderer stereo support determines the effect. |
| `r_subdivisions` | `"2"` | Subdivision granularity for curved/warped map surfaces; renderer/map-format support determines the effect. |
| `r_sun_colour` | `"0 0 0"` | Specifies the colour of sunlight that appears in the form of crepuscular rays. |
| `r_sun_dir` | `"0.2 0.5 0.8"` | Specifies the direction that crepusular rays appear along |
| `r_telealpha` | `""` | **Q1 target:** teleporter-liquid opacity; an unset/negative override follows the general liquid setting. |
| `r_telestyle` | `"1"` | See r_waterstyle, but affects only teleporters. If empty, defers to r_waterstyle. |
| `r_temporalscenecache` (alias: `r_scenecache`) | `""` | Controls whether to generate+reuse a scene cache over multiple frames. This is generated on a separate thread to avoid any associated costs. This can significantly boost framerates on complex maps, but can also stress the gpu more (performance tradeoff that varies per map). An outdated cache may be used if the cache takes too long to build (eg: lightmap animations), which could cause the odd glitch when moving fast (but retain more consistent framerates - another tradeoff).<br>0: Tranditional quake rendering.<br>1: Generate+Use the scene cache. |
| `r_texturebits` | on demand | **Q3:** Legacy Quake 3 UI texture bit-depth preference; not an SDL3 GPU texture-storage selection switch. |
| `r_torch` | `"0"` | Generate a dynamic light at the player's position. [cheat/ruleset restricted.] |
| `r_tracker_fadetime` | `"1"` | how long it takes for r_tracker messages to fully fade once they start fading |
| `r_tracker_frags` | `"0"` | 0: like vanilla quake<br>1: shows only your kills/deaths<br>2: shows all kills |
| `r_tracker_lines` (alias: `r_tracker_messages`) | `"8"` | number of r_tracker messages to display |
| `r_tracker_time` | `"4"` | how long it takes for r_tracker messages to start fading |
| `r_tracker_w` | `"0.5"` | width of the r_tracker messages, as a fraction of the screen's width, eg 0.5 |
| `r_tracker_x` | `"0.5"` | left position of the r_tracker messages, as a fraction of the screen's width, eg 0.5 |
| `r_tracker_y` | `"0.333"` | top position of the r_tracker messages, as a fraction of the screen's height, eg 0.333 |
| `r_vertexLight` | `"0"` | **Q3:** Legacy Quake 3 vertex-lighting preference created by gamecode/UI; registration does not implement ioquake3's vertex-light renderer switch in SDL3 GPU. |
| `r_viewmodel_fov` | `""` | field of vision, 1-170 degrees, standard fov is 90, nquake defaults to 108. |
| `r_viewmodel_quake` | `"0"` | Controls whether to use weird viewmodel movements from vanilla quake. |
| `r_viewpreselgun` | `"0"` | HACK: Display the preselected weaponmodel, instead of the current weaponmodel. |
| `r_wateralpha` | `"1"` | **Different:** Water opacity. Quake 1 liquid surfaces use this; Quake 3 shader-stage blending is defined by the material instead. |
| `r_waterstyle` | `"1"` | Changes how water, and teleporters are drawn. Possible values are:<br>0: fastturb-style block colour.<br>1: regular q1-style water.<br>2: refraction(ripply and transparent)<br>3: refraction with reflection at an angle<br>4: ripplemapped without reflections (requires particle effects)<br>5: ripples+reflections |
| `r_waterwarp` | `"1"` | Enables fullscreen warp, preferably via glsl. -1 specifies to force the fov warp fallback instead which can give a smidge more performance. |
| `r_wireframe_smooth` | `"0"` | Wireframe line-smoothing preference retained by renderer code; backend support varies. |
| `r_zProj` | on demand | **Q3:** Legacy Quake 3 gamecode stereo projection-distance lookup; not the engine's FOV setting. |
| `rank_autoadd` | `"1"` | Automatically register players into the ranking system |
| `rank_filename` | `""` | Specifies which file to use as a rankings database. Enables the ranking system if set. |
| `rank_needlogin` | `"0"` | If set to 1, prohibits players from joining if they're not yet registered. This will require an external mechanism to register users, presumably via rcon. |
| `rank_parms_first` | `"0"` | Mod setting: first parm saved |
| `rank_parms_last` | `"31"` | Mod setting: the index of the last parm to be saved. Clamped to 32. |
| `rate` (alias: `_cl_rate`) | `"30000"` | A rough measure of the bandwidth to try to use while playing. Too high a value may result in 'buffer bloat'. |
| `rcon_address` | not exposed | Optional remote-console destination; otherwise use the connected server. |
| `rcon_level` | `"20"` | Permission level used when executing remote-console commands. |
| `rcon_password` | not exposed | Password used for remote console authentication; do not publish it in configs. |
| `record_flush` | `"0"` | If set, explicitly flushes demo data to disk while recording. This may be inefficient, depending on how your operating system is configured. |
| `registered` | dynamic / platform dependent | **Q1 target:** status indicating whether the registered Quake data (pak1.pak) is available; informational, not an OpenArena license requirement. |
| `route_shownodes` | `"0"` | Draw navigation-route nodes for diagnosis where route support is active. |
| `ruleset` | `"none"` | Known rulesets are:<br>none: no explicit rules, all 'minor cheats' are allowed.<br>strict: equivelent to the smackdown ruleset. Note that this will block certain graphical enhancements too. |
| `ruleset_allow_fbmodels` | Q1: `"1"`; Q3: `"0"` | When 1, allows all models to be displayed fullbright, completely ignoring the lightmaps. This feature exists only for parity with ezquake's defaults. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_frj` | `"1"` | Specifies whether Forward-Rocket-Jump scripts are allowed in the current ruleset. If 0, limits on yaw speed will be imposed so they cannot be scripted. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_in` | `"1"` | Allow ruleset-controlled config inclusion according to the ruleset loader. |
| `ruleset_allow_larger_models` | `"1"` | Enforces a maximum bounds limit on models, to prevent the use of additional spikes attached to the model from being used as a kind of wallhack. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_localvolume` | `"1"` | Allows the use of the snd_playersoundvolume cvar. Muting your own sounds can make it easier to hear where your opponent is. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_modified_eyes` | `"0"` | When 0, completely hides progs/eyes.mdl if it is not strictly identical to vanilla quake. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_overlong_sounds` | `"1"` | A setting of 0 will block the use of extra-long pickup sounds as item respawn timers. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_packet` | `"1"` | If 0, network packets sent via the 'packet' command will be blocked. This makes scripting timers a little harder. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_particle_lightning` | `"1"` | A setting of 0 blocks using the particle system to replace lightning gun trails. This prevents making the trails thinner thus preventing them from obscuring your view of your enemies. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_playercount` | `"1"` | Specifies whether teamplay triggers that count nearby players are allowed in the current ruleset. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_semicheats` | `"1"` | If 0, this blocks a range of cvars that are marked as semi-cheats. Such cvars will be locked to their empty/0 value. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_sensitive_texture_replacements` | `"1"` | Allows the replacement of certain model textures (as well as the models themselves). This prevents adding extra fullbrights to make them blatently obvious. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_shaders` | `"1"` | When 0, this completely disables the use of external shader files, preventing custom shaders from being used for wallhacks. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |
| `ruleset_allow_watervis` | `"1"` | When 0, this enforces ugly opaque water. You should not normally change this cvar from its permissive default, instead impose limits on yourself only through the 'ruleset' cvar. |

### Cvars: S–Z

| Cvar (aliases) | Default | Definition / game notes |
| --- | --- | --- |
| `s_al_debug` | `"0"` | Enables periodic checks for OpenAL errors. |
| `s_al_disable` | `"0"` | 0: OpenAL works (generally as the highest priority).<br>1: OpenAL will be used only when a specific device is selected.<br>2: Don't allow ANY use of OpenAl.<br>With OpenAL disabled, audio ouput will fall back to platform-specific output, avoiding miscilaneous third-party openal limitation bugs. |
| `s_al_distancemodel` | `"2"` | Controls how sounds fade with distance.<br>0: Inverse (most realistic)<br>1: Inverse Clamped<br>2: Linear (Quake-like)<br>3: Linear Clamped<br>4: Exponential<br>5: Exponential Clamped<br>6: None |
| `s_al_dopplerfactor` | `"1.0"` | Multiplies the strength of doppler effects. |
| `s_al_hrtf` | `""` | Enables use of HRTF, and which HRTF table to use.<br>empty: auto, depending on openal config to enable it. |
| `s_al_reference_distance` | `"120"` | This is the distance at which the sound is audiable with standard volume in the inverse distance models. Nearer sounds will be louder than the original sample. |
| `s_al_speedofsound` | `"343.3"` | Configures the speed of sound, in game units per second. This affects doppler. |
| `s_al_static_listener` | `"0"` | OpenAL-specific static-listener behavior; does not control SDL audio mixing. |
| `s_al_use_reverb` | `"1"` | Controls whether reverb effects will be used. Set to 0 to block them. Reverb requires gamecode to configure the reverb properties, other than underwater. |
| `s_al_velocityscale` | `"1"` | Rescales velocity values, before doppler can be calculated. |
| `s_ambientfade` (alias: `ambient_fade`) | `"100"` | Rate of volume change for ambient sound channels. |
| `s_ambientlevel` (alias: `ambient_level`) | `"0.3"` | This controls the volume levels of automatic area-based sounds (like water or sky), and is quite annoying. If you're playing deathmatch you'll definitely want this OFF. |
| `s_bits` (alias: `snd_samplebits`) | `"16"` | Requested sound sample/output bit depth. |
| `s_buffersize` (alias: `snd_buffersize`) | `"0"` | Sound buffering size preference. |
| `s_compression` | on demand | **Q3:** Legacy Quake 3 UI compressed-sound preference; not an SDL3 sound-decoder selection switch. |
| `s_device` (alias: `snd_device`) | `""` | This is the sound device(s) to use, in the form of driver:device.<br>If desired, multiple devices can be listed in space-seperated (quoted) tokens. _s_device_opts contains any enumerated options.<br>In all seriousness, use the menu to set this if you wish to keep your hair. |
| `s_doppler` (alias: `snd_doppler`) | `"0"` | Enables doppler, with a multiplier for the scale. |
| `s_doppler_max` (alias: `snd_doppler_max`) | `"2"` | Highest allowed doppler scale, to avoid things getting too weird. |
| `s_doppler_min` (alias: `snd_doppler_min`) | `"0.5"` | Slowest allowed doppler scale. |
| `s_eax` (alias: `snd_eax`) | `"0"` | Legacy environmental-audio preference; depends on the selected audio driver. |
| `s_inactive` (alias: `snd_inactive`) | `"1"` | Play sound while application is inactive (ie: tabbed out). Needs a snd_restart if changed. |
| `s_khz` (alias: `snd_khz`) | `"48"` | Sound speed, in kilohertz. Common values are 11, 22, 44, 48. Values above 1000 are explicitly in hertz. |
| `s_linearresample` (alias: `snd_linearresample`) | `"1"` | Use linear interpolation when resampling sound effects. |
| `s_linearresample_stream` (alias: `snd_linearresample_stream`) | `"0"` | Use linear interpolation when resampling streamed audio. |
| `s_loadas8bit` (alias: `loadas8bit`) | `"0"` | Downsample sounds on load as lower quality 8-bit sound, to save memory. |
| `s_localvolume` (alias: `snd_localvolume`) | `"1"` | Sound level for sounds local or originating from the player such as firing and pain sounds. |
| `s_mixahead` (alias: `_snd_mixahead`) | `"0.1"` | Specifies how many seconds to prebuffer audio. Lower values give less latency, but might result in crackling. Different hardware/drivers have different tolerances, and this setting may be ignored completely where drivers are expected to provide their own tolerances. |
| `s_mixerthread` (alias: `snd_mixerthread`) | `"1"` | When enabled sound mixing will be run on a separate thread. Currently supported only by directsound. Other drivers may unconditionally thread audio. Set to 0 only if you have issues. |
| `s_noextraupdate` (alias: `snd_noextraupdate`) | `"0"` | Disable extra sound-mixer updates during long frames. |
| `s_nominaldistance` (alias: `snd_soundradius`) | `"1000"` | This cvar defines how far an attenuation=1 sound can be heard. [cheat/ruleset restricted.] |
| `s_numspeakers` (alias: `snd_numspeakers`) | `"2"` | Number of hardware audio channels to use. FTE QW supports up to 6. |
| `s_precache` (alias: `precache`) | `"1"` | Load sound samples during precaching instead of waiting for first playback. |
| `s_show` (alias: `snd_show`) | `"0"` | Print active sound/mixer diagnostics. |
| `s_swapstereo` (alias: `snd_leftisright`) | `"0"` | Exchange left and right sound channels. |
| `s_useopenal` | on demand | **Q3:** OpenArena sound-menu preference for the legacy ioquake3 OpenAL backend; this fork selects its audio backend through its own sound settings. Creating this cvar does not switch SDL audio to OpenAL. |
| `samelevel` | `""` | **Q1 target:** stay on the same level after an exit in supporting gamecode. |
| `saved1` | `"0"` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `saved2` | `"0"` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `saved3` | `"0"` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `saved4` | `"0"` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `savedgamecfg` | `"0"` | **Q1 target:** gamecode-owned configuration value; meaning is defined by the mod. |
| `sb_alpha` | `"0.7"` | Opacity of the native server-browser UI. |
| `sb_filtertext` | `""` | Text filter for the native server browser. |
| `sb_hidedead` | `"1"` | Hide servers that failed to reply. |
| `sb_hideempty` | `"0"` | Hide empty servers. |
| `sb_hidefull` | `"0"` | Hide full servers. |
| `sb_hidenetquake` | `"0"` | Hide NetQuake/QuakeWorld protocol entries respectively. |
| `sb_hidenotempty` | `"0"` | Hide servers containing players. |
| `sb_hideproxies` | `"1"` | Hide proxy entries. |
| `sb_hidequakeworld` | `"0"` | Hide NetQuake/QuakeWorld protocol entries respectively. |
| `sb_showaddress` | `"0"` | Show the corresponding address, limit, game directory, map, latency, player count or time-limit column in the native server browser. |
| `sb_showfraglimit` | `"0"` | Show the corresponding address, limit, game directory, map, latency, player count or time-limit column in the native server browser. |
| `sb_showgamedir` | `"0"` | Show the corresponding address, limit, game directory, map, latency, player count or time-limit column in the native server browser. |
| `sb_showmap` | `"0"` | Show the corresponding address, limit, game directory, map, latency, player count or time-limit column in the native server browser. |
| `sb_showping` | `"1"` | Show the corresponding address, limit, game directory, map, latency, player count or time-limit column in the native server browser. |
| `sb_showplayers` | `"1"` | Show the corresponding address, limit, game directory, map, latency, player count or time-limit column in the native server browser. |
| `sb_showtimelimit` | `"0"` | Show the corresponding address, limit, game directory, map, latency, player count or time-limit column in the native server browser. |
| `sb_sortcolumn` | `"0"` | Column used to sort the native server browser. |
| `sbar_teamstatus` | `"1"` | Display the last team say from each of your team members just above the sbar area. |
| `scr_allowsnap` | `"0"` | Whether to allow server-requested screenshot requests to check for gpu driver hacks. |
| `scr_autoid` | `"1"` | Display nametags above all players while spectating. |
| `scr_autoid_armor` | `"1"` | Display armour as part of nametags (when known). |
| `scr_autoid_enemycolour` | `"15"` | The colour for the text on the nametags of non-team members. |
| `scr_autoid_health` | `"1"` | Display health as part of nametags (when known). |
| `scr_autoid_team` | `"0"` | Display nametags above team members. 0: off. 1: display with half-alpha if occluded. 2: hide when occluded. |
| `scr_autoid_teamcolour` | `"9"` | The colour for the text on the nametags of team members. |
| `scr_autoid_weapon` | `"1"` | Display the player's best weapon as part of their nametag (when known). |
| `scr_autoid_weapon_mask` | `"126"` | Mask of bits for weapon icons to actually show.<br>+1: Shotgun.<br>+2: Super Shotgun.<br>+4: Nail Gun.<br>+8: Super Nail Gun.<br>+16: Grenade Launcher.<br>+32: Rocket Launcher.<br>+64: Lightning<br>Showing only RL and GL is 96 |
| `scr_centersbar` | `"2"` | **Q1 target:** center the classic status bar horizontally. |
| `scr_centertime` | `"2"` | Centerprint messages will be displayed for this long before disappearing. |
| `scr_conalpha` | `"0.7"` | How opaque the console should be when there's still stuff going on behind. |
| `scr_consize` | `"0.5"` | Proportion of the screen to be covered by the console when its focused. Valid range is 0ish to 1. |
| `scr_conspeed` | `"2000"` | How fast the console careers onto the screen. |
| `scr_diskicontimeout` | `"0.3"` | Time to retain the disk-access indicator after activity. |
| `scr_fov_mode` | `"4"` | Controls what the fov cvar actually controls:<br>0: largest axis (ultra-wide monitors means less height will be visible).<br>1: smallest axis (ultra-wide monitors will distort at the edges).<br>2: horizontal axis.<br>3: vertical axis.<br>4: 4:3 horizontal axis, padded for wider resolutions (for a more classic fov) Native menu label for mode 4: **Classic 4:3 / Widescreen**. Quake 3 cgame establishes its own aspect/FOV convention; changing the shared fov setting is the supported OpenArena view-width control. |
| `scr_loadingrefresh` | `"0"` | Force redrawing of the loading screen, in order to display EVERY resource that is loaded |
| `scr_loadingscreen_aspect` | `"0"` | Controls the aspect of levelshot images.<br>0: Use source image's aspect.<br>1: Force 4:3 aspect (ignore image's aspect), for best q3 compat.<br>2: Ignore aspect considerations and just smear it over the entire screen.<br>-1: Disable levelshot use. |
| `scr_loadingscreen_picture` | `"gfx/loading"` | Image used by the engine loading screen. |
| `scr_loadingscreen_scale` | `"1"` | Scaling mode for the engine loading-screen image. |
| `scr_loadingscreen_scale_limit` | `"2"` | Maximum scaling limit for the engine loading-screen image. |
| `scr_neticontimeout` | `"0.3"` | Time without updates before showing the network warning icon. |
| `scr_printspeed` | `"16"` | How rapidly each character is displayed during outro messages. |
| `scr_scoreboard_afk` | `"1"` | Show 'afk' in the packetloss column when they're afk. |
| `scr_scoreboard_backgroundalpha` | `"0.5"` | Further multiplier for the background alphas. |
| `scr_scoreboard_drawtitle` | `"1"` | Wastes screen space when looking at the scoreboard. |
| `scr_scoreboard_fillalpha` | `"0.7"` | Transparency amount for newstyle scoreboard. |
| `scr_scoreboard_forcecolors` | `"0"` | Makes the scoreboard colours obey enemycolor/teamcolor rules. |
| `scr_scoreboard_newstyle` | `"1"` | Display team colours and stuff in a style popularised by Electro. Looks more modern, but might not quite fit classic huds. |
| `scr_scoreboard_ping_status` | `"25 50 100 150"` | Threshholds required to switch ping display from green to white, yellow, megenta and red. |
| `scr_scoreboard_showflags` | `"2"` | Display flag caps+touches on the scoreboard, where our fragfile.dat supports them.<br>0: off<br>1: on<br>2: on only if someone appears to have interacted with a flag. |
| `scr_scoreboard_showfrags` | `"0"` | Display kills+deaths+teamkills, as determined by fragfile.dat-based conprint parsing. These may be inaccurate if you join mid-game. |
| `scr_scoreboard_showhealth` | `"3"` | Display health information when playing mvd/qtv streams.<br>0: off<br>1: on<br>2: show armour too. 3: combined health ('+' says more armour than health allows). |
| `scr_scoreboard_showlocation` | `"1"` | Display player location names when playing mvd/qtv streams, if available. |
| `scr_scoreboard_showruleset` | `"1"` | **Q1 target:** show players' reported rulesets on the engine scoreboard. |
| `scr_scoreboard_showweapon` | `"1"` | Display weapon information when playing mvd/qtv streams. |
| `scr_scoreboard_teamscores` | `"1"` | Makes +showscores act as +showteamscores. Because reasons. |
| `scr_scoreboard_teamsort` | `"0"` | On the scoreboard, sort players by their team BEFORE their personal score. |
| `scr_scoreboard_titleseperator` | `"1"` | Separator text used in the engine scoreboard title. |
| `scr_showdisk` | `"0"` | Display the engine disk-access indicator. |
| `scr_showdisk_x` | `"-24"` | Screen position of the scr showdisk overlay on the x axis. |
| `scr_showdisk_y` | `"0"` | Screen position of the scr showdisk overlay on the y axis. |
| `scr_showloading` | `"1"` | Display the engine loading indicator. |
| `scr_showobituaries` | `"0"` | Enable the engine's obituary/death-message overlay when supported by the protocol/gamecode. |
| `scr_sshot_compression` | `"75"` | Requsted compression ratio as a percentage. For jpeg this is the quantisation quality and has a direct impact on image quality vs size. For png this ranges between 0 for best and 100 for worst with final image quality being unchanged because png is loseless. |
| `scr_sshot_prefix` | `"screenshots/fte-"` | Prefix used when choosing automatic screenshot filenames. |
| `scr_sshot_type` | `"png"` | This specifies the default extension(and thus file format) for screenshots.<br>Known extensions are: png, jpg/jpeg, bmp, pcx, tga, ktx, dds. |
| `scr_turtlefps` | `"10"` | Frame-rate threshold for the slow-frame/turtle warning icon. |
| `scr_usekfont` | `"0"` | Exists for compat with the quake rerelease, changing the behaviour of QC's sprint/bprint/centerprint builtins. |
| `scratch1` | `"0"` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `scratch2` | `"0"` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `scratch3` | `"0"` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `scratch4` | `"0"` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `secure` | `""` | **Q1 target:** legacy client-validation/security mode; not a substitute for encrypted transport. |
| `sensitivity` | `"10"` | Mouse sensitivity multiplier; shared engine input setting in both games. |
| `server1` | `""` | **Q3:** Favorite server address slot 1 in the OpenArena menu. |
| `server10` | `""` | **Q3:** Favorite server address slot 10 in the OpenArena menu. |
| `server11` | `""` | **Q3:** Favorite server address slot 11 in the OpenArena menu. |
| `server12` | `""` | **Q3:** Favorite server address slot 12 in the OpenArena menu. |
| `server13` | `""` | **Q3:** Favorite server address slot 13 in the OpenArena menu. |
| `server14` | `""` | **Q3:** Favorite server address slot 14 in the OpenArena menu. |
| `server15` | `""` | **Q3:** Favorite server address slot 15 in the OpenArena menu. |
| `server16` | `""` | **Q3:** Favorite server address slot 16 in the OpenArena menu. |
| `server2` | `""` | **Q3:** Favorite server address slot 2 in the OpenArena menu. |
| `server3` | `""` | **Q3:** Favorite server address slot 3 in the OpenArena menu. |
| `server4` | `""` | **Q3:** Favorite server address slot 4 in the OpenArena menu. |
| `server5` | `""` | **Q3:** Favorite server address slot 5 in the OpenArena menu. |
| `server6` | `""` | **Q3:** Favorite server address slot 6 in the OpenArena menu. |
| `server7` | `""` | **Q3:** Favorite server address slot 7 in the OpenArena menu. |
| `server8` | `""` | **Q3:** Favorite server address slot 8 in the OpenArena menu. |
| `server9` | `""` | **Q3:** Favorite server address slot 9 in the OpenArena menu. |
| `session` | dynamic / platform dependent | **Q3:** session game-type marker used to decide whether stored player sessions remain valid. |
| `session0` | dynamic / platform dependent | **Q3:** Quake 3 session data for client slot 0, maintained by gamecode; additional session slots are created as clients join. |
| `show_fps` (alias: `scr_showfps`) | `"0"` | Displays the current framerate on-screen.<br>0: Off.<br>1: framerate average over a second.<br>2: Show a frametimes graph (with additional timing info).<br>-1: Normalized graph that focuses on the variation ignoring base times. |
| `show_fps_x` | `"-1"` | Screen position of the show fps overlay on the x axis. |
| `show_fps_y` | `"-1"` | Screen position of the show fps overlay on the y axis. |
| `show_speed` | `"0"` | Display the engine's player-speed overlay. |
| `show_speed_x` | `"-1"` | Screen position of the show speed overlay on the x axis. |
| `show_speed_y` | `"-9"` | Screen position of the show speed overlay on the y axis. |
| `showdrop` | `"0"` | Print dropped/out-of-order packet diagnostics. |
| `showpackets` | `"0"` | Print packet traffic diagnostics. |
| `showpause` | `"1"` | Display the engine pause indicator. |
| `showturtle` | `"0"` | Enables a low-framerate indicator. |
| `skill` | `""` | **Q1 target:** gamecode difficulty level. Quake 3 bot difficulty is supplied to addbot or the single-player UI. |
| `skin` (alias: `_cl_playerskin`) | `""` | **Different:** Player skin selection; Quake 1 skin name. Quake 3 normally selects a skin through model/headmodel. |
| `skyroom` | `""` | Specifies the center position of the skyroom's view. Skyrooms are drawn instead of skyboxes (typically with their own skybox around them). Entities in skyrooms will be drawn only when r_ignoreentpvs is 0. Can also be set with the _skyroom worldspawn key. This is overriden by csqc's VF_SKYROOM_CAMERA. [cheat/ruleset restricted.] |
| `slist_cacheinfo` | `"0"` | Cache detailed server-browser information. |
| `slist_writeservers` | `"1"` | Write remembered server-browser addresses to disk. |
| `snd_ignorecueloops` | Q1: `"0"`; Q3: `"1"` | Ignores cue commands in wav files, for q3 compat. |
| `snd_ignoregamespeed` | `"0"` | When set, allows sounds to desynchronise with game time or demo speeds. |
| `snd_loadasstereo` | `"0"` | Force mono sounds to load as if stereo ones, to waste memory. Not normally useful. |
| `snd_playbackrate` | `"1"` | Debugging cvar that changes the playback rate of all new sounds. [cheat/ruleset restricted.] |
| `spawn` | `""` | **Q1 target:** spawn-setting compatibility value exposed to gamecode. |
| `spectator` | `""` | **Different:** **Different:** Quake 1/QuakeWorld spectator request/userinfo. Quake 3 uses team spectator instead. |
| `spectator_password` | not exposed | **Q1 target:** password for joining as a spectator. |
| `sv_accelerate` | `"10"` | **Q1 target:** ground acceleration in engine player physics. Quake 3 uses qagame movement code. |
| `sv_aim` | `"2"` | The value should be cos(angle), where angle is the greatest allowed angle from which to deviate from the direction the shot would have been along. Values >1 thus disable auto-aim. This can be overridden with setinfo aim 0.73. |
| `sv_airaccelerate` | `"0.7"` | **Q1 target:** airborne acceleration in engine player physics. |
| `sv_antilag` | `""` | Attempt to backdate impacts to compensate for lag via the MOVE_ANTILAG feature.<br>0=completely off.<br>1=mod-controlled (default).<br>2=forced, which might break certain uses of traceline.<br>3=Also attempt to recalculate trace start positions to avoid lagged knockbacks. |
| `sv_antilag_frac` | `""` | **Q1 target:** fraction of latency compensation applied to server anti-lag traces. |
| `sv_autooffload` | `"0"` | Automatically start the server in a separate process, so that sporadic or persistent gamecode slowdowns do not affect visual framerates (equivelent to the mapcluster command). Note: Offloaded servers have separate cvar+command states which may complicate usage. |
| `sv_autosave` | `"5"` | **Q1 target:** Interval for autosaves, in minutes. Set to 0 to disable autosave. Does not provide Quake 3/OpenArena saved-game support. |
| `sv_banproxies` | `"0"` | If enabled, anyone connecting via known proxy software will be refused entry. This should aid with blocking aimbots, but is only reliable for certain public proxies. |
| `sv_bigcoords` | Q1: `""`; Q3: `"1"` | Uses floats for coordinates instead of 16bit values.<br>Also boosts angle precision, so can be useful even on small maps.<br>Affects clients thusly:<br>QW: enforces a mandatory protocol extension<br>DP: enables DPP7 protocol support<br>NQ: uses RMQ protocol (protocol 999). |
| `sv_brokenmovetypes` | `"0"` | Emulate vanilla quakeworld by forcing MOVETYPE_WALK on all players. Shouldn't be used for any games other than QuakeWorld. |
| `sv_calcphs` | `"2"` | Enables culling of sound effects. 0=always skip phs. Sounds are globally broadcast. 1=always generate phs. Sounds are always culled. On large maps the phs will be dumped to disk. 2=On large single-player maps, generation of phs is skipped. Otherwise like option 1. |
| `sv_chatfilter` | `"0"` | Server-side chat filtering preference. |
| `sv_cheats` | `"0"` | Authorize engine/gamecode cheats on the hosted server; connected clients cannot enable a remote server's cheats. |
| `sv_cmdlikercon` | `"0"` | Run forwarded client console commands with remote-console-like permissions where supported. |
| `sv_compatiblehulls` | `"1"` | **Q1 target:** collision-hull compatibility preference. |
| `sv_crypt_rcon` | `""` | Controls whether the rcon password must be hashed or not. Hashed passwords also partially prevent replay attacks, but does NOT prevent malicious actors from reading the commands/results.<br>0: completely insecure. ONLY allows plain-text passwords. Do not use.<br>1: Mandatory hashing (recommended).<br>Empty: Allow either, whether the password is secure or not is purely the client's responsibility/fault. Only use this for comptibility with old clients. |
| `sv_csqc_progname` (alias: `csqc_progname`) | `"csprogs.dat"` | **Q1 target:** client-side QuakeC filename advertised by the server. |
| `sv_csqcdebug` | `"0"` | Inject packet size information for data directed to csqc. |
| `sv_cullentities_trace` | `""` | Attempt to cull non-player entities using tracelines as an extreeme anti-wallhack. |
| `sv_cullplayers_trace` | `""` | Attempt to cull player entities using tracelines as an anti-wallhack. |
| `sv_demo_write_csqc` | `""` | **Q1 target:** Writes a copy of the csprogs into recorded demos. This ensures that the demo can be played back despite future gamecode changes. |
| `sv_demoAutoCompress` | `""` | **Q1 target:** Specifies whether to compress demos as they're recorded.<br>0 = no compression.<br>1 = gzip compression. |
| `sv_demoAutoPrefix` | `"auto_"` | **Q1 target:** Quake 1/MVD: automatically generated server-demo filename prefix. |
| `sv_demoAutoRecord` (alias: `cl_autodemo`) | `"0"` | **Q1 target:** If set, automatically record demos.<br>-1: record on client connection only.<br>1+: record once there's this many active players on the server (or if connected to a remote server). |
| `sv_demoCacheSize` | `"0x80000"` | **Q1 target:** Quake 1/MVD: memory cache size for server-demo recording. |
| `sv_demoClearOld` | `"0"` | **Q1 target:** Automatically delete demos to keep the demos count reasonable. |
| `sv_demoDir` | `"demos"` | **Q1 target:** Quake 1/MVD: directory for server-demo recordings. |
| `sv_demoDirAlt` | `""` | **Q1 target:** Provides a fallback directory name for demo downloads, for when sv_demoDir doesn't contain the requested demo. |
| `sv_demoExtensions` | Q1: `""`; Q3: `"1"` | **Q1 target:** Enables protocol extensions within MVDs. This will cause older/non-fte clients to error upon playback.<br>0: off.<br>1: all extensions.<br>2: extensions also supported by a certain other engine. |
| `sv_demoExtraNames` | `""` | **Q1 target:** Quake 1/MVD: include additional identifying text in demo filenames. |
| `sv_demofps` | `"30"` | **Q1 target:** Quake 1/MVD: server-demo recording frame rate. |
| `sv_demoMaxDirAge` | `"0"` | **Q1 target:** Maximum allowed age for demos, any older demos will be deleted when sv_demoClearOld is set (this doesn't prevent recording new demos). |
| `sv_demoMaxDirCount` | `"500"` | **Q1 target:** Maximum allowed serverside mvds to record. Set to 0 to remove the limit. New demos cannot be recorded once this many demos have already been recorded. |
| `sv_demoMaxDirSize` | `"100mb"` | **Q1 target:** Maximum allowed serverside storage space for mvds. set to blank to remove the limit. New demos cannot be recorded once this size is reached. |
| `sv_demoMaxSize` | `""` | **Q1 target:** Demos will be truncated to be no larger than this size. |
| `sv_demoPings` | `"10"` | **Q1 target:** Interval between ping updates in mvds |
| `sv_demoPrefix` | `""` | **Q1 target:** Quake 1/MVD: user-defined filename prefix/suffix. |
| `sv_demoSuffix` | `""` | **Q1 target:** Quake 1/MVD: user-defined filename prefix/suffix. |
| `sv_demotxt` | `"1"` | **Q1 target:** Quake 1/MVD: write associated demo-information text files. |
| `sv_demoUseCache` | `""` | **Q1 target:** If set, demo data will be flushed only periodically |
| `sv_dlURL` (alias: `sv_curl_defaulturl`) | `""` | Provides clients with an external url from which they can obtain pk3s/packages from an external http server instead of having to download over udp. |
| `sv_dorestart` | on demand | **Q3:** gamecode restart-request compatibility value; engine support determines how the request is acted on. |
| `sv_floodprotect` | `"1"` | Enable the engine's chat flood protection. |
| `sv_floodprotect_interval` | `"4"` | Time interval used to count chat-flood messages. |
| `sv_floodprotect_messages` | `"4"` | Number of messages allowed within the flood-protection interval. |
| `sv_floodprotect_sendmessage` (alias: `floodprotmsg`) | `""` | Send a notification when flood protection blocks a message. |
| `sv_floodprotect_silencetime` | `"10"` | Duration of a chat-flood penalty. |
| `sv_floodprotect_suicide` | `"1"` | **Q1 target:** apply flood protection to repeated suicide requests. |
| `sv_fps` | `"20"` | **Q3:** Server update/frame-rate preference exposed to Quake 3 gamecode; effective scheduling is owned by the host engine. |
| `sv_friction` | `"4"` | **Q1 target:** ground friction in engine player physics. |
| `sv_fullredirect` | `""` | This is the ip:port to redirect players to when the server is full |
| `sv_gameplayfix_blowupfallenzombies` | `"0"` | **Q1 target:** Allow findradius to find non-solid entities. This may break certain mods. It is better for mods to use FL_FINDABLE_NONSOLID instead. |
| `sv_gameplayfix_droptofloorstartsolid` | `"0"` | **Q1 target:** When droptofloor fails, this causes a second attemp, but with traceline instead. |
| `sv_gameplayfix_findradiusdistancetobox` | `"0"` | **Q1 target:** When 1, findradius checks to the nearest part of the entity instead of only its origin, making it find slightly more entities. |
| `sv_gameplayfix_grenadebouncedownslopes` | `"0"` | MOVETYPE_BOUNCE speeds are calculated relative to the impacted surface, instead of the vertical, reducing the chance of grenades just sitting there on slopes. |
| `sv_gameplayfix_honest_tracelines` | `"1"` | **Q1 target:** compatibility/accuracy switch for server QuakeC line traces. |
| `sv_gameplayfix_multiplethinks` (alias: `sv_gameplayfix_multiplethinksperframe`) | `"1"` | Enables multiple thinks per entity per frame so small nextthink times are accurate. QuakeWorld mods expect a value of 1, while NQ expects 0. |
| `sv_gameplayfix_noairborncorpse` | `"0"` | **Q1 target:** compatibility fix for corpse behavior while airborne. |
| `sv_gameplayfix_nolinknonsolid` | `"1"` | **Q1 target:** When 0, setorigin et al will not link the entity into the collision nodes (which is faster, especially if you have a lot of non-solid entities. When 1, allows entities to freely switch between .solid values (except for SOLID_BSP) without relinking. A lot of DP mods assume a value of 1 and will bug out otherwise, while 0 will restore a bugs present in various mods. |
| `sv_gameplayfix_setmodelrealbox` | `"0"` | **Q1 target:** Vanilla setmodel will setsize the entity to a hardcoded size for non-bsp models. This cvar will always use the real size of the model instead, but will require that the server actually loads the model. |
| `sv_gameplayfix_setmodelsize_qw` | `"0"` | **Q1 target:** The setmodel builtin will act as a setsize for QuakeWorld mods also. |
| `sv_gameplayfix_spawnbeforethinks` | `"0"` | Fixes an issue where player thinks (including Pre+Post) can be called before PutClientInServer. Unfortunately at least one mod depends upon PreThink being called first in order to correctly determine spawn positions. |
| `sv_gameplayfix_stepdown` | `"0"` | Attempt to step down steps, instead of only up them. Affects non-predicted movetype_walk. |
| `sv_gameplayfix_trappedwithin` | `"0"` | Blocks further entity movement when an entity is already inside another entity. This ensures that bsp precision issues cannot allow the entity to completely pass through eg the world. |
| `sv_gamespeed` (alias: `slowmo`) | `"1"` | **Q1 target:** server simulation speed multiplier. |
| `sv_getrealip` | `"0"` | Attempt to obtain a more reliable IP for clients, rather than just their proxy.<br>0: Don't attempt.<br>1: Unreliable checks.<br>2: Validate if possible.<br>3: Mandatory validation. |
| `sv_gravity` | `"800"` | **Q1 target:** engine gravity. Quake 3/OpenArena uses g_gravity and gamecode prediction instead. |
| `sv_guidkey` | `""` | If set, clients will calculate their GUID values against this string instead of the server's IP address. This allows consistency between multiple servers (for stats tracking), but do NOT treat the client's GUID as something that is secure. |
| `sv_heartbeat_checks` | `"1"` | Report when sv_public 1 fails due to PROBABLE router/NAT issues. |
| `sv_heartbeat_interval` | `"110"` | Interval between heartbeats. Low values are abusive, high values may cause NAT/ghost issues. |
| `sv_highchars` | `"1"` | Permit high-bit/decorative characters in server messages/names. |
| `sv_hostname` | on demand | **Q3:** OpenArena create-server UI hostname preference; the engine's advertised name is hostname. |
| `sv_killserver` | `"1"` | **Q3:** Quake 3 gamecode request to stop the local server. |
| `sv_lanforcerate` | on demand | **Q3:** Legacy Quake 3 create-server UI LAN-rate preference; this does not replace the engine's rate controls. |
| `sv_limittics` | `"3"` | The maximum number of ticks that may be run within a frame, to allow the server to catch up if it stalled or if sv_maxtic is too low. |
| `sv_listen_dp` | `"0"` | Allows the server to respond with the DP-specific handshake protocol.<br>Warning: this can potentially get confused with quake2, and results in race conditions with both vanilla netquake and quakeworld protocols.<br>On the plus side, DP clients can usually be identified correctly, enabling a model+sound limit boost. |
| `sv_listen_nq` | Q1: `"2"`; Q3: `"0"` | Allow new (net)quake clients to connect to the server.<br>0 = don't let them in.<br>1 = allow them in (WARNING: this allows 'qsmurf' DOS attacks).<br>2 = accept (net)quake clients by emulating a challenge (as secure as QW/Q2 but does not fully conform to the NQ protocol).<br>You may also need to set net_enable_dtls if you wish for the rerelease's client to connect. |
| `sv_listen_qw` (alias: `sv_listen`) | `"1"` | Specifies whether normal clients are allowed to connect. |
| `sv_loadentfiles` | `"1"` | Load external map entity-definition files instead of the embedded entity lump. |
| `sv_loadentfiles_dir` | `""` | Directory searched for external map entity-definition files. |
| `sv_mapcheck` | `"1"` | **Q1 target:** check client map identity/checksums against the server. |
| `sv_mapChecksum` | dynamic / platform dependent | **Different:** Map checksum exposed to Quake 3 gamecode; informational. |
| `sv_mapname` | dynamic / platform dependent | **Q3:** Map name exposed to Quake 3 gamecode; informational. |
| `sv_maxdrate` (alias: `sv_maxdownloadrate`) | `"500000"` | This cvar controls the maximum number of bytes sent to each player per second while that player is downloading.<br>If this cvar is set to 0, there will be NO CAP for download rates (if the user's drate is empty/0 too, then expect really fast+abusive downloads that could potentially be considered denial of service attacks) |
| `sv_maxrate` | `"50000"` | This controls the maximum number of bytes any indivual player may receive (when not downloading). The individual user's rate will also be controlled by the user's rate cvar. |
| `sv_maxspeed` | `"320"` | **Q1 target:** maximum player movement speed in engine physics; Quake 3 uses g_speed. |
| `sv_maxtic` | `"0.1"` | The maximum interval between running physics frames. If the value is too low, multiple physics interations might be run at a time (based upon sv_limittics). Set to 0 for fixed-interval ticks, which may be required if ODE is used. |
| `sv_maxvelocity` | `"10000"` | **Q1 target:** maximum entity velocity permitted by engine physics. |
| `sv_minping` | `""` | Simulate fake lag for any players with a ping under the value specified here. Value is in milliseconds. |
| `sv_mintic` | `"0"` | The minimum interval between running physics frames. The [Quake rerelease profile](QuakeRemaster.md#launch) uses `0.016666667` for both `sv_mintic` and `sv_maxtic` (60 Hz). |
| `sv_motd1` | `""` | **Q1 target:** line 1 of the server message of the day. |
| `sv_motd2` | `""` | **Q1 target:** line 2 of the server message of the day. |
| `sv_motd3` | `""` | **Q1 target:** line 3 of the server message of the day. |
| `sv_motd4` | `""` | **Q1 target:** line 4 of the server message of the day. |
| `sv_nailhack` | `"1"` | If set to 1, disables the nail entity networking optimisation. This hack was popularised by qizmo which recommends it for better compression. Also allows clients to interplate nail positions and add trails. |
| `sv_nopvs` | `"0"` | Set to 1 to ignore pvs on the server. This can make wallhacks more dangerous, so should only be used for debugging. |
| `sv_nqplayerphysics` (alias: `sv_nomsec`) | `"auto"` | Disable player prediction and run NQ-style player physics instead. This can be used for compatibility with mods that expect exact behaviour. A value of 2 will not block prediction, and may be juddery/jerky/swimmy. |
| `sv_phs` | `"1"` | If 1, do not use the phs. It is generally better to use sv_calcphs instead, and leave this as 1. |
| `sv_playermodelchecks` | `"0"` | **Q1 target:** check player-model identity for compatibility/fairness. |
| `sv_playerslots` (alias: `maxplayers`) | `""` | Specify maximum number of player/spectator/bot slots, new value takes effect on the next map (this may result in players getting kicked). This should generally be set to maxclients+maxspectators. Leave blank for a default value.<br>Maximum value of 255. Values above 16 will result in issues with vanilla NQ clients. Effective values other than 32 will result in issues with vanilla QW clients. |
| `sv_port` | Q1: `"27500 26000"`; Q3: `"27960"` | Port number to list on for inbound udp-based connections (including dtls variants). Can be a list for multiple ports. If ips are included then binds to that specific interface. Whether any specific protocol is accepted depends upon other settings. |
| `sv_port_ipv6` | `""` | Port to use for incoming ipv6 udp connections. Due to hybrid sockets this might not be needed. You can specify an ipv4 address:port for a second ipv4 port if you want. |
| `sv_port_natpmp` | `""` | If set (typically to 5351), automatically configures your router's port forwarding. You can instead specify the full ip address of your router (192.168.1.1:5351 for example). Your router must have NAT-PMP supported and enabled. |
| `sv_port_rtc` | `""` | This specifies the broker url to use to obtain clients from. If the hostname is ommitted, it'll come from the manifest. If omitted, the broker service will randomize the resource part, so be sure to tell your friends the path reported by eg status rather than just this cvar value. Or just set it to 'rtc:///example' and tell clients to connect to the same sservevalue. |
| `sv_port_tcp` | `""` | Port number to list on for inbound tcp-based connections (including tls variants). Can be a list for multiple ports. If ips are included then binds to that specific interface. Whether any specific protocol is accepted depends upon other settings. |
| `sv_port_tcp6` | `""` | Equivelent to sv_port_tcp, but port numbers are assumed to be ipv6 only. relevant only when not using hybrid sockets. |
| `sv_progs` (alias: `progs`) | `""` | **Q1 target:** Specifies the filename of the gamecode to use serverside. Empty means autodetect - typically loading either progs or qwprogs depending on gamedir priority, then based upon deathmatch settings. |
| `sv_protocol` | `""` | Specifies which protocol extensions to force. Clients which do not support the named protocols will be kicked. Recognised values: fte1 fte2 csqc. |
| `sv_protocol_nq` | `""` | Specifies the default protocol to use for new NQ clients. This is only relevent for clients that do not report their supported protocols. Supported values are<br>0 = autodetect<br>15 = vanilla<br>666 = fitzquake<br>999 = rmq protocol<br>The sv_bigcoords cvar forces upgrades as required. |
| `sv_public` | `"0"` | -1: Fully blocks all inbound connections.<br>0: Disable subscribing to master servers (for private lan-only games).<br>1: Subscribe to public master servers. Your IP address will be listed publicly. Make sure your Router/NAT+Firewall are set to allow inbound connections.<br>2: Subscribe to a broker master, allowing firewall hole punching. |
| `sv_pupglow` | `""` | Instructs clients to enable hexen2-style powerup pulsing. |
| `sv_pure` | `""` | The most evil cvar in the world, many clients will ignore this.<br>0=standard quake rules.<br>1=clients should prefer files within packages present on the server.<br>2=clients should use *only* files within packages present on the server.<br>Due to quake 1.01/1.06 differences, a setting of 2 only works in total conversions. |
| `sv_pushplayers` | `"0"` | **Q1 target:** allow the engine to push player entities where supported. |
| `sv_rconlim` | `"4"` | Blocks repeated (invalid) rcon attempts. |
| `sv_readonlyworld` | `"1"` | **Q1 target:** forbid gamecode writes to the world entity where supported. |
| `sv_realip_kick` | `"0"` | Kicks clients if their realip could not be validated to the level specified by sv_getrealip. |
| `sv_realip_timeout` | `"10"` | Timeout for real-IP/proxy-address verification. |
| `sv_realiphostname_ipv4` | `""` | This is the server's public ip:port. This is needed for realip to work when the autodetected/local ip is not globally routable |
| `sv_realiphostname_ipv6` | `""` | This is the server's public ip:port. This is needed for realip to work when the autodetected/local ip is not globally routable |
| `sv_reconnectlimit` | `"0"` | Blocks dupe connection within the specified length of time . |
| `sv_reliable_sound` | `"0"` | Causes all sounds to be sent reliably, so they will not be missed due to packetloss. However, this will cause them to be delayed somewhat, and slightly bursty. This can be overriden using the 'rsnd' userinfo setting (either forced on or forced off). Note: this does not affect sounds attached to particle effects. |
| `sv_reportheartbeats` | `"2"` | Print a notice each time a heartbeat is sent to a master server. When set to 2, the message will be displayed once. |
| `sv_resetparms` | `"0"` | **Q1 target:** reset saved spawn parameters when changing map where applicable. |
| `sv_running` | dynamic / platform dependent | **Q3:** Whether a local server is running; engine/plugin-maintained status. |
| `sv_savefmt` | `""` | Specifies the format used for the saved game.<br>0=legacy.<br>1=fte<br>2=binary. Native FTE and extended legacy paths retain Jolt entity state; custom mod save formats remain mod-owned. See [entity saves](JoltPhysics.md#saving-entity-scenes). |
| `sv_serverip` | `""` | Set this cvar to the server's public ip address if the server is behind a firewall and cannot detect its own public address. Providing a port is required if the firewall/nat remaps it, but is otherwise optional. |
| `sv_showconnectionlessmessages` | `"0"` | Display a line describing each connectionless message that arrives on the server. Primarily a debugging feature, but also potentially useful to admins. |
| `sv_showpredloss` | `"0"` | Print messages whenever input frames are ignored or forced serverside, to prevent speedcheats or hover cheats. Any such prints will be accompanied by prediction misses in the named client. |
| `sv_sound_land` | `"demon/dland2.wav"` | **Q1 target:** sound names used by engine movement for landing/water splashes respectively. |
| `sv_sound_watersplash` | `"misc/h2ohit1.wav"` | **Q1 target:** sound names used by engine movement for landing/water splashes respectively. |
| `sv_specprint` | `"3"` | Bitfield that controls which player events spectators see when tracking that player.<br>&1: spectators will see centerprints.<br>&2: spectators will see sprints (pickup messages etc).<br>&4: spectators will receive console commands, this is potentially risky.<br>Individual spectators can use 'setinfo sp foo' to limit this setting. |
| `sv_spectalk` | `"1"` | **Q1 target:** permit spectator-to-player chat. |
| `sv_spectatormaxspeed` | `"500"` | **Q1 target:** maximum speed for engine spectator movement. |
| `sv_sql_defaultdb` | not exposed | SQL backend default database name for server database features; requires the corresponding optional driver. |
| `sv_sql_driver` | not exposed | SQL backend driver selection for server database features; requires the corresponding optional driver. |
| `sv_sql_host` | not exposed | SQL backend server address for server database features; requires the corresponding optional driver. |
| `sv_sql_password` | not exposed | SQL backend authentication password (keep private) for server database features; requires the corresponding optional driver. |
| `sv_sql_username` | not exposed | SQL backend authentication username for server database features; requires the corresponding optional driver. |
| `sv_stopspeed` | `"100"` | **Q1 target:** speed threshold used in applying ground friction. |
| `sv_use_dns` | `""` | Performs a reverse-dns lookup in order to report more info about where clients are connecting from. |
| `sv_userinfo_bytelimit` | `"8192"` | This is the maximum number of bytes that may be stored into each user's userinfo. Note that this includes key names too. |
| `sv_userinfo_keylimit` | `"128"` | This is the maximum number of userinfo keys each user may create. |
| `sv_voip` | `"1"` | Enable reception of voice packets. |
| `sv_voip_echo` | `"0"` | Echo voice packets back to their sender, a debug/test setting. |
| `sv_voip_record` | `"0"` | Record voicechat into mvds. Requires player support. 0=noone, 1=everyone, 2=spectators only |
| `sv_wallfriction` | `"1"` | Additional friction when running into walls |
| `sv_wateraccelerate` | `"10"` | **Q1 target:** underwater acceleration in engine player physics. |
| `sv_waterfriction` | `"4"` | **Q1 target:** underwater friction in engine player physics. |
| `sys_clocktype` | `""` | Controls which system clock to base timings from.<br>Available Clocks:<br>ticksns<br>perf<br>ticks |
| `sys_colorconsole` | `"1"` | Parse colour escapes, with ansi colours on stdout. |
| `sys_extrasleep` | `"0"` | Extra sleep/yield time in the engine frame loop. |
| `sys_linebuffer` | `"1"` | Line-buffer standard output. |
| `sys_nostdout` | `"0"` | Suppress output to the process's standard-output stream. |
| `sys_osk` | `"0"` | Enables support for text input. This will be ignored when the console has focus, but gamecode may end up with composition boxes appearing. |
| `sys_platform` | dynamic / platform dependent | Engine/platform identifier exposed to gamecode; informational. |
| `sys_timestamps` | `"0"` | Show timesamps on stdout prints. |
| `team_headmodel` | `"sarge"` | **Q3:** model/head-model preference for team games. |
| `team_model` | `"sarge"` | **Q3:** model/head-model preference for team games. |
| `teamcolor` | `"off"` | **Q1 target:** forced enemy/team player colors respectively. |
| `teamoverlay` | `"0"` | **Q3:** Userinfo request for teammate-overlay data; normally set by client gamecode. |
| `teamplay` | `""` | **Q1 target:** teamplay rule value interpreted by QuakeC; Quake 3 uses g_gametype/g_friendlyFire. |
| `temp1` | `"0"` | **Q1 target:** general-purpose gamecode-owned value; the mod defines its meaning. There is no universal engine rule for its contents. |
| `timelimit` | `""` | Match time limit in minutes; actual match-ending behavior belongs to the game's server code. |
| `timeout` | `"65"` | Connections will time out if no packets are received for this duration of time. |
| `timescale` | `"1"` | **Q3:** Gamecode-accessible simulation/playback time-scale setting; engine support and cheat restrictions apply. |
| `tls_ignorecertificateerrors` | not exposed | This should NEVER be set to 1! |
| `tls_provider` | `""` | Controls which TLS provider to use. |
| `topcolor` | `"13"` | **Different:** Quake 1 player-shirt/trouser palette colors; Quake 3 uses model skins and color1/color2 instead. |
| `tp_autostatus` | `""` | **Q1 target:** automatic teamplay status reporting. |
| `tp_disputablemacros` | `"1"` | **Q1 target:** allow teamplay macros subject to ruleset restrictions. [cheat/ruleset restricted.] |
| `tp_forceTriggers` | `"0"` | **Q1 target:** force execution of applicable teamplay triggers. |
| `tp_loadlocs` | `"1"` | **Q1 target:** automatically load map location files for teamplay macros. |
| `tp_name_armor` | `"armor"` | **Q1 target:** text label for armor in engine teamplay macro output. |
| `tp_name_armortype_ga` | `"g"` | **Q1 target:** text label for armortype ga in engine teamplay macro output. |
| `tp_name_armortype_ra` | `"r"` | **Q1 target:** text label for armortype ra in engine teamplay macro output. |
| `tp_name_armortype_ya` | `"y"` | **Q1 target:** text label for armortype ya in engine teamplay macro output. |
| `tp_name_at` | `"at"` | **Q1 target:** text label for armor type in engine teamplay macro output. |
| `tp_name_axe` | `"axe"` | **Q1 target:** text label for axe in engine teamplay macro output. |
| `tp_name_backpack` | `"pack"` | **Q1 target:** text label for backpack in engine teamplay macro output. |
| `tp_name_cells` | `"cells"` | **Q1 target:** text label for cells in engine teamplay macro output. |
| `tp_name_disp` | `"dispenser"` | **Q1 target:** text label for dispenser in engine teamplay macro output. |
| `tp_name_enemy` | `"enemy"` | **Q1 target:** text label for enemy in engine teamplay macro output. |
| `tp_name_eyes` | `"eyes"` | **Q1 target:** text label for invisible-player eyes in engine teamplay macro output. |
| `tp_name_flag` | `"flag"` | **Q1 target:** text label for flag in engine teamplay macro output. |
| `tp_name_ga` | `"ga"` | **Q1 target:** text label for green armor in engine teamplay macro output. |
| `tp_name_gl` | `"gl"` | **Q1 target:** text label for grenade launcher in engine teamplay macro output. |
| `tp_name_health` | `"health"` | **Q1 target:** text label for health in engine teamplay macro output. |
| `tp_name_lg` | `"lg"` | **Q1 target:** text label for lightning gun in engine teamplay macro output. |
| `tp_name_mh` | `"mega"` | **Q1 target:** text label for megahealth in engine teamplay macro output. |
| `tp_name_nails` | `"nails"` | **Q1 target:** text label for nails in engine teamplay macro output. |
| `tp_name_ng` | `"ng"` | **Q1 target:** text label for nailgun in engine teamplay macro output. |
| `tp_name_none` | `""` | **Q1 target:** text label for no item in engine teamplay macro output. |
| `tp_name_nothing` | `"nothing"` | **Q1 target:** text label for nothing in engine teamplay macro output. |
| `tp_name_pent` | `"pent"` | **Q1 target:** text label for Pentagram of Protection in engine teamplay macro output. |
| `tp_name_pented` | `"pented"` | **Q1 target:** text label for invulnerability status in engine teamplay macro output. |
| `tp_name_quad` | `"quad"` | **Q1 target:** text label for Quad Damage in engine teamplay macro output. |
| `tp_name_quaded` | `"quaded"` | **Q1 target:** text label for Quad Damage status in engine teamplay macro output. |
| `tp_name_ra` | `"ra"` | **Q1 target:** text label for red armor in engine teamplay macro output. |
| `tp_name_ring` | `"ring"` | **Q1 target:** text label for Ring of Shadows in engine teamplay macro output. |
| `tp_name_rl` | `"rl"` | **Q1 target:** text label for rocket launcher in engine teamplay macro output. |
| `tp_name_rockets` | `"rockets"` | **Q1 target:** text label for rockets in engine teamplay macro output. |
| `tp_name_rune_1` | `"resistance rune"` | **Q1 target:** text label for rune 1 in engine teamplay macro output. |
| `tp_name_rune_2` | `"strength rune"` | **Q1 target:** text label for rune 2 in engine teamplay macro output. |
| `tp_name_rune_3` | `"haste rune"` | **Q1 target:** text label for rune 3 in engine teamplay macro output. |
| `tp_name_rune_4` | `"regeneration rune"` | **Q1 target:** text label for rune 4 in engine teamplay macro output. |
| `tp_name_sentry` | `"sentry gun"` | **Q1 target:** text label for sentry gun in engine teamplay macro output. |
| `tp_name_separator` | `"/"` | **Q1 target:** text label for list separator in engine teamplay macro output. |
| `tp_name_sg` | `"sg"` | **Q1 target:** text label for shotgun in engine teamplay macro output. |
| `tp_name_shells` | `"shells"` | **Q1 target:** text label for shells in engine teamplay macro output. |
| `tp_name_sng` | `"sng"` | **Q1 target:** text label for super nailgun in engine teamplay macro output. |
| `tp_name_someplace` | `"someplace"` | **Q1 target:** text label for unknown location in engine teamplay macro output. |
| `tp_name_ssg` | `"ssg"` | **Q1 target:** text label for super shotgun in engine teamplay macro output. |
| `tp_name_status_blue` | `"$B"` | **Q1 target:** text label for status blue in engine teamplay macro output. |
| `tp_name_status_green` | `"$G"` | **Q1 target:** text label for status green in engine teamplay macro output. |
| `tp_name_status_red` | `"$R"` | **Q1 target:** text label for status red in engine teamplay macro output. |
| `tp_name_status_yellow` | `"$Y"` | **Q1 target:** text label for status yellow in engine teamplay macro output. |
| `tp_name_suit` | `"suit"` | **Q1 target:** text label for biosuit in engine teamplay macro output. |
| `tp_name_teammate` | `""` | **Q1 target:** text label for teammate in engine teamplay macro output. |
| `tp_name_weapon` | `"weapon"` | **Q1 target:** text label for weapon in engine teamplay macro output. |
| `tp_name_ya` | `"ya"` | **Q1 target:** text label for yellow armor in engine teamplay macro output. |
| `tp_need_cells` | `"20"` | **Q1 target:** cells need threshold/preference used by teamplay need/status macros. |
| `tp_need_ga` | `"50"` | **Q1 target:** green armor need threshold/preference used by teamplay need/status macros. |
| `tp_need_health` | `"50"` | **Q1 target:** health need threshold/preference used by teamplay need/status macros. |
| `tp_need_nails` | `"40"` | **Q1 target:** nails need threshold/preference used by teamplay need/status macros. |
| `tp_need_ra` | `"50"` | **Q1 target:** red armor need threshold/preference used by teamplay need/status macros. |
| `tp_need_rl` | `"1"` | **Q1 target:** rocket launcher need threshold/preference used by teamplay need/status macros. |
| `tp_need_rockets` | `"5"` | **Q1 target:** rockets need threshold/preference used by teamplay need/status macros. |
| `tp_need_shells` | `"10"` | **Q1 target:** shells need threshold/preference used by teamplay need/status macros. |
| `tp_need_weapon` | `"35687"` | **Q1 target:** weapon need threshold/preference used by teamplay need/status macros. |
| `tp_need_ya` | `"50"` | **Q1 target:** yellow armor need threshold/preference used by teamplay need/status macros. |
| `tp_soundtrigger` | `"~"` | **Q1 target:** allow chat-triggered sound notifications. |
| `tp_weapon_order` | `"78654321"` | **Q1 target:** weapon priority order used by teamplay reporting/macros. |
| `ui_1fctf_capturelimit` | `"8"` | **Q3:** Saved OpenArena create-server menu preference for 1fctf: capture/round-score limit. Applied when the UI starts a server. |
| `ui_1fctf_friendly` | `"0"` | **Q3:** Saved OpenArena create-server menu preference for 1fctf: friendly-fire toggle. Applied when the UI starts a server. |
| `ui_1fctf_timelimit` | `"30"` | **Q3:** Saved OpenArena create-server menu preference for 1fctf: time limit in minutes. Applied when the UI starts a server. |
| `ui_bigFont` | `"0.4"` | **Q3:** Size thresholds used by the game's UI font rendering. |
| `ui_browserGameType` | `"0"` | **Q3:** OpenArena server-browser game-type filter. |
| `ui_browserMaster` | `"0"` | **Q3:** OpenArena server-browser selected master/source. |
| `ui_browserOnlyHumans` | `"0"` | **Q3:** OpenArena server-browser human-player filter. |
| `ui_browserShowEmpty` | `"1"` | **Q3:** OpenArena server-browser show-empty-servers toggle. |
| `ui_browserShowFull` | `"1"` | **Q3:** OpenArena server-browser show-full-servers toggle. |
| `ui_browserSortKey` | `"4"` | **Q3:** OpenArena server-browser sort-column selection. |
| `ui_cdkeychecked` | on demand | **Q3:** Legacy Quake 3 UI key-check state; OpenArena itself does not require a commercial Quake 3 key. |
| `ui_ctf_capturelimit` | `"8"` | **Q3:** Saved OpenArena create-server menu preference for ctf: capture/round-score limit. Applied when the UI starts a server. |
| `ui_ctf_elimination_capturelimit` | `"8"` | **Q3:** Saved OpenArena create-server menu preference for ctf elimination: capture/round-score limit. Applied when the UI starts a server. |
| `ui_ctf_elimination_timelimit` | `"30"` | **Q3:** Saved OpenArena create-server menu preference for ctf elimination: time limit in minutes. Applied when the UI starts a server. |
| `ui_ctf_friendly` | `"0"` | **Q3:** Saved OpenArena create-server menu preference for ctf: friendly-fire toggle. Applied when the UI starts a server. |
| `ui_ctf_timelimit` | `"30"` | **Q3:** Saved OpenArena create-server menu preference for ctf: time limit in minutes. Applied when the UI starts a server. |
| `ui_dd_capturelimit` | `"8"` | **Q3:** Saved OpenArena create-server menu preference for dd: capture/round-score limit. Applied when the UI starts a server. |
| `ui_dd_friendly` | `"0"` | **Q3:** Saved OpenArena create-server menu preference for dd: friendly-fire toggle. Applied when the UI starts a server. |
| `ui_dd_timelimit` | `"30"` | **Q3:** Saved OpenArena create-server menu preference for dd: time limit in minutes. Applied when the UI starts a server. |
| `ui_dom_capturelimit` | `"500"` | **Q3:** Saved OpenArena create-server menu preference for dom: capture/round-score limit. Applied when the UI starts a server. |
| `ui_dom_friendly` | `"0"` | **Q3:** Saved OpenArena create-server menu preference for dom: friendly-fire toggle. Applied when the UI starts a server. |
| `ui_dom_timelimit` | `"30"` | **Q3:** Saved OpenArena create-server menu preference for dom: time limit in minutes. Applied when the UI starts a server. |
| `ui_elimination_capturelimit` | `"8"` | **Q3:** Saved OpenArena create-server menu preference for elimination: capture/round-score limit. Applied when the UI starts a server. |
| `ui_elimination_timelimit` | `"20"` | **Q3:** Saved OpenArena create-server menu preference for elimination: time limit in minutes. Applied when the UI starts a server. |
| `ui_ffa_fraglimit` | `"20"` | **Q3:** Saved OpenArena create-server menu preference for ffa: frag limit. Applied when the UI starts a server. |
| `ui_ffa_timelimit` | `"0"` | **Q3:** Saved OpenArena create-server menu preference for ffa: time limit in minutes. Applied when the UI starts a server. |
| `ui_harvester_capturelimit` | `"20"` | **Q3:** Saved OpenArena create-server menu preference for harvester: capture/round-score limit. Applied when the UI starts a server. |
| `ui_harvester_friendly` | `"0"` | **Q3:** Saved OpenArena create-server menu preference for harvester: friendly-fire toggle. Applied when the UI starts a server. |
| `ui_harvester_timelimit` | `"30"` | **Q3:** Saved OpenArena create-server menu preference for harvester: time limit in minutes. Applied when the UI starts a server. |
| `ui_lms_fraglimit` | `"20"` | **Q3:** Saved OpenArena create-server menu preference for lms: frag limit. Applied when the UI starts a server. |
| `ui_lms_timelimit` | `"0"` | **Q3:** Saved OpenArena create-server menu preference for lms: time limit in minutes. Applied when the UI starts a server. |
| `ui_overload_capturelimit` | `"8"` | **Q3:** Saved OpenArena create-server menu preference for overload: capture/round-score limit. Applied when the UI starts a server. |
| `ui_overload_friendly` | `"0"` | **Q3:** Saved OpenArena create-server menu preference for overload: friendly-fire toggle. Applied when the UI starts a server. |
| `ui_overload_timelimit` | `"30"` | **Q3:** Saved OpenArena create-server menu preference for overload: time limit in minutes. Applied when the UI starts a server. |
| `ui_setupchecked` | `"0"` | **Q3:** UI first-run/setup-check state; normally maintained by the OpenArena menu. |
| `ui_smallFont` | `"0.25"` | **Q3:** Size thresholds used by the game's UI font rendering. |
| `ui_spScoreboard` | on demand | **Q3:** Single-player postgame score state consumed by the OpenArena UI. |
| `ui_spSelection` | `""` | **Q3:** Single-player arena currently selected in the UI. |
| `ui_team_fraglimit` | `"0"` | **Q3:** Saved OpenArena create-server menu preference for team: frag limit. Applied when the UI starts a server. |
| `ui_team_friendly` | `"1"` | **Q3:** Saved OpenArena create-server menu preference for team: friendly-fire toggle. Applied when the UI starts a server. |
| `ui_team_timelimit` | `"20"` | **Q3:** Saved OpenArena create-server menu preference for team: time limit in minutes. Applied when the UI starts a server. |
| `ui_tourney_fraglimit` | `"0"` | **Q3:** Saved OpenArena create-server menu preference for tourney: frag limit. Applied when the UI starts a server. |
| `ui_tourney_timelimit` | `"15"` | **Q3:** Saved OpenArena create-server menu preference for tourney: time limit in minutes. Applied when the UI starts a server. |
| `utf8_enable` | `"0"` | **Q1 target:** When 1, changes the qc builtins to act upon codepoints instead of bytes. Do not use unless com_parseutf8 is also set. |
| `v2_enabled` | `"2"` | Enable this secondary engine view. Gamecode-rendered views may bypass this engine view feature. |
| `v2_scalex` | `"0.25"` | Secondary-view width fraction. Gamecode-rendered views may bypass this engine view feature. |
| `v2_scaley` | `"0.25"` | Secondary-view height fraction. Gamecode-rendered views may bypass this engine view feature. |
| `v2_x` | `"0"` | Horizontal position of the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v2_y` | `"0"` | Vertical position of the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v2_yaw` | `"180"` | Yaw angle offset for the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v3_enabled` | `"2"` | Enable this secondary engine view. Gamecode-rendered views may bypass this engine view feature. |
| `v3_scalex` | `"0.25"` | Secondary-view width fraction. Gamecode-rendered views may bypass this engine view feature. |
| `v3_scaley` | `"0.25"` | Secondary-view height fraction. Gamecode-rendered views may bypass this engine view feature. |
| `v3_x` | `"0.25"` | Horizontal position of the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v3_y` | `"0"` | Vertical position of the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v3_yaw` | `"90"` | Yaw angle offset for the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v4_enabled` | `"2"` | Enable this secondary engine view. Gamecode-rendered views may bypass this engine view feature. |
| `v4_scalex` | `"0.25"` | Secondary-view width fraction. Gamecode-rendered views may bypass this engine view feature. |
| `v4_scaley` | `"0.25"` | Secondary-view height fraction. Gamecode-rendered views may bypass this engine view feature. |
| `v4_x` | `"0.5"` | Horizontal position of the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v4_y` | `"0"` | Vertical position of the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v4_yaw` | `"270"` | Yaw angle offset for the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v5_enabled` | `"2"` | Enable this secondary engine view. Gamecode-rendered views may bypass this engine view feature. |
| `v5_scalex` | `"0.25"` | Secondary-view width fraction. Gamecode-rendered views may bypass this engine view feature. |
| `v5_scaley` | `"0.25"` | Secondary-view height fraction. Gamecode-rendered views may bypass this engine view feature. |
| `v5_x` | `"0.75"` | Horizontal position of the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v5_y` | `"0"` | Vertical position of the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v5_yaw` | `"0"` | Yaw angle offset for the secondary view. Gamecode-rendered views may bypass this engine view feature. |
| `v_bonusflash` | `"1"` | Controls the strength of temporary screen flashes when picking up items (gl_polyblend must be enabled). |
| `v_centermove` | `"0.15"` | **Q1 target:** movement interval before automatic pitch recentering. |
| `v_centerspeed` | `"500"` | **Q1 target:** rate of automatic pitch recentering. |
| `v_contentblend` | `"1"` | Controls the strength of underwater colour tints (gl_polyblend must be enabled). |
| `v_contrastboost` | `"1.0"` | Amplifies contrast in dark areas |
| `v_cshift_empty` | `"130 80 50 0"` | The colour tint to use when in the open air (additionally scaled by v_contentblend. |
| `v_cshift_lava` | `"255 80 0 150"` | The colour tint to use when burried in lava (ouchie!) (additionally scaled by v_contentblend. |
| `v_cshift_slime` | `"0 25 5 150"` | The colour tint to use when submerged in slime (additionally scaled by v_contentblend. |
| `v_cshift_water` | `"130 80 50 128"` | The colour tint to use when underwater (additionally scaled by v_contentblend. |
| `v_damagecshift` | `"1"` | Controls the strength of damage-taken colour tints (gl_polyblend must be enabled). |
| `v_deathtilt` | `"1"` | Specifies whether to tilt the view when dead. |
| `v_depthsortentities` (alias: `v_reorderentitiesrandomly`) | `"0"` | Reorder entities for transparency such that the furthest entities are drawn first, allowing nearer transparent entities to draw over the top of them. |
| `v_gammainverted` | Q1: `"1"`; Q3: `"0"` | Boolean that controls whether the gamma should be inverted (like quake) or not. Game profiles select different compatibility conventions; adjust brightness through the native menu. |
| `v_gunkick` | `"0"` | Controls the strength of view angle changes when firing weapons. |
| `v_gunkick_q2` | `"1"` | Controls the strength of view angle changes when firing weapons (in Quake2). |
| `v_idlescale` | `"0"` | Enable swishing of the view (whether idle or otherwise). Often used for concussion effects. |
| `v_ipitch_cycle` | `"1"` | **Q1 target:** idle pitch sway frequency. |
| `v_ipitch_level` | `"0.3"` | **Q1 target:** idle pitch sway amplitude. |
| `v_iroll_cycle` | `"0.5"` | **Q1 target:** idle roll sway frequency. |
| `v_iroll_level` | `"0.1"` | **Q1 target:** idle roll sway amplitude. |
| `v_iyaw_cycle` | `"2"` | **Q1 target:** idle yaw sway frequency. |
| `v_iyaw_level` | `"0.3"` | **Q1 target:** idle yaw sway amplitude. |
| `v_kickpitch` | `"0.6"` | This controls part of the strength of viewangle changes from taking damage. |
| `v_kickroll` | `"0.6"` | This controls part of the strength of viewangle changes from taking damage. |
| `v_kicktime` | `"0.5"` | This controls how long viewangle changes from taking damage will last. |
| `v_pentcshift` | `"1"` | Controls the strength of pentagram-of-protection colour tints (gl_polyblend must be enabled). |
| `v_powerupshell` | `"0"` | **Q1 target:** intensity/enablement of colored player shells for powerups. |
| `v_quadcshift` | `"1"` | Controls the strength of quad-damage colour tints (gl_polyblend must be enabled). |
| `v_ringcshift` | `"1"` | Controls the strength of ring-of-invisibility colour tints (gl_polyblend must be enabled). |
| `v_suitcshift` | `"1"` | Controls the strength of envirosuit colour tints (gl_polyblend must be enabled). |
| `v_viewheight` | `"0"` | **Q1 target:** view-height adjustment used by the engine view calculation; gamecode can override camera placement. |
| `vid_baseheight` | `""` | Specifies a mod's target height and used only when the 2d scale is not otherwise forced. Unlike vid_conheight the size is not fixed and will be padded to avoid inconsistent filtering. |
| `vid_conautoscale` (alias: `scr_conscale`) | `"0"` | Changes the 2d scale, including hud, console, and fonts. To specify an explicit font size, divide the desired 'point' size by 8 to get the scale. High values will be clamped to maintain at least a 320*200 virtual size. |
| `vid_conheight` | `"0"` | Specifies the virtual height of the screen. If 0, will be inferred by vid_conwidth or other settings, according to aspect etc. |
| `vid_conwidth` | `"0"` | Specifies the virtual width of the screen. Should generally be left as 0 to allow the correct aspect to be used despite video mode changes. |
| `vid_desktopgamma` | `"0"` | Legacy hardware/display-ramp gamma preference retained for compatibility. SDL3 GPU always applies display correction in its presentation shader; this setting does not select hardware ramps or texture-baked gamma. Use vid_sceneonlygamma for scene-only correction. |
| `vid_desktopsettings` | `"0"` | Ignore the values of vid_width and vid_height, and just use the same settings that are used for the desktop. [applies on video restart.] |
| `vid_devicename` | `""` | Specifies which video device to try to use. If blank or invalid then one will be guessed. [applies on video restart.] |
| `vid_displayfrequency` | `"0"` | Requested display refresh rate for fullscreen video mode selection. [applies on video restart.] |
| `vid_dpi_x` | `"0"` | For mods that need to determine the physical screen size (like with touchscreens). 0 means unknown |
| `vid_dpi_y` | `"0"` | For mods that need to determine the physical screen size (like with touchscreens). 0 means unknown |
| `vid_fullscreen` | `"2"` | **Different:** Specifies whether the game should be fullscreen or not (requires vid_restart).<br>0: Run in a resizable window, which can be manually maximized (with borders).<br>1: Traditional fullscreen-exclusive video mode with mode switching and everything.<br>2: Simply maximize the window and hide any borders without interfering with any other parts of the system. Quake 3 UI VM requests for r_fullscreen are translated to this setting; r_fullscreen is not an engine console alias. [applies on video restart.] |
| `vid_hardwaregamma` | `"1"` | Legacy hardware/display-ramp gamma preference retained for compatibility. SDL3 GPU always applies display correction in its presentation shader; this setting does not select hardware ramps or texture-baked gamma. Use vid_sceneonlygamma for scene-only correction. |
| `vid_height` | `"0"` | The screen height to attempt to use, in physical pixels. 0 means use desktop resolution. [applies on video restart.] |
| `vid_minsize` | `"320 200"` | Specifies a mod's minimum virtual size (to be set inside the mod's default.cfg file). The virtual size will always be at least this big. Windows may not be resized below this value either (which may reduce video mode options), but might require a vid_restart to take effect when switching between mods. |
| `vid_preservegamma` | `"0"` | Legacy hardware/display-ramp gamma preference retained for compatibility. SDL3 GPU always applies display correction in its presentation shader; this setting does not select hardware ramps or texture-baked gamma. Use vid_sceneonlygamma for scene-only correction. |
| `vid_renderer` | `"sdlgpu"` | Selects the renderer: sdlgpu (Metal on macOS, Vulkan on Windows/Linux), headless, or sv (dedicated server). [applies on video restart.] |
| `vid_sceneonlygamma` | `"0"` | Apply gamma, contrast, contrast boost, and brightness only to the 3D view, preserving HUD, menu, and console colors. Takes effect immediately in the SDL3 GPU renderer. Native **Options → Video → Scene-Only Gamma**: 0 corrects the full image, 1 corrects the 3D scene before HUD, menus and console. |
| `vid_triplebuffer` (alias: `gl_triplebuffer`) | `"1"` | Specifies whether the hardware is forcing tripplebuffering on us, this is the number of extra page swaps required before old data has been completely overwritten. |
| `vid_vsync` (alias: `vid_wait`) | `"0"` | Presentation synchronization preference. SDL3 GPU support depends on the GPU backend and available present modes. |
| `vid_width` | `"0"` | The screen width to attempt to use, in physical pixels. 0 means use desktop resolution. [applies on video restart.] |
| `vid_wndalpha` | `"1"` | When running windowed, specifies the window's transparency level. |
| `videoflags` | `"7"` | **Q3:** OpenArena video/fairness restriction bitmask; includes restrictions on view settings. |
| `viewsize` | `"100"` | **Different:** Engine view/status-bar size. Quake 1 uses this for its classic HUD; Quake 3 gamecode uses cg_viewsize for its own viewport. |
| `volume` (alias: `s_volume`) | `"0.7"` | Volume level for game sounds (does not affect music, voice, or cinematics). |
| `voteflags` | `"0"` | **Q3:** OpenArena allowed-vote bitmask advertised by the server. |
| `votelevel` | `"0"` | This is the restriction level of commands that players may vote for. You can reconfigure commands, cvars, or aliases individually. Additionally, aliases can be configured via aliaslevel to be executed at a different level from their restriction level. This can be used to indirectly allow voting for 'map dm4' for instance, without allowing people to vote for every map. |
| `voteminimum` | `"4"` | At least this many players must vote the same way for the vote to pass. |
| `votepercent` | `"-1"` | At least this percentage of players must vote the same way for the vote to pass. |
| `votetime` | `"10"` | Votes will be discarded after this many minutes |
| `w_switch` | `""` | **Q1 target:** weapon-switch preference userinfo, interpreted by supporting gamecode when picking up weapons/backpacks. |
| `watervis` | `""` | **Q1 target:** water-visibility/transparency compatibility information. |
| `worker_count` | `""` | Specifies the number of worker threads to utilise. |
| `worker_flush` | `"1"` | If set, process the entire load queue, loading stuff faster but at the risk of stalling the main thread. |
| `worker_sleeptime` | `"0"` | Causes workers to sleep for a period of time after each job. |
| `zombietime` | `"2"` | Client slots will not be reused for this number of seconds. |

## OpenArena administrator chat commands

These **Q3/OpenArena-only** commands belong to the bundled administration
gamecode, not FTE's command registry. From a client use `say !help` or
`say !help ban`; direct forms depend on the gamecode's chat/command parsing.
Permission levels and enabled commands come from the server's admin config.
Custom admin configs can add commands. They are distinct from similarly named
engine commands: `!kick` is not the engine's `kick`, for example.

| Gamecode command | Definition |
| --- | --- |
| `!adjustban` | Edit a numbered ban's reason or expiry; signed durations extend or shorten it. |
| `!admintest` | Report the requesting player's administration level. |
| `!allready` | Mark every player ready to leave intermission. |
| `!ban` | Block a player by address/GUID, optionally with a reason and expiry. |
| `!cancelvote` | Stop the current vote without passing it. |
| `!disorient` | Apply the administration view/control inversion penalty to a player. |
| `!help` | List permitted administration actions or explain one named action. |
| `!kick` | Disconnect a selected player, optionally recording a reason. |
| `!listadmins` | Show configured administrators and their permission levels. |
| `!listplayers` | Show connected players with their slot and administration information. |
| `!lock` | Prevent new players from joining a selected team. |
| `!map` | Change the hosted game to a named map through the admin system. |
| `!mute` | Prevent a selected player from sending chat. |
| `!namelog` | Inspect recent player-name history. |
| `!nextmap` | Advance the hosted game to the next configured map. |
| `!orient` | Remove the disorient penalty from a player. |
| `!passvote` | Approve the active vote immediately. |
| `!putteam` | Assign a selected player to a team or spectator state. |
| `!readconfig` | Refresh administration settings and permissions from the config file. |
| `!rename` | Assign a new name to a selected player. |
| `!restart` | Restart the active level through gamecode administration. |
| `!setlevel` | Assign a player's administration permission level. |
| `!showbans` | Inspect active bans and their identifying numbers. |
| `!slap` | Apply a specified amount of administration damage to a player. |
| `!spec999` | Move clients reporting a 999 ping into spectator state. |
| `!time` | Report the server's local clock time. |
| `!unban` | Remove a numbered ban shown by showbans. |
| `!unlock` | Allow players to join a previously locked team. |
| `!unmute` | Restore chat privileges to a muted player. |
| `!warn` | Record/send an administration warning to a player. |

## Dynamic gamecode cvars

Some names are generated at runtime rather than registered as a fixed set:

| Name pattern | Definition / game notes |
| --- | --- |
| `session<N>` | **Q3:** qagame session data for client slot N. The initialized slot-0 value is listed above; additional slots are created as clients join. Game managed; do not edit manually. |
| `disable_<pickup-classname>` | **Q3/OpenArena:** suppress spawning a named pickup, for example `disable_weapon_railgun 1`. Maps register the pickup classes they use; initialized entries are listed above. Class names come from the game's item definitions, not model filenames. |
| Mod-defined names | QuakeC and QVM gamecode can register/read additional settings or commands. Their meaning belongs to that mod, not the engine. |

## Related documentation and source

See [SDL3 GPU renderer controls](SDL3-GPU.md),
[Writing Your progs.dat](writing_progs.md), and the [FTEQCC manual](FTEQCC.md).
QuakeC builtin functions and compiler command-line switches are separate
interfaces from the console commands and cvars listed here.

The engine registry/dispatch and permissions live in
[cmd.c](../engine/common/cmd.c); cvar lookup, aliases and defaults in
[cvar.c](../engine/common/cvar.c). See [client initialization](../engine/client/cl_main.c),
[input](../engine/client/cl_input.c), [native options](../engine/client/m_options.c),
[renderer settings](../engine/client/renderer.c), [view calculation](../engine/client/view.c)
and [server administration](../engine/server/sv_ccmds.c).

Quake 3 integration and UI-only cvar-name translations live in this fork's
[cgame bridge](../plugins/quake3/clq3_cg.c),
[UI bridge](../plugins/quake3/clq3_ui.c) and
[qagame bridge](../plugins/quake3/svq3_game.c).
OpenArena's [cgame commands](https://github.com/OpenArena/gamecode/blob/main/code/cgame/cg_consolecmds.c),
[client cvars](https://github.com/OpenArena/gamecode/blob/main/code/cgame/cg_main.c),
[qagame client commands](https://github.com/OpenArena/gamecode/blob/main/code/game/g_cmds.c),
[server commands](https://github.com/OpenArena/gamecode/blob/main/code/game/g_svcmds.c),
[server cvars](https://github.com/OpenArena/gamecode/blob/main/code/game/g_main.c),
[UI](https://github.com/OpenArena/gamecode/tree/main/code/q3_ui) and
[administration](https://github.com/OpenArena/gamecode/blob/main/code/game/g_admin.c)
provide handler context. Those upstream links track main and may describe
newer features; this inventory filters gamecode names against the loaded
OpenArena 0.8.8 QVMs rather than assuming every newer feature is present.

To audit after an update, initialize each game in an isolated configuration,
capture `cmdlist`, `apropos ""`, and `cvarlist -adF` from the client and server,
and compare their union with this reference. Include loaded VM command
dispatchers and on-demand cvar paths: an engine-only `cmdlist` is insufficient.
Keep current values, passwords and local device/path information out of the
published inventory. The console and gamecode of your installed build remain
the authority for its actual availability.
