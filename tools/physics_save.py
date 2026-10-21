#!/usr/bin/env python3
"""Save/load active original Jolt props and entity joints through native engine saves."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import sys
from physics_integration import ROOT, bsp, write_rig


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('server','qcc','plugin','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--skeletal',action='store_true',help='Exercise live skeletal rigs and shared animation resources')
    p.add_argument('--legacy',action='store_true',help='Test the legacy single-player .sav path with a full engine')
    p.add_argument('--legacy-engine',type=Path,help='Also run legacy saves in a full headless engine')
    args=p.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='save-',dir=args.out.resolve()))
    game=out/'id1';(game/'maps').mkdir(parents=True);(game/'maps/savephysics.bsp').write_bytes(bsp())
    write_rig(game)
    if args.skeletal:
        doll=(game/'progs/physics-rig.doll').read_text().replace('type hinge','type fixed')+'updatebody top\nanimate 1\n'
        (game/'progs/physics-saved-mixed.doll').write_text(doll)
        (game/'inline_rig.qh').write_text('#define CHECKPOINT_DOLL '+json.dumps(doll+'animate .5\n')+'\n')
    (game/'declarations.qh').write_text((ROOT/'tools/tests/physics_fixture.qc').read_text().split('float simulated,phase')[0])
    shutil.copyfile(ROOT/('tools/tests/physics_rig_save.qc' if args.skeletal else 'tools/tests/physics_save.qc'),game/'fixture.qc')
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    with (out/'compile.log').open('w') as log:
        subprocess.run([str(args.qcc.resolve()),'-srcfile','fixture.qc','-o','progs.dat'],cwd=game,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=30)
    shutil.copyfile(args.plugin.resolve(),out/args.plugin.name)
    startup='vid_renderer headless\n' if args.legacy else ''
    (game/'save.cfg').write_text(startup+"""sys_colorconsole 0
sv_public 0
sv_port 0
sv_listen_nq 0
sv_listen_qw 0
plug_loaddefault 0
plug_load jolt
physics_jolt_enabled 1
physics_jolt_pointanchor2 1
sv_mintic .008333333
sv_maxtic .008333333
sv_gravity 800
developer 1
maxclients 1
sv_savefmt %SAVEFMT%
set checkpoint_loaded 0
set checkpoint_refused 0
alias resume_load_a "load physics-resume-a;set checkpoint_loaded 1"
alias resume_load_b "load physics-resume-b;set checkpoint_loaded 2"
map savephysics
""".replace("%SAVEFMT%","0" if args.legacy else "1"))
    path=out/'save.log'
    with path.open('w') as log:
        proc=subprocess.run([str(args.server.resolve()),'-basedir',str(out),'-quake','-nohome','-nostdin','-nosound',*(['+vid_renderer','headless'] if args.legacy else []),'+set','plug_loaddefault','0','+exec','save.cfg'],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=75)
    text=path.read_text(errors='replace')
    marker='SAVE_RIG_OK' if args.skeletal else 'SAVE_ENGINE_OK'
    refusals=0 if args.skeletal else 1
    if proc.returncode or text.count(marker)!=1 or text.count('Jolt entity scene restored')!=2 or text.count('skeletal checkpoint rejected')!=refusals or re.search(r'SAVE_RIG_FAIL|SAVE_PHYSICS_FAIL|AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|Jolt .*rejected|Jolt simulation stopped|Host_Error|Sys_Error',text):
        raise AssertionError(f'{path}\n{text[-9000:]}')
    a=list(out.rglob('physics-resume-a.sav' if args.legacy else 'saves/physics-resume-a/savephysics.lvc'));b=list(out.rglob('physics-resume-b.sav' if args.legacy else 'saves/physics-resume-b/savephysics.lvc'))
    if len(a)!=1 or len(b)!=1:raise AssertionError('Missing native save caches: '+str(out))
    for file in a+b:
        saved=file.read_text();assert 'rbe_state {' in saved and 'jolt_entities 2' in saved,file
    # A refusal must leave the first checkpoint's VM state intact.
    first=a[0].read_text();assert re.search(r'"phase"\s+"1"',first),a[0]
    # Load from a fresh process: runtime IDs, native bodies and cached state
    # from the producing process must not be required.
    common=startup+"""sys_colorconsole 0
sv_public 0
sv_port 0
sv_listen_nq 0
sv_listen_qw 0
plug_loaddefault 0
sv_mintic .008333333
sv_maxtic .008333333
maxclients 1
sv_savefmt %SAVEFMT%
sv_gravity 800
developer 1
"""
    common=common.replace("%SAVEFMT%","0" if args.legacy else "1")
    def run_config(name,cfg):
        (game/(name+'.cfg')).write_text(common+cfg)
        path=out/(name+'.log')
        with path.open('w') as log:
            result=subprocess.run([str(args.server.resolve()),'-basedir',str(out),'-quake','-nohome','-nostdin','-nosound',*(['+vid_renderer','headless'] if args.legacy else []),
                '+set','plug_loaddefault','0','+exec',name+'.cfg'],cwd=out,env=env,stdout=log,
                stderr=subprocess.STDOUT,timeout=75)
        return result.returncode,path.read_text(errors='replace'),path
    resume_cfg="""plug_load jolt
physics_jolt_enabled 1
physics_jolt_pointanchor2 1
set checkpoint_loaded 1
set checkpoint_refused 0
alias resume_load_b "load physics-resume-b;set checkpoint_loaded 2"
load physics-resume-a
"""
    code,cold,coldpath=run_config('cold',resume_cfg)
    if code or cold.count('Jolt entity scene restored')!=2 or marker not in cold or re.search(
            r'SAVE_RIG_FAIL|SAVE_PHYSICS_FAIL|AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|Host_Error|Sys_Error',cold):
        raise AssertionError(f'Cold restore failed: {coldpath}\n{cold[-6000:]}')
    if args.skeletal:
        assert text.count('Jolt skeletal resources restored')==2 and cold.count('Jolt skeletal resources restored')==2,(path,coldpath)
    original=a[0].read_text()

    if not args.skeletal:
        # Version 1 predates skeletal resources; keep its original entity decoder.
        assert 'skeletons 0' in original
        compatible=original.replace('jolt_entities 2 ','jolt_entities 1 ',1).replace('skeletons 0\n','',1)
        if args.legacy:
            compatpath=a[0].parent/'compat-v1.sav'
        else:
            compatfolder=a[0].parent.parent/'compat-v1';shutil.copytree(a[0].parent,compatfolder);compatpath=compatfolder/a[0].name
        compatpath.write_text(compatible)
        code,log,logpath=run_config('compat-v1',resume_cfg.replace('load physics-resume-a','load compat-v1'))
        if code or log.count('Jolt entity scene restored')!=2 or marker not in log or re.search(
                r'SAVE_PHYSICS_FAIL|AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|Host_Error|Sys_Error',log):
            raise AssertionError(f'Version-1 checkpoint did not continue correctly: {logpath}\n{log[-5000:]}')
    prefix,rest=original.split('rbe_state {',1);payload,suffix=rest.split('}',1)
    lines=payload.strip().splitlines()
    def mutate(name):
        changed=list(lines)
        if name=='oversize':return 'x'*(0x1000000+1)
        if name=='nested':return '{'
        if name=='quoted':return '"bad"'
        if name.startswith('skel-'):
            if name=='skel-count':changed[next(i for i,l in enumerate(changed) if l.startswith('skeletons '))]='skeletons 1025'
            elif name=='skel-pose':
                index=next(i for i,l in enumerate(changed) if l.startswith('pose '));tokens=changed[index].split();tokens[4]='nan';changed[index]=' '.join(tokens)
            elif name.startswith('skel-definition'):
                index=next(i for i,l in enumerate(changed) if l.startswith('definition '))
                stop=index+1
                while changed[stop].startswith('hex '):stop+=1
                text=bytes.fromhex(''.join(l.split()[1] for l in changed[index+1:stop])).decode()
                if name=='skel-definition-size':changed[index]='definition 1048577'
                elif name=='skel-definition-hex':changed[index+1]='hex zz'
                elif name=='skel-definition-nul':changed[index+1]='hex 00'+changed[index+1][6:]
                else:
                    if name=='skel-definition-mass':text=text.replace('mass 1','mass nan',1)
                    elif name=='skel-definition-shape':text=text.replace('dimensions 8 8 20','dimensions 0 0 0',1)
                    elif name=='skel-definition-count':text+='body unexpected bone0\n'
                    elif name=='skel-definition-refpose':text=text.replace('refpose skin','refpose 99999 0',1)
                    raw=text.encode();replacement=['definition '+str(len(raw))]+['hex '+raw[i:i+30].hex() for i in range(0,len(raw),30)]
                    changed[index:stop]=replacement
            elif name.startswith('skel-body'):
                index=next(i for i,l in enumerate(changed) if l.startswith('rigbody '));tokens=changed[index].split()
                if name=='skel-body-mask':tokens[2]='1.5'
                elif name=='skel-body-nan':tokens[2]='nan'
                elif name=='skel-body-flags':tokens[3]='2'
                elif name=='skel-body-quaternion':tokens[11]='0'
                elif name=='skel-body-servo':tokens[43]='0'
                elif name=='skel-body-velocity':tokens[50]='1e30'
                changed[index]=' '.join(tokens)
            elif name.startswith('skel-joint'):
                index=next(i for i,l in enumerate(changed) if l.startswith('rigjoint '));tokens=changed[index].split()
                tokens[2 if name=='skel-joint-type' else 4]='0' if name=='skel-joint-type' else '2';changed[index]=' '.join(tokens)
            else:
                records=[i for i,l in enumerate(changed) if l.startswith('skeleton ')]
                index=records[1] if name=='skel-duplicate' else next(i for i in records if changed[i].split()[7]!='0')
                tokens=changed[index].split()
                if name=='skel-duplicate':tokens[1]=changed[records[0]].split()[1]
                elif name=='skel-model':tokens[2]='1'
                elif name=='skel-bones':tokens[4]='32768'
                elif name=='skel-owner':tokens[6]='0'
                elif name=='skel-source':tokens[5]='1024'
                elif name=='skel-source-cycle':tokens[5]=tokens[1]
                changed[index]=' '.join(tokens)
            return '\n'+'\n'.join(changed)+'\n'
        if name=='version':changed[0]='jolt_entities 99 0'
        elif name=='mode':changed[0]='jolt_entities 1 0'
        elif name=='clock':changed[1]='clock nan 0'
        elif name=='counts':changed[2]='counts 8193 0'
        elif name=='joint-count':changed[2]='counts 5 16385'
        elif name=='truncated':changed=changed[:-2]
        elif name=='trailing':changed+=['unexpected']
        else:
            index=next(i for i,line in enumerate(changed) if line.startswith('body ') and
                       (name not in ('tiny-mass','tiny-shape') or line.split()[3]=='2'))
            tokens=changed[index].split()
            if name=='entity':tokens[1]='1048576'
            elif name=='quaternion':tokens[13]='0'
            elif name=='coordinate':tokens[45]='1e30'
            elif name=='flag':tokens[6]='2'
            elif name=='nonfinite':tokens[7]='inf'
            elif name=='long-token':tokens[7]='9'*80
            elif name=='tiny-mass':tokens[63]='1e-45'
            elif name=='tiny-shape':tokens[60]=tokens[57]
            changed[index]=' '.join(tokens)
        return '\n'+ '\n'.join(changed)+'\n'
    cases=('version','mode','clock','counts','joint-count','truncated','trailing','entity',
           'quaternion','coordinate','flag','nonfinite','long-token','oversize','nested','quoted','tiny-mass','tiny-shape')
    if args.skeletal:
        cases+=('skel-count','skel-duplicate','skel-model','skel-bones','skel-owner','skel-source','skel-source-cycle','skel-pose',
                'skel-definition-size','skel-definition-hex','skel-definition-nul','skel-definition-mass','skel-definition-shape',
                'skel-definition-count','skel-definition-refpose','skel-body-mask','skel-body-nan','skel-body-flags',
                'skel-body-quaternion','skel-body-servo','skel-body-velocity','skel-joint-type','skel-joint-flags')
    diagnostics={}
    finish='defer 1 quit force\n' if args.legacy else 'quit force\n'
    for name in cases:
        if args.legacy:
            target=a[0].parent/('bad-'+name+'.sav')
        else:
            folder=a[0].parent.parent/('bad-'+name)
            shutil.copytree(a[0].parent,folder);target=folder/a[0].name
        target.write_text(prefix+'rbe_state {'+mutate(name)+'}'+suffix)
        code,log,path=run_config('bad-'+name,'plug_load jolt\nphysics_jolt_enabled 1\nphysics_jolt_pointanchor2 1\nload bad-'+name+'\n'+finish)
        if code not in (0,1) or not re.search(r'Rigid-body saved state rejected|Bad entity lump:',log) or 'Jolt entity scene restored' in log or re.search(
                r'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:',log):
            raise AssertionError(f'Malformed state not safely rejected (exit {code}): {path}\n{log[-4000:]}')
        diagnostics[name]='rejected'
    code,log,path=run_config('missing-backend','load physics-resume-a\n'+finish)
    if code or 'Rigid-body saved state rejected' not in log or 'Jolt entity scene restored' in log or re.search(
            r'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:',log):
        raise AssertionError(f'Missing backend accepted: {path}\n{log[-4000:]}')
    if args.legacy_engine:
        subprocess.run([sys.executable,str(ROOT/'tools/physics_save.py'),
            '--server',str(args.legacy_engine.resolve()),'--qcc',str(args.qcc.resolve()),
            '--plugin',str(args.plugin.resolve()),'--out',str(out/'legacy'),'--legacy',*(['--skeletal'] if args.skeletal else [])],check=True,timeout=180 if args.skeletal else 90)
    (out/'results.json').write_text(json.dumps({'status':'passed','legacy':args.legacy,'loads':4 if args.skeletal else 6,'save_refusals':refusals*(2 if args.skeletal else 3),'v1_compatibility':'not applicable' if args.skeletal else 'passed','skeletal':args.skeletal,'malformed':diagnostics,'missing_backend':'rejected','files':[str(f) for f in a+b]},indent=2)+'\n')
    print('Physics save/load checks passed: '+str(out/'results.json'))


if __name__=='__main__':main()
