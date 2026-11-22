#!/usr/bin/env python3
"""Measure live QuakeWorld snapshot timing through a loopback-only UDP relay."""

import argparse
import heapq
import json
import os
from pathlib import Path
import random
import re
import select
import shutil
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BURSTS = ((2.0, 2.3), (5.0, 5.9), (8.0, 8.3))
RECOVERY_BURST = ((5.0, 5.85),)
RECOVERY_UPLINK = 0.220
RECOVERY_DOWNLINK = 0.030
LIMIT_BPS = 1200
CLIENT_REPORT = re.compile(
    r'Net benchmark: (\d+) snapshots, (\d+) invalid, (\d+) gap samples, '
    r'p50 ([\d.]+) ms, p95 ([\d.]+) ms, p99 ([\d.]+) ms, max ([\d.]+) ms, '
    r'(\d+) prediction samples, (\d+) history fallbacks, (\d+) missing bases')
SERVER_REPORT = re.compile(
    r'Server benchmark: (\d+) frames in ([\d.]+) s, mean ([\d.]+) ms, '
    r'p50 ([\d.]+) ms, p95 ([\d.]+) ms, p99 ([\d.]+) ms, max ([\d.]+) ms')


def wait_for_server(process, log):
    deadline = time.monotonic() + 20
    while process.poll() is None and time.monotonic() < deadline:
        if 'Server spawned.' in log.read_text(errors='replace'):
            return
        time.sleep(0.05)
    raise RuntimeError(f'Server did not start. See {log}')


def relay(proxy, server_addr, client, log, profile):
    rng = random.Random(11)
    pending = []
    order = 0
    client_addr = None
    start = stop = None
    next_downlink = 0.0
    counters = {'burst_drops': 0, 'queue_drops': 0, 'downlink_bytes': 0, 'uplink_bytes': 0}
    deadline = time.monotonic() + 38
    next_log_check = 0.0
    bursts = RECOVERY_BURST if profile == 'recovery' else BURSTS
    while client.poll() is None:
        now = time.monotonic()
        if now >= deadline:
            raise TimeoutError(f'Client benchmark timed out. See {log}')
        if stop is None and now >= next_log_check:
            data = log.read_text(errors='replace')
            if start is None and 'Net benchmark started' in data:
                start = now
            if start is not None and CLIENT_REPORT.search(data):
                stop = now
            next_log_check = now + 0.02
        measuring = start is not None and stop is None
        while pending and pending[0][0] <= now:
            _, _, data, destination, metric = heapq.heappop(pending)
            proxy.sendto(data, destination)
            if measuring:
                counters[metric] += len(data)

        timeout = min(0.01, max(0, pending[0][0] - now)) if pending else 0.01
        ready, _, _ = select.select([proxy], [], [], timeout)
        if not ready:
            continue
        data, sender = proxy.recvfrom(65535)
        now = time.monotonic()
        downlink = sender == server_addr
        if downlink:
            if client_addr is None:
                continue
            destination = client_addr
        else:
            if client_addr is None:
                client_addr = sender
            if sender != client_addr:
                continue
            destination = server_addr
        metric = 'downlink_bytes' if downlink else 'uplink_bytes'
        if not measuring or profile == 'clean':
            proxy.sendto(data, destination)
            if measuring:
                counters[metric] += len(data)
            continue
        elapsed = now - start
        if downlink and any(lo <= elapsed < hi for lo, hi in bursts):
            counters['burst_drops'] += 1
            continue
        due = now
        if profile == 'limited' and downlink:
            due = max(now, next_downlink) + len(data) / LIMIT_BPS
            if due - now > 0.250:
                counters['queue_drops'] += 1
                continue
            next_downlink = due
        if profile == 'recovery':
            delay = RECOVERY_DOWNLINK if downlink else RECOVERY_UPLINK
        else:
            delay = rng.uniform(0.015, 0.105)
        heapq.heappush(pending, (due + delay, order, data, destination, metric))
        order += 1
    if start is None or stop is None or client.returncode:
        raise RuntimeError(f'Client benchmark did not finish. See {log}')
    counters['seconds'] = stop - start
    return counters


def run_case(args, root, profile):
    out = root / profile
    game = out / 'id1'
    game.mkdir(parents=True)
    for name in ('pak0.pak', 'pak1.pak'):
        source = args.game / 'id1' / name
        if source.is_file():
            try:
                (game / name).symlink_to(source)
            except OSError:
                shutil.copyfile(source, game / name)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as proxy:
        proxy.bind(('127.0.0.1', 0))
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserved:
            reserved.bind(('127.0.0.1', 0))
            server_addr = reserved.getsockname()
        port = server_addr[1]
        (game / 'gameplay-sv.cfg').write_text(f'''sys_colorconsole 0
cfg_save_auto 0
plug_loaddefault 0
ruleset_allow_in 1
net_enable_dtls 0
sv_port 127.0.0.1:{port}
sv_port_ipv6 ""
sv_port_tcp ""
sv_public 0
sv_listen_nq 0
sv_nqplayerphysics 0
deathmatch 1
map start
defer 6 sv_benchmark 180
defer 28 quit force
''')
        (game / 'gameplay-cl.cfg').write_text(f'''cfg_save_auto 0
plug_loaddefault 0
ruleset_allow_in 1
m_preset_chosen 1
con_notifytime 0
cl_maxfps 120
cl_netfps {60 if profile == 'recovery' else 77}
cl_nopext {int(args.classic_qw)}
rate 30000
name gameplay_{profile}
connect qw://127.0.0.1:{proxy.getsockname()[1]}
defer 5 net_benchmark start
defer 5.05 rate {args.limited_client_rate if profile == 'limited' else 30000}
defer 5.1 +forward
defer 9 +right
defer 12 -right
defer 14 say GAMEPLAY_CHAT_{profile}
defer 16 -forward
defer 17 net_benchmark stop
defer 19 quit force
''')
        server_log, client_log = out/'server.log', out/'client.log'
        common = ['-basedir', str(out), '-nohome', '-quake', '-nostdin']
        renderer = 'headless' if args.headless else 'sdlgpu'
        mode = [] if args.headless else ['-window', '-width', '640', '-height', '480']
        env = dict(os.environ)
        if args.headless:
            env['SDL_VIDEODRIVER'] = 'dummy'
        with server_log.open('w') as slog, client_log.open('w') as clog:
            server = subprocess.Popen([str(args.server), *common, '-ip', '127.0.0.1', '-port', str(port),
                                       '+exec', 'gameplay-sv.cfg'], cwd=out, stdout=slog, stderr=subprocess.STDOUT)
            client = None
            try:
                wait_for_server(server, server_log)
                client = subprocess.Popen([str(args.client), *common, '-nosound', *mode,
                                           '+vid_renderer', renderer, '+exec', 'gameplay-cl.cfg'],
                                          cwd=out, env=env, stdout=clog, stderr=subprocess.STDOUT)
                counts = relay(proxy, server_addr, client, client_log, profile)
                server.wait(timeout=16)
            finally:
                for proc in (client, server):
                    if proc is not None and proc.poll() is None:
                        proc.kill()
                        proc.wait(timeout=5)
    cl_text, sv_text = (log.read_text(errors='replace') for log in (client_log, server_log))
    cl_report, sv_report = CLIENT_REPORT.search(cl_text), SERVER_REPORT.search(sv_text)
    if (server.returncode or not cl_report or not sv_report or 'entered the game' not in sv_text or
            f'GAMEPLAY_CHAT_{profile}' not in sv_text or
            re.search(r'Host_Error|Host_EndGame|Fatal error:|AddressSanitizer|runtime error:', cl_text + sv_text)):
        raise RuntimeError(f'Gameplay check failed. See {client_log} and {server_log}')
    client_keys = ('snapshots', 'invalid_snapshot_updates', 'gap_samples', 'p50_ms', 'p95_ms', 'p99_ms',
                   'max_ms', 'prediction_samples', 'history_fallbacks', 'missing_bases')
    client_stats = dict(zip(client_keys, map(float, cl_report.groups())))
    server_stats = dict(zip(('frames', 'wall_seconds', 'mean_ms', 'p50_ms', 'p95_ms', 'p99_ms', 'max_ms'),
                            map(float, sv_report.groups())))
    if (client_stats['snapshots'] < 40 or
            (not args.headless and client_stats['prediction_samples'] < 40) or
            (profile != 'clean' and not counts['burst_drops'])):
        raise RuntimeError(f'Insufficient gameplay samples under {profile}. See {client_log}')
    return {'profile': profile, 'client': client_stats, 'server': server_stats, 'relay': counts}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path, help='Quake installation with id1 PAKs')
    parser.add_argument('--client', type=Path, default=ROOT/'build/fteqw')
    parser.add_argument('--server', type=Path, default=ROOT/'build/all/fteqw-sv')
    parser.add_argument('--out', type=Path, default=ROOT/'build/network-gameplay')
    parser.add_argument('--profiles', nargs='+', choices=('clean', 'jitter', 'limited', 'recovery'),
                        default=['clean', 'jitter', 'limited'])
    parser.add_argument('--headless', action='store_true', help='Measure snapshot processing without rendering prediction')
    parser.add_argument('--classic-qw', action='store_true', help='Disable negotiated extensions on the benchmark client')
    parser.add_argument('--limited-client-rate', type=int, default=30000,
                        help='In the limited profile, request this server send rate in bytes per second')
    args = parser.parse_args()
    args.game, args.client, args.server = (path.resolve() for path in (args.game, args.client, args.server))
    args.out = args.out.resolve()
    if len(set(args.profiles)) != len(args.profiles):
        parser.error('profile names must be unique')
    if not 500 <= args.limited_client_rate <= 50000:
        parser.error('--limited-client-rate must be 500–50000 bytes per second')
    if args.out == args.game or args.game in args.out.parents:
        parser.error('output must be separate from the game installation')
    if not (args.game / 'id1/pak0.pak').is_file():
        parser.error('id1/pak0.pak is required')
    args.out.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='run-', dir=args.out))
    results = {'renderer': 'headless' if args.headless else 'sdlgpu', 'jitter_one_way_ms': [15, 105],
               'downlink_outages_seconds': BURSTS, 'limited_downlink_payload_bytes_per_second': LIMIT_BPS,
               'limited_client_requested_rate': args.limited_client_rate,
               'classic_qw': args.classic_qw,
               'cases': []}
    if 'recovery' in args.profiles:
        results['recovery_link'] = {'uplink_ms': RECOVERY_UPLINK * 1000,
                                    'downlink_ms': RECOVERY_DOWNLINK * 1000,
                                    'downlink_outages_seconds': RECOVERY_BURST}
    print(f'Gameplay benchmark output: {out}', flush=True)
    for profile in args.profiles:
        result = run_case(args, out, profile)
        results['cases'].append(result)
        print(json.dumps(result), flush=True)
        (out/'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
