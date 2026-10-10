#!/usr/bin/env python3
"""Prepare, run, or check an isolated SDL GPU integration test using owned Quake data.

No game data is downloaded. Originals are only read; all outputs live in --out.
Example: python3 tools/gpu_integration.py prepare /games/Quake --qcc build/fteqcc
         python3 tools/gpu_integration.py run build/fteqw
         python3 tools/gpu_integration.py check
"""
import argparse
import filecmp
import json
import math
import pathlib
import re
import shutil
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CAPTURES = ('baseline', 'shadow-off', 'normal-off', 'lights-off', 'dlight',
            'specular-off', 'offset-on', 'bloom', 'hdr', 'fxaa', 'water-classic', 'water-refract', 'water-refract-opaque', 'water-alpha035', 'water-reflect', 'second-map', 'restart', 'static-lit', 'fullbright', 'static-restored', 'video-menu', 'static-restart')
CONFIG = '''ruleset_allow_in 1
cfg_save_auto 0
m_preset_chosen 1
con_notifytime 0
r_fullbright 0
sv_csqc_progname gpu-rtt.dat
set gpu_test_rtt 0
set gpu_test_water 0
set gpu_test_slime 0
set gpu_test_ad 0
set gpu_test_tears 0
map start
defer 7 r_shadow_realtime_world 1
defer 7.1 r_shadow_realtime_world_lightmaps 0.5
defer 7.2 r_shadow_realtime_world_shadows 1
defer 7.3 r_shadow_realtime_dlight 0
defer 7.35 r_dynamic 0
defer 7.4 r_normalmapping 1
defer 7.5 gl_specular 1
defer 7.6 r_glsl_offsetmapping 0
defer 7.7 r_bloom 0
defer 7.8 r_hdr_framebuffer 0
defer 7.9 r_fxaa 0
defer 8 pause
defer 9 gpu_info
defer 10 screenshot gpu-baseline.png
defer 11 r_shadow_realtime_world_shadows 0
defer 12 screenshot gpu-shadow-off.png
defer 13 r_normalmapping 0
defer 14 screenshot gpu-normal-off.png
defer 15 r_normalmapping 1
defer 16 gl_specular 0
defer 17 screenshot gpu-specular-off.png
defer 18 gl_specular 1
defer 19 r_glsl_offsetmapping 1
defer 20 screenshot gpu-offset-on.png
defer 21 r_glsl_offsetmapping 0
defer 22 r_shadow_realtime_world 0
defer 23 screenshot gpu-lights-off.png
defer 24 r_shadow_realtime_dlight 1
defer 25 screenshot gpu-dlight.png
defer 26 r_shadow_realtime_dlight 0
defer 27 r_bloom 1
defer 28 screenshot gpu-bloom.png
defer 29 r_hdr_framebuffer 1
defer 30 screenshot gpu-hdr.png
defer 31 r_fxaa 1
defer 32 screenshot gpu-fxaa.png
defer 33 gpu_info
defer 34 set gpu_test_water 1
defer 35 r_waterstyle 1
defer 36 screenshot gpu-water-classic.png
defer 37 r_waterstyle 2
defer 37.1 r_wateralpha 0.35
defer 38 screenshot gpu-water-refract.png
defer 39 r_waterstyle 3
defer 40 screenshot gpu-water-reflect.png
defer 41 gpu_info
defer 42 r_waterstyle 1
defer 42.1 r_wateralpha 1
defer 42.5 set gpu_test_water 0
defer 43 set gpu_test_rtt 1
defer 44 map e1m1
defer 49 screenshot gpu-second-map.png
defer 50 vid_restart
defer 55 screenshot gpu-restart.png
defer 56 set gpu_test_rtt 0
defer 56.1 r_bloom 0
defer 56.2 r_hdr_framebuffer 0
defer 56.3 r_fxaa 0
defer 57 screenshot gpu-static-lit.png
defer 58 r_fullbright 1
defer 59 screenshot gpu-fullbright.png
defer 60 r_fullbright 0
defer 61 screenshot gpu-static-restored.png
defer 62 menu_video
defer 63 screenshot gpu-video-menu.png
defer 64 closemenu
defer 65 vid_restart
defer 70 gpu_info
defer 71 screenshot gpu-static-restart.png
defer 72 map start
defer 76 set gpu_test_water 1
defer 76.1 r_waterstyle 2
defer 77 r_wateralpha 0.35
defer 79 screenshot gpu-water-alpha035.png
defer 80 r_wateralpha 1
defer 82 screenshot gpu-water-refract-opaque.png
defer 83 quit force
'''

def prepare(game, out, qcc, profile="graphics"):
    from PIL import Image
    game = pathlib.Path(game).resolve()
    source = game / 'id1'
    if not (source / 'pak0.pak').is_file():
        raise RuntimeError(f'{source}/pak0.pak is missing')
    if source == (out / 'id1').resolve() or game == out:
        raise RuntimeError('Test output must be separate from original game data')
    (out / 'id1/maps').mkdir(parents=True, exist_ok=True)
    (out / 'fte').mkdir(exist_ok=True)
    for name in ('pak0.pak', 'pak1.pak'):
        original = source / name
        dest = out / 'id1' / name
        if not original.exists():
            continue
        if dest.exists():
            if dest.resolve() != original and not filecmp.cmp(dest, original, shallow=False):
                raise RuntimeError(f'Refusing to overwrite existing {dest}')
        else:
            try:
                dest.symlink_to(original)
            except OSError:
                shutil.copyfile(original, dest)  # Windows without symlink privileges.
    (out / 'id1/maps/start.rtlights').write_text(
        '394 730 84 600 1 0.6 0.3 0 "" 0 0 0 0 0.25 0 1 1 2\n')
    textures = out / 'id1/textures/start'
    textures.mkdir(parents=True, exist_ok=True)
    normal = Image.new('RGBA', (64, 64))
    for y in range(64):
        for x in range(64):
            nx, ny = math.sin(x*math.pi/8)*0.5, math.cos(y*math.pi/8)*0.5
            nz = math.sqrt(1-nx*nx-ny*ny)
            normal.putpixel((x,y), (round((nx+1)*127.5), round((ny+1)*127.5),
                                    round((nz+1)*127.5), round(128+100*math.sin(x*math.pi/8))))
    for name in ('wizmet1_2', 'wizmet1_1', 'wiz1_4', 'wbrick1_5', 'city4_6',
                 'city4_7', 'sfloor4_5', 'sfloor4_2', 'quake'):
        normal.save(textures / f'{name}_norm.tga')
        Image.new('RGBA', (64,64), (180,180,180,128)).save(textures / f'{name}_gloss.tga')
    (out / 'id1/scripts').mkdir(exist_ok=True)
    shutil.copyfile(ROOT/'tools/tests/gpu_rtt.shader', out/'id1/scripts/gpu_rtt.shader')
    subprocess.run([str(pathlib.Path(qcc).resolve()), '-srcfile', str(ROOT/'tools/tests/gpu_rtt.qc'),
                    '-o', str(out/'id1/gpu-rtt.dat')], check=True, cwd=ROOT)
    if profile in ("ad", "tears"):
        modsource = game/'ad_v1_80p1final'
        if not (modsource/'pak0.pak').is_file():
            raise RuntimeError(f'{modsource}/pak0.pak is missing')
        mod = out/'ad_v1_80p1final'
        mod.mkdir(exist_ok=True)
        for original in modsource.glob('pak*.pak'):
            dest = mod/original.name
            if not dest.exists():
                try:
                    dest.symlink_to(original)
                except OSError:
                    shutil.copyfile(original,dest)
            elif dest.resolve() != original and not filecmp.cmp(dest,original,shallow=False):
                raise RuntimeError(f'Refusing to overwrite existing {dest}')
        shutil.copyfile(out/'id1/gpu-rtt.dat',mod/'gpu-rtt.dat')
    config = CONFIG if profile == "graphics" else (ROOT/f"tools/tests/gpu_{profile}.cfg").read_text()
    music = source / 'music/track02.ogg'
    if music.exists():
        dest = out / 'id1/music/track02.ogg'
        dest.parent.mkdir(exist_ok=True)
        if not dest.exists():
            try:
                dest.symlink_to(music)
            except OSError:
                shutil.copyfile(music, dest)
        config += 'defer 5 music music/track02.ogg\ndefer 6.5 soundlist\n'
    (out / 'id1/gpu-test.cfg').write_text(config)
    captures = CAPTURES if profile == 'graphics' else tuple(profile+'-'+name for name in {'ad':AD_CAPTURES, 'slime':SLIME_CAPTURES, 'tears':TEARS_CAPTURES}[profile])
    for name in captures:
        capture = out / ('ad_v1_80p1final' if profile in ('ad','tears') else 'fte') / f'gpu-{name}.png'
        if capture.exists():
            capture.unlink()
    print(f'Prepared isolated test data in {out}')

def check(out, log):
    from PIL import Image, ImageChops, ImageStat
    text = pathlib.Path(log).read_text(errors='replace')
    expected = 'metal' if sys.platform == 'darwin' else 'vulkan'
    if f'SDL3 GPU renderer: {expected}' not in text:
        raise RuntimeError(f'Expected native {expected} backend')
    if re.search(r'(Error:|GPU.*[Vv]alidation [Ee]rror|initialization failed|Sys_Error|invalid string offset|runaway loop|Bad builtin)', text):
        raise RuntimeError('Engine/GPU validation error; inspect log')
    if text.count('renderer initialized') < 2:
        raise RuntimeError('Video restart did not complete')
    if not re.search(r'reflection/refraction [1-9]\d*/[1-9]\d* draws', text):
        raise RuntimeError('Reflection/refraction subviews were not rendered')
    lightmaps = re.findall(r'Lightmaps: (\d+)/(\d+) loaded, (\d+) draws using fallback', text)
    if not lightmaps or any(int(total) == 0 or loaded != total or int(fallback) for loaded,total,fallback in lightmaps):
        raise RuntimeError('Map lightmaps are missing or world draws use the white fallback')
    if 'Fixed initialvalue for Size' in text or 'ogg vorbis library is not loaded' in text or "Couldn't load library:" in text:
        raise RuntimeError('Resolution menu or Vorbis regression; inspect log')
    if (out/'id1/music/track02.ogg').exists() and not re.search(r'\(16b 2c\).*\d+s : music/track02\.ogg', text):
        raise RuntimeError('Ogg music did not decode to stereo PCM')
    frames = {}
    for name in CAPTURES:
        im = Image.open(out / 'fte' / f'gpu-{name}.png').convert('RGB')
        im.load()
        if min(im.size) < 200 or max(ImageStat.Stat(im).stddev) < (3 if name.startswith("water-") else 8):
            raise RuntimeError(f'{name}: empty or implausible frame')
        frames[name] = im
    results = {}
    # Each pair changes one control, while the simulation is paused.
    for feature, a, b, threshold in (
        ('static map lighting', 'static-lit', 'fullbright', 5),
        ('shadows', 'baseline', 'shadow-off', 0.01),
        ('normal mapping', 'shadow-off', 'normal-off', 0.05),
        ('specular mapping', 'shadow-off', 'specular-off', 0.001),
        ('offset mapping', 'shadow-off', 'offset-on', 0.001),
        ('world lighting', 'shadow-off', 'lights-off', 0.1),
        ('dynamic lighting', 'lights-off', 'dlight', 0.01),
        ('bloom', 'lights-off', 'bloom', 0.001),
        ('HDR', 'bloom', 'hdr', 0.001),
        ('FXAA', 'hdr', 'fxaa', 0.001),
        ('refraction', 'water-classic', 'water-refract', 0.05),
        ('reflection', 'water-refract', 'water-reflect', 0.01)):
        difference = sum(ImageStat.Stat(ImageChops.difference(frames[a],frames[b])).mean)/3
        if difference <= threshold:
            raise RuntimeError(f'{feature} produced no meaningful image change ({difference})')
        results[feature] = difference
    # Verify opacity in a settled scene away from console notifications and UI.
    box = (200,200,1200,850)
    delta = ImageChops.difference(frames['water-alpha035'].crop(box),frames['water-refract-opaque'].crop(box))
    results['water opacity'] = sum(ImageStat.Stat(delta).mean)/3
    if results['water opacity'] <= 1:
        raise RuntimeError('Water opacity did not visibly change the surface')
    # Compare map geometry away from animated sky/torches, viewmodels, and HUD.
    # This must recover the original static lighting after the cvar toggle and restart.
    for name in ('static-restored', 'static-restart'):
        im = frames['static-lit']
        crop = (im.width//4, im.height//4, im.width*3//4, im.height//2)
        difference = sum(ImageStat.Stat(ImageChops.difference(im.crop(crop), frames[name].crop(crop))).mean)/3
        if difference > 1:
            raise RuntimeError(f'{name}: static lighting was not restored ({difference})')
    # CSQC creates a 64x64 target with red/blue halves and displays it at (16,16).
    # Default virtual width is 320; use the screenshot's scale to locate its interior.
    for name in ('second-map','restart'):
        im = frames[name]
        sx, sy = im.width/320, im.height/240
        red = im.getpixel((round(40*sx),round(60*sy)))
        blue = im.getpixel((round(115*sx),round(60*sy)))
        if not (red[0] > red[2]+50 and blue[2] > blue[0]+50):
            raise RuntimeError(f'{name}: offscreen target not displayed correctly ({red}, {blue})')
    result = {'backend': expected, 'size': frames['baseline'].size,
              'mean_pixel_difference': results, 'offscreen_and_restart': 'passed',
              'static_lighting_toggle_and_restart': 'passed', 'resolution_menu': 'passed',
              'ogg_decode': 'passed' if (out/'id1/music/track02.ogg').exists() else 'no music supplied'}
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))

SLIME_CAPTURES = ('normal', 'nice', 'realtime', 'transparent', 'reflect',
                  'underwater', 'underwater-opaque', 'warp-a', 'warp-b', 'restart')

def check_slime(out, log):
    from PIL import Image, ImageChops, ImageStat
    text = pathlib.Path(log).read_text(errors='replace')
    expected = 'metal' if sys.platform == 'darwin' else 'vulkan'
    if f'SDL3 GPU renderer: {expected}' not in text or 'Server ended' not in text:
        raise RuntimeError('Slime run did not complete on the native backend')
    if re.search(r'(Error:|GPU.*[Vv]alidation [Ee]rror|initialization failed|Sys_Error|invalid string offset|runaway loop|Bad builtin)', text):
        raise RuntimeError('Engine/GPU error in slime run')
    if not re.search(r'reflection/refraction [1-9]\d*/[1-9]\d* draws', text):
        raise RuntimeError('Slime reflection/refraction views were not rendered')
    frames = {}
    for name in SLIME_CAPTURES:
        im = Image.open(out/'fte'/f'gpu-slime-{name}.png').convert('RGB')
        im.load()
        if min(im.size) < 200 or max(ImageStat.Stat(im).stddev) < 3:
            raise RuntimeError(f'{name}: empty slime view')
        frames[name] = im
    def region(im, box):
        return im.crop(tuple(round(v*(im.width if i%2 == 0 else im.height)) for i,v in enumerate(box)))
    pool = (0.25, 0.5, 0.40, 0.75)
    baseline = ImageStat.Stat(region(frames['normal'],pool)).mean
    for name in ('nice','realtime','restart'):
        means = ImageStat.Stat(region(frames[name],pool)).mean
        if max(abs(a-b)/max(a,1) for a,b in zip(baseline,means)) > 0.25:
            raise RuntimeError(f'{name}: slime lost its surface colour ({means} vs {baseline})')
        wall = region(frames[name],(0.1,0.05,0.75,0.3))
        pixels = wall.get_flattened_data() if hasattr(wall, 'get_flattened_data') else wall.getdata()
        black = sum(max(rgb) < 2 for rgb in pixels)/(wall.width*wall.height)
        if black > 0.02:
            raise RuntimeError(f'{name}: directional lighting blackened room surfaces ({black})')
    differences = {}
    for feature,a,b,threshold in (('transparency','realtime','transparent',0.1),
                                  ('reflection','transparent','reflect',0.01),
                                  ('animated surface','warp-a','warp-b',0.1)):
        delta = ImageChops.difference(region(frames[a],pool),region(frames[b],pool))
        value = sum(ImageStat.Stat(delta).mean)/3
        if value <= threshold:
            raise RuntimeError(f'Slime {feature} has no visible effect ({value})')
        differences[feature] = value
    result = {'backend': expected, 'preset_slime_visibility': 'passed',
              'directional_lightmaps': 'passed', 'underwater_and_restart': 'passed',
              'mean_pixel_difference': differences}
    (out/'slime-results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

AD_CAPTURES = ('normal','nice','realtime','reflect','sky-up','fastsky',
               'sky-restart','restart','sky-override','sky-restored',
               'warp-a','warp-b','normal-restored')

def check_ad(out, log):
    from PIL import Image, ImageChops, ImageStat
    text = pathlib.Path(log).read_text(errors='replace')
    expected = 'metal' if sys.platform == 'darwin' else 'vulkan'
    if f'SDL3 GPU renderer: {expected}' not in text or 'Server ended' not in text:
        raise RuntimeError('AD run did not complete on the native backend')
    if re.search(r'(Error:|GPU.*[Vv]alidation [Ee]rror|initialization failed|Sys_Error|invalid string offset|runaway loop|Bad builtin)', text):
        raise RuntimeError('Engine/GPU error in AD run')
    if text.count('renderer initialized') < 2:
        raise RuntimeError('AD video restart did not complete')
    materials = re.findall(r'material draws (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)',text)
    if len(materials) < 4 or any(not int(m[6]) for m in materials):
        raise RuntimeError('Native cube sky missing across presets or restart')
    if not re.search(r'reflection/refraction [1-9]\d*/[1-9]\d* draws',text):
        raise RuntimeError('AD reflection/refraction views were not rendered')
    lightmaps = re.findall(r'Lightmaps: (\d+)/(\d+) loaded, (\d+) draws using fallback',text)
    if not lightmaps or any(loaded != total or int(fallback) for loaded,total,fallback in lightmaps):
        raise RuntimeError('AD world lightmaps are missing')
    frames = {name: Image.open(out/'ad_v1_80p1final'/f'gpu-ad-{name}.png').convert('RGB') for name in AD_CAPTURES}
    def region(im,box):
        return im.crop(tuple(round(v*(im.width if i%2==0 else im.height)) for i,v in enumerate(box)))
    pool = (0.04,0.85,0.25,0.98)
    baseline = ImageStat.Stat(region(frames['normal'],pool)).mean
    for name in ('nice','realtime','restart','normal-restored'):
        means = ImageStat.Stat(region(frames[name],pool)).mean
        if means[0] < 30 or means[0] < max(means[1],means[2])*4 or abs(means[0]-baseline[0])/baseline[0] > 0.25:
            raise RuntimeError(f'{name}: blood lost its red surface ({means} vs {baseline})')
    differences = {}
    for feature,a,b,box,minimum in (
        ('reflection','realtime','reflect',pool,0.01),
        ('sky texture','sky-up','fastsky',(0.05,0.08,0.95,0.8),1),
        ('sky override','restart','sky-override',(0.53,0.04,0.58,0.07),1),
        ('surface animation','warp-a','warp-b',pool,0.1)):
        delta = ImageChops.difference(region(frames[a],box),region(frames[b],box))
        value = sum(ImageStat.Stat(delta).mean)/3
        if value <= minimum:
            raise RuntimeError(f'AD {feature} has no visible effect ({value})')
        differences[feature] = value
    # Static sky pixels must survive restart and a temporary user sky override.
    for a,b in (('normal','nice'),('normal','realtime'),('sky-up','sky-restart'),('restart','sky-restored')):
        box = (0.53,0.04,0.58,0.07)
        delta = ImageChops.difference(region(frames[a],box),region(frames[b],box))
        if max(ImageStat.Stat(delta).mean) > 1:
            raise RuntimeError(f'AD sky changed after restoring/restarting ({a}/{b})')
    result = {'backend':expected, 'blood_preset_visibility':'passed',
              'skybox_and_restart':'passed','mean_pixel_difference':differences}
    (out/'ad-results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

TEARS_CAPTURES = ('normal', 'plants-normal', 'nice', 'plants-nice',
                  'realtime', 'plants-realtime', 'restart')

def check_tears(out, log):
    from PIL import Image, ImageStat
    text = pathlib.Path(log).read_text(errors='replace')
    expected = 'metal' if sys.platform == 'darwin' else 'vulkan'
    if f'SDL3 GPU renderer: {expected}' not in text or 'Server ended' not in text:
        raise RuntimeError('Tears run did not complete on the native backend')
    if re.search(r'(Error:|GPU.*[Vv]alidation [Ee]rror|initialization failed|Sys_Error|invalid string offset|runaway loop|Bad builtin)', text):
        raise RuntimeError('Engine/GPU error in Tears run')
    if text.count('renderer initialized') < 2:
        raise RuntimeError('Tears video restart did not complete')
    cutouts = re.findall(r'Cutouts: (\d+) draws, (\d+) using missing texture, (\d+) shadow caster draws',text)
    if len(cutouts) < 5 or any(not int(draws) or int(missing) for draws,missing,_ in cutouts):
        raise RuntimeError('Cutout textures missing across presets/restart')
    if not any(int(shadows) for _,_,shadows in cutouts):
        raise RuntimeError('No cutout shadow caster batches were exercised')
    lightmaps = re.findall(r'Lightmaps: (\d+)/(\d+) loaded, (\d+) draws using fallback',text)
    if not lightmaps or any(loaded != total or int(fallback) for loaded,total,fallback in lightmaps):
        raise RuntimeError('Tears world lightmaps are missing')
    for name in TEARS_CAPTURES:
        im = Image.open(out/'ad_v1_80p1final'/f'gpu-tears-{name}.png').convert('RGB')
        if min(im.size) < 200 or max(ImageStat.Stat(im).stddev) < 8:
            raise RuntimeError(f'{name}: empty Tears view')
    result = {'backend':expected, 'cutout_base_textures_across_presets':'passed',
              'cutout_shadow_batches':'passed', 'lightmaps_and_restart':'passed',
              'cutout_draw_counts':[int(draws) for draws,_,_ in cutouts]}
    (out/'tears-results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare','run','check'))
    parser.add_argument('path', nargs='?')
    parser.add_argument('--profile', choices=('graphics','slime','ad','tears'), default='graphics')
    parser.add_argument('--out', default=str(ROOT/'build/testdata'))
    parser.add_argument('--qcc', default=str(ROOT/'build/fteqcc'))
    parser.add_argument('--log', default=str(ROOT/'build/map-smoke.log'))
    args = parser.parse_args()
    out = pathlib.Path(args.out).resolve()
    validate = {'graphics':check, 'slime':check_slime, 'ad':check_ad, 'tears':check_tears}[args.profile]
    if args.action == 'prepare':
        if not args.path:
            parser.error('prepare requires the folder containing id1/pak0.pak')
        prepare(args.path, out, args.qcc, args.profile)
    elif args.action == 'run':
        if not args.path:
            parser.error('run requires the engine executable')
        with open(args.log, 'w') as log:
            subprocess.run([str(pathlib.Path(args.path).resolve()), '-nohome', '-basedir', str(out),
                            '-quake', '-window', '-width', '800', '-height', '600', '-gpu_debug',
                            '+vid_renderer', 'sdlgpu', '+exec', 'gpu-test.cfg'],
                           stdout=log, stderr=subprocess.STDOUT, timeout=120, check=True)
        validate(out, args.log)
    else:
        validate(out, args.log)

if __name__ == '__main__':
    main()
