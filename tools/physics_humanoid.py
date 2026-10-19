#!/usr/bin/env python3
"""Prepare/run humanoid Jolt rig checks, optionally with an owned remaster soldier.
Without --remaster-data all mesh/texture/rig/map/gamecode assets are original.
Game installations are read-only; every run uses a fresh isolated output child.
"""
import argparse
import hashlib
import socket
import time
import json
import math
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import subprocess
import tempfile
from physics_integration import ROOT, bsp, write_rig
from gpu_remaster import Pak

# Original test mannequin, with a humanoid hierarchy independent of game data.
BONES = [
    ('Root',-1,(0,0,0)),('Character',0,(0,0,0)),('Hip',1,(0,0,2)),
    ('UpperLeg_L',2,(-4,0,0)),('LowerLeg_L',3,(-4,0,-12)),('Foot_L',4,(-4,2,-24)),
    ('UpperLeg_R',2,(4,0,0)),('LowerLeg_R',6,(4,0,-12)),('Foot_R',7,(4,2,-24)),
    ('Spine1',2,(0,0,7)),('Spine2',9,(0,0,15)),('Neck',10,(0,0,23)),('Head',11,(0,0,28)),
    ('Shoulder_L',10,(-4,0,21)),('UpperArm_L',13,(-8,0,20)),
    ('LowerArm_L',14,(-15,0,12)),('Hand_L',15,(-19,0,5)),
    ('Shoulder_R',10,(4,0,21)),('UpperArm_R',17,(8,0,20)),
    ('LowerArm_R',18,(15,0,12)),('Hand_R',19,(19,0,5)),
    ('flame',9,(0,0,10)),('Gun',0,(18,5,8)),('GunHandle',22,(18,5,7))]


def md5_mesh(parts):
    """Make original boxes, each rigidly weighted to one mannequin joint."""
    triangles=[(0,2,1),(0,3,2),(4,5,6),(4,6,7),(0,1,5),(0,5,4),
               (3,7,6),(3,6,2),(0,4,7),(0,7,3),(1,2,6),(1,6,5)]
    lines=['MD5Version 10','commandline ""',f'numJoints {len(BONES)}','numMeshes 1','joints {']
    for name,parent,position in BONES:
        lines.append(f'"{name}" {parent} ( '+ ' '.join(map(str,position))+' ) ( 0 0 0 )')
    count=8*len(parts)
    lines+=['}','mesh {','shader "textures/physics/rig"',f'numverts {count}']
    for i in range(count):lines.append(f'vert {i} ( {i%2} {(i//2)%2} ) {i} 1')
    lines.append(f'numtris {12*len(parts)}')
    for part in range(len(parts)):
        for i,t in enumerate(triangles):
            lines.append(f'tri {part*12+i} '+' '.join(str(v+part*8) for v in t))
    lines.append(f'numweights {count}')
    for part,(bone,center,size) in enumerate(parts):
        for i,(x,y,z) in enumerate([(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),
                                     (-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]):
            v=[center[k]+sign*size[k]/2 for k,sign in enumerate((x,y,z))]
            lines.append(f'weight {part*8+i} {bone} 1 ( '+' '.join(map(str,v))+' )')
    return '\n'.join(lines+['}'])+'\n'


def md5_animation():
    """Eight original mannequin poses with parent-relative transforms."""
    lines=['MD5Version 10','commandline ""','numFrames 8',f'numJoints {len(BONES)}',
           'frameRate 10',f'numAnimatedComponents {6*len(BONES)}','hierarchy {']
    for i,(name,parent,pos) in enumerate(BONES):lines.append(f'"{name}" {parent} 63 {6*i}')
    lines+=['}','bounds {']+['( -64 -64 -40 ) ( 64 64 64 )']*8+['}','baseframe {']
    relative=[]
    for name,parent,pos in BONES:
        local=tuple(pos[k]-(BONES[parent][2][k] if parent>=0 else 0) for k in range(3))
        relative.append(local)
        lines.append('( '+' '.join(map(str,local))+' ) ( 0 0 0 )')
    lines+=['}']
    for frame in range(8):
        lines.append(f'frame {frame} {{')
        phase=math.sin(2*math.pi*frame/8)
        for i,(name,parent,pos) in enumerate(BONES):
            x,y,z=relative[i];qx=qy=qz=0
            if name=='Hip':z+=.5*phase
            if name in ('UpperArm_L','UpperArm_R'):
                qx=math.sin(.18*phase)*(1 if name.endswith('L') else -1)
            lines.append(' '.join(map(str,(x,y,z,qx,qy,qz))))
        lines.append('}')
    return '\n'.join(lines)+'\n'


def joint_positions(mesh):
    block=re.search(r'\bjoints\s*\{(.*?)\}',mesh,re.S)[1]
    return {name:tuple(map(float,position.split())) for name,position in
            re.findall(r'"([^"]+)"\s+-?\d+\s*\(\s*([^)]*)\)',block)}


def make_doll(positions):
    lines=['// Original passive humanoid rig recipe; generated for this local skeleton.',
           'refpose skin','updatebody default','animate 0','mass 2','shape sphere','dimensions 2']
    def sphere(name,bone,radius,mass):
        lines.extend([f'body {name} {bone}','shape sphere',f'dimensions {radius}',f'mass {mass}'])
    def segment(name,bone,peer,radius,mass):
        length=math.dist(positions[bone],positions[peer])
        if length<=2*radius:raise ValueError(f'Rig segment {bone}/{peer} is too short')
        lines.extend([f'body {name} {bone}','shape capsule',f'orient {peer}',
                      f'dimensions {radius} {radius} {length-2*radius:.8f}',
                      f'offset 0 0 {length/2:.8f}',f'mass {mass}'])
    sphere('pelvis','Hip',3.5,8)
    segment('torso','Spine1','Neck',3.2,15)
    sphere('head','Head',3.5,4)
    for side in ('L','R'):
        segment(f'thigh_{side}',f'UpperLeg_{side}',f'LowerLeg_{side}',2.1,5)
        segment(f'shin_{side}',f'LowerLeg_{side}',f'Foot_{side}',1.7,3)
        sphere(f'foot_{side}',f'Foot_{side}',2.4,1)
        segment(f'arm_{side}',f'UpperArm_{side}',f'LowerArm_{side}',1.6,2)
        segment(f'forearm_{side}',f'LowerArm_{side}',f'Hand_{side}',1.3,1.5)
        sphere(f'hand_{side}',f'Hand_{side}',1.6,.5)
    lines+=['body gun Gun','shape box','dimensions 3 8 3','mass .5',
            'updatejoint default','type universal','axis 1 0 0','axis2 0 1 0',
            'CFM 0','CFM2 0','LoStop -.6','HiStop .6','LoStop2 -.6','HiStop2 .6']
    def joint(name,a,b,pivot,kind='universal',limit=.6,limit2=.6):
        lines.extend([f'joint {name} {a} {b}',f'type {kind}',f'pivot {pivot}',
                      f'LoStop {-limit}',f'HiStop {limit}',f'LoStop2 {-limit2}',f'HiStop2 {limit2}'])
    joint('waist','pelvis','torso','Spine1',limit=.35,limit2=.3)
    joint('neck','torso','head','Neck',limit=.5,limit2=.5)
    for side in ('L','R'):
        joint(f'hip_{side}','pelvis',f'thigh_{side}',f'UpperLeg_{side}',limit=1,limit2=.7)
        joint(f'knee_{side}',f'thigh_{side}',f'shin_{side}',f'LowerLeg_{side}','hinge',1.4)
        joint(f'ankle_{side}',f'shin_{side}',f'foot_{side}',f'Foot_{side}',limit=.4,limit2=.3)
        joint(f'shoulder_{side}','torso',f'arm_{side}',f'UpperArm_{side}',limit=1.2,limit2=1.1)
        joint(f'elbow_{side}',f'arm_{side}',f'forearm_{side}',f'LowerArm_{side}','hinge',1.4)
        joint(f'wrist_{side}',f'forearm_{side}',f'hand_{side}',f'Hand_{side}',limit=.4,limit2=.4)
    joint('grip','hand_R','gun','Hand_R','fixed')
    return '\n'.join(lines)+'\n'


def prepare(args):
    root=args.out.resolve()
    sources=[p.resolve() for p in (args.remaster_data,args.quake_data) if p]
    for source in sources:
        if root==source or source in root.parents or root in source.parents:
            raise ValueError('Output must be separate from owned game data')
    root.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='humanoid-',dir=root));game=out/'id1'
    (game/'maps').mkdir(parents=True)
    (game/'maps/physics.bsp').write_bytes(bsp());write_rig(game)
    model='progs/physics-human.md5mesh'
    positions={name:pos for name,parent,pos in BONES}
    if args.remaster_data:
        source=args.remaster_data.resolve()/'id1'
        pak=Pak(source/'pak0.pak');model='progs/soldier.md5mesh'
        positions=joint_positions(pak.read(model).decode())
    else:
        # Original block mannequin; no game model or animation is copied.
        parts=[]
        hidden={'Root','Character','Shoulder_L','Shoulder_R','flame','GunHandle'}
        for bone,(name,parent,pos) in enumerate(BONES):
            if name not in hidden:parts.append((bone,(0,0,0),(5,5,7)))
        (game/model).write_text(md5_mesh(parts))
        (game/'progs/physics-human.md5anim').write_text(md5_animation())
    source=(args.remaster_data or args.quake_data)
    if source:
        for name in ('pak0.pak','pak1.pak'):
            src=source.resolve()/'id1'/name
            if src.is_file():(game/name).symlink_to(src)
    (game/'progs/physics-human.doll').write_text(make_doll(positions))
    # Original visible collision boxes for stairs, slope and wall.
    for name,size in [('step0',(32,80,8)),('step1',(32,80,16)),('step2',(32,80,24)),
                      ('step3',(32,80,32)),('ramp',(160,96,8)),('wall',(8,96,96))]:
        mesh=md5_mesh([(0,(0,0,0),size)])
        (game/f'progs/physics-{name}.md5mesh').write_text(mesh)
    declarations=(ROOT/'tools/tests/physics_fixture.qc').read_text().split('float simulated,phase')[0]
    (game/'physics-declarations.qh').write_text(declarations)
    (game/'physics-model.qh').write_text(f'#define HUMAN_MODEL "{model}"\n#define HUMAN_BONES {len(positions)}\n')
    shutil.copyfile(ROOT/'tools/tests/physics_humanoid.qc',game/'humanoid.qc')
    with (out/'compile.log').open('w') as log:
        subprocess.run([str(args.qcc.resolve()),'-srcfile','humanoid.qc','-o','humanoid.dat'],cwd=game,
                       stdout=log,stderr=subprocess.STDOUT,check=True,timeout=30)
    shutil.copyfile(args.plugin.resolve(),out/args.plugin.name)
    cfg='''sys_colorconsole 0
sv_public 0
cfg_save_auto 0
plug_loaddefault 0
plug_load jolt
physics_jolt_enabled 1
sv_progs humanoid.dat
sv_mintic 0.008333333
sv_maxtic 0.008333333
sv_gravity 800
developer 1
mod_md5_singleanimation 1
r_fullbright 1
r_novis 1
viewsize 120
con_notifytime 0
vid_fullscreen 0
set physics_human_active 0
set physics_human_scenario 0
bind w +forward
bind s +back
bind a +moveleft
bind d +moveright
bind F5 "set physics_human_scenario 0; map physics"
bind F6 "toggle physics_human_active"
bind F7 "inc physics_human_scenario 1"
bind F8 "map physics"
'''
    if args.test:cfg+='sv_port 0\nsv_listen_nq 0\nsv_listen_qw 0\nset physics_human_test 1\n'
    if args.capture:cfg+='set physics_human_capture 1\n'
    cfg+='map physics\n'
    (game/'humanoid.cfg').write_text(cfg)
    return out


def network_test(args,out):
    """Two actual native clients, with no physics plugin loaded on either."""
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    game=out/'id1'
    cfg=(game/'humanoid.cfg').read_text().replace('map physics\n','')
    cfg+=f'sv_port 127.0.0.1:{port}\nsv_port_ipv6 ""\nsv_port_tcp ""\nmaxclients 2\nset physics_human_network 1\nmap physics\n'
    (game/'network-server.cfg').write_text(cfg)
    processes=[];logs=[]
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def start(binary,config,name,extra):
        log=(out/f'{name}.log').open('w');logs.append(log)
        proc=subprocess.Popen([str(binary.resolve()),'-basedir',str(out),'-nohome','-quake']+extra+
                              ['+set','plug_loaddefault','0','+exec',config],cwd=out,stdout=log,
                              stderr=subprocess.STDOUT,env=env)
        processes.append(proc);return proc
    try:
        server=start(args.server,'network-server.cfg','network-server',['-nostdin','-ip','127.0.0.1','-port',str(port)])
        deadline=time.monotonic()+20
        while 'Server spawned.' not in (out/'network-server.log').read_text(errors='replace'):
            if server.poll() is not None or time.monotonic()>deadline:
                raise RuntimeError(f'Dedicated server did not start: {out}')
            time.sleep(.05)
        clients=[]
        for index in range(2):
            cfg=f"""sys_colorconsole 0
cfg_save_auto 0
plug_loaddefault 0
con_notifytime 0
r_fullbright 1
viewsize 120
vid_fullscreen 0
vid_vsync 0
cl_idlefps 60
name humanoid_client_{index}
connect qw://127.0.0.1:{port}
defer 16 quit force
"""
            (game/f'network-client-{index}.cfg').write_text(cfg)
            clients.append(start(args.engine,f'network-client-{index}.cfg',f'network-client-{index}',
                                 ['-window','-width','640','-height','480','-gpu_debug']))
        for proc in [server]+clients:
            if proc.wait(timeout=60):raise RuntimeError(f'Network process failed: {out}')
    finally:
        for proc in processes:
            if proc.poll() is None:
                proc.terminate()
                try:proc.wait(timeout=5)
                except subprocess.TimeoutExpired:proc.kill();proc.wait()
        for log in logs:log.close()
    frames={}
    for index in range(2):
        text=(out/f'network-client-{index}.log').read_text(errors='replace')
        if re.search(r'AddressSanitizer:|runtime error:|Host_Error|Sys_Error|GPU.*Validation Error',text):
            raise RuntimeError(f'Client error: {out}')
        if not re.search(r'CSQC\s+type: parentspace\s+bone count: 24',text):
            raise RuntimeError(f'Client {index} did not receive the network skeleton: {out}')
        phases={}
        for phase in ('animation','falling','settled','recovered'):
            if f'HUMAN_NETWORK_{phase}' not in text:raise RuntimeError(f'Missing client phase: {out}')
            frame=out/'fte'/f'jolt-network-{index}-{phase}.png'
            data=frame.read_bytes()
            if data[:8]!=b'\x89PNG\r\n\x1a\n':raise RuntimeError(f'Invalid client screenshot: {frame}')
            phases[phase]=hashlib.sha256(data).hexdigest()
        if len(set(phases.values()))<3:raise RuntimeError(f'Client {index} poses did not change: {out}')
        frames[str(index)]=phases
    servertext=(out/'network-server.log').read_text(errors='replace')
    if (re.search(r'HUMANOID_FAIL|AddressSanitizer:|runtime error:|Host_Error|Sys_Error|Jolt .* rejected',servertext) or
            servertext.count('HUMANOID_CLIENT_READY')!=2 or 'HUMANOID_NETWORK_SERVER_OK' not in servertext):
        raise RuntimeError(f'Both clients did not complete the dedicated-server test: {out}')
    (out/'results.json').write_text(json.dumps({'status':'passed','network_clients':2,'pose_images':frames},indent=2)+'\n')
    print(f'Two-client network checks passed: {out / "results.json"}',flush=True)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--remaster-data',type=Path,help='Optional owned Quake Enhanced installation')
    p.add_argument('--quake-data',type=Path,help='Optional classic UI assets for the original mannequin')
    p.add_argument('--engine',type=Path,default=ROOT/'build/jolt/fteqw')
    p.add_argument('--server',type=Path,default=ROOT/'build/jolt/fteqw-sv')
    p.add_argument('--qcc',type=Path,default=ROOT/'build/jolt/fteqcc')
    p.add_argument('--plugin',type=Path,default=ROOT/'build/jolt/fteplug_jolt.so')
    p.add_argument('--test',action='store_true',help='Run the automated headless lifecycle/terrain checks')
    p.add_argument('--network-test',action='store_true',help='Connect two native clients to a loopback dedicated server')
    p.add_argument('--run',action='store_true',help='Launch the interactive native demo')
    p.add_argument('--capture',action='store_true',help='Capture animation/drop/recovery and exit')
    args=p.parse_args()
    if sum((args.test,args.run,args.network_test))>1:p.error('Choose one of --test, --run or --network-test')
    if args.capture and not args.run:p.error('--capture requires --run')
    if (args.run or args.network_test) and not (args.quake_data or args.remaster_data):p.error('--run needs owned UI assets via --quake-data or --remaster-data')
    out=prepare(args)
    if args.network_test:
        network_test(args,out);return
    command=[str((args.server if args.test else args.engine).resolve()),'-basedir',str(out),'-nohome','-quake']
    if args.test:command+=['-nostdin']
    else:command+=['-window','-width','960','-height','720','-gpu_debug']
    command+=['+set','plug_loaddefault','0','+exec','humanoid.cfg']
    (out/'launch.json').write_text(json.dumps(command,indent=2)+'\n')
    print(f'Humanoid directory: {out}',flush=True)
    print('Launch: '+shlex.join(command),flush=True)
    if args.test or args.run:
        logpath=out/'humanoid.log'
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with logpath.open('w') as log:
            subprocess.run(command,cwd=out,stdout=log,stderr=subprocess.STDOUT,env=env,
                           check=True,timeout=90 if args.test or args.capture else None)
        text=logpath.read_text(errors='replace')
        if re.search(r'HUMANOID_FAIL|AddressSanitizer:|UndefinedBehaviorSanitizer:|runtime error:|Host_Error|Sys_Error|Jolt .* rejected',text):
            raise RuntimeError(f'Engine/rig/sanitizer failure: {logpath}\n{text[-5000:]}')
        if args.test:
            for marker in ('HUMANOID_TERRAIN_OK 0','HUMANOID_TERRAIN_OK 1','HUMANOID_TERRAIN_OK 2',
                           'HUMANOID_TERRAIN_OK 3','HUMANOID_TERRAIN_OK 4','HUMANOID_LIFECYCLE_OK'):
                if marker not in text:raise RuntimeError(f'Missing {marker}: {logpath}\n{text[-5000:]}')
        if args.capture:
            for name in ('animation','falling','settled','recovered'):
                path=out/'fte'/f'jolt-human-{name}.png'
                if path.read_bytes()[:8]!=b'\x89PNG\r\n\x1a\n':raise RuntimeError(f'Missing screenshot {path}')
        (out/'results.json').write_text(json.dumps({'status':'passed','owned_model':bool(args.remaster_data),
            'headless':args.test,'capture':args.capture},indent=2)+'\n')
        print(f'Humanoid checks passed: {out / "results.json"}',flush=True)


if __name__=='__main__':main()
