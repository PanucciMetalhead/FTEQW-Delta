#!/usr/bin/env python3
"""Build an original Quake grid room using local KVX samples from a PK3."""
import argparse
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
# Sprite, Spawn frame order, tics per frame (35 Hz), fullbright, pedestal Y, yaw.
EXHIBITS = (
    ("BAR1", "AB", 6, False, -114, 145),
    ("COLU", "A", 0, True, -38, 180),
    ("CEYE", "ABCB", 6, True, 38, 195),
    ("TROO", "AB", 10, False, 114, 240),
)


def grid_wad():
    layers = bytearray()
    offsets = []
    for size in (64, 32, 16, 8):
        offsets.append(40 + len(layers))
        layers.extend(10 if x == 0 or y == 0 else 6 if x % (size//4) == 0
                      or y % (size//4) == 0 else 3
                      for y in range(size) for x in range(size))
    texture = struct.pack("<16s6I", b"kvx_grid", 64, 64, *offsets) + layers
    directory = struct.pack("<3IBBH16s", 12, len(texture), len(texture), 68, 0, 0, b"kvx_grid")
    return struct.pack("<4sII", b"WAD2", 1, 12+len(texture)) + texture + directory


def brush(lo, hi):
    faces = []
    for axis in range(3):
        u, v = (axis+1)%3, (axis+2)%3
        for side in (0, 1):
            points = []
            for uu, vv in ((0, 0), (0, 1), (1, 0)) if side else ((0, 0), (1, 0), (0, 1)):
                point = [0, 0, 0]
                point[axis] = (lo, hi)[side][axis]
                point[u] = (lo, hi)[uu][u]
                point[v] = (lo, hi)[vv][v]
                points.append("( {} {} {} )".format(*point))
            faces.append(" ".join(points) + " kvx_grid 0 0 0 1 1")
    return "{\n" + "\n".join(faces) + "\n}\n"


def room():
    walls = [
        ((-272, -208, -16), (272, 208, 0)),
        ((-272, -208, 160), (272, 208, 176)),
        ((-272, -208, 0), (-256, 208, 160)),
        ((256, -208, 0), (272, 208, 160)),
        ((-256, -208, 0), (256, -192, 160)),
        ((-256, 192, 0), (256, 208, 160)),
    ]
    pedestals = [((40, y-30, 0), (104, y+30, 16)) for y in (-114, -38, 38, 114)]
    return (
        '{\n"classname" "worldspawn"\n"wad" "kvx_grid.wad"\n"message" "KVX Grid Room"\n"light" "155"\n'
        + "".join(brush(*bounds) for bounds in walls+pedestals)
        + '}\n{\n"classname" "info_player_start"\n"origin" "-108 0 52"\n}\n'
        + '{\n"classname" "light"\n"origin" "-25 -90 118"\n"light" "360"\n}\n'
        + '{\n"classname" "light"\n"origin" "-25 90 118"\n"light" "360"\n}\n'
    )


def gamecode(animations, models):
    return """
#pragma target fte
#pragma noref 1
// FTE resolves the QuakeC fixture's reduced interface by name.
#pragma warning disable Q208
entity self, other, world;
float time, frametime, force_retouch;
string mapname;
float deathmatch, coop, teamplay, serverflags;
float total_secrets, total_monsters, found_secrets, killed_monsters;
void() main, StartFrame, PlayerPreThink, PlayerPostThink, ClientKill;
void() ClientConnect, PutClientInServer, ClientDisconnect, SetNewParms, SetChangeParms;
void end_sys_globals;
.float modelindex, movetype, solid, health, fixangle, flags;
.vector origin, mins, maxs, angles, view_ofs;
.string classname, model;
void end_sys_fields;
.float spawn_pose, spawn_interval, nextthink;
.float drawflags, abslight;
.void() think;
void(entity e, vector p) setorigin = #2;
void(entity e, string name) setmodel = #3;
void(entity e, vector lo, vector hi) setsize = #4;
void(string s) error = #10;
entity() spawn = #14;
string(string s) precache_model = #20;
void(string s) bprint = #23;
void(string s) localcmd = #46;
float(string s) cvar = #45;

void(string name, vector position, float yaw, float fullbright,
     void() advance, float tics) exhibit =
{
    entity item = spawn();
    setmodel(item, name);
    if (item.maxs == item.mins)
        error(name);
    item.angles_y = yaw;
    if (fullbright)
    {
        item.drawflags = 7; // MLS_ABSLIGHT
        item.abslight = 1;
    }
    setorigin(item, position - '0 0 1' * item.mins_z);
    if (advance)
    {
        item.think = advance;
        item.spawn_interval = tics / 35;
        item.nextthink = time + item.spawn_interval;
    }
};

""" + animations + """

void() worldspawn =
{
""" + models + """
    bprint("KVX exhibits loaded\\n");
};

void() info_player_start = {};
void() light = {};
void() main = {};
void() StartFrame = {};
void() PlayerPreThink = {};
void() PlayerPostThink = {};
void() ClientKill = {};
void() ClientConnect = {};
void() ClientDisconnect = {};
void() SetNewParms = {};
void() SetChangeParms = {};
void() PutClientInServer =
{
    self.classname = "player";
    self.movetype = 8;
    self.solid = 0;
    self.flags = 8;
    self.health = 100;
    self.view_ofs = '0 0 22';
    self.angles = '9 0 0';
    self.fixangle = 1;
    setsize(self, '-16 -16 -24', '16 16 32');
    setorigin(self, '-108 0 52');
    if (cvar("kvx_capture"))
    {
        localcmd("defer 2 screenshot kvx-preview.png\\n");
        localcmd("defer 3 quit force\\n");
    }
};
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pk3", type=Path)
    parser.add_argument("--quake-data", type=Path, required=True, help="Directory containing id1/pak0.pak")
    parser.add_argument("--qbsp", type=Path, required=True)
    parser.add_argument("--light", type=Path, help="ericw-tools light executable")
    parser.add_argument("--qcc", type=Path, default=ROOT/"build/fteqcc")
    parser.add_argument("--engine", type=Path, default=ROOT/"build/fteqw")
    parser.add_argument("--out", type=Path, default=ROOT/"build/kvx-preview")
    args = parser.parse_args()
    pak = args.quake_data.resolve()/"id1/pak0.pak"
    if not pak.is_file():
        parser.error(f"Missing {pak}")
    args.out.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="room-", dir=args.out.resolve()))
    game = out/"id1"
    (game/"maps").mkdir(parents=True)
    (game/"models").mkdir()
    (game/"pak0.pak").symlink_to(pak)
    precache = []
    exhibits = []
    animations = []
    with zipfile.ZipFile(args.pk3) as source:
        for sprite, poses, tics, bright, y, yaw in EXHIBITS:
            paths = {pose: f"models/{sprite.lower()}{pose.lower()}.kvx" for pose in poses}
            for pose, model in paths.items():
                (game/model).write_bytes(source.read(f"voxels/{sprite}{pose}.kvx"))
                precache.append(f'    precache_model("{model}");')

            callback = "__NULL__"
            if tics:
                callback = f"advance_{sprite.lower()}"
                cases = "\n".join(f'        case {i}: setmodel(self, "{paths[pose]}"); break;'
                                  for i, pose in enumerate(poses))
                animations.append(f"""void() {callback} =
{{
    self.spawn_pose = (self.spawn_pose + 1) % {len(poses)};
    switch (self.spawn_pose)
    {{
{cases}
    }}
    self.nextthink = time + self.spawn_interval;
}};
""")
            exhibits.append(f'    exhibit("{paths[poses[0]]}", \'72 {y} 16\', '
                            f'{yaw}, {int(bright)}, {callback}, {tics});')
    (out/"kvx_grid.wad").write_bytes(grid_wad())
    (out/"kvx_grid.map").write_text(room())
    subprocess.run([str(args.qbsp.resolve()), "kvx_grid.map", str(game/"maps/kvx_grid.bsp")],
                   cwd=out, check=True)
    if args.light:
        subprocess.run([str(args.light.resolve()), str(game/"maps/kvx_grid.bsp")], cwd=out, check=True)
    (game/"preview.qc").write_text(gamecode("\n".join(animations), "\n".join(precache + exhibits)))
    subprocess.run([str(args.qcc.resolve()), "-srcfile", "preview.qc", "-o", "kvx-preview.dat"],
                   cwd=game, check=True)
    (game/"kvx-preview.cfg").write_text(f"""cfg_save_auto 0
sv_progs kvx-preview.dat
sv_gameplayfix_setmodelrealbox 1
r_fullbright {0 if args.light else 1}
crosshair 0
viewsize 120
con_notifytime 0
bind w +forward
bind s +back
bind a +moveleft
bind d +moveright
map kvx_grid
""")
    command = shlex.join([str(args.engine.resolve()), "-basedir", str(out), "-nohome", "-quake",
                          "-window", "-width", "1120", "-height", "700",
                          "+set", "plug_loaddefault", "0", "+exec", "kvx-preview.cfg"])
    launcher = out/"Play KVX Grid.command"
    launcher.write_text("#!/bin/sh\nexec " + command + "\n")
    launcher.chmod(0o755)
    print(f"Launcher: {launcher}")
    print(command)


if __name__ == "__main__":
    main()
