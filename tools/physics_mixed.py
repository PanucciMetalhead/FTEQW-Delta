#!/usr/bin/env python3
"""Exercise mixed kinematic/passive skeletal rigs through real SSQC, without game assets."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from physics_integration import ROOT, bsp, write_rig


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('server', 'qcc', 'plugin', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--client-host', action='store_true', help='Test the client executable in dedicated mode')
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='mixed-', dir=args.out.resolve()))
    game = out / 'id1'
    (game / 'maps').mkdir(parents=True)
    (game / 'maps/mixed.bsp').write_bytes(bsp())
    write_rig(game)
    doll = (game / 'progs/physics-rig.doll').read_text().replace('type hinge', 'type fixed')
    doll += 'updatebody top\nanimate 1\n'
    (game / 'progs/physics-mixed.doll').write_text(doll)
    (game / 'progs/physics-mixed-invalid.doll').write_text(doll + 'animate .5\n')
    declarations = (ROOT / 'tools/tests/physics_fixture.qc').read_text().split('float simulated,phase')[0]
    (game / 'declarations.qh').write_text(declarations)
    shutil.copyfile(ROOT / 'tools/tests/physics_mixed.qc', game / 'fixture.qc')
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    with (out / 'compile.log').open('w') as log:
        subprocess.run([str(args.qcc.resolve()), '-srcfile', 'fixture.qc', '-o', 'progs.dat'],
                       cwd=game, stdout=log, stderr=subprocess.STDOUT, env=env, check=True, timeout=30)
    shutil.copyfile(args.plugin.resolve(), out / args.plugin.name)
    cfg = """sys_colorconsole 0
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
alias mixed_done "quit force"
alias mixed_cycle2 "alias mixed_next mixed_done;plug_close jolt;plug_load jolt;map mixed"
alias mixed_cycle1 "alias mixed_next mixed_cycle2;map mixed"
alias mixed_next mixed_cycle1
map mixed
"""
    if args.client_host:
        cfg = 'deathmatch 1\n' + cfg
    (game / 'mixed.cfg').write_text(cfg)
    logpath = out / 'mixed.log'
    with logpath.open('w') as log:
        subprocess.run([str(args.server.resolve()), *(['-dedicated'] if args.client_host else []),
                        '-basedir', str(out), '-quake', '-nohome', '-nostdin',
                        '+set', 'plug_loaddefault', '0', '+exec', 'mixed.cfg'],
                       cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=90)
    text = logpath.read_text(errors='replace')
    if text.count('MIXED_ENGINE_OK') != 3 or re.search(
            r'MIXED_FAIL|AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|Jolt simulation stopped|Host_Error|Sys_Error', text):
        raise AssertionError(f'{logpath}\n{text[-7000:]}')
    (out / 'results.json').write_text(json.dumps({'status': 'passed', 'map_runs': 3}, indent=2) + '\n')
    print('Mixed animation checks passed: ' + str(out / 'results.json'))


if __name__ == '__main__':
    main()
