#!/usr/bin/env python3
"""Prepare/run/check mip streaming on owned Arcane Dimensions ad_tower data.

Uses an isolated fixture; original game files are only read. Normal runs measure
throughput; --debug enables SDL GPU validation and should not be used for timing.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

from gpu_integration import ROOT, prepare as prepare_fixture


def prepare(game, out, qcc, warmup=0):
    if out.exists():
        raise ValueError('Use a fresh output directory')
    prepare_fixture(game, out, qcc, 'tears')
    mod = out/'ad_v1_80p1final'
    (mod/'maps').mkdir(exist_ok=True)
    source = game/'ad_v1_80p1final/maps/ad_tower.bsp'
    if source.exists():
        try:
            (mod/'maps/ad_tower.bsp').symlink_to(source)
        except OSError:
            shutil.copyfile(source, mod/'maps/ad_tower.bsp')
    # Author-supplied intermission views cover the tower and a second area.
    qc = (ROOT/'tools/tests/gpu_rtt.qc').read_text()
    qc = qc.replace('    renderscene();', '''    setproperty(11, '151.608 0.321415 -142.786');
    setproperty(15, '-35 138 0');
    if (cvar("gpu_stream_camera") == 1) setproperty(15, [0, gettimef(0)*35, 0]);
    if (cvar("gpu_stream_camera") == 2) {
        setproperty(11, '-1145.07 -1074.07 -313.308');
        setproperty(15, '-13 -59 0');
    }
    renderscene();''')
    (out/'stream.qc').write_text(qc)
    subprocess.run([str(qcc), '-srcfile', str(out/'stream.qc'),
                    '-o', str(mod/'gpu-rtt.dat')], cwd=ROOT, check=True)
    commands = [
        'gamedir ad_v1_80p1final', 'ruleset_allow_in 1', 'cfg_save_auto 0',
        'm_preset_chosen 1', 'con_notifytime 0', 'sv_public 0',
        'sv_csqc_progname gpu-rtt.dat', 'r_drawviewmodel 0', 'cl_maxfps 1000',
        'cl_idlefps 0', 'cl_yieldcpu 0', 'vid_vsync 0',
        'gpu_texturestream 1', 'gpu_texturebudget 512', 'vid_fullscreen 2', 'vid_restart',
        'map ad_tower', 'defer 6 pause', 'defer 7 fps_preset normal', 'defer 7.1 vid_vsync 0',
        # Full-resolution reference from the same prepared cache and scene.
        'defer 8 gpu_texturestream 0', 'defer 14 gpu_info',
        'defer 15 screenshot stream-reference.png',
        'defer 16 gpu_benchmark 5 resident-a',
        'defer 22 gpu_texturestream 1', 'defer 28 gpu_info',
        'defer 29 screenshot stream-enabled.png',
        'defer 30 gpu_benchmark 5 streaming',
        'defer 36 gpu_texturebudget 32', 'defer 42 gpu_info',
        'defer 43 screenshot stream-pressure.png',
        'defer 44 gpu_texturebudget 64', 'defer 44.1 set gpu_stream_camera 1', 'defer 45 gpu_benchmark 8 sweep',
        'defer 54 gpu_info', 'defer 55 set gpu_stream_camera 2',
        'defer 60 screenshot stream-moved.png', 'defer 61 gpu_info',
        'defer 62 set gpu_stream_camera 0', 'defer 63 gpu_texturebudget 512',
        'defer 64 gpu_texturestream 0', 'defer 70 gpu_info',
        'defer 71 screenshot stream-restored.png',
        'defer 72 gpu_benchmark 5 resident-b',
        'defer 78 gpu_texturestream 1', 'defer 78.1 vid_restart',
        'defer 85 gpu_info', 'defer 86 screenshot stream-restart.png',
        'defer 87 map e1m1', 'defer 93 gpu_info',
        'defer 94 screenshot stream-map.png', 'defer 95 quit force',
    ]
    commands = [re.sub(r'^defer ([\d.]+) ',
                       lambda m: f'defer {float(m[1])+warmup:g} ', command)
                for command in commands]
    (out/'id1/gpu-test.cfg').write_text('\n'.join(commands)+'\n')


def check(out):
    from PIL import Image, ImageChops, ImageStat
    log = (out/'stream.log').read_text(errors='replace')
    backend = 'metal' if sys.platform == 'darwin' else 'vulkan'
    if f'SDL3 GPU renderer: {backend}' not in log or 'Server ended' not in log:
        raise RuntimeError('Native GPU run did not complete')
    if re.search(r'Error:|GPU.*[Vv]alidation [Ee]rror|initialization failed|Sys_Error|Bad builtin|runaway loop|Unknown command "gpu_texture|runtime error:|AddressSanitizer', log):
        raise RuntimeError('Engine/GPU error; inspect stream.log')
    pattern = (r'Texture streaming: (enabled|disabled); ([\d.]+) MiB allocated, ([\d.]+) MiB budget; '
               r'(\d+) managed, (\d+) reduced; ([\d.]+)/([\d.]+) MiB resident/full; '
               r'(\d+) pending, ([\d.]+) MiB ready; (\d+) uploads, ([\d.]+) MiB uploaded, '
               r'(\d+) failures; ([\d.]+) MiB temporary cache')
    fields = ('allocated_mib', 'budget_mib', 'managed', 'reduced', 'resident_mib',
              'full_mib', 'pending', 'ready_mib', 'uploads', 'upload_mib', 'failures', 'cache_mib')
    records = [dict(enabled=m[0] == 'enabled', **dict(zip(fields, map(float, m[1:]))))
               for m in re.findall(pattern, log)]
    if len(records) < 8:
        raise RuntimeError(f'Missing streaming diagnostics: {len(records)}')
    if any(r['failures'] for r in records):
        raise RuntimeError('Streaming read/upload failure')
    reference, enabled, pressure, sweep, moved, restored, restart, changed_map = records[:8]
    if not enabled['managed'] or enabled['resident_mib'] >= enabled['full_mib']*0.9:
        raise RuntimeError('Streaming did not reduce texture residency')
    if pressure['resident_mib'] >= enabled['resident_mib']:
        raise RuntimeError('Lower budget did not reduce texture residency')
    if restored['reduced'] or restored['pending']:
        raise RuntimeError('Disabling streaming did not restore all mipmaps')
    if not restart['enabled'] or not restart['managed'] or log.count('renderer initialized') < 2:
        raise RuntimeError('Video restart did not recreate streaming sources')
    if changed_map['managed'] or changed_map['cache_mib']:
        raise RuntimeError('Previous map streaming sources/cache were not released')
    if sweep['budget_mib'] == 64 and sweep['uploads'] <= pressure['uploads']:
        raise RuntimeError('Moving camera did not stream new mip data')
    maps = re.findall(r'Lightmaps: (\d+)/(\d+) loaded, (\d+) draws using fallback', log)
    if not maps or any(a != b or int(c) for a, b, c in maps):
        raise RuntimeError('Streaming lost world lightmaps')
    directory = out/'ad_v1_80p1final'
    images = {}
    for name in ('reference', 'enabled', 'pressure', 'moved', 'restored', 'restart', 'map'):
        im = Image.open(directory/f'stream-{name}.png').convert('RGB')
        im.load(); images[name] = im
    # Pause fixes map animation. The crop omits HUD and console notifications.
    w, h = images['reference'].size
    box = (round(w*0.05), round(h*0.1), round(w*0.95), round(h*0.8))
    diffs = {name: sum(ImageStat.Stat(ImageChops.difference(
        images['reference'].crop(box), images[name].crop(box))).mean)/3
        for name in ('enabled', 'restored')}
    if max(diffs.values()) > 0.05:
        raise RuntimeError(f'Streaming/restoration changed paused scene pixels: {diffs}')
    result = {'backend': backend, 'map': 'ad_tower', 'snapshots': records,
              'pixel_mean_differences': diffs,
              'benchmarks': re.findall(r'SDL GPU benchmark (?!started:).*', log),
              'restart_mapchange_lightmaps': 'passed'}
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare', 'run', 'check'))
    parser.add_argument('path', nargs='?', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--qcc', type=Path, default=ROOT/'build/fteqcc')
    parser.add_argument('--debug', action='store_true')
    parser.add_argument('--warmup', type=float, default=0,
                        help='extra seconds before captures; use 20 for debug/sanitizer builds')
    args = parser.parse_args()
    out = args.out.resolve()
    if args.action == 'prepare':
        if not args.path: parser.error('prepare requires owned Quake/AD data')
        prepare(args.path.resolve(), out, args.qcc.resolve(), max(0,args.warmup))
    elif args.action == 'run':
        if not args.path: parser.error('run requires the engine executable')
        with (out/'stream.log').open('w') as log:
            subprocess.run([str(args.path.resolve()), '-nohome', '-nosound', '-basedir', str(out), '-quake',
                            '-window', '-width', '1280', '-height', '720',
                            *(['-gpu_debug'] if args.debug else []),
                            '+vid_renderer', 'sdlgpu', '+exec', 'gpu-test.cfg'],
                           stdout=log, stderr=subprocess.STDOUT, timeout=300, check=True)
        check(out)
    else:
        check(out)


if __name__ == '__main__':
    main()
