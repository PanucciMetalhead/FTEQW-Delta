#!/usr/bin/env python3
"""Run a headless engine startup/shutdown with isolated, data-free game paths."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    server, out = args.server.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'results.json').unlink(missing_ok=True)
    (out / 'id1').mkdir(exist_ok=True)
    env = dict(os.environ)
    # Process-lifetime engine caches are not yet a shutdown-ownership audit.
    # Invalid accesses and undefined behavior must still terminate the process.
    env['ASAN_OPTIONS'] = 'detect_leaks=0'
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    with (out / 'server.log').open('w') as log:
        proc = subprocess.run([str(server), '-basedir', str(out), '-quake', '-nohome',
                               '-nostdin', '+sv_public', '0', '+echo', 'ENGINE_SANITIZER_OK',
                               '+quit', 'force'], cwd=out, env=env, stdout=log,
                              stderr=subprocess.STDOUT, timeout=30)
    text = (out / 'server.log').read_text(errors='replace')
    assert proc.returncode == 0, f'Server exited {proc.returncode}; inspect {out / "server.log"}\n{text}'
    assert 'ENGINE_SANITIZER_OK' in text, 'Startup did not reach the command fixture'
    assert not re.search(r'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:', text), text
    (out / 'results.json').write_text(json.dumps({'server_startup_shutdown': 'passed'}, indent=2) + '\n')
    print('Sanitized server cvar registration, filesystem initialization, commands and shutdown passed')


if __name__ == '__main__':
    main()
