#!/usr/bin/env python3
"""Prepare/check an isolated AD Tears performance and image-equivalence run.

Prepare uses owned PAKs, then run launches the client and checks the result.
Use a Release build without -gpu_debug for performance measurements.
"""
import argparse
import json
import pathlib
import re
import subprocess
import sys

from gpu_integration import ROOT, prepare

def map_image_delta(out, first, second, size):
    from PIL import Image, ImageChops, ImageStat
    folder = out/'ad_v1_80p1final'
    images = [Image.open(folder/f'gpu-perf-{name}.png').convert('RGB') for name in (first, second)]
    if any(list(im.size) != size for im in images):
        raise RuntimeError('Benchmark capture resolutions differ')
    # Exclude HUD and animated liquid. Include cutout lettering/foliage.
    w, h = images[0].size
    box = (round(w*0.1), round(h*0.08), round(w*0.95), round(h*0.72))
    return sum(ImageStat.Stat(ImageChops.difference(*(im.crop(box) for im in images))).mean)/3


def check(out, log, target, lightmaps=False):
    text = pathlib.Path(log).read_text(errors='replace')
    backend = 'metal' if sys.platform == 'darwin' else 'vulkan'
    if f'SDL3 GPU renderer: {backend}' not in text or 'Server ended' not in text:
        raise RuntimeError('Performance run did not complete on the native backend')
    if re.search(r'(Error:|GPU.*[Vv]alidation [Ee]rror|initialization failed|Sys_Error|invalid string offset|runaway loop|Bad builtin)', text):
        raise RuntimeError('Engine/GPU error in performance run')
    pattern = (r'SDL GPU benchmark (\S+) (\d+)x(\d+): ([\d.]+) fps, ([\d.]+) ms/frame, '
               r'p50 ([\d.]+) ms, p95 ([\d.]+) ms; acquire ([\d.]+) ms, build ([\d.]+) ms, '
               r'encode ([\d.]+) ms; ([\d.]+) draws, ([\d.]+) vertices/frame \((\d+) frames\)'
               r'(?:; p99 ([\d.]+) ms, max ([\d.]+) ms, first encode ([\d.]+) ms \((\d+) percentile samples\))?'
               r'(?:; (\d+) lightmap bytes/frame)?')
    names = ('fps','frame_ms','p50_ms','p95_ms','acquire_ms','build_ms','encode_ms','draws','vertices','frames',
             'p99_ms','max_ms','first_encode_ms','percentile_samples','lightmap_bytes_per_frame')
    records = {}
    for label, width, height, *values in re.findall(pattern, text):
        if label in records:
            raise RuntimeError(f'Duplicate benchmark label: {label}')
        record = {key: float(value) for key, value in zip(names, values) if value}
        records[label] = dict(record, size=[int(width), int(height)])
    maps = re.findall(r'Lightmaps: (\d+)/(\d+) loaded, (\d+) draws using fallback',text)
    if not maps or any(a!=b or int(c) for a,b,c in maps):
        raise RuntimeError('Performance run lost world lightmaps')
    if lightmaps:
        first, partial, last = ('lightmap-full', 'lightmap-partial', 'lightmap-full-again')
        if not all(name in records and records[name].get('lightmap_bytes_per_frame', 0) > 0
                   for name in (first, partial, last)):
            raise RuntimeError('Lightmap comparison did not exercise uploads in all three phases')
        if records[partial]['lightmap_bytes_per_frame'] >= min(records[first]['lightmap_bytes_per_frame'],
                                                               records[last]['lightmap_bytes_per_frame']):
            raise RuntimeError('Partial updates did not reduce lightmap upload bytes')
        delta = max(map_image_delta(out, reference, partial, records[partial]['size'])
                    for reference in (first, last))
        if delta > 0.5:
            raise RuntimeError(f'Partial uploads altered the paused map image ({delta:.4f})')
        if target and (records[partial]['fps'] < target or records[partial]['p95_ms'] > 1000/target):
            raise RuntimeError(f'Partial uploads missed the {target:g} FPS target')
        result = {'backend': backend, 'benchmarks': records, 'map_image_mean_difference': delta,
                  'target_fps': target or 'not enforced'}
        (out/'lightmap-results.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2))
        return
    reference, batched, live = (records[name] for name in
                               ('faithful-reference','faithful-batched','faithful-live-sweep'))
    if any('p99_ms' in record for record in records.values()):
        for name in ('faithful-cold-entry', 'faithful-warm-entry', 'faithful-reference',
                     'faithful-batched', 'faithful-live-sweep'):
            record = records.get(name, {})
            if ('p99_ms' not in record or record['draws'] <= 0 or
                    not 0 < record['percentile_samples'] <= record['frames'] or
                    not record['p95_ms'] <= record['p99_ms'] <= record['max_ms']):
                raise RuntimeError(f'Missing or invalid frame timing for {name}')
    if batched['draws'] >= reference['draws']*0.5:
        raise RuntimeError('Adjacent surface batching did not reduce draw submissions')
    if abs(reference['vertices']-batched['vertices']) > max(1,reference['vertices']*0.005):
        raise RuntimeError('Paused reference/batched geometry counts changed')
    delta = map_image_delta(out, 'reference', 'batched', batched['size'])
    if delta > 0.5:
        raise RuntimeError(f'Batching altered the paused map image ({delta:.4f})')
    if target and any(r['fps'] < target or r['p95_ms'] > 1000/target for r in (batched,live)):
        raise RuntimeError(f'Batched/live throughput or p95 missed the {target:g} FPS target: {records}')
    result = {'backend':backend,'benchmarks':records,'map_image_mean_difference':delta,
              'geometry_and_lightmaps':'passed','target_fps':target or 'not enforced'}
    (out/'performance-results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('prepare','run','check'))
    parser.add_argument('game',nargs='?')
    parser.add_argument('--out',default=str(ROOT/'build/perfdata'))
    parser.add_argument('--qcc',default=str(ROOT/'build/all/fteqcc'))
    parser.add_argument('--client',type=pathlib.Path,default=ROOT/'build/fteqw')
    parser.add_argument('--log',default=str(ROOT/'build/performance.log'))
    parser.add_argument('--target-fps',type=float,default=0)
    parser.add_argument('--lightmaps', action='store_true', help='Compare full, partial, and full lightmap uploads')
    args = parser.parse_args()
    out = pathlib.Path(args.out).resolve()
    if args.action == 'prepare':
        if not args.game: parser.error('prepare requires the folder containing id1 and ad_v1_80p1final')
        prepare(args.game,out,args.qcc,'tears')
        config = 'gpu_lightmap_uploads.cfg' if args.lightmaps else 'gpu_performance.cfg'
        (out/'id1/gpu-test.cfg').write_text((ROOT/'tools/tests'/config).read_text())
        captures = ('lightmap-full', 'lightmap-partial', 'lightmap-full-again') if args.lightmaps else ('reference', 'batched', 'live')
        for name in captures:
            p = out/'ad_v1_80p1final'/f'gpu-perf-{name}.png'
            if p.exists(): p.unlink()
    else:
        if args.action == 'run':
            if not (out/'id1/gpu-test.cfg').is_file():
                parser.error('prepare the benchmark data before running')
            log_path = pathlib.Path(args.log).resolve()
            log_path.parent.mkdir(parents=True, exist_ok=True)
            print(f'Renderer benchmark log: {log_path}', flush=True)
            with log_path.open('w') as log:
                subprocess.run([str(args.client.resolve()), '-nohome', '-basedir', str(out),
                                '-quake', '-window', '-width', '1470', '-height', '923',
                                '+vid_renderer', 'sdlgpu', '+exec', 'gpu-test.cfg'],
                               stdout=log, stderr=subprocess.STDOUT, check=True, timeout=150)
        check(out,args.log,args.target_fps,args.lightmaps)

if __name__ == '__main__':
    main()
