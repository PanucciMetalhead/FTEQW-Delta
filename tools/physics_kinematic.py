#!/usr/bin/env python3
"""Data-free, real-SSQC brush-pusher tests; never modifies an installed game."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
from physics_integration import ROOT, bsp, write_rig


def brush_map():
    """Add an original six-sided inline brush, with BSP and expanded clip hulls."""
    source=bsp()
    lumps=[source[offset:offset+size] for offset,size in
           (struct.unpack_from('<2i',source,4+i*8) for i in range(15))]
    bounds=(-64,-64,-8,64,64,8)
    # Positive axial planes: even faces are maximums, odd faces minimums.
    def planes(extents):
        first=len(lumps[1])//20
        for axis in range(3):
            normal=[0,0,0];normal[axis]=1
            for distance in (extents[axis+3],extents[axis]):
                lumps[1]+=struct.pack('<4fi',*normal,distance,axis)
        return first
    plane=planes(bounds)
    firstnode=len(lumps[5])//24
    for i in range(6):
        inside=firstnode+i+1 if i<5 else -1
        children=(-2,inside) if i%2==0 else (inside,-2)
        lumps[5]+=struct.pack('<i2h6h2H',plane+i,*children,*bounds,0,0)
    heads=[firstnode]
    for lo,hi in [((-16,-16,-24),(16,16,32)),((-32,-32,-24),(32,32,64)),((-16,-16,-12),(16,16,16))]:
        extents=tuple(bounds[i]-hi[i] for i in range(3))+tuple(bounds[i+3]-lo[i] for i in range(3))
        hullplane=planes(extents);head=len(lumps[9])//8;heads.append(head)
        for i in range(6):
            inside=head+i+1 if i<5 else -2
            children=(-1,inside) if i%2==0 else (inside,-1)
            lumps[9]+=struct.pack('<i2h',hullplane+i,*children)
    # Four vertices per face; clockwise when looking from outside, matching BSP mesh extraction.
    faces=[[(64,-64,-8),(64,-64,8),(64,64,8),(64,64,-8)],
           [(-64,64,-8),(-64,64,8),(-64,-64,8),(-64,-64,-8)],
           [(64,64,-8),(64,64,8),(-64,64,8),(-64,64,-8)],
           [(-64,-64,-8),(-64,-64,8),(64,-64,8),(64,-64,-8)],
           [(-64,-64,8),(-64,64,8),(64,64,8),(64,-64,8)],
           [(-64,64,-8),(-64,-64,-8),(64,-64,-8),(64,64,-8)]]
    firstface=len(lumps[7])//20
    for i,vertices in enumerate(faces):
        vertex=len(lumps[3])//12;edge=len(lumps[12])//4;surfedge=len(lumps[13])//4
        lumps[3]+=b''.join(struct.pack('<3f',*v) for v in vertices)
        lumps[12]+=b''.join(struct.pack('<2H',vertex+j,vertex+(j+1)%4) for j in range(4))
        lumps[13]+=struct.pack('<4i',*(edge+j for j in range(4)))
        lumps[7]+=struct.pack('<Hhihh4Bi',plane+i,i%2,surfedge,4,0,0,255,255,255,-1)
    lumps[14]+=struct.pack('<9f7i',*bounds,0,0,0,*heads,0,firstface,6)
    payload=bytearray();directory=bytearray()
    for data in lumps:
        payload.extend(bytes((-len(payload))%4))
        directory.extend(struct.pack('<2i',124+len(payload),len(data)));payload.extend(data)
    return struct.pack('<i',29)+directory+payload


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('server','qcc','plugin','out'):p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='kinematic-',dir=args.out.resolve()));game=out/'id1'
    (game/'maps').mkdir(parents=True);(game/'maps/kinematic.bsp').write_bytes(brush_map());write_rig(game)
    declarations=(ROOT/'tools/tests/physics_fixture.qc').read_text().split('float simulated,phase')[0]
    (game/'declarations.qh').write_text(declarations)
    shutil.copyfile(ROOT/'tools/tests/physics_kinematic.qc',game/'fixture.qc')
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    with (out/'compile.log').open('w') as log:
        subprocess.run([str(args.qcc.resolve()),'-srcfile','fixture.qc','-o','progs.dat'],cwd=game,
                       stdout=log,stderr=subprocess.STDOUT,env=env,check=True,timeout=30)
    shutil.copyfile(args.plugin.resolve(),out/args.plugin.name)
    results={}
    for name,on,rate in [('classic',0,120),('jolt',1,120),('jolt-60',1,60)]:
        cfg=f'''sys_colorconsole 0
sv_public 0
sv_port 0
sv_mintic {1/rate:.12f}
sv_maxtic {1/rate:.12f}
sv_gravity 800
sv_listen_nq 0
sv_listen_qw 0
plug_loaddefault 0
developer 1
set kinematic_test {on}
'''
        if on:cfg+='plug_load jolt\nphysics_jolt_enabled 1\n'
        cfg+='alias kinematic_done "quit force"\n'
        reload='plug_close jolt;plug_load jolt;' if on else ''
        cfg+=f'alias kinematic_cycle2 "alias kinematic_next kinematic_done;{reload}map kinematic"\n'
        cfg+='alias kinematic_cycle1 "alias kinematic_next kinematic_cycle2;map kinematic"\n'
        cfg+='alias kinematic_next kinematic_cycle1\nmap kinematic\n'
        (game/'kinematic.cfg').write_text(cfg);logpath=out/(name+'.log')
        with logpath.open('w') as log:
            subprocess.run([str(args.server.resolve()),'-basedir',str(out),'-quake','-nohome','-nostdin',
                            '+set','plug_loaddefault','0','+exec','kinematic.cfg'],cwd=out,env=env,
                           stdout=log,stderr=subprocess.STDOUT,check=True,timeout=75)
        text=logpath.read_text(errors='replace')
        if text.count('KINEMATIC_ENGINE_OK')!=3 or re.search(r'KINEMATIC_FAIL|AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|Jolt .* rejected|Jolt simulation stopped|Host_Error|Sys_Error',text):
            raise AssertionError(f'{name}: {logpath}\n{text[-6000:]}')
        results[name]={'classic_z':[float(x) for x in re.findall(r'KINEMATIC_CLASSIC_Z ([\d.-]+)',text)],'map_runs':3}
    for name in ('jolt','jolt-60'):
        assert all(abs(a-b)<1 for a,b in zip(results['classic']['classic_z'],results[name]['classic_z'])),results
    (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('Kinematic engine checks passed: '+str(out/'results.json'))

if __name__=='__main__':main()
