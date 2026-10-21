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
    p.add_argument('--legacy',action='store_true',help='Test the legacy single-player .sav path with a full engine')
    p.add_argument('--legacy-engine',type=Path,help='Also run legacy saves in a full headless engine')
    args=p.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='save-',dir=args.out.resolve()))
    game=out/'id1';(game/'maps').mkdir(parents=True);(game/'maps/savephysics.bsp').write_bytes(bsp())
    write_rig(game)
    (game/'declarations.qh').write_text((ROOT/'tools/tests/physics_fixture.qc').read_text().split('float simulated,phase')[0])
    shutil.copyfile(ROOT/'tools/tests/physics_save.qc',game/'fixture.qc')
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
    if proc.returncode or text.count('SAVE_ENGINE_OK')!=1 or text.count('Jolt entity scene restored')!=2 or text.count('saving requires a healthy entity scene')!=1 or re.search(r'SAVE_PHYSICS_FAIL|AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|Jolt .*rejected|Jolt simulation stopped|Host_Error|Sys_Error',text):
        raise AssertionError(f'{path}\n{text[-9000:]}')
    a=list(out.rglob('physics-resume-a.sav' if args.legacy else 'saves/physics-resume-a/savephysics.lvc'));b=list(out.rglob('physics-resume-b.sav' if args.legacy else 'saves/physics-resume-b/savephysics.lvc'))
    if len(a)!=1 or len(b)!=1:raise AssertionError('Missing native save caches: '+str(out))
    for file in a+b:
        saved=file.read_text();assert 'rbe_state {' in saved and 'jolt_entities 1' in saved,file
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
    code,cold,coldpath=run_config('cold',"""plug_load jolt
physics_jolt_enabled 1
physics_jolt_pointanchor2 1
set checkpoint_loaded 1
set checkpoint_refused 0
alias resume_load_b "load physics-resume-b;set checkpoint_loaded 2"
load physics-resume-a
""")
    if code or cold.count('Jolt entity scene restored')!=2 or 'SAVE_ENGINE_OK' not in cold or re.search(
            r'SAVE_PHYSICS_FAIL|AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|Host_Error|Sys_Error',cold):
        raise AssertionError(f'Cold restore failed: {coldpath}\n{cold[-6000:]}')
    original=a[0].read_text()
    prefix,rest=original.split('rbe_state {',1);payload,suffix=rest.split('}',1)
    lines=payload.strip().splitlines()
    def mutate(name):
        changed=list(lines)
        if name=='oversize':return 'x'*(0x1000000+1)
        if name=='nested':return '{'
        if name=='quoted':return '"bad"'
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
            '--plugin',str(args.plugin.resolve()),'--out',str(out/'legacy'),'--legacy'],check=True,timeout=90)
    (out/'results.json').write_text(json.dumps({'status':'passed','legacy':args.legacy,'loads':4,'save_refusals':2,'malformed':diagnostics,'missing_backend':'rejected','files':[str(f) for f in a+b]},indent=2)+'\n')
    print('Physics save/load checks passed: '+str(out/'results.json'))


if __name__=='__main__':main()
