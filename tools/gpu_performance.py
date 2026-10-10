#!/usr/bin/env python3
"""Prepare/check an isolated AD Tears performance and image-equivalence run.

Prepare uses owned PAKs, then run the engine with +exec gpu-test.cfg.
Use a Release build without -gpu_debug for performance measurements.
"""
import argparse
import json
import pathlib
import re
import sys

from gpu_integration import ROOT, prepare

def check(out, log, target):
    from PIL import Image, ImageChops, ImageStat
    text = pathlib.Path(log).read_text(errors='replace')
    backend = 'metal' if sys.platform == 'darwin' else 'vulkan'
    if f'SDL3 GPU renderer: {backend}' not in text or 'Server ended' not in text:
        raise RuntimeError('Performance run did not complete on the native backend')
    if re.search(r'(Error:|GPU.*[Vv]alidation [Ee]rror|initialization failed|Sys_Error|invalid string offset|runaway loop|Bad builtin)', text):
        raise RuntimeError('Engine/GPU error in performance run')
    pattern = (r'SDL GPU benchmark (\S+) (\d+)x(\d+): ([\d.]+) fps, ([\d.]+) ms/frame, '
               r'p50 ([\d.]+) ms, p95 ([\d.]+) ms; acquire ([\d.]+) ms, build ([\d.]+) ms, '
               r'encode ([\d.]+) ms; ([\d.]+) draws, ([\d.]+) vertices/frame \((\d+) frames\)')
    names = ('fps','frame_ms','p50_ms','p95_ms','acquire_ms','build_ms','encode_ms','draws','vertices','frames')
    records = {m[0]: dict(zip(names,map(float,m[3:])),size=[int(m[1]),int(m[2])])
               for m in re.findall(pattern,text)}
    reference, batched, live = (records[name] for name in
                               ('faithful-reference','faithful-batched','faithful-live-sweep'))
    if batched['draws'] >= reference['draws']*0.5:
        raise RuntimeError('Adjacent surface batching did not reduce draw submissions')
    if abs(reference['vertices']-batched['vertices']) > max(1,reference['vertices']*0.005):
        raise RuntimeError('Paused reference/batched geometry counts changed')
    maps = re.findall(r'Lightmaps: (\d+)/(\d+) loaded, (\d+) draws using fallback',text)
    if not maps or any(a!=b or int(c) for a,b,c in maps):
        raise RuntimeError('Performance run lost world lightmaps')
    images = [Image.open(out/'ad_v1_80p1final'/f'gpu-perf-{name}.png').convert('RGB')
              for name in ('reference','batched')]
    if images[0].size != images[1].size or list(images[0].size) != batched['size']:
        raise RuntimeError('Reference/batched resolutions differ')
    # Exclude HUD and animated liquid at the bottom. Include ceiling, arches,
    # cutout lettering/vines and background visible through their gaps.
    w,h = images[0].size
    box = (round(w*0.1),round(h*0.08),round(w*0.95),round(h*0.72))
    delta = sum(ImageStat.Stat(ImageChops.difference(*(im.crop(box) for im in images))).mean)/3
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
    parser.add_argument('action',choices=('prepare','check'))
    parser.add_argument('game',nargs='?')
    parser.add_argument('--out',default=str(ROOT/'build/perfdata'))
    parser.add_argument('--qcc',default=str(ROOT/'build/all/fteqcc'))
    parser.add_argument('--log',default=str(ROOT/'build/performance.log'))
    parser.add_argument('--target-fps',type=float,default=0)
    args = parser.parse_args()
    out = pathlib.Path(args.out).resolve()
    if args.action == 'prepare':
        if not args.game: parser.error('prepare requires the folder containing id1 and ad_v1_80p1final')
        prepare(args.game,out,args.qcc,'tears')
        (out/'id1/gpu-test.cfg').write_text((ROOT/'tools/tests/gpu_performance.cfg').read_text())
        for name in ('reference','batched','live'):
            p = out/'ad_v1_80p1final'/f'gpu-perf-{name}.png'
            if p.exists(): p.unlink()
    else:
        check(out,args.log,args.target_fps)

if __name__ == '__main__':
    main()
