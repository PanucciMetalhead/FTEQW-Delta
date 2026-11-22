#!/usr/bin/env python3
"""Measure dedicated-server frame work with a synthetic QuakeC collision load."""
import argparse
import json
from pathlib import Path
import platform
import re
import statistics
import subprocess
import tempfile

from physics_integration import ROOT, bsp


def run_case(server, out, traces, frames, log_name=None):
    pattern = (r'Server benchmark: (\d+) frames in ([\d.]+) s, mean ([\d.]+) ms, '
               r'p50 ([\d.]+) ms, p95 ([\d.]+) ms, p99 ([\d.]+) ms, max ([\d.]+) ms')
    # Allow for scheduler delays beyond the nominal 120 physics frames per second.
    finish = 4 + frames / 60
    cfg = out / 'id1/bench.cfg'
    cfg.write_text(f'''sys_colorconsole 0
developer 1
sv_public -1
sv_listen_qw 0
sv_listen_nq 0
sv_port ""
sv_port_ipv6 ""
sv_port_tcp ""
plug_loaddefault 0
ruleset_allow_in 1
sv_mintic 0.008333333
sv_maxtic 0.008333333
sv_nqplayerphysics 0
deathmatch 1
set bench_traces {traces}
map bench
defer 2 sv_benchmark {frames}
defer {finish:.3f} quit force
''')
    log = out / (log_name or f'traces-{traces}.log')
    with log.open('w') as output:
        subprocess.run([str(server), '-nohome', '-nostdin', '-basedir', str(out),
                        '-quake', '+exec', 'bench.cfg'], cwd=out, stdin=subprocess.DEVNULL,
                       stdout=output, stderr=subprocess.STDOUT, timeout=finish + 30, check=True)
    data = log.read_text(errors='replace')
    match = re.search(pattern, data)
    if (not re.search(rf'^SERVER_BENCH_TRACES {traces}$', data, re.MULTILINE)
            or not match or int(match[1]) != frames or
            re.search(r'Sys_Error|Host_Error|AddressSanitizer|runtime error:', data)):
        raise RuntimeError(f'Server benchmark did not complete. See {log}')
    keys = ('wall_seconds', 'mean_ms', 'p50_ms', 'p95_ms', 'p99_ms', 'max_ms')
    return dict(zip(keys, map(float, match.groups()[1:])), frames=frames, traces_per_tick=traces)


def compare(args, traces, out):
    servers = {'baseline': args.baseline.resolve(), 'candidate': args.server.resolve()}
    samples = {name: [] for name in servers}
    for repeat in range(args.repeats):
        order = list(servers)
        if repeat % 2:
            order.reverse()
        for name in order:
            result = run_case(servers[name], out, traces, args.frames,
                              f'traces-{traces}-{name}-{repeat + 1}.log')
            samples[name].append(result)
            print(f'{traces:4} traces/tick  {name} {repeat + 1}  '
                  f'p99 {result["p99_ms"]:.3f} ms', flush=True)
    medians = {name: {metric: statistics.median(case[metric] for case in runs)
                      for metric in ('mean_ms', 'p50_ms', 'p95_ms', 'p99_ms', 'max_ms')}
               for name, runs in samples.items()}
    print(f'{traces:4} traces/tick  median p99: '
          f'{medians["baseline"]["p99_ms"]:.3f} ms baseline, '
          f'{medians["candidate"]["p99_ms"]:.3f} ms candidate', flush=True)
    return {'traces_per_tick': traces, 'samples': samples, 'medians': medians}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, default=ROOT/'build/all/fteqw-sv')
    parser.add_argument('--baseline', type=Path, help='Compare this server with --server, alternating run order')
    parser.add_argument('--repeats', type=int, default=1, help='Number of repetitions per build with --baseline')
    parser.add_argument('--qcc', type=Path, default=ROOT/'build/all/fteqcc')
    parser.add_argument('--out', type=Path, default=ROOT/'build/server-benchmark')
    parser.add_argument('--frames', type=int, default=600)
    parser.add_argument('--traces', type=int, nargs='+', default=[0, 256, 1024])
    args = parser.parse_args()
    if not 2 <= args.frames <= 8192 or not all(0 <= n <= 8192 for n in args.traces):
        parser.error('use 2–8192 frames and 0–8192 traces')
    if len(set(args.traces)) != len(args.traces):
        parser.error('trace counts must be unique')
    if not 1 <= args.repeats <= 10 or (args.repeats != 1 and args.baseline is None):
        parser.error('use 1–10 repeats with --baseline')
    root = args.out.resolve()
    root.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='run-', dir=root))
    print(f'Server benchmark output: {out}', flush=True)
    game = out/'id1'
    (game/'maps').mkdir(parents=True)
    (game/'maps/bench.bsp').write_bytes(bsp())
    with (out/'compile.log').open('w') as log:
        subprocess.run([str(args.qcc.resolve()), '-srcfile', str(ROOT/'tools/tests/server_benchmark.qc'),
                        '-o', 'progs.dat'], cwd=game, stdout=log, stderr=subprocess.STDOUT,
                       timeout=30, check=True)
    results = {'platform': platform.platform(), 'server': str(args.server.resolve()),
               'requested_tick_hz': 120, 'cases': []}
    if args.baseline is not None:
        results['baseline'] = str(args.baseline.resolve())
    for traces in args.traces:
        if args.baseline is not None:
            results['cases'].append(compare(args, traces, out))
            continue
        result = run_case(args.server.resolve(), out, traces, args.frames)
        results['cases'].append(result)
        print(f'{traces:4} traces/tick  p99 {result["p99_ms"]:.3f} ms  '
              f'max {result["max_ms"]:.3f} ms', flush=True)
    (out/'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
