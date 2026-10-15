#!/usr/bin/env python3
"""Exercise optional physics plugins through real SSQC and an original synthetic BSP.
No Quake assets, external servers, or player movement changes are needed.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def bsp():
    lumps = [b''] * 15
    lumps[0] = b'{"classname" "worldspawn" "message" "Physics fixture"}\n\0'
    lumps[1] = b''.join(struct.pack('<4fi',0,0,1,z,2) for z in [0,24,24,24])
    mip = struct.pack('<16s6I', b'fixture', 16, 16, 40, 296, 360, 376) + bytes([80])*340
    lumps[2] = struct.pack('<2i', 1, 8) + mip
    lumps[3] = b''.join(struct.pack('<3f', *v) for v in
                        [(-1024,-1024,0),(-1024,1024,0),(1024,1024,0),(1024,-1024,0)])
    bounds = (-2048,-2048,-64,2048,2048,2048)
    lumps[5] = struct.pack('<i2h6h2H', 0, -2, -1, *bounds, 0, 1)
    lumps[6] = struct.pack('<8f2i', 1,0,0,0,0,1,0,0,0,0)
    lumps[7] = struct.pack('<Hhihh4Bi', 0,0,0,4,0,0,255,255,255,-1)
    lumps[9] = b''.join(struct.pack('<i2h',i,-1,-2) for i in range(4))
    lumps[10] = b''.join(struct.pack('<ii6h2H4B', contents,-1,*bounds,0,count,0,0,0,0)
                          for contents,count in [(-2,0),(-1,1)])
    lumps[11] = struct.pack('<H', 0)
    lumps[12] = b''.join(struct.pack('<2H', *e) for e in [(0,0),(0,1),(1,2),(2,3),(3,0)])
    lumps[13] = struct.pack('<4i', 1,2,3,4)
    lumps[14] = struct.pack('<9f7i', -1024,-1024,-64,1024,1024,2048,0,0,0,0,1,2,0,1,0,1)
    payload, directory = bytearray(), bytearray()
    for data in lumps:
        payload.extend(bytes((-len(payload)) % 4))
        directory.extend(struct.pack('<2i', 124+len(payload),len(data)))
        payload.extend(data)
    return struct.pack('<i',29)+directory+payload


def write_rig(game):
    """Original three-bone MD5 mesh, weights and rigs; no commercial assets."""
    models=game/'progs'
    models.mkdir(exist_ok=True)
    lines=['MD5Version 10','commandline ""','numJoints 3','numMeshes 1','joints {']
    for i,z in enumerate([64,32,0]):
        lines.append(f'"bone{i}" {i-1} ( 0 0 {z} ) ( 0 0 0 )')
    lines+=['}','mesh {','shader "textures/physics/rig"','numverts 24']
    corners=[(-4,-4,-10),(4,-4,-10),(4,4,-10),(-4,4,-10),(-4,-4,10),(4,-4,10),(4,4,10),(-4,4,10)]
    triangles=[(0,2,1),(0,3,2),(4,5,6),(4,6,7),(0,1,5),(0,5,4),(3,7,6),(3,6,2),(0,4,7),(0,7,3),(1,2,6),(1,6,5)]
    for i in range(24):lines.append(f'vert {i} ( {i%2} {(i//2)%2} ) {i} 1')
    lines+=['numtris 36']
    for bone in range(3):
        for i,t in enumerate(triangles):lines.append(f'tri {bone*12+i} '+ ' '.join(str(v+bone*8) for v in t))
    lines+=['numweights 24']
    for bone in range(3):
        for i,v in enumerate(corners):lines.append(f'weight {bone*8+i} {bone} 1 ( {v[0]} {v[1]} {v[2]} )')
    lines+=['}']
    (models/'physics-rig.md5mesh').write_text('\n'.join(lines)+'\n')
    doll='refpose skin\nupdatebody default\nshape box\ndimensions 8 8 20\nmass 1\nanimate 0\n'
    doll+='body top bone0\nbody mid bone1\nbody tip bone2\n'
    for i,(a,b) in enumerate([('top','mid'),('mid','tip')]):
        doll+=f'joint link{i} {a} {b}\ntype hinge\naxis 0 1 0\nLoStop -0.5\nHiStop 0.5\nCFM 0\n'
    (models/'physics-rig.doll').write_text(doll)
    invalid=doll.replace('body mid bone1\n','body mid bone1\nmass -1\n')
    (models/'physics-rig-invalid.doll').write_text(invalid)
    (models/'physics-rig-unsupported.doll').write_text(doll.replace('type hinge','type universal',1))
    textures=game/'textures/physics';textures.mkdir(parents=True,exist_ok=True)
    # An original checker gives the skeletal test mesh a self-contained skin.
    header=struct.pack('<BBBHHBHHHHBB',0,0,2,0,0,0,0,0,16,16,32,0x28)
    pixels=b''.join(bytes((40,180 if (x//4+y//4)%2 else 100,240,255)) for y in range(16) for x in range(16))
    (textures/'rig.tga').write_bytes(header+pixels)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server', type=Path, required=True)
    p.add_argument('--qcc', type=Path, required=True)
    p.add_argument('--jolt-plugin', type=Path, required=True)
    p.add_argument('--ode-plugin', type=Path)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--only-jolt', action='store_true', help='Run only the enabled-Jolt integration fixture')
    args=p.parse_args()
    root=args.out.resolve()
    root.mkdir(parents=True,exist_ok=True)
    # Fresh child for every invocation: CTest can repeat without replacing any
    # existing configs or game data. Keep logs from every run for diagnosis.
    out=Path(tempfile.mkdtemp(prefix='run-',dir=root))
    game=out/'id1'
    (game/'maps').mkdir(parents=True)
    (game/'maps/physics.bsp').write_bytes(bsp())
    write_rig(game)
    shutil.copyfile(ROOT/'tools/tests/physics_fixture.qc',game/'fixture.qc')
    env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    with (out/'compile.log').open('w') as log:
        subprocess.run([str(args.qcc.resolve()),'-srcfile','fixture.qc','-o','progs.dat'],cwd=game,env=env,
                       stdout=log,stderr=subprocess.STDOUT,check=True,timeout=30)
    plugins=[('no-plugin',None,-1),('disabled',args.jolt_plugin,0),('jolt',args.jolt_plugin,1)]
    if args.ode_plugin: plugins.append(('ode',args.ode_plugin,2))
    if args.only_jolt: plugins=[('jolt',args.jolt_plugin,1)]
    results={}
    for name,plugin,mode in plugins:
        if plugin:
            shutil.copyfile(plugin.resolve(),out/plugin.name)
        backend='ode' if mode==2 else 'jolt'
        load=f'plug_load {backend}' if plugin else 'echo PHYSICS_NO_PLUGIN'
        close=f'plug_close {backend}' if plugin else 'echo PHYSICS_NO_PLUGIN'
        # Plain stdio is required when Windows stdout is redirected to a file.
        cfg=f'''sys_colorconsole 0
sv_public 0
sv_port 0
sv_mintic 0.008333333
sv_maxtic 0.008333333
sv_gravity 800
mod_md5_singleanimation 0
developer 1
sv_listen_nq 0
sv_listen_qw 0
plug_loaddefault 0
{load}
set physics_jolt_enabled {int(mode==1)}
set physics_fixture_mode {max(mode,0)}
alias physics_fixture_done "quit force"
alias physics_fixture_cycle2 "alias physics_fixture_next physics_fixture_done; {close}; {load}; map physics"
alias physics_fixture_cycle1 "alias physics_fixture_next physics_fixture_cycle2; map physics"
alias physics_fixture_next physics_fixture_cycle1
map physics
'''
        (game/'physics.cfg').write_text(cfg)
        logpath=out/f'{name}.log'
        with logpath.open('w') as log:
            proc=subprocess.run([str(args.server.resolve()),'-basedir',str(out),'-nohome','-nostdin',
                                 '-quake','+set','plug_loaddefault','0','+exec','physics.cfg'],cwd=out,env=env,
                                stdout=log,stderr=subprocess.STDOUT,timeout=25)
        text=logpath.read_text(errors='replace')
        if proc.returncode or text.count('PHYSICS_ENGINE_OK')!=3 or re.search(
                r'PHYSICS_FAIL|AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|Jolt entity .* rejected|Jolt simulation stopped',text):
            raise AssertionError(f'{name}: exit={proc.returncode}; engine checks failed; inspect {logpath}\n{text[-7000:]}')
        if mode==1 and text.count('PHYSICS_RAGDOLL_OK')!=3:
            raise AssertionError(f'Missing ragdoll checks: {logpath}')
        heights=[float(x) for x in re.findall(r'PHYSICS_CLASSIC_Z ([\d.-]+)',text)]
        results[name]={'map_runs':3,'classic_z':heights,
                       'drop_z':[float(x) for x in re.findall(r'PHYSICS_DROP_Z ([\d.-]+)',text)]}
    reference=results.get('no-plugin',results['jolt'])['classic_z']
    for name,result in results.items():
        if result['classic_z']!=reference:
            raise AssertionError(f'Classic toss result changed with {name}')
    (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(f'Physics logs: {out}')
    print('PHYSICS_INTEGRATION_OK: opt-in, BSP collision, QC forces/touches/removal, sleep, restart/reload; classic baseline unchanged')


if __name__=='__main__':
    main()
