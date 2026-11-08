#!/usr/bin/env python3
"""Stage a playable Arcane Dimensions 1.80 Patch 1 overlay from local game data."""

import argparse
import math
from pathlib import Path
import random
import shlex
import shutil
import struct
import subprocess

HERE = Path(__file__).resolve().parent
MAP = "jolt_range"
AD_PATCH = "pak2.pak"


def pak_files(pak, accept):
    with pak.open("rb") as stream:
        magic, start, size = struct.unpack("<4s2I", stream.read(12))
        if magic != b"PACK" or size % 64 or start + size > pak.stat().st_size:
            raise ValueError(f"Invalid PAK directory: {pak}")
        stream.seek(start)
        for name, offset, length in struct.iter_unpack("<56s2I", stream.read(size)):
            name = name.split(b"\0", 1)[0].decode("ascii")
            if accept(name):
                if offset + length > start:
                    raise ValueError(f"Invalid PAK entry: {name}")
                stream.seek(offset)
                yield name, stream.read(length)


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"AD source mismatch (expected Patch 1): {old!r}")
    return text.replace(old, new, 1)


def patch_source(sources, smoke=False):
    patches = {
        "progs.src": [
            ("../progs.dat", "../jolt_range.dat"),
            ("mathlib.qc\n", "mathlib.qc\nrange.qh\n"),
        ],
        "ai_combat.qc": [
            ("targ.health = targ.health - take;", "if (range_hit(targ, inflictor, take)) return;\n\ttarg.health = targ.health - take;"),
            ("if (targ.health < 1) {\n\t\tKilled (targ, attacker);",
             "if (range_soldier_hit(targ, inflictor, attacker, take)) return;\n\tif (targ.health < 1) {\n\t\tKilled (targ, attacker);"),
        ],
        "ai_projectiles.qc": [
            ("// bullet touch has been disabled\n\tentity_remove(self, 1);",
             "// bullet touch has been disabled\n\tself.range_impact = self.velocity;\n\tentity_remove(self, 1);"),
        ],
        "mon_soldier.qc": [
            ("void() army_die =\n{", "void() army_die =\n{\n\trange_end_push(self);"),
            ("// regular death\n\tif (!self.gibbed)", "if (range_ragdoll_death()) return;\n\n\t// regular death\n\tif (!self.gibbed)"),
        ],
        "world.qc": [
            ("switch_shadow_setuparray();", "switch_shadow_setuparray();\n\trange_init();"),
            ("void() StartFrame =\n{", "void() StartFrame =\n{\n\trange_frame();"),
        ],
    }
    for name, changes in patches.items():
        # Preserve any non-UTF-8 characters in the original comments.
        source = sources[name].decode("latin-1").replace("\r\n", "\n")
        for old, new in changes:
            source = replace_once(source, old, new)
        sources[name] = source.encode("latin-1")
    sources["progs.src"] += b"\nrange.qc\n"
    if smoke:
        sources["progs.src"] = sources["progs.src"].replace(b"../jolt_range.dat", b"../jolt_range_smoke.dat")
        sources["progs.src"] += b"range_smoke.qc\n"
        sources["world.qc"] = sources["world.qc"].replace(b"\trange_frame();",
                                                          b"\trange_frame();\n\trange_smoke_tick();")
        sources["range_smoke.qc"] = (HERE / "range_smoke.qc").read_bytes()


def vec(p):
    return " ".join(f"{v:.6f}".rstrip("0").rstrip(".") or "0" for v in p)


def md5(shader, bones, verts, tris):
    lines = ["MD5Version 10", 'commandline ""', f"numJoints {len(bones)}", "numMeshes 1", "joints {"]
    for i, (name, center) in enumerate(bones):
        lines.append(f'  "{name}" {0 if i else -1} ( {vec(center)} ) ( 0 0 0 )')
    lines += ["}", "mesh {", f'  shader "{shader}"', f"  numverts {len(verts)}"]
    weights = []
    for i, (p, uv, influences) in enumerate(verts):
        lines.append(f"  vert {i} ( {vec(uv)} ) {len(weights)} {len(influences)}")
        for bone, strength in influences:
            offset = tuple(a - b for a, b in zip(p, bones[bone][1]))
            weights.append(f"  weight {len(weights)} {bone} {strength:.8f} ( {vec(offset)} )")
    lines.append(f"  numtris {len(tris)}")
    lines.extend(f"  tri {i} {a} {b} {c}" for i, (a, b, c) in enumerate(tris))
    lines += [f"  numweights {len(weights)}", *weights, "}"]
    return "\n".join(lines) + "\n"


def rock_model(radius, seed, collision=None):
    """Use the same irregular vertices for the mesh and its collision hull."""
    rng = random.Random(seed)
    rings, sectors = (6, 11) if collision else (7, 13)
    points = {}
    for j in range(rings + 1):
        for i in range(sectors if j not in (0, rings) else 1):
            a, b = math.pi * j / rings, math.tau * i / sectors
            r = radius * rng.uniform(.88, 1.10)
            points[j, i] = (math.sin(a) * math.cos(b) * r,
                            math.sin(a) * math.sin(b) * r * .91, math.cos(a) * r * .96)
    verts, tris = [], []
    for j in range(rings + 1):
        for i in range(sectors + 1):
            p = points[j, 0 if j in (0, rings) else i % sectors]
            verts.append((p, (i / sectors, j / rings), [(0, 1)]))
    for j in range(rings):
        for i in range(sectors):
            a = j * (sectors + 1) + i
            b = a + sectors + 1
            # FTE's alias mesh loader expects clockwise faces viewed from outside.
            if j:
                tris.append((a, a + 1, b + 1))
            if j < rings - 1:
                tris.append((a, b + 1, b))
    if collision:
        collision.write_text("ftejolt 1\nhull {\n" + "\n".join(vec(p) for p in points.values()) + "\n}\n")
    return md5("range_stone", [("Root", (0, 0, 0))], verts, tris)


# AD grunt's standing pose. (Bone, center, collision half extents, mass, parent, pivot, joint, stop)
GRUNT = (
    ("Pelvis", (-6, 0, 1), (4.5, 6, 4), 14, None, (), 0, 0),
    ("Torso", (-3, 0, 13), (5, 7, 7), 25, "Pelvis", (-5, 0, 6), 4, .5),
    ("Head", (3, 0, 23), (4.2, 3.7, 4.3), 5, "Torso", (1, 0, 19), 4, .7),
    ("ThighL", (-4, 5, -5), (3.7, 3, 5.6), 9, "Pelvis", (-6, 4, 0), 4, 1.05),
    ("ThighR", (-4, -5, -5), (3.7, 3, 5.6), 9, "Pelvis", (-6, -4, 0), 4, 1.05),
    ("ShinL", (-5, 6.5, -16.5), (4, 2.9, 5.8), 5, "ThighL", (-2, 6, -11), 2, 1.3),
    ("ShinR", (-5, -6.5, -16.5), (4, 2.9, 5.8), 5, "ThighR", (-2, -6, -11), 2, 1.3),
    ("ArmL", (4.5, 9, 12), (5, 2.9, 4), 4, "Torso", (-2, 7, 16), 4, 1.35),
    ("ArmR", (4.5, -9, 12), (5, 2.9, 4), 4, "Torso", (-2, -7, 16), 4, 1.35),
)


def rig_source(envelopes):
    lines = ["void(entity rag, entity soldier) range_build_parts = {"]
    for i, (name, center, _, mass, parent, pivot, kind, stop) in enumerate(GRUNT, 2):
        lower, upper = envelopes[i - 1]
        lines.append(f"    local entity p{name} = range_limb(rag, soldier, {i}, '{vec(center)}', "
                     f"'{vec(lower)}', '{vec(upper)}', {mass});")
        if parent:
            lines.append(f"    range_joint(rag, soldier, p{parent}, p{name}, '{vec(pivot)}', {kind}, {stop});")
    lines += ["    rag.enemy = pPelvis;", "};", ""]
    return "\n".join(lines)


def grunt_model(mdl, progs):
    header = struct.unpack_from("<ii10f8if", mdl)
    if header[:2] != (0x4F504449, 6):
        raise ValueError("Unexpected AD soldier MDL format")
    skins, width, height, nv, nt, nf = header[12:18]
    pos = 84
    for i in range(skins):
        if struct.unpack_from("<i", mdl, pos)[0] != 0:
            raise ValueError("Expected single-image AD soldier skins")
        pos += 4
        end = pos + width * height
        (progs / f"range_grunt_{i:02d}_00.lmp").write_bytes(struct.pack("<2I", width, height) + mdl[pos:end])
        pos = end
    uvs = list(struct.iter_unpack("<3i", mdl[pos:pos + nv * 12]))
    pos += nv * 12
    faces = list(struct.iter_unpack("<4i", mdl[pos:pos + nt * 16]))
    pos += nt * 16
    if not nf or mdl[pos:pos + 4] != b"\0" * 4 or mdl[pos + 12:pos + 18] != b"stand1":
        raise ValueError("Expected AD grunt standing pose as first frame")
    pos += 28
    positions = [tuple(v[i] * header[2 + i] + header[5 + i] for i in range(3))
                 for v in struct.iter_unpack("<4B", mdl[pos:pos + nv * 4])]
    bones = [("Root", (0, 0, 0))] + [(part[0], part[1]) for part in GRUNT]
    verts, tris, lookup = [], [], {}
    for front, a, b, c in faces:
        tri = []
        for i in (a, b, c):
            seam, s, t = uvs[i]
            s += width // 2 if seam and not front else 0
            key = i, s
            if key not in lookup:
                p = positions[i]
                # Blend near boundaries while keeping the head/helmet, boots and gun coherent.
                costs = []
                for bone, (name, center, bounds, *_) in enumerate(GRUNT, 1):
                    q = sum(((v - k) / (h + 2)) ** 2 for v, k, h in zip(p, center, bounds))
                    if p[2] > 19 and name == "Head":
                        q *= .45
                    if p[0] > 8 and p[2] < 17 and name == "ArmR":
                        q *= .4
                    costs.append((q, bone))
                near = sorted(costs)[:3]
                ws = [(bone, math.exp(-3.4 * (q - near[0][0]))) for q, bone in near]
                ws = [(bone, w) for bone, w in ws if w > .035]
                total = sum(w for _, w in ws)
                lookup[key] = len(verts)
                verts.append((p, ((s + .5) / width, (t + .5) / height),
                              [(bone, w / total) for bone, w in ws]))
            tri.append(lookup[key])
        tris.append(tuple(tri))
    # Fit each body to the vertices it primarily drives. A skinned vertex may
    # blend across a seam only if each contributing body encloses that point
    # in its bind pose. The mesh then stays within the bodies' support plane.
    primary = [[] for _ in bones]
    for point, _, weights in verts:
        primary[weights[0][0]].append(point)
    envelopes = {}
    for i, (_, center, bounds, *_) in enumerate(GRUNT, 1):
        lower, upper = [], []
        for axis in range(3):
            coords = [p[axis] - center[axis] for p in primary[i]]
            lower.append(min([-bounds[axis], *coords]) - 1.25)
            upper.append(max([bounds[axis], *coords]) + 1.25)
        envelopes[i] = lower, upper
    for i, (point, uv, weights) in enumerate(verts):
        kept = []
        for bone, weight in weights:
            lower, upper = envelopes[bone]
            offset = [p - c for p, c in zip(point, bones[bone][1])]
            if all(lo + .5 <= coord <= hi - .5 for lo, coord, hi in zip(lower, offset, upper)):
                kept.append((bone, weight))
        total = sum(weight for _, weight in kept)
        verts[i] = point, uv, [(bone, weight / total) for bone, weight in kept]
    (progs / "range_grunt.md5mesh").write_text(md5("range_grunt", bones, verts, tris))
    return envelopes


def palette_index(palette, rgb):
    return min(range(224), key=lambda i: sum((a-b) ** 2 for a, b in zip(rgb, palette[i])))


def tiles(palette):
    rng = random.Random(95)
    specs = {
        "range_concrete": ((109, 105, 89), (69, 68, 61)),
        "range_wall": ((87, 91, 87), (49, 53, 51)),
        "range_floor": ((88, 93, 94), (46, 53, 54)),
        "range_hazard": ((162, 122, 40), (47, 45, 42)),
        "range_portal": ((66, 147, 143), (27, 56, 60)),
        "skip": ((72, 72, 72), (72, 72, 72)),
        "trigger": ((72, 72, 72), (72, 72, 72)),
    }
    for name, (base, dark) in specs.items():
        ramps = [[palette_index(palette, [max(0, min(255, c + n)) for c in color])
                  for n in range(-16, 17)] for color in (base, dark)]
        data = bytearray()
        for y in range(64):
            for x in range(64):
                if name in ("range_hazard", "range_portal"):
                    seam = ((x + y) % 32 >= 17) if name == "range_hazard" else (x % 16 > 4)
                elif name == "range_wall":
                    seam = y % 32 < 2 or (x + (y // 32) * 32) % 64 < 2
                else:
                    seam = x < 2 or y < 2
                data.append(ramps[int(seam)][16 + rng.randrange(-9, 10)])
        yield name, bytes(data)


CHECKPOINTS = [(176, -168, 520), (512, 156, 872), (856, -44, 1216), (1192, 132, 1510)]
STRIPE_WIDTH = 16


def ramp_texture(textures):
    # Quake's default top-face texture axes use +X and -Y. Paint the trigger
    # lines into one texture so the ramp collision is one uninterrupted plane.
    rows = []
    for row in range(2048):
        y = (-row) % 2048
        name = "range_hazard" if any(start <= y < start + STRIPE_WIDTH
                                     for start, _, _ in CHECKPOINTS) else "range_concrete"
        offset = (row % 64) * 64
        rows.append(textures[name][offset:offset + 64])
    return b"".join(rows)


def wad_textures(path, palette):
    textures = dict(tiles(palette))
    textures["range_ramp"] = ramp_texture(textures)
    data, directory = bytearray(), []
    for name, pixels in textures.items():
        label = name.encode("ascii")
        start = len(data) + 12
        width, height = 64, len(pixels) // 64
        mip = [pixels]
        for step in (2, 4, 8):
            mip.append(bytes(pixels[y * width + x] for y in range(0, height, step) for x in range(0, width, step)))
        offsets = [40]
        for level in mip[:-1]:
            offsets.append(offsets[-1] + len(level))
        data += struct.pack("<16s6I", label, width, height, *offsets) + b"".join(mip)
        directory.append(struct.pack("<3IBBH16s", start, len(data) + 12 - start,
                                     len(data) + 12 - start, 68, 0, 0, label))
    path.write_bytes(struct.pack("<4s2I", b"WAD2", len(directory), len(data) + 12) + data + b"".join(directory))


def stone_skin(path, palette):
    rng = random.Random(262)
    size = 128
    colors = [palette_index(palette, (v + 11, v + 5, v - 4)) for v in range(37, 148)]
    sites = [(rng.randrange(size), rng.randrange(size), rng.randrange(-19, 17)) for _ in range(38)]
    pixels = bytearray()
    for y in range(size):
        for x in range(size):
            near = sorted((min(abs(x - a), size - abs(x - a)) ** 2 + (y - b) ** 2, v)
                          for a, b, v in sites)
            vein = near[1][0] - near[0][0] < 27
            strata = 11 * math.sin((y + 7 * math.sin(x * math.tau / size)) / 6.4)
            val = int(95 + near[0][1] + strata + rng.randrange(-10, 11) - 25 * vein)
            # One crooked quartz seam provides a fixed, readable rotation cue.
            if 17 < x < 86 and abs(y - (22 + 0.58 * x + 4 * math.sin(x / 8))) < 2.5:
                val += 45
            pixels.append(colors[max(0, min(len(colors) - 1, val - 37))])
    path.write_bytes(struct.pack("<2I", size, size) + pixels)


def brush(x0, x1, y0, y1, bottom, z0, z1, tex, top=None):
    # Quake map face points are clockwise when viewed from outside.
    a, b, c, d = (x0, y0, bottom), (x1, y0, bottom), (x1, y1, bottom), (x0, y1, bottom)
    e, f, g, h = (x0, y0, z0), (x1, y0, z0), (x1, y1, z1), (x0, y1, z1)
    faces = [(a, b, c), (e, h, g), (a, e, f), (b, f, g), (c, g, h), (d, h, e)]
    return "{\n" + "".join(" ".join(f"( {vec(p)} )" for p in face) +
                            f" {top if i == 1 and top else tex} 0 0 0 1 1\n"
                            for i, face in enumerate(faces)) + "}\n"


def box(x0, x1, y0, y1, z0, z1, tex):
    return brush(x0, x1, y0, y1, z0, z1, z1, tex)


def entity(classname, origin=None, brushes=(), **keys):
    if origin is not None:
        keys["origin"] = vec(origin)
    return ('{\n"classname" "' + classname + '"\n' +
            "".join(f'"{k}" "{v}"\n' for k, v in keys.items()) + "".join(brushes) + "}\n")


def make_map():
    wall, floor, portal = "range_wall", "range_floor", "range_portal"
    top_y = 1536
    rise = 887
    height = lambda y: rise * y / top_y
    world = [
        box(-544, 544, -448, 0, -64, 0, floor),
        box(-544, 544, top_y, 1824, -64, rise, floor),
        box(-544, -512, -448, 1824, -64, 1200, wall),
        box(512, 544, -448, 1824, -64, 1200, wall),
        box(-512, 512, -448, -416, -64, 1200, wall),
        box(-512, 512, 1792, 1824, -64, 1200, wall),
        box(-512, 512, -416, 1792, 1168, 1200, wall),
        brush(-512, 512, 0, top_y, -64, 0, rise, "range_concrete", "range_ramp"),
    ]
    # Portal arch and sealed destination room above and behind the range.
    world += [
        box(-144, -64, 1712, 1760, rise, rise + 140, wall),
        box(64, 144, 1712, 1760, rise, rise + 140, wall),
        box(-64, 64, 1712, 1760, rise + 104, rise + 140, portal),
        box(-64, 64, 1744, 1760, rise, rise + 104, portal),
        box(-160, 160, 1840, 2080, 920, 952, floor),
        box(-160, 160, 1840, 2080, 1104, 1136, wall),
        box(-160, -128, 1840, 2080, 952, 1104, wall),
        box(128, 160, 1840, 2080, 952, 1104, wall),
        box(-128, 128, 1840, 1872, 952, 1104, portal),
        box(-128, 128, 2048, 2080, 952, 1104, portal),
    ]
    result = [entity("worldspawn", brushes=world, wad="range.wad", message="JOLT RANGE / ROCKFALL",
                     light=28, no_sgprojectile=0, give_weapons=50,
                     ammo_shells=55, ammo_rockets=26, armortype=2, armorvalue=150)]
    result.append(entity("info_player_start", (0, -328, 40), angle=90))
    for cls, x, y in (("item_rockets", -80, -188), ("item_rockets", 80, -188),
                      ("item_shells", 0, -188), ("item_health", 128, -188)):
        result.append(entity(cls, (x, y, 14), spawnflags=1 if cls == "item_rockets" else 0))
    # Tougher grunts let a blast demonstrate both non-gib deaths and knockdown.
    for i, (x, y, hp) in enumerate(((0, -32, 100), (-270, 190, 100), (258, 388, 110),
                                   (-230, 650, 100), (272, 905, 95), (-270, 1190, 110),
                                   (222, 1408, 90), (200, 1608, 105))):
        result.append(entity("monster_army", (x, y, height(max(0, min(y, top_y))) + 42),
                             angle=270, health=hp, targetname=f"grunt{i + 1}", spawnflags=1))
    for i, (y, x, spawn_y) in enumerate(CHECKPOINTS):
        target = f"rockfall{i + 1}"
        result.append(entity("trigger_once", brushes=[box(-512, 512, y, y + STRIPE_WIDTH,
                            height(y) - 16, height(y) + 96, "trigger")], target=target, sounds=4))
        result.append(entity("range_boulder_spawner", (x, spawn_y, height(spawn_y) + 74),
                             targetname=target, angle=47 + i * 69))
        result.append(entity("light", (x, spawn_y, height(spawn_y) + 135), light=250, _color="1 .66 .36"))
        for side in (-1, 1):
            result.append(entity("light", (side * 400, y, height(y) + 95), light=360, _color=".56 .72 .83"))
    for y, z in ((-340, 94), (-180, 130), (50, 144), (350, 330), (700, 530),
                 (1030, 725), (1360, 935), (1600, 1020)):
        result.append(entity("light", (0, y, z), light=420, _color=".86 .93 1"))
    result += [
        entity("trigger_teleport", brushes=[box(-64, 64, 1704, 1744, rise, rise + 102, "trigger")],
               target="range_landing", spawnflags=1),
        entity("light", (0, 1680, rise + 62), light=360, _color=".31 1 .85"),
        entity("info_teleport_destination", (0, 1944, 952), angle=90, targetname="range_landing"),
        entity("light", (0, 1970, 1040), light=270, _color=".4 1 .73"),
        entity("trigger_changelevel", brushes=[box(-104, 104, 1890, 2030, 952, 1060, "trigger")],
               map=MAP, spawnflags=2),
        entity("info_intermission", (0, 1710, 983), mangle="18 270 0"),
    ]
    return "".join(result)


def run(command, cwd, log):
    with log.open("w") as stream:
        result = subprocess.run([str(x) for x in command], cwd=cwd, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=90)
    if result.returncode:
        raise ValueError(f"{command[0]} failed; see {log}")


def stage(args):
    out = args.out.resolve()
    quake, ad = args.quake.resolve(), args.ad.resolve()
    if any(out == src or src in out.parents or out in src.parents for src in (quake, ad)):
        raise ValueError("Choose an output folder separate from the Quake/AD installs")
    if out.exists() and any(out.iterdir()):
        raise ValueError(f"Use an empty output folder: {out}")
    sources = {}
    soldier = None
    for name in ("pak0.pak", AD_PATCH):
        for member, content in pak_files(ad / name, lambda p: p.startswith("my_progs/") or p == "progs/mon_soldier.mdl"):
            if member == "progs/mon_soldier.mdl":
                soldier = content
            else:
                leaf = member.removeprefix("my_progs/")
                if "/" not in leaf and leaf.endswith((".qc", ".src", ".qh")):
                    sources[leaf] = content
    if not soldier:
        raise ValueError("AD soldier model not found in the supplied Arcane Dimensions PAKs")
    sources_before_patch = sources.copy() if args.smoke_server else None
    patch_source(sources)
    palette_bytes = dict(pak_files(quake / "id1/pak0.pak", lambda p: p == "gfx/palette.lmp"))["gfx/palette.lmp"]
    palette = list(struct.iter_unpack("3B", palette_bytes))

    mod = out / "jolt_range"
    work = out / "source"
    qc = work / "qc"
    for d in (qc, mod / "maps", mod / "progs", out / "id1", out / "ad"):
        d.mkdir(parents=True, exist_ok=True)
    for name in ("pak0.pak", "pak1.pak"):
        source = quake / "id1" / name
        if not source.is_file():
            raise ValueError(f"Full Quake data required: {source}")
        (out / "id1" / name).symlink_to(source)
    for i in range(3):
        name = f"pak{i}.pak"
        (out / "ad" / name).symlink_to(ad / name)
    for name in ("quake.rc", "music"):
        if (ad / name).exists():
            (out / "ad" / name).symlink_to(ad / name)
    for name, data in sources.items():
        (qc / name).write_bytes(data)
    for name in ("range.qh", "range.qc"):
        shutil.copyfile(HERE / name, qc / name)
    (mod / "progs/range_boulder.md5mesh").write_text(
        rock_model(42, 64, mod / "progs/range_boulder.jolt"))
    (mod / "progs/range_chip.md5mesh").write_text(rock_model(8, 15))
    (qc / "range_grunt.qh").write_text(rig_source(grunt_model(soldier, mod / "progs")))
    stone_skin(mod / "progs/range_stone_00_00.lmp", palette)
    wad_textures(work / "range.wad", palette)
    (work / f"{MAP}.map").write_text(make_map())
    shutil.copyfile(HERE / "jolt_range.cfg", mod / "jolt_range.cfg")
    run([args.qcc.resolve()], qc, work / "qcc.log")
    shutil.move(work / "jolt_range.dat", mod / "jolt_range.dat")
    if args.smoke_server:
        smoke_qc = work / "smoke-qc"
        shutil.copytree(qc, smoke_qc)
        patch_source(sources_before_patch, smoke=True)
        for name, data in sources_before_patch.items():
            (smoke_qc / name).write_bytes(data)
        with (smoke_qc / "range.qh").open("a") as header:
            header.write("void() range_smoke_tick;\n")
        run([args.qcc.resolve()], smoke_qc, work / "qcc-smoke.log")
        shutil.move(work / "jolt_range_smoke.dat", mod / "jolt_range_smoke.dat")
    bsp = work / f"{MAP}.bsp"
    run([args.qbsp.resolve(), "-wadpath", work, work / f"{MAP}.map", bsp], work, work / "qbsp.log")
    if args.vis:
        run([args.vis.resolve(), "-fast", bsp], work, work / "vis.log")
    run([args.light.resolve(), "-extra", bsp], work, work / "light.log")
    for extension in (".bsp", ".lit"):
        path = bsp.with_suffix(extension)
        if path.is_file():
            shutil.copyfile(path, mod / "maps" / path.name)
    if args.engine:
        write_launcher(out, args.engine.resolve())
    print(f"Built {mod / 'maps' / bsp.name}")
    if args.smoke_server:
        run_smoke(out, args.smoke_server.resolve())


def write_launcher(out, engine):
    path = out / "Play Jolt Range.command"
    path.write_text(
        '#!/bin/sh\nset -eu\n'
        'range_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"\n'
        f'cd {shlex.quote(str(engine.parent))}\n'
        f'exec {shlex.quote(str(engine))} -nohome -quake -basedir "$range_dir" '
        '-game ad -game jolt_range +set plug_loaddefault 0 +exec jolt_range.cfg "$@"\n'
    )
    path.chmod(0o755)


def run_smoke(out, server):
    log = out / "smoke.log"
    run([server, "-nostdin", "-nohome", "-basedir", out, "-quake", "-game", "ad", "-game", "jolt_range",
         "+set", "plug_loaddefault", "0", "+deathmatch", "0", "+coop", "0", "+sv_public", "0",
         "+set", "sv_port", "0", "+set", "sv_listen_nq", "0", "+set", "sv_listen_qw", "0",
         "+developer", "1", "+plug_load", "jolt", "+physics_jolt_enabled", "1",
         "+set", "sv_mintic", ".008333333", "+set", "sv_maxtic", ".008333333",
         "+sv_progs", "jolt_range_smoke.dat", "+map", MAP, "+defer", "18", "quit", "force"], server.parent, log)
    output = log.read_text(errors="replace")
    if ("RANGE_SMOKE_OK" not in output or any(flag in output for flag in
            ("RANGE_SMOKE_FAIL", "Jolt simulation stopped", "Jolt: rejected", "Host_Error", "runtime error:"))):
        raise ValueError(f"Jolt Range smoke test failed; see {log}")
    print(f"Smoke test passed: {log}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("quake", "ad", "qcc", "qbsp", "light", "out"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--vis", type=Path)
    parser.add_argument("--engine", type=Path, help="Write a launcher for this Jolt-enabled FTE client")
    parser.add_argument("--smoke-server", type=Path, help="Compile and run the smoke test with this FTE dedicated server")
    args = parser.parse_args()
    try:
        stage(args)
    except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as exc:
        parser.exit(1, f"{exc}\n")


if __name__ == "__main__":
    main()
