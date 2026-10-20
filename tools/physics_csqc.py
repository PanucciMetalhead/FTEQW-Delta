#!/usr/bin/env python3
"""Run original CSQC physics fixtures in a real client and independent Jolt world.
Headless by default; --gpu exercises native rendering and captures original assets.
No installed games, external downloads or public game servers are needed.
"""
import argparse
import hashlib
import struct
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time
from physics_integration import ROOT, write_rig
from physics_kinematic import brush_map

ERROR = re.compile(r'CSQC_PHYSICS_FAIL|AddressSanitizer:|UndefinedBehaviorSanitizer:|runtime error:|Host_Error|Host_EndGame|Sys_Error|Jolt simulation stopped|Jolt entity .* rejected|Jolt ragdoll .* rejected|GPU.*Validation Error|csqc error')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('engine', 'server', 'qcc', 'plugin', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--client-only', type=Path, help='Also test a client-only executable against a private dedicated server')
    p.add_argument('--gpu', action='store_true', help='Use a native SDL GPU window instead of the null renderer')
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='csqc-', dir=args.out.resolve()))
    game = out / 'id1'
    (game / 'maps').mkdir(parents=True)
    (game / 'maps/csqc.bsp').write_bytes(brush_map())
    write_rig(game)
    doll = (game / 'progs/physics-rig.doll').read_text().replace('type hinge', 'type fixed')
    (game / 'progs/physics-csqc.doll').write_text(doll + 'updatebody top\nanimate 1\n')
    (game / 'declarations.qh').write_text((ROOT / 'tools/tests/physics_fixture.qc').read_text().split('float simulated,phase')[0])
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for side, target in [('server', 'progs.dat'), ('client', 'csprogs.dat')]:
        shutil.copyfile(ROOT / f'tools/tests/physics_csqc_{side}.qc', game / f'{side}.qc')
        with (out / f'compile-{side}.log').open('w') as log:
            subprocess.run([str(args.qcc.resolve()), '-srcfile', f'{side}.qc', '-o', target],
                           cwd=game, env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
    shutil.copyfile(args.plugin.resolve(), out / args.plugin.name)
    common = """sys_colorconsole 0
cfg_save_auto 0
plug_loaddefault 0
plug_load jolt
physics_jolt_enabled 1
sv_public 0
sv_port_ipv6 ""
sv_port_tcp ""
mod_md5_singleanimation 0
developer 1
con_notifytime 0
m_preset_chosen 1
vid_fullscreen 0
vid_vsync 0
cl_maxfps 60
cl_idlefps 60
r_fullbright 1
sv_csqc_progname csprogs.dat
pr_csqc_coreonerror 0
"""
    common += f"set csqc_fixture_capture {int(args.gpu)}\n"
    mode = ['-window', '-width', '640', '-height', '480', '-gpu_debug'] if args.gpu else ['+vid_renderer', 'headless']
    processes = []
    logs = []

    def start(binary, cfg, name, extra=(), basedir=out):
        log = (out / f'{name}.log').open('w'); logs.append(log)
        proc = subprocess.Popen([str(binary.resolve()), '-basedir', str(basedir), '-nohome', '-quake', '-nosound', '-nostdin',
                                 *extra, '+set', 'plug_loaddefault', '0', '+exec', cfg],
                                cwd=out, env=env, stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT)
        processes.append(proc)
        return proc

    def check_log(name):
        text = (out / f'{name}.log').read_text(errors='replace')
        if ERROR.search(text):
            raise RuntimeError(f'Engine failure in {out / (name + ".log")}\n{text[-5000:]}')
        return text

    def wait_for(proc, name, predicate, timeout=70, watch=()):
        deadline = time.monotonic() + timeout
        observed = [(proc, name), *watch]
        while True:
            texts = {}
            for peer, peer_name in observed:
                # Let the runtime finish symbolizing a failure before cleanup
                # sends SIGTERM, including failures in the other process.
                partial = (out / f'{peer_name}.log').read_text(errors='replace')
                if ERROR.search(partial):
                    try:
                        peer.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        pass
                texts[peer_name] = check_log(peer_name)
            if predicate(texts[name]):
                return texts[name]
            for peer, peer_name in observed:
                code = peer.poll()
                if code is not None:
                    raise RuntimeError(f'{peer_name} exited with code {code} (0x{code & 0xffffffff:08x}): {out}\n{texts[peer_name][-5000:]}')
            if time.monotonic() > deadline:
                details = '\n'.join(f'{label}:\n{body[-5000:]}' for label, body in texts.items())
                raise RuntimeError(f'Timed out waiting for {name}: {out}\n{details}')
            time.sleep(.05)

    results = {}
    try:
        cfg = common + """sv_port 0
sv_listen_nq 0
sv_listen_qw 0
deathmatch 1
set csqc_fixture_shared 1
set csqc_fixture_server_skel 0
set csqc_fixture_client_skel 0
alias csqc_done "defer .2 quit force"
alias csqc_cycle2 "alias csqc_next csqc_done;defer .2 csqc_reload"
alias csqc_reload "plug_close jolt;plug_load jolt;map csqc"
alias csqc_cycle1 "alias csqc_next csqc_cycle2;defer .2 map csqc"
alias csqc_next csqc_cycle1
map csqc
"""
        (game / 'listen.cfg').write_text(cfg)
        proc = start(args.engine, 'listen.cfg', 'listen', mode)
        text = wait_for(proc, 'listen', lambda s: s.count('CSQC_PHYSICS_OK') == 3)
        if proc.wait(timeout=15):
            raise RuntimeError('Listen client failed: ' + str(out))
        text = check_log('listen')
        if text.count('CSQC_PHYSICS_SHUTDOWN') != 3 or 'SSQC_ISOLATION_OK' not in text:
            raise RuntimeError('Missing listen-world isolation/cleanup checks: ' + str(out))
        results['listen'] = {'client_world_runs': 3, 'shutdowns': 3}
        if args.client_only:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                sock.bind(('127.0.0.1', 0)); port = sock.getsockname()[1]
            cfg = common + f'sv_port 127.0.0.1:{port}\nmaxclients 1\nset csqc_fixture_shared 0\nmap csqc\necho CSQC_SERVER_READY\n'
            (game / 'server.cfg').write_text(cfg)
            server = start(args.server, 'server.cfg', 'server', ['-ip', '127.0.0.1', '-port', str(port)])
            wait_for(server, 'server', lambda s: 'CSQC_SERVER_READY' in s, 20)
            cfg = common + f"""set csqc_fixture_shared 0
alias csqc_done "cmd csqc_fixture_done;defer .3 quit force"
alias csqc_cycle2 "alias csqc_next csqc_done;disconnect;plug_close jolt;plug_load jolt;defer .2 connect qw://127.0.0.1:{port}"
alias csqc_next "alias csqc_next csqc_cycle2;defer .2 cmd csqc_fixture_map"
connect qw://127.0.0.1:{port}
"""
            # A separate install without CSQC forces the real checksum-named
            # download path, including completion and reconnect cache reuse.
            remote_root = out / 'remote-install'
            shutil.copytree(game, remote_root / 'id1',
                            ignore=shutil.ignore_patterns('csprogs.dat', 'progs.dat', '*.qc', '*.qh', '*.cfg'))
            shutil.copyfile(args.plugin.resolve(), remote_root / args.plugin.name)
            (remote_root / 'id1/remote.cfg').write_text(cfg)
            client = start(args.client_only, 'remote.cfg', 'remote', mode, remote_root)
            wait_for(client, 'remote', lambda s: s.count('CSQC_PHYSICS_OK') == 3, watch=[(server, 'server')])
            if client.wait(timeout=15):
                raise RuntimeError('Remote client failed: ' + str(out))
            text = check_log('remote')
            if 'Downloading csprogsvers/' not in text or not list(remote_root.rglob('csprogsvers/*.dat')):
                raise RuntimeError('Client did not download the fixture CSQC: ' + str(out))
            if text.count('CSQC_PHYSICS_SHUTDOWN') != 3:
                raise RuntimeError('Missing remote cleanup checks: ' + str(out))
            if server.wait(timeout=15):
                raise RuntimeError('Dedicated server failed: ' + str(out))
            check_log('server')
            results['client_only'] = {'client_world_runs': 3, 'shutdowns': 3, 'csqc_download': 'passed'}
    finally:
        for proc in processes:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill(); proc.wait()
            proc.stdin.close()
        for log in logs:
            log.close()
    if args.gpu:
        images = {}
        for host in ('listen', 'remote') if args.client_only else ('listen',):
            for run in range(1, 4):
                hashes = []
                for phase in ('mixed', 'released', 'restored'):
                    name = f'csqc-{host}-{run}-{phase}.png'
                    files = list(out.rglob(name))
                    if len(files) != 1:
                        raise RuntimeError(f'Missing or ambiguous screenshot {name}: {out}')
                    data = files[0].read_bytes()
                    if data[:8] != b'\x89PNG\r\n\x1a\n' or len(data) < 1024:
                        raise RuntimeError(f'Invalid screenshot: {files[0]}')
                    digest = hashlib.sha256(data).hexdigest(); hashes.append(digest)
                    images[name] = {'sha256': digest, 'size': struct.unpack('>II', data[16:24])}
                if len(set(hashes)) < 2:
                    raise RuntimeError(f'Rendered client poses did not change: {host} run {run}')
        results['screenshots'] = images
    (out / 'results.json').write_text(json.dumps({'status': 'passed', 'gpu': args.gpu, **results}, indent=2) + '\n')
    print('Independent CSQC physics checks passed: ' + str(out / 'results.json'))


if __name__ == '__main__':
    main()
