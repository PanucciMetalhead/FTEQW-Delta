#!/usr/bin/env python3
"""Prepare/run an isolated, interactive Jolt skeletal demo using owned UI assets.
Original PAKs are linked read-only; the mesh, rig, map and gamecode are original.
"""
import argparse
import json
import re
import struct
from pathlib import Path
import shutil
import subprocess
import tempfile
from physics_integration import ROOT,bsp,write_rig


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--quake-data',type=Path,required=True,help='Quake folder containing id1/pak0.pak')
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--engine',type=Path,default=ROOT/'build/jolt/fteqw')
    p.add_argument('--qcc',type=Path,default=ROOT/'build/jolt/fteqcc')
    p.add_argument('--plugin',type=Path,default=ROOT/'build/jolt/fteplug_jolt.so')
    p.add_argument('--run',action='store_true')
    p.add_argument('--capture',action='store_true',help='Capture the native renderer, restart the map, and quit automatically')
    args=p.parse_args()
    source=args.quake_data.resolve()/'id1'
    if not (source/'pak0.pak').is_file():p.error('Quake data must contain id1/pak0.pak')
    root=args.out.resolve()
    if root==source or source in root.parents or root in source.parents:p.error('Output must be separate from game data')
    root.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='demo-',dir=root));game=out/'id1';(game/'maps').mkdir(parents=True)
    for name in ('pak0.pak','pak1.pak'):
        if (source/name).is_file():(game/name).symlink_to(source/name)
    (game/'maps/physics.bsp').write_bytes(bsp());write_rig(game)
    shutil.copyfile(ROOT/'tools/tests/physics_fixture.qc',game/'fixture.qc')
    with (out/'compile.log').open('w') as log:
        subprocess.run([str(args.qcc.resolve()),'-srcfile','fixture.qc','-o','physics-demo.dat'],cwd=game,
                       stdout=log,stderr=subprocess.STDOUT,check=True,timeout=30)
    shutil.copyfile(args.plugin.resolve(),out/args.plugin.name)
    config='''sv_public 0
cfg_save_auto 0
plug_loaddefault 0
plug_load jolt
physics_jolt_enabled 1
set physics_fixture_mode 1
set physics_fixture_demo 1
set physics_fixture_joint 1
sv_progs physics-demo.dat
mod_md5_singleanimation 0
r_fullbright 1
r_novis 1
viewsize 120
con_notifytime 0
vid_fullscreen 0
bind w +forward
bind s +back
bind a +moveleft
bind d +moveright
bind MOUSE1 +attack
bind F5 "map physics"
bind F6 "toggle physics_fixture_joint"
map physics
echo "Jolt demo: fly with WASD/mouse; F5 resets the drop; F6 toggles the top joint."
'''
    if args.capture:
        # Start timers from the QC client-spawn callback, after initial loading.
        config='set physics_fixture_capture 1\nset physics_fixture_capture_started 0\n'+config
    (game/'physics-demo.cfg').write_text(config)
    command=[str(args.engine.resolve()),'-basedir',str(out),'-nohome','-quake','-window','-width','960','-height','720',
             '-gpu_debug','+set','plug_loaddefault','0','+exec','physics-demo.cfg']
    (out/'launch.json').write_text(json.dumps(command,indent=2)+'\n')
    print(f'Demo directory: {out}',flush=True)
    print('Launch: '+shlex_join(command),flush=True)
    if args.run:
        with (out/'demo.log').open('w') as log:
            subprocess.run(command,cwd=out,stdout=log,stderr=subprocess.STDOUT,check=True,
                           timeout=45 if args.capture else None)
        print(f'Log: {out / "demo.log"}',flush=True)
        if args.capture:
            log=(out/'demo.log').read_text(errors='replace')
            if re.search(r'PHYSICS_FAIL|AddressSanitizer:|UndefinedBehaviorSanitizer:|runtime error:|Host_Error|Sys_Error',log):
                raise RuntimeError(f'Engine/sanitizer failure: {out / "demo.log"}')
            if log.count('JOLT_DEMO_CLIENT_READY')<2 or log.count('rigid-body backend initialized')<2:
                raise RuntimeError('The client did not connect to both Jolt map instances')
            frames={}
            for name in ('connected','settled','released','reloaded'):
                frame=out/'fte'/f'jolt-rig-{name}.png'
                data=frame.read_bytes()
                if data[:8]!=b'\x89PNG\r\n\x1a\n' or len(data)<1024:
                    raise RuntimeError(f'Invalid screenshot: {frame}')
                frames[name]={'path':str(frame),'size':struct.unpack('>II',data[16:24])}
            (out/'results.json').write_text(json.dumps({'status':'passed','screenshots':frames},indent=2)+'\n')
            print(f'Capture/reload checks passed: {out / "results.json"}',flush=True)


def shlex_join(command):
    import shlex
    return shlex.join(command)


if __name__=='__main__':main()
