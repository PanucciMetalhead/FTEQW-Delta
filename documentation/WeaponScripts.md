# Quake 1 weapon scripts

FTEQW Delta has built-in weapon preselection, priority lists, and automatic
switching to a less valuable weapon after firing. These features are similar
to those in [ezQuake's weapon-script guide](https://ezquake.com/docs/weapon-scripts.html#weapon-scripts):
the basic `cl_weaponpreselect 1`, `cl_weaponhide 1`, `weapon`, and `+attack`
configuration works here too. Long switch/fire/hide aliases are unnecessary
for this setup.

This guide applies to Quake 1 (NetQuake/QuakeWorld). Quake 3/OpenArena uses
its own gamecode weapon selection; its `weapon <number>` command does not
provide this Quake 1 priority-list behavior. Mods can change weapon numbers,
ammo requirements, switching delays, and what a killed player drops.

## Basic setup

Enter these commands in the console while running Quake 1, or put them in
a config file and load it with `exec <filename>.cfg`:

```text
seta cl_weaponpreselect 1
seta cl_weaponhide 1
bind q "weapon 7"
bind mouse1 "+attack"
cfg_save
```

`seta` marks the settings for saving; `cfg_save` saves archived settings and
bindings. Both features default to `0` (off).

1. Press **Q** to preselect the rocket launcher. Your held weapon stays the same.
2. Press **Mouse1** to request the rocket launcher and attack together.
3. Release **Mouse1** to request the hiding weapon, keeping the rocket launcher
   preselected for your next attack.

The hide request happens when you release attack, rather than after every
individual shot while holding the button. The server's weapon cooldown and
gamecode determine when a requested switch takes effect. This feature switches
your actual equipped weapon; it does not simply hide the viewmodel. In modes
that drop your equipped weapon on death, switching away can deny that weapon
to an opponent.

## Choose the best available weapon

Replace the Q binding with a priority list:

```text
bind q "weapon 7 5 3 2 1"
```

The first available weapon with sufficient ammo wins. With standard Quake
weapon numbers, this tries rocket launcher (7), super nailgun (5), super
shotgun (3), shotgun (2), then axe (1). For example, having no rockets makes
the client try the super nailgun next.

`weapon 7 6` tries only the rocket launcher and grenade launcher. If neither
is usable, it requests no switch. By default, the client checks the stored
priority list again when you attack, so pickups or ammo changes between
selection and firing can change the result.

Use `weapon` throughout your selection binds when using preselection:

```text
bind 7 "weapon 7"
bind 8 "weapon 8"
```

`impulse` and the compatibility command `bestweapon` bypass preselection.
An old `impulse 7` bind immediately requests a switch and does not replace
the priority list used by your next `+attack`.

## Choose the hiding weapon

`cl_weaponhide_preference` defaults to `"2 1"`. Unlike `weapon` lists,
**the last usable entry wins**: this default prefers the axe, falling back
to the shotgun if the axe is unavailable. To prefer the shotgun and fall
back to the axe when you have no shells, use:

```text
seta cl_weaponhide_preference "1 2"
```

Use `"1"` for axe only or `"2"` for shotgun only. If no listed weapon is
usable, no hide switch is requested. `cl_weaponhide_axe` is a compatibility
alias for this setting; `0` is specially interpreted as shotgun (`2`),
and `1` selects the axe. Use `cl_weaponhide 0` to turn hiding off.

## Optional controls

| Setting | Behavior |
| --- | --- |
| `cl_weaponpreselect 0` | `weapon` requests a switch immediately. |
| `cl_weaponpreselect 1` | Wait for a new attack before switching. |
| `cl_weaponpreselect 2` | Switch immediately if already firing; otherwise wait for the next attack. |
| `cl_weaponforgetorder 1` | Keep only the best usable weapon found when `weapon` runs, instead of checking the entire priority list again at attack time. Defaults to `0`. |
| `r_viewpreselgun 1` | Show the preselected weapon model instead of the equipped weapon model. This changes the display, not which weapon the server considers equipped. Defaults to `0`. |

For a dedicated select-and-fire key, use `+fire`:

```text
bind mouse2 "+fire 8 7"
```

Pressing Mouse2 selects and attacks with the lightning gun if usable,
otherwise the rocket launcher. If neither is usable, it uses your existing
preselection when enabled, otherwise your current weapon. Releasing the
button also uses `cl_weaponhide`. The matching release command is sent
automatically by the binding.

To return to immediate selection without automatic hiding:

```text
seta cl_weaponpreselect 0
seta cl_weaponhide 0
cfg_save
```

See the [console command and cvar reference](console_commands.md) for the
complete inventory. The implementation is in
[`engine/client/cl_input.c`](../engine/client/cl_input.c), with the Quake 1
`weapon` and `+fire` command registrations in
[`engine/client/cl_main.c`](../engine/client/cl_main.c).
