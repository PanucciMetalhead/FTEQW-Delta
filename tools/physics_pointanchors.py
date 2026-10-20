#!/usr/bin/env python3
"""Check shared and legacy ODE point anchors through real SSQC, without game assets."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from physics_integration import ROOT, bsp


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for arg in ('server','qcc','plugin','out'):
        parser.add_argument('--'+arg,type=Path,required=True)
    args=parser.parse_args()
    args.out.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='points-',dir=args.out.resolve()))
    game=out/'id1';(game/'maps').mkdir(parents=True)
    (game/'maps/points.bsp').write_bytes(bsp())
    (game/'declarations.qh').write_text((ROOT/'tools/tests/physics_fixture.qc').read_text().split('float simulated,phase')[0])
    for name in ('physics_pointanchors.qc','physics_pointanchors.qh'):
        shutil.copyfile(ROOT/'tools/tests'/name,game/name)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    with (out/'compile.log').open('w') as log:
        subprocess.run([str(args.qcc.resolve()),'-srcfile','physics_pointanchors.qc','-o','progs.dat'],
                       cwd=game,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=30)
    shutil.copyfile(args.plugin.resolve(),out/args.plugin.name)
    results={}
    for mode in (0,1):
        cfg=f"""sys_colorconsole 0
sv_public 0
sv_port 0
sv_mintic 0.008333333
sv_maxtic 0.008333333
sv_gravity 800
sv_listen_nq 0
sv_listen_qw 0
plug_loaddefault 0
plug_load jolt
physics_jolt_enabled 1
physics_jolt_pointanchor2 {mode}
developer 1
alias point_done "quit force"
alias point_cycle2 "alias point_next point_done;plug_close jolt;plug_load jolt;map points"
alias point_cycle1 "alias point_next point_cycle2;map points"
alias point_next point_cycle1
map points
"""
        (game/'points.cfg').write_text(cfg)
        path=out/f'mode-{mode}.log'
        try:
            with path.open('w') as log:
                proc=subprocess.run([str(args.server.resolve()),'-basedir',str(out),'-quake','-nohome','-nostdin',
                    '+set','plug_loaddefault','0','+exec','points.cfg'],cwd=out,env=env,stdout=log,
                    stderr=subprocess.STDOUT,timeout=60)
        except subprocess.TimeoutExpired as exc:
            raise AssertionError(f'Point-anchor timeout: {path}\n{path.read_text(errors="replace")[-7000:]}') from exc
        text=path.read_text(errors='replace')
        expected=r'Jolt joint entity \d+ rejected: Invalid point-anchor mode or coordinates'
        warnings=re.findall(expected,text)
        clean=re.sub(expected,'',text)
        if proc.returncode or text.count('POINT_ENGINE_OK')!=3 or len(warnings)!=(3 if mode else 0) or re.search(
                r'POINT_ANCHOR_FAIL|AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|Jolt .*rejected|Jolt simulation stopped|Host_Error|Sys_Error',clean):
            raise AssertionError(f'Point-anchor checks failed: {path}\n{text[-7000:]}')
        results[str(mode)]={'map_runs':3,'expected_rejections':len(warnings)}
    (out/'results.json').write_text(json.dumps({'status':'passed','modes':results},indent=2)+'\n')
    print('Point-anchor checks passed: '+str(out/'results.json'))


if __name__=='__main__':
    main()
