#!/usr/bin/env python3
"""Validate SDL GPU geometry caching, clustered lights, shadow culling and bloom.

Uses owned Quake/AD PAKs through the existing isolated integration fixture.
Run geometry without --debug for performance, effects with --debug for GPU
validation. Original game data is only read. Use a fresh --out per run.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

from gpu_integration import ROOT, prepare as prepare_fixture


def prepare(game, out, qcc, profile, fullscreen=False):
    if out.exists():
        raise ValueError('Use a new output directory')
    prepare_fixture(game, out, qcc, 'tears' if profile == 'geometry' else 'graphics')
    common = ['ruleset_allow_in 1', 'cfg_save_auto 0', 'm_preset_chosen 1',
              'con_notifytime 0', 'sv_public 0', 'cl_maxfps 1000', 'cl_idlefps 0',
              'cl_yieldcpu 0', 'vid_vsync 0', 'r_drawviewmodel 0', 'defer 4 gpu_batching 1',
              'sv_csqc_progname gpu-rtt.dat']
    if profile == 'geometry':
        commands = ['gamedir ad_v1_80p1final', *common, 'vid_fullscreen 2', 'vid_restart',
                    'set gpu_test_tears 1', 'map ad_tears', 'defer 6 pause',
                    'defer 7 fps_preset normal', 'defer 8 gpu_staticgeometry 0',
                    'defer 10 screenshot renderer-stream.png',
                    'defer 11 gpu_benchmark 6 stream-a', 'defer 18 gpu_info',
                    'defer 19 gpu_staticgeometry 1',
                    'defer 21 screenshot renderer-static.png',
                    'defer 22 gpu_benchmark 6 static', 'defer 29 gpu_info',
                    'defer 30 gpu_staticgeometry 0',
                    'defer 31 gpu_benchmark 6 stream-b',
                    'defer 38 gpu_staticgeometry 1', 'defer 39 vid_restart',
                    'defer 43 screenshot renderer-restart.png', 'defer 44 gpu_info',
                    'defer 45 set gpu_test_tears 0', 'defer 45.1 set gpu_test_static 1',
                    'defer 46 map e1m1', 'defer 51 screenshot renderer-map.png',
                    'defer 52 gpu_info', 'defer 53 quit force']
    else:
        (out/'id1/maps/start.rtlights').write_text(''.join(
            f'{400+(i%8)*40} {500+((i//8)%4)*40} {60+(i%3)*20} 1500 0.012 0.008 0.004 0 "" 0 0 0 0 0.25 0 1 1 2\n'
            for i in range(160)))
        qc = (ROOT/'tools/tests/gpu_rtt.qc').read_text()
        qc = qc.replace("    dynamiclight_add('394 730 84', 600, '1 0.6 0.3', 0, \"\", 0);", '''    float i, count;
    vector pos;
    if (!cvar("gpu_test_water")) {
        setproperty(11, '544 288 54');
        setproperty(15, '0 90 0');
    }
    count = cvar("gpu_renderer_lights");
    for (i = 0; i < count; i = i + 1) {
        pos = [400 + (i - floor(i/8)*8)*40, 500 + (floor(i/8) - floor(i/32)*4)*40, 60 + (i - floor(i/3)*3)*20];
        if (count == 1) dynamiclight_add(pos, 600, '1 0.6 0.3', 0, "", 0);
        else dynamiclight_add(pos, cvar("gpu_renderer_dense") ? 1500 : 220, '0.012 0.008 0.004', 0, "", 0);
    }''')
        qc = qc.replace('float(float timer) gettimef', 'float(float x) floor = #37;\nfloat(float timer) gettimef')
        qc = qc.replace('    renderscene();', '''    renderscene();
    if (cvar("gpu_renderer_split")) {
        clearscene(); addentities(3);
        setproperty(1, [width*0.5,0,0]);
        setproperty(4, [width*0.5,height,0]);
        setproperty(11, '544 288 54'); setproperty(15, '0 90 0');
        setproperty(8, '90 85 0');
        dynamiclight_add('540 500 100', 600, '0.1 0.3 1', 0, "", 0);
        renderscene();
    }
    if (cvar("gpu_renderer_bloom")) {
        drawfill([width*0.25,height*0.25,0], [width*0.25,height*0.25,0], '1 1 1', 1, 0);
    }''')
        qc = qc.replace('    count = cvar("gpu_renderer_lights");', '''    if (cvar("gpu_renderer_split")) {
        setproperty(4, [width*0.5,height,0]); setproperty(8, '90 85 0');
    }
    count = cvar("gpu_renderer_lights");''')
        (out/'renderer.qc').write_text(qc)
        subprocess.run([str(qcc.resolve()), '-srcfile', str(out/'renderer.qc'),
                        '-o', str(out/'id1/gpu-rtt.dat')], cwd=ROOT, check=True)
        commands = [*common, 'set gpu_renderer_lights 1', 'set gpu_renderer_dense 0',
                    'gamma 1', 'contrast 1', 'brightness 0', 'v_contrastboost 1',
                    'r_shadow_realtime_world 0', 'r_shadow_realtime_dlight 1',
                    'r_shadow_realtime_dlight_shadows 0', 'r_dynamic 0',
                    'defer 4.1 r_bloom 0', 'r_fxaa 0', 'r_hdr_framebuffer 0',
                    'map start', 'defer 6 pause']
        time = 7

        def capture(name, *settings):
            nonlocal time
            for i, setting in enumerate(settings):
                commands.append(f'defer {time+i/20:g} {setting}')
            commands.extend([f'defer {time+1:g} screenshot renderer-{name}.png',
                             f'defer {time+1.2:g} gpu_info'])
            time += 2

        capture('legacy', 'gpu_clustered 0')
        capture('clustered', 'gpu_clustered 1')
        capture('full', 'gpu_clustered 2')
        capture('shadow-reference', 'gpu_clustered 1', 'r_shadow_realtime_dlight_shadows 1', 'gpu_shadowcull 0')
        capture('shadow-culled', 'gpu_shadowcull 1')
        capture('many-full', 'r_shadow_realtime_dlight_shadows 0', 'set gpu_renderer_lights 64', 'gpu_clustered 2')
        capture('many-clustered', 'gpu_clustered 1')
        capture('overflow-full', 'r_shadow_realtime_dlight 0', 'r_shadow_realtime_world 1',
                'r_shadow_realtime_world_shadows 0', 'r_shadow_realtime_world_lightmaps 1', 'gpu_clustered 2')
        capture('overflow-clustered', 'gpu_clustered 1')
        capture('panini-full', 'r_shadow_realtime_world 0', 'r_shadow_realtime_dlight 1',
                'set gpu_renderer_lights 64', 'set gpu_renderer_dense 0', 'r_projection 6', 'gpu_clustered 2')
        capture('panini-clustered', 'gpu_clustered 1')
        capture('split-legacy', 'r_projection 0', 'set gpu_renderer_lights 1', 'set gpu_renderer_split 1', 'gpu_clustered 0')
        capture('split-full', 'gpu_clustered 2')
        capture('split-clustered', 'gpu_clustered 1')
        capture('water-full', 'r_projection 0', 'set gpu_renderer_split 0', 'set gpu_renderer_lights 64',
                'set gpu_test_water 1', 'set gpu_renderer_dense 1',
                'r_hdr_framebuffer 1', 'r_waterstyle 3', 'r_wateralpha 0.35', 'gpu_clustered 2')
        capture('water-clustered', 'gpu_clustered 1')
        capture('bloom-off', 'set gpu_test_water 0', 'set gpu_renderer_lights 1', 'set gpu_renderer_bloom 1', 'r_bloom 0')
        capture('bloom-reference', 'r_bloom 1', 'gpu_bloom_pyramid 0')
        commands.append(f'defer {time:g} gpu_benchmark 4 bloom-reference')
        time += 5
        capture('bloom-pyramid', 'gpu_bloom_pyramid 1')
        commands.append(f'defer {time:g} gpu_benchmark 4 bloom-pyramid')
        time += 5
        capture('scene-ui', 'vid_sceneonlygamma 1')
        capture('scene-ui-off', 'r_bloom 0')
        capture('restart', 'r_bloom 1', 'vid_restart')
        commands.extend([f'defer {time:g} quit force'])
    if fullscreen and profile != 'geometry':
        commands = ['vid_fullscreen 2', 'vid_restart', *commands]
    (out/'id1/gpu-test.cfg').write_text('\n'.join(commands)+'\n')
    (out/'profile.txt').write_text(profile)


def check(out):
    from PIL import Image, ImageChops, ImageStat
    log = (out/'renderer.log').read_text(errors='replace')
    backend = 'metal' if sys.platform == 'darwin' else 'vulkan'
    if f'SDL3 GPU renderer: {backend}' not in log or 'Server ended' not in log:
        raise RuntimeError('Native GPU run did not complete')
    if re.search(r'Error:|GPU.*[Vv]alidation [Ee]rror|initialization failed|Sys_Error|Bad builtin|runaway loop',log):
        raise RuntimeError('Engine/GPU error; inspect renderer.log')
    if log.count('renderer initialized') < 2:
        raise RuntimeError('Video restart was not exercised')
    lightmaps = re.findall(r'Lightmaps: (\d+)/(\d+) loaded, (\d+) draws using fallback',log)
    if not lightmaps or any(a != b or int(c) for a,b,c in lightmaps):
        raise RuntimeError('Missing map lightmaps')
    profile = (out/'profile.txt').read_text()
    directory = out/('ad_v1_80p1final' if profile == 'geometry' else 'fte')
    diffs = {}

    def difference(a,b,limit=0.05,box=None):
        images = [Image.open(directory/f'renderer-{name}.png').convert('RGB') for name in (a,b)]
        if images[0].size != images[1].size:
            raise RuntimeError('Comparison resolutions differ')
        if box:
            w,h = images[0].size
            images = [im.crop(tuple(round(v*(w if i%2 == 0 else h)) for i,v in enumerate(box))) for im in images]
        delta = sum(ImageStat.Stat(ImageChops.difference(*images)).mean)/3
        diffs[f'{a}/{b}'] = delta
        if limit is not None and delta > limit:
            raise RuntimeError(f'{a}/{b} changed pixels: {delta:.6f}')
        return delta

    if profile == 'geometry':
        difference('stream','static',box=(0.1,0.08,0.95,0.72))
        geometry = [tuple(map(int,m)) for m in re.findall(r'Geometry: (\d+) cached, (\d+) streamed vertices, (\d+) bytes uploaded',log)]
        if len(geometry) < 4 or geometry[0][0] or not geometry[1][0] or geometry[1][2] >= geometry[0][2]*0.8:
            raise RuntimeError(f'Static geometry did not reduce upload traffic: {geometry}')
        for name in ('restart','map'):
            Image.open(directory/f'renderer-{name}.png').verify()
    else:
        for a,b in [('legacy','clustered'),('clustered','full'),('shadow-reference','shadow-culled'),
                    ('many-full','many-clustered'),('overflow-full','overflow-clustered'),
                    ('panini-full','panini-clustered'),('split-legacy','split-full'),
                    ('split-full','split-clustered'),('water-full','water-clustered')]:
            difference(a,b)
        if not re.search(r'shadows: [1-9]\d* submitted, [1-9]\d* culled',log):
            raise RuntimeError('Shadow culling was not exercised')
        clusters = [tuple(map(int,m)) for m in re.findall(r'Clustered lighting: (\d+) views, (\d+) lights, (\d+) indices, (\d+) overflow cells',log)]
        if not any(n > 32 for _,n,_,_ in clusters) or not any(n for *_,n in clusters):
            raise RuntimeError('Many-light/overflow coverage missing')
        if not any(v > 1 for v,_,_,_ in clusters):
            raise RuntimeError('Subview lighting coverage missing')
        if difference('bloom-off','bloom-pyramid',limit=None) < 0.05:
            raise RuntimeError('Bloom had no visible effect')
        difference('scene-ui','scene-ui-off',box=(0.26,0.26,0.49,0.49))
        Image.open(directory/'renderer-restart.png').verify()
    benchmarks = re.findall(r'SDL GPU benchmark (?!started:).*',log)
    result = {'backend':backend,'profile':profile,'pixel_differences':diffs,'benchmarks':benchmarks,
              'map_lightmaps_and_restart':'passed'}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('prepare','run','check'))
    parser.add_argument('path',nargs='?')
    parser.add_argument('--profile',choices=('geometry','effects'),default='effects')
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--qcc',type=Path,default=ROOT/'build/fteqcc')
    parser.add_argument('--debug',action='store_true')
    parser.add_argument('--fullscreen',action='store_true',help='prepare the effects profile in fullscreen for throughput measurements')
    args = parser.parse_args()
    out = args.out.resolve()
    if args.action == 'prepare':
        if not args.path: parser.error('prepare requires owned game data')
        prepare(args.path,out,args.qcc,args.profile,args.fullscreen)
    elif args.action == 'run':
        if not args.path: parser.error('run requires the engine executable')
        with (out/'renderer.log').open('w') as log:
            geometry = (out/'profile.txt').read_text() == 'geometry'
            subprocess.run([str(Path(args.path).resolve()),'-nohome','-basedir',str(out),'-quake',
                            '-window','-width','1470' if geometry else '800','-height','923' if geometry else '600',
                            *(['-gpu_debug'] if args.debug else []),
                            '+vid_renderer','sdlgpu','+exec','gpu-test.cfg'],stdout=log,stderr=subprocess.STDOUT,
                           timeout=150,check=True)
        check(out)
    else:
        check(out)


if __name__ == '__main__':
    main()
