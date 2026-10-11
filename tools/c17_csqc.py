#!/usr/bin/env python3
"""Test default entity-chain fields and coroutines in full/client-only CSQC.

Requires owned Quake game data and a native SDL GPU display. Game data is
linked into an isolated directory; the original files are never modified.
"""
import argparse
import json
import os
import socket
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path, help='Directory containing id1/pak0.pak')
    parser.add_argument('--build', type=Path, default=ROOT/'build/c17')
    parser.add_argument('--out', type=Path, default=ROOT/'build/c17-csqc')
    args = parser.parse_args()
    game, build, out = args.game.resolve(), args.build.resolve(), args.out.resolve()
    if out == game or game in out.parents:
        raise ValueError('Output must be separate from original game data')
    source = game/'id1'
    if not (source/'pak0.pak').is_file():
        raise FileNotFoundError(source/'pak0.pak')
    (out/'id1').mkdir(parents=True, exist_ok=True)
    for name in ('pak0.pak', 'pak1.pak'):
        target = out/'id1'/name
        if (source/name).is_file() and not target.exists():
            target.symlink_to(source/name)
    (out/'id1/c17-csqc.qc').write_bytes((ROOT/'tools/tests/c17_csqc.qc').read_bytes())
    suffix = '.exe' if os.name == 'nt' else ''
    def binary(name):
        path = build/(name+suffix)
        if not path.is_file():
            path = build/'Release'/(name+suffix)
        if not path.is_file():
            raise FileNotFoundError(path)
        return str(path)
    compiler, server_binary = binary('fteqcc'), binary('fteqw-sv')
    clients = [(name, binary(name)) for name in ('fteqw', 'fteqw-cl')]
    with (out/'compile.log').open('w') as log:
        subprocess.run([compiler, '-srcfile', 'c17-csqc.qc', '-o', 'c17-csqc.dat',
                        '-O2', '-Wnone'], cwd=out/'id1', stdout=log,
                       stderr=subprocess.STDOUT, check=True, timeout=30)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    common = 'cfg_save_auto 0\nm_preset_chosen 1\ncon_notifytime 0\nsv_public 0\n'
    (out/'id1/server.cfg').write_text(common + f'sv_port 127.0.0.1:{port}\n'
        'sv_port_ipv6 ""\nsv_port_tcp ""\nsv_csqc_progname c17-csqc.dat\nmap start\n')
    (out/'id1/client.cfg').write_text(common + f'connect qw://127.0.0.1:{port}\n'
        'defer 10 quit force\n')
    results = {}
    with (out/'server.log').open('w') as serverlog:
        server = subprocess.Popen([server_binary, '-nohome', '-basedir', str(out),
            '-quake', '-ip', '127.0.0.1', '-port', str(port), '+exec', 'server.cfg'],
            cwd=ROOT, stdin=subprocess.PIPE, stdout=serverlog, stderr=subprocess.STDOUT)
        try:
            time.sleep(1)
            if server.poll() is not None:
                raise RuntimeError('Server exited; see server.log')
            for name, client in clients:
                with (out/(name+'.log')).open('w') as log:
                    subprocess.run([client, '-nohome', '-basedir', str(out), '-quake',
                        '-window', '-width', '640', '-height', '480', '-gpu_debug',
                        '+vid_renderer', 'sdlgpu', '+exec', 'client.cfg'], cwd=ROOT,
                        stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
                text = (out/(name+'.log')).read_text(errors='replace')
                for marker in ('C17_FIELDS_PASS', 'C17_COROUTINE_ZERO_PASS',
                               'C17_COROUTINE_DELAY_PASS'):
                    if marker not in text:
                        raise AssertionError(f'{name}: missing {marker}; see client log')
                if 'Host_Error' in text or 'Validation Error' in text:
                    raise AssertionError(f'{name}: runtime error; see client log')
                results[name] = 'chain fields, radius and coroutines passed'
        finally:
            if server.poll() is None:
                try:
                    server.stdin.write(b'quit\n')
                    server.stdin.flush()
                    server.wait(timeout=5)
                except (BrokenPipeError, subprocess.TimeoutExpired):
                    server.terminate()
                    server.wait(timeout=5)
            server.stdin.close()
    (out/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    print(json.dumps(results, indent=2))

if __name__ == '__main__':
    main()
