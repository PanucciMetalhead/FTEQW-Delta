#!/usr/bin/env python3
"""Original convex/compound models and recipes exercised through real SSQC."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
from physics_integration import ROOT,bsp,write_rig

TRIANGLES=[(0,2,1),(0,3,2),(4,5,6),(4,6,7),(0,1,5),(0,5,4),
           (3,7,6),(3,6,2),(0,4,7),(0,7,3),(1,2,6),(1,6,5)]

def box(center,size):
    return [(center[0]+x*size[0]/2,center[1]+y*size[1]/2,center[2]+z*size[2]/2)
            for x,y,z in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]

def recipe(parts):
    return 'ftejolt 1\n'+''.join('hull {\n'+''.join(' '.join(map(str,v))+'\n' for v in part)+'}\n' for part in parts)

def model(parts):
    vertices=[v for part in parts for v in part]
    lines=['MD5Version 10','commandline ""','numJoints 1','numMeshes 1','joints {','"origin" -1 ( 0 0 0 ) ( 0 0 0 )','}',
           'mesh {','shader "textures/physics/rig"',f'numverts {len(vertices)}']
    lines += [f'vert {i} ( {i%2} {(i//2)%2} ) {i} 1' for i in range(len(vertices))]
    lines += [f'numtris {12*len(parts)}']
    for part in range(len(parts)):
        lines += [f'tri {part*12+i} '+ ' '.join(str(part*8+j) for j in t) for i,t in enumerate(TRIANGLES)]
    lines += [f'numweights {len(vertices)}']
    lines += [f'weight {i} 0 1 ( '+ ' '.join(map(str,v))+' )' for i,v in enumerate(vertices)]
    return '\n'.join(lines+['}'])+'\n'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('server','qcc','plugin','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--client-host',action='store_true',help='Use a client executable in dedicated mode to check its model path too')
    args=p.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='shapes-',dir=args.out.resolve()));game=out/'id1'
    (game/'maps').mkdir(parents=True);(game/'maps/shapes.bsp').write_bytes(bsp());write_rig(game)
    cube=[box((0,0,0),(32,32,32))]
    frame=[box((-24,0,0),(8,16,64)),box((24,0,0),(8,16,64)),box((0,0,28),(56,16,8))]
    (game/'progs/cube.md5mesh').write_text(model(cube));(game/'progs/frame.md5mesh').write_text(model(frame))
    (game/'progs/cube.jolt').write_text(recipe(cube));(game/'progs/frame.jolt').write_text(recipe(frame))
    (game/'progs/tall.jolt').write_text(recipe([box((0,0,0),(32,32,64))]))
    (game/'progs/bad.jolt').write_text('ftejolt 1 hull { nope }')
    (game/'progs/flat.jolt').write_text(recipe([box((0,0,0),(32,32,0))]))
    (game/'progs/large.jolt').write_bytes(b' '*262145)
    declarations=(ROOT/'tools/tests/physics_fixture.qc').read_text().split('float simulated,phase')[0]
    (game/'declarations.qh').write_text(declarations)
    shutil.copyfile(ROOT/'tools/tests/physics_shapes.qc',game/'fixture.qc')
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    with (out/'compile.log').open('w') as log:
        subprocess.run([str(args.qcc.resolve()),'-srcfile','fixture.qc','-o','progs.dat'],cwd=game,
                       stdout=log,stderr=subprocess.STDOUT,env=env,check=True,timeout=30)
    shutil.copyfile(args.plugin.resolve(),out/args.plugin.name)
    cfg='''sys_colorconsole 0
sv_public 0
sv_port 0
sv_mintic 0.008333333
sv_maxtic 0.008333333
sv_gravity 800
sv_listen_nq 0
sv_listen_qw 0
plug_loaddefault 0
developer 1
plug_load jolt
physics_jolt_enabled 1
alias shapes_done "quit force"
alias shapes_cycle2 "alias shapes_next shapes_done;plug_close jolt;plug_load jolt;map shapes"
alias shapes_cycle1 "alias shapes_next shapes_cycle2;map shapes"
alias shapes_next shapes_cycle1
map shapes
'''
    if args.client_host:cfg='deathmatch 1\n'+cfg  # Avoid listen-host single-player auto-pause with no clients.
    (game/'shapes.cfg').write_text(cfg);logpath=out/'shapes.log'
    with logpath.open('w') as log:
        subprocess.run([str(args.server.resolve()),*(['-dedicated'] if args.client_host else []),'-basedir',str(out),'-quake','-nohome','-nostdin',
                        '+set','plug_loaddefault','0','+exec','shapes.cfg'],cwd=out,env=env,
                       stdout=log,stderr=subprocess.STDOUT,check=True,timeout=90)
    text=logpath.read_text(errors='replace')
    if text.count('SHAPES_ENGINE_OK')!=3 or re.search(r'SHAPES_FAIL|AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|Jolt simulation stopped|Host_Error|Sys_Error',text):
        raise AssertionError(f'{logpath}\n{text[-7000:]}')
    errors=re.findall(r'Jolt entity \d+ rejected: ([^\n]+)',text)
    expected=['relative progs/','not found','Invalid compound vertex','at least 0.25','1..262144','static/kinematic','Unsupported shape']
    assert len(errors)==21 and all(sum(reason in error for error in errors)==3 for reason in expected),errors
    (out/'results.json').write_text(json.dumps({'status':'passed','map_runs':3,'expected_rejections':len(errors)},indent=2)+'\n')
    print('Shape engine checks passed: '+str(out/'results.json'))

if __name__=='__main__':main()
