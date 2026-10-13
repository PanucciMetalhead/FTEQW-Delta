#!/usr/bin/env python3
"""Exercise Q3 cgame camera, movement, weapon drawing and video restart on SDL GPU.

Use owned OpenArena data; assets are only read and outputs are isolated.
Requires Pillow for screenshot comparisons and a built Quake 3 plugin beside
the engine. Example:
  python3 tools/gpu_openarena.py prepare '/games/OpenArena'
  python3 tools/gpu_openarena.py run
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CONFIG = '''cfg_save_auto 0
m_preset_chosen 1
con_notifytime 0
sv_public 0
ruleset_allow_in 1
vid_conautoscale 0
vid_conwidth 800
vid_conheight 600
cl_yawspeed 140
plug_load quake3
set bot_enable 0
defer 1 devmap pxlfan
defer 5 closemenu
defer 5.5 cmd noclip
defer 6 cmd setviewpos 0 -680 400 90
defer 8 screenshot oa-start.png
defer 9 +right
defer 9.3 -right
defer 11 screenshot oa-turned.png
defer 12 +back
defer 12.75 -back
defer 14 screenshot oa-moved.png
defer 15 vid_restart
defer 18 screenshot oa-restarted.png
defer 19 cg_drawGun 0
defer 20 screenshot oa-no-weapon.png
defer 21 gpu_info
defer 22 quit force
'''

MATERIAL_CONFIG = '''cfg_save_auto 0
m_preset_chosen 1
con_notifytime 0
sv_public 0
ruleset_allow_in 1
vid_conautoscale 0
vid_conwidth 800
vid_conheight 600
r_fullbright 0
plug_load quake3
defer 1 devmap pxlfan
defer 4 closemenu
defer 4.5 cmd noclip
defer 5 cmd setviewpos 0 -300 100 90
defer 6 +lookdown
defer 6.2 -lookdown
defer 7 cg_drawGun 0
defer 8 screenshot oa-fan.png
defer 9 gpu_info
defer 10 vid_restart
defer 13 screenshot oa-fan-restart.png
defer 14 cmd setviewpos 0 -680 400 90
defer 16 screenshot oa-health.png
defer 17 r_showshader smallCrossEnv
defer 18 r_showshader mediumCrossEnv
defer 19 r_showshader megaCrossEnv
defer 20 gpu_info
defer 21 quit force
'''


CONTROL_CONFIG = """cfg_save_auto 0
m_preset_chosen 1
con_notifytime 0
sv_public 0
ruleset_allow_in 1
vid_conautoscale 0
vid_conwidth 800
vid_conheight 600
gamma 1
contrast 1
brightness 0
v_contrastboost 1
r_projection 0
plug_load quake3
defer 1 devmap pxlfan
defer 4 ui_restart off
defer 4.5 cmd noclip
defer 5 cmd setviewpos 0 -680 400 90
defer 6 cg_drawGun 0
defer 6.1 cg_draw2D 0
defer 7 r_fullbright 0
defer 7.1 fov 90
defer 8 screenshot oa-lit.png
defer 9 r_fullbright 1
defer 10 screenshot oa-fullbright.png
defer 11 r_fullbright 0
defer 12 screenshot oa-restored.png
defer 13 fov 120
defer 14 screenshot oa-fov120.png
defer 15 cg_fov 170
defer 16 screenshot oa-fov170.png
defer 17 fov 175
defer 18 screenshot oa-fov175.png
defer 19 +zoom
defer 22 screenshot oa-zoom.png
defer 23 -zoom
defer 26 screenshot oa-unzoomed.png
defer 26.2 gpu_info
defer 27 menu_options
defer 28 screenshot oa-fov-menu.png
defer 29 cfg_save oa-controls
defer 30 quit force
"""



SKY_CONFIG = """cfg_save_auto 0
m_preset_chosen 1
con_notifytime 0
sv_public 0
ruleset_allow_in 1
set bot_enable 0
gamma 1
contrast 1
brightness 0
v_contrastboost 1
r_projection 0
set r_bloom 0
r_fxaa 0
fov 90
cl_pitchspeed 1000
plug_load quake3
alias sky_camera "ui_restart off; cmd noclip; cmd setviewpos 0 0 1100 90; cg_drawGun 0; cg_draw2D 0; +lookup; defer 0.2 -lookup"
defer 1 devmap gpu-sky-low
defer 7 sky_camera
defer 10 screenshot oa-sky-low.png
defer 11 devmap gpu-sky-high
defer 14 sky_camera
defer 16 screenshot oa-sky-high.png
defer 17 devmap gpu-sky-low
defer 20 sky_camera
defer 22 screenshot oa-sky-restored.png
defer 23 devmap pxlfan
defer 26 sky_camera
defer 28 screenshot oa-cloud-a.png
defer 32 screenshot oa-cloud-b.png
defer 33 r_fastskycolour 1 0 1
defer 33.1 r_fastsky 1
defer 34 screenshot oa-sky-fast.png
defer 35 r_fastsky 0
defer 36 vid_restart
defer 39 screenshot oa-cloud-restart.png
defer 40 devmap gpu-sky-box
defer 43 sky_camera
defer 45 screenshot oa-skybox-a.png
defer 48 screenshot oa-skybox-b.png
defer 49 vid_restart
defer 52 screenshot oa-skybox-restart.png
defer 53 gpu_info
defer 54 quit force
"""


def prepare_skies(source, out):
    """Test-only BSP material substitutions; owned game files remain unchanged."""
    from PIL import Image
    bsp = None
    for pak in sorted(source.glob('*.pk3')):
        with zipfile.ZipFile(pak) as archive:
            if 'maps/pxlfan.bsp' in archive.namelist():
                bsp = archive.read('maps/pxlfan.bsp')
    if bsp is None or bsp[:4] != b'IBSP':
        raise ValueError('The sky profile needs OpenArena pxlfan.bsp')
    shaders, length = struct.unpack_from('<ii', bsp, 16)
    offsets = [i for i in range(shaders, shaders+length, 72)
               if struct.unpack_from('<I', bsp, i+64)[0] & 4]
    if not offsets:
        raise ValueError('No sky surfaces in pxlfan.bsp')
    maps = out/'baseoa/maps'
    maps.mkdir()
    for name, material in (('low','textures/gpu/sky-low'),
                           ('high','textures/gpu/sky-high'),
                           ('box','textures/common/sky_space')):
        data = bytearray(bsp)
        for offset in offsets:
            data[offset:offset+64] = material.encode().ljust(64,b'\0')
        (maps/f'gpu-sky-{name}.bsp').write_bytes(data)
    textures = out/'baseoa/textures/gpu'
    textures.mkdir(parents=True)
    image = Image.new('RGB',(256,256))
    image.putdata([(x,y,64) for y in range(256) for x in range(256)])
    image.save(textures/'sky-coordinates.tga')
    scripts = out/'baseoa/scripts'
    scripts.mkdir()
    body = ''
    for name, height in (('low',512),('high',1024)):
        body += f"""textures/gpu/sky-{name}
{{
    surfaceparm sky
    surfaceparm noimpact
    surfaceparm nolightmap
    skyparms - {height} -
    {{
        clampmap textures/gpu/sky-coordinates.tga
        tcmod scale 0.25 0.25
    }}
}}
"""
    (scripts/'gpu-skies.shader').write_text(body)


def prepare(game, out, profile):
    source = game.resolve() / 'baseoa'
    paks = sorted(source.glob('*.pk3'))
    if not paks:
        raise FileNotFoundError(f'No OpenArena PK3 files in {source}')
    if out.exists() or game.resolve() == out or game.resolve() in out.parents:
        raise ValueError('Use a new output directory outside the original game data')
    (out / 'baseoa').mkdir(parents=True)
    (out / 'fte').mkdir()
    for pak in paks:
        target = out / 'baseoa' / pak.name
        try:
            target.symlink_to(pak)
        except OSError:
            shutil.copyfile(pak, target)
    (out / 'baseoa/gpu-openarena.cfg').write_text(
        {'gameplay': CONFIG, 'materials': MATERIAL_CONFIG, 'controls': CONTROL_CONFIG, 'skies': SKY_CONFIG}[profile])
    if profile == 'skies':
        prepare_skies(source, out)


def check(out, profile):
    from PIL import Image, ImageChops, ImageStat
    log = (out / 'openarena.log').read_text(errors='replace')
    assert re.search(r'SDL3 GPU renderer: (metal|vulkan)', log)
    assert not re.search(r'Host_Error|Host_EndGame|Sys_Error|Validation Error|Bad system trap|invalid pointer', log, re.I)

    def capture(name):
        matches = list(out.rglob(f'oa-{name}.png'))
        assert len(matches) == 1, (name, matches)
        return Image.open(matches[0]).convert('RGB')

    if profile == 'skies':
        names = ('sky-low','sky-high','sky-restored','cloud-a','cloud-b',
                 'sky-fast','cloud-restart','skybox-a','skybox-b','skybox-restart')
        frames = {name: capture(name) for name in names}
        def region(name):
            im = frames[name]
            return im.crop((im.width//5,im.height//5,im.width*4//5,im.height*3//5))
        def difference(a,b):
            return sum(ImageStat.Stat(ImageChops.difference(region(a),region(b))).mean)/3
        low = frames['sky-low']
        centre = low.crop((low.width//2-3,low.height//2-3,low.width//2+3,low.height//2+3))
        rgb = ImageStat.Stat(centre).mean
        # Looking vertically up: cloud UV = pi/2 radians, scaled by 1/4.
        # The diagnostic gradient therefore samples R=G=100, B=64. Tiled
        # BSP UVs or a colored depth mask cannot produce this coordinate.
        assert abs(rgb[0]-100)<12 and abs(rgb[1]-100)<12 and abs(rgb[2]-64)<5, ('Bad sky projection/depth mask',rgb)
        results = {
            'projection_centre': rgb,
            'cloud_height': difference('sky-low','sky-high'),
            'height_restored': difference('sky-low','sky-restored'),
            'cloud_motion': difference('cloud-a','cloud-b'),
            'fastsky': difference('cloud-b','sky-fast'),
            'skybox_static': difference('skybox-a','skybox-b'),
            'skybox_restart': difference('skybox-b','skybox-restart'),
        }
        assert results['cloud_height']>0.5, ('skyParms height ignored',results)
        assert results['height_restored']<0.3, ('Projection cache not restored',results)
        assert results['cloud_motion']>0.5, ('Cloud layers stopped animating',results)
        assert results['fastsky']>10, ('Fast-sky switch had no effect',results)
        assert results['skybox_static']<0.3 and results['skybox_restart']<0.3, ('Static skybox changed',results)
        for name in ('cloud-a','cloud-b','cloud-restart','skybox-a'):
            stats = ImageStat.Stat(region(name))
            assert max(stats.mean)>3 and max(stats.stddev)>1, ('Missing sky',name,stats.mean)
        assert log.count('renderer initialized')>=3, 'Sky video restarts did not complete'
        (out/'openarena-sky-results.json').write_text(json.dumps(results,indent=2)+'\n')
        print(json.dumps(results,indent=2))
        return

    if profile == 'controls':
        ims = {name: capture(name) for name in ('lit', 'fullbright', 'restored',
                'fov120', 'fov170', 'fov175', 'zoom', 'unzoomed', 'fov-menu')}
        w, h = ims['lit'].size
        # Static central wall above the animated health pickups and fan.
        wall = (w*45//100, h*36//100, w*55//100, h*48//100)
        def difference(a, b, box=None):
            x, y = ims[a], ims[b]
            if box: x, y = x.crop(box), y.crop(box)
            return sum(ImageStat.Stat(ImageChops.difference(x,y)).mean)/3
        result = {
            'lightmap_toggle': difference('lit', 'fullbright', wall),
            'lighting_restored': difference('lit', 'restored', wall),
            'fov90_to120': difference('lit', 'fov120'),
            'fov120_to170': difference('fov120', 'fov170'),
            'fov170_to175': difference('fov170', 'fov175'),
            'zoom': difference('fov175', 'zoom'),
            'zoom_restored': difference('fov175', 'unzoomed', wall),
        }
        assert result['lightmap_toggle'] > 5, ('World lighting unchanged', result)
        assert result['lighting_restored'] < 1, ('Lightmaps not restored', result)
        for name in ('fov90_to120', 'fov120_to170', 'fov170_to175', 'zoom'):
            assert result[name] > 3, (name, result)
        assert result['zoom_restored'] < 1, ('Zoom did not restore FOV', result)
        saved = list(out.rglob('oa-controls.cfg'))
        assert len(saved) == 1 and re.search(r'\bfov\s+"?175', saved[0].read_text()), 'Console FOV was capped or not saved'
        (out / 'openarena-control-results.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
        return

    if profile == 'materials':
        # MD3 names use mixed case; definitions in shells.shader use lower case.
        for name in ('smallCrossEnv', 'mediumCrossEnv', 'megaCrossEnv'):
            assert re.search(re.escape(name) + r'\s*\{', log), (name, 'Script not found')
        assert 'textures/effects/smallhelth' in log
        assert log.count('renderer initialized') >= 2, 'Video restart did not complete'
        lightmaps = re.findall(r'Lightmaps: (\d+)/(\d+) loaded, (\d+) draws using fallback', log)
        assert lightmaps and all(a == b and int(a) > 0 and int(c) == 0
                                for a, b, c in lightmaps), lightmaps
        results = {}
        for name in ('fan', 'fan-restart'):
            im = capture(name)
            # Centre disk of the rotating fan; avoid the floating megahealth.
            box = (im.width * 44 // 100, im.height * 53 // 100,
                   im.width * 54 // 100, im.height * 58 // 100)
            pixels = list(im.crop(box).getdata())
            dark = sum(max(p) < 4 for p in pixels) / len(pixels)
            mean = sum(sum(p) / 3 for p in pixels) / len(pixels)
            assert dark < 0.05 and mean > 10, (name, 'Black fan', dark, mean)
            results[name] = {'black_fraction': dark, 'mean': mean}
        im = capture('health')
        # The row of small health pickups along the left walkway must glow green.
        box = (0, im.height * 55 // 100, im.width * 20 // 100, im.height * 62 // 100)
        green = sum(g > 40 and g > r * 1.4 and g > b * 1.4
                    for r, g, b in im.crop(box).getdata())
        assert green > 100, ('Health pickup effects missing', green)
        results['health_green_pixels'] = green
        (out / 'openarena-material-results.json').write_text(json.dumps(results, indent=2) + '\n')
        print(json.dumps(results, indent=2))
        return

    names = ('start', 'turned', 'moved', 'restarted', 'no-weapon')
    pictures = {name: capture(name) for name in names}
    width, height = pictures['start'].size
    assert all(p.size == (width, height) for p in pictures.values())
    # Avoid HUD, weapon bob and animated pickup models when testing the camera.
    world = (width // 20, height // 3, width // 2, height * 2 // 3)
    weapon = (width * 2 // 3, height * 2 // 3, width, height * 5 // 6)

    def delta(a, b, box):
        return sum(ImageStat.Stat(ImageChops.difference(
            pictures[a].crop(box), pictures[b].crop(box))).mean) / 3

    result = {
        'turn': delta('start', 'turned', world),
        'move': delta('turned', 'moved', world),
        'restart': delta('moved', 'restarted', world),
        'weapon_toggle': delta('restarted', 'no-weapon', weapon),
    }
    assert result['turn'] > 3, ('Camera did not turn', result)
    assert result['move'] > 1, ('Camera did not move', result)
    assert result['restart'] < 8, ('Video restart lost the camera', result)
    assert result['weapon_toggle'] > 3, ('Cgame weapon switch had no effect', result)
    (out / 'openarena-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare', 'run', 'check'))
    parser.add_argument('game', nargs='?', type=Path)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/openarenadata')
    parser.add_argument('--engine', type=Path, default=ROOT / 'build/fteqw')
    parser.add_argument('--port', type=int, default=29847)
    parser.add_argument('--profile', choices=('gameplay', 'materials', 'controls', 'skies'), default='gameplay')
    args = parser.parse_args()
    out = args.out.resolve()
    if args.action == 'prepare':
        if not args.game:
            parser.error('prepare requires the folder containing baseoa')
        prepare(args.game, out, args.profile)
    elif args.action == 'run':
        with (out / 'openarena.log').open('w') as log:
            subprocess.run([str(args.engine.resolve()), '-nohome', '-basedir', str(out),
                            '-quake3', '-game', 'baseoa', '-window', '-width', '800',
                            '-height', '600', '-port', str(args.port), '-gpu_debug',
                            '+vid_renderer', 'sdlgpu', '+exec', 'gpu-openarena.cfg'],
                           cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                           check=True, timeout=60)
        check(out, args.profile)
    else:
        check(out, args.profile)


if __name__ == '__main__':
    main()
