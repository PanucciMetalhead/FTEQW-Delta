#!/usr/bin/env python3
"""Measure dedicated-server work and UDP payload with headless loopback clients."""

import argparse
from contextlib import ExitStack
import json
import os
from pathlib import Path
import re
import selectors
import shutil
import socket
import statistics
import subprocess
import tempfile
import time

from network_gameplay import ROOT, SERVER_REPORT, wait_for_server

PHYSICS_INTERVAL = 0.013
TRAFFIC_COUNTERS = ('downlink_bytes', 'downlink_datagrams', 'uplink_datagrams')
JOINED = re.compile(r'^(scale_\d+) entered the game$', re.MULTILINE)
REMOVED = re.compile(r'^Client "(scale_\d+)" removed$', re.MULTILINE)


def stop(process):
    if process.poll() is None:
        process.kill()
        process.wait(timeout=5)


def launch(stack, command, out, log, *, console=False):
    stream = stack.enter_context(log.open('w'))
    process = subprocess.Popen(
        command, cwd=out, stdout=stream, stderr=subprocess.STDOUT,
        stdin=subprocess.PIPE if console else subprocess.DEVNULL,
        env=dict(os.environ, SDL_VIDEODRIVER='dummy'))
    if console:
        stack.enter_context(process.stdin)
    stack.callback(stop, process)
    return process


def command(process, text):
    process.stdin.write((text + '\n').encode())
    process.stdin.flush()


def sample_resources(server, selector, elapsed):
    rss = subprocess.check_output(['ps', '-o', 'rss=', '-p', str(server.pid)],
                                  text=True, timeout=2)
    return {'seconds': elapsed, 'server_rss_kib': int(rss.strip()),
            **{counter: sum(key.data[counter] for key in selector.get_map().values())
               for counter in TRAFFIC_COUNTERS}}


def relay_packets(selector, server_addr, counting=False):
    for key, _ in selector.select(0.01):
        proxy, link = key.fileobj, key.data
        payload, sender = proxy.recvfrom(65535)
        if sender == server_addr:
            if link['client_addr'] is not None:
                proxy.sendto(payload, link['client_addr'])
                if counting:
                    link['downlink_bytes'] += len(payload)
                    link['downlink_datagrams'] += 1
        else:
            if link['client_addr'] is None:
                link['client_addr'] = sender
            if sender == link['client_addr']:
                proxy.sendto(payload, server_addr)
                if counting:
                    link['uplink_datagrams'] += 1


def wait_for_players(selector, server, server_addr, log, offset, pattern, expected):
    start = time.monotonic()
    observed = set()
    partial = ''
    next_check = start
    with log.open(errors='replace') as console:
        console.seek(offset)
        while time.monotonic() - start < 30 and server.poll() is None:
            now = time.monotonic()
            if now >= next_check:
                next_check = now + 0.02
                data, _, partial = (partial + console.read()).rpartition('\n')
                observed.update(pattern.findall(data))
                if expected <= observed:
                    return now - start
            relay_packets(selector, server_addr)
    raise RuntimeError(f'Missing player events from {expected - observed}. See {log}')


def measure(selector, server, clients, server_addr, log, frames, sample_interval, log_offset=0):
    deadline = time.monotonic() + 45
    entered = set()
    requested = False
    start = None
    next_sample = 0
    next_check = 0
    samples = []
    partial = ''
    joined_seconds = None
    waiting_since = time.monotonic()
    with log.open(errors='replace') as console:
        console.seek(log_offset)
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_check:
                next_check = now + 0.02
                if server.poll() is not None or any(client.poll() is not None for client in clients):
                    raise RuntimeError(f'Benchmark process exited early. See {log.parent}')
                data, _, partial = (partial + console.read()).rpartition('\n')
                entered.update(JOINED.findall(data))
                if not requested and len(entered) == len(clients):
                    joined_seconds = now - waiting_since
                    command(server, f'sv_benchmark {frames}')
                    requested = True
                    deadline = now + max(45, frames * PHYSICS_INTERVAL * 2)
                if start is None and 'Server benchmark started:' in data:
                    start = next_sample = now
                report = SERVER_REPORT.search(data)
                if report and start is not None:
                    if sample_interval:
                        samples.append(sample_resources(server, selector, now - start))
                    return report, now - start, samples, joined_seconds
                if sample_interval and start is not None and now >= next_sample:
                    samples.append(sample_resources(server, selector, now - start))
                    next_sample = now + sample_interval

            relay_packets(selector, server_addr, start is not None)
    raise TimeoutError(f'Server benchmark did not finish. See {log.parent}')


def run_case(args, out, count, server_path):
    game = out / 'id1'
    game.mkdir(parents=True)
    for name in ('pak0.pak', 'pak1.pak'):
        source = args.game / 'id1' / name
        if source.is_file():
            try:
                (game / name).symlink_to(source)
            except OSError:
                shutil.copyfile(source, game / name)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserved:
        reserved.bind(('127.0.0.1', 0))
        server_addr = reserved.getsockname()
    port = server_addr[1]
    (game / 'scale-sv.cfg').write_text(f'''sys_colorconsole 0
cfg_save_auto 0
plug_loaddefault 0
ruleset_allow_in 1
net_enable_dtls 0
sv_port 127.0.0.1:{port}
sv_port_ipv6 ""
sv_port_tcp ""
sv_public -1
sv_listen_nq 0
sv_mintic {PHYSICS_INTERVAL}
sv_maxtic {PHYSICS_INTERVAL}
sv_nqplayerphysics 0
maxclients 32
maxplayers 32
deathmatch 1
map {args.map}
''')
    common = ['-basedir', str(out), '-nohome', '-quake']
    log = out / 'server.log'
    with ExitStack() as stack:
        selector = stack.enter_context(selectors.DefaultSelector())
        server = launch(stack, [str(server_path), *common, '-ip', '127.0.0.1', '-port', str(port),
                                '+exec', 'scale-sv.cfg'], out, log, console=True)
        wait_for_server(server, log)
        def start_client(i, suffix=''):
            name = f'scale_{i + 1:02}'
            proxy = stack.enter_context(socket.socket(socket.AF_INET, socket.SOCK_DGRAM))
            proxy.bind(('127.0.0.1', 0))
            link = {'client_addr': None, 'downlink_bytes': 0, 'downlink_datagrams': 0, 'uplink_datagrams': 0}
            selector.register(proxy, selectors.EVENT_READ, link)
            (game / f'{name}.cfg').write_text(f'''cfg_save_auto 0
plug_loaddefault 0
ruleset_allow_in 1
m_preset_chosen 1
cl_maxfps {args.client_rate}
cl_netfps {args.client_rate}
rate 30000
name {name}
connect qw://127.0.0.1:{proxy.getsockname()[1]}
+forward
+right
+attack
''')
            mode = ['-headless-active'] if args.client_rate > 10 else []
            client = launch(stack, [str(args.client), *common, '-nostdin', '-nosound', *mode,
                                    '+vid_renderer', 'headless', '+exec', f'{name}.cfg'],
                            out, out / f'{name}{suffix}.log')
            return client, proxy, link

        clients, proxies, links = [], [], []
        for i in range(count):
            client, proxy, link = start_client(i)
            clients.append(client)
            proxies.append(proxy)
            links.append(link)
        report, elapsed, samples, _ = measure(
            selector, server, clients, server_addr, log, args.frames, args.sample_interval)
        result = summarize(args, out, args.map, report, elapsed, samples, links)
        if args.rotate_map:
            departing = {f'scale_{i + 1:02}' for i in range(args.reconnect_clients)}
            offset = log.stat().st_size
            command(server, ';'.join(f'kick {name}' for name in sorted(departing)))
            removed_seconds = wait_for_players(selector, server, server_addr, log, offset, REMOVED, departing)
            offset = log.stat().st_size
            rejoin_start = time.monotonic()
            for i in range(args.reconnect_clients):
                stop(clients[i])
                selector.unregister(proxies[i])
                proxies[i].close()
                clients[i], proxies[i], links[i] = start_client(i, '-rejoin')
            wait_for_players(selector, server, server_addr, log, offset, JOINED, departing)
            rejoin_seconds = time.monotonic() - rejoin_start
            print(f'{count:2} clients  {len(departing)} slots refilled, rotating to {args.rotate_map}', flush=True)
            for link in links:
                link.update(dict.fromkeys(TRAFFIC_COUNTERS, 0))
            offset = log.stat().st_size
            command(server, f'changelevel {args.rotate_map}')
            report, elapsed, samples, signon_seconds = measure(
                selector, server, clients, server_addr, log, args.frames, args.sample_interval, offset)
            result['lifecycle'] = {'rejoined_clients': sorted(departing),
                                   'removal_seconds': removed_seconds, 'rejoin_seconds': rejoin_seconds,
                                   'rotation_signon_seconds': signon_seconds,
                                   'after_rotation': summarize(args, out, args.rotate_map, report,
                                                               elapsed, samples, links)}
        command(server, 'quit force')
        if server.wait(timeout=8):
            raise RuntimeError(f'Server did not shut down cleanly. See {log}')
    return result


def summarize(args, out, map_name, report, elapsed, samples, links):
    keys = ('frames', 'wall_seconds', 'mean_ms', 'p50_ms', 'p95_ms', 'p99_ms', 'max_ms')
    server_stats = dict(zip(keys, map(float, report.groups())))
    if server_stats['frames'] != args.frames or any(not link['downlink_datagrams'] for link in links):
        raise RuntimeError(f'Incomplete benchmark samples. See {out}')
    if min(link['uplink_datagrams'] for link in links) / elapsed < args.client_rate * 0.75:
        raise RuntimeError(f'Client packet rate fell below 75% of the target. See {out}')
    traffic = [{k: link[k] for k in TRAFFIC_COUNTERS} for link in links]
    result = {'map': map_name, 'clients': len(links), 'requested_client_rate': args.client_rate,
              'server': server_stats, 'traffic_seconds': elapsed,
              'downlink_udp_payload_bytes_per_second': sum(link['downlink_bytes'] for link in links) / elapsed,
              'per_client_traffic': traffic}
    if samples:
        result['resource_samples'] = samples
    return result


def compare(args, out, count):
    servers = {'baseline': args.baseline, 'candidate': args.server}
    samples = {label: [] for label in servers}
    for repeat in range(args.repeats):
        order = list(servers) if repeat % 2 == 0 else list(reversed(servers))
        for label in order:
            result = run_case(args, out / f'{label}-{repeat + 1}', count, servers[label])
            samples[label].append(result)
            print(f'{count:2} clients  {label} {repeat + 1}  '
                  f'p99 {result["server"]["p99_ms"]:.3f} ms', flush=True)
    medians = {
        label: {metric: statistics.median(row['server'][metric] for row in rows)
                for metric in ('mean_ms', 'p50_ms', 'p95_ms', 'p99_ms', 'max_ms')}
        for label, rows in samples.items()
    }
    return {'map': args.map, 'clients': count, 'samples': samples, 'medians': medians}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path, help='Quake installation with id1 PAKs')
    parser.add_argument('--client', type=Path, default=ROOT / 'build/all/fteqw')
    parser.add_argument('--server', type=Path, default=ROOT / 'build/all/fteqw-sv')
    parser.add_argument('--baseline', type=Path, help='Compare against this build in alternating order')
    parser.add_argument('--repeats', type=int, default=1, help='Samples per build and client count')
    parser.add_argument('--map', default='start', help='Map from the supplied Quake PAKs')
    parser.add_argument('--rotate-map', help='Sample again after refilling slots and changing level')
    parser.add_argument('--reconnect-clients', type=int, default=0,
                        help='Number of players to disconnect and reconnect before the rotation')
    parser.add_argument('--out', type=Path, default=ROOT / 'build/network-scaling')
    parser.add_argument('--clients', type=int, nargs='+', default=[1, 4, 8, 16])
    parser.add_argument('--frames', type=int, default=600)
    parser.add_argument('--sample-interval', type=int, default=0, metavar='SECONDS',
                        help='Record server RSS and cumulative packet counts periodically using ps')
    parser.add_argument('--client-rate', type=int, choices=(10, 30, 60), default=60,
                        help='Requested client packets per second. 10 uses the stock headless throttle.')
    args = parser.parse_args()
    args.game, args.client, args.server, args.out = (path.resolve() for path in
                                                    (args.game, args.client, args.server, args.out))
    if args.baseline:
        args.baseline = args.baseline.resolve()
    if any(not re.fullmatch(r'[A-Za-z0-9_-]+', name) for name in (args.map, args.rotate_map or args.map)):
        parser.error('map names must have no path or extension')
    if not 1 <= args.repeats <= 10 or (args.repeats != 1 and args.baseline is None):
        parser.error('use 1–10 repeats with --baseline')
    if len(args.clients) != len(set(args.clients)) or not all(1 <= n <= 32 for n in args.clients):
        parser.error('--clients must contain distinct counts from 1 to 32')
    if args.rotate_map or args.reconnect_clients:
        if (not args.rotate_map or args.rotate_map.lower() == args.map.lower() or args.baseline or
                not 1 <= args.reconnect_clients <= min(8, min(args.clients))):
            parser.error('choose a different rotation map and 1–8 reconnecting clients without --baseline')
    if not 120 <= args.frames <= 8192:
        parser.error('--frames must be between 120 and 8192')
    if args.sample_interval < 0:
        parser.error('--sample-interval must be nonnegative')
    if args.out == args.game or args.game in args.out.parents or not (args.game / 'id1/pak0.pak').is_file():
        parser.error('provide a Quake installation with id1/pak0.pak and a separate output directory')
    args.out.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='run-', dir=args.out))
    print(f'Network scaling output: {out}', flush=True)
    results = []
    for count in args.clients:
        case_dir = out / f'clients-{count}'
        result = (compare(args, case_dir, count) if args.baseline else
                  run_case(args, case_dir, count, args.server))
        results.append(result)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        if args.baseline:
            medians = result['medians']
            print(f'{count:2} clients  median p99 {medians["baseline"]["p99_ms"]:.3f} ms baseline, '
                  f'{medians["candidate"]["p99_ms"]:.3f} ms candidate', flush=True)
        else:
            print(f'{count:2} clients  server p99 {result["server"]["p99_ms"]:.3f} ms  '
                  f'downlink {result["downlink_udp_payload_bytes_per_second"] / 1000:.1f} kB/s', flush=True)


if __name__ == '__main__':
    main()
