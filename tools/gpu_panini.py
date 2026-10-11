#!/usr/bin/env python3
"""Check native Panini projection against a regular-rendered reference.

Prepare with owned Quake data, then run. Screenshots, config, and logs are
isolated from the original installation. Requires Pillow and build/all/fteqcc.
"""
import argparse
import json
import math
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def prepare(game, out, qcc):
    source = game.resolve() / 'id1'
    if not (source / 'pak0.pak').is_file():
        raise FileNotFoundError(source / 'pak0.pak')
    if out.exists() or source == out or source in out.parents:
        raise ValueError('Use a new output directory outside the original game data')
    (out / 'id1/scripts').mkdir(parents=True)
    (out / 'fte').mkdir()
    for name in ('pak0.pak', 'pak1.pak'):
        if (source / name).exists():
            try:
                (out / 'id1' / name).symlink_to(source / name)
            except OSError:
                shutil.copyfile(source / name, out / 'id1' / name)
    shutil.copyfile(ROOT / 'tools/tests/gpu_rtt.shader', out / 'id1/scripts/gpu_rtt.shader')
    subprocess.run([str(qcc.resolve()), '-srcfile', str(ROOT / 'tools/tests/gpu_panini.qc'),
                    '-o', str(out / 'id1/gpu-panini.dat')], cwd=ROOT, check=True)
    commands = ['ruleset_allow_in 1', 'cfg_save_auto 0', 'm_preset_chosen 1',
                'con_notifytime 0', 'sv_public 0', 'r_bloom 0', 'r_fxaa 0',
                'r_hdr_framebuffer 0', 'r_shadow_realtime_world 0',
                'r_shadow_realtime_dlight 0', 'r_dynamic 0',
                'gamma 1', 'contrast 1', 'brightness 0', 'v_contrastboost 1',
                'r_projection 0', 'vid_sceneonlygamma 0', 'set gpu_panini_overlay 0',
                'set gpu_panini_fov 140', 'set gpu_panini_layout 0', 'r_waterstyle 3',
                'sv_csqc_progname gpu-panini.dat', 'map start', 'defer 5 pause']
    time = 6

    def capture(name, *settings):
        nonlocal time
        for i, setting in enumerate(settings):
            commands.append(f'defer {time + i / 10:g} {setting}')
        commands.append(f'defer {time + 1:g} screenshot panini-{name}.png')
        time += 2

    for name, hdr, fov, layout in [('wide',0,140,0), ('hdr',1,140,0),
                                  ('narrow',0,90,0), ('limit',0,170,0),
                                  ('split',0,140,1), ('rtt',0,140,2), ('water',0,140,3)]:
        capture(name + '-regular', f'r_hdr_framebuffer {hdr}', f'set gpu_panini_fov {fov}',
                f'set gpu_panini_layout {layout}', 'r_projection 0')
        capture(name + '-panini', 'r_projection 6', *(['gpu_info'] if name == 'water' else []))
    capture('preview-regular', 'set gpu_panini_layout 4', 'r_projection 0')
    capture('preview-panini', 'r_projection 6')
    capture('overlay-regular', 'set gpu_panini_layout 0', 'set gpu_panini_overlay 1', 'r_projection 0')
    capture('overlay-panini', 'r_projection 6')
    capture('scene-gamma', 'vid_sceneonlygamma 1', 'gamma 0.65', 'contrast 1.2', 'brightness 0.04')
    capture('restart', 'vid_restart')
    capture('menu', 'menu_options')
    commands += [f'defer {time:g} cfg_save panini-test', f'defer {time+1:g} quit force']
    (out / 'id1/gpu-test.cfg').write_text('\n'.join(commands) + '\n')


def check(out):
    from PIL import Image, ImageChops, ImageStat
    text = (out / 'panini.log').read_text(errors='replace')
    assert 'SDL3 GPU renderer: metal' in text or 'SDL3 GPU renderer: vulkan' in text
    assert re.search(r'Water view \d+:.*reflection/refraction [1-9]\d*/[1-9]\d* draws', text), 'Reflective liquid was not exercised'
    assert not re.search(r'SDL3? GPU(?: error|:.*error)|Host_Error|Sys_Error|Bad builtin|Validation Error', text, re.I)

    def image(name):
        matches = list(out.rglob(f'panini-{name}.png'))
        assert len(matches) == 1, (name, matches)
        return Image.open(matches[0]).convert('RGB')

    def delta(a, b):
        return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3

    def bilinear(im, x, y):
        x = max(0, min(im.width-1, x)); y = max(0, min(im.height-1, y))
        ix, iy = math.floor(x), math.floor(y)
        tx, ty = x-ix, y-iy
        samples = [(im.getpixel((min(ix+dx,im.width-1),min(iy+dy,im.height-1))),
                    (tx if dx else 1-tx)*(ty if dy else 1-ty))
                   for dx in (0,1) for dy in (0,1)]
        return tuple(sum(c[k]*w for c,w in samples) for k in range(3))

    def projection_error(source, projected, fov):
        # Independent ray construction: output longitude follows the Panini
        # half-angle rule; convert that ray back to perspective image coords.
        tangent = math.tan(math.radians(fov/2))
        extent = 2*math.tan(math.radians(fov/4))
        error = 0; count = 0
        for y in range(4,projected.height-4,11):
            for x in range(4,projected.width-4,11):
                nx = 2*(x+0.5)/projected.width-1
                ny = 2*(y+0.5)/projected.height-1
                longitude = 2*math.atan(nx*extent/2)
                px = math.tan(longitude)/tangent
                py = ny*extent/tangent*(1+math.cos(longitude))/(2*math.cos(longitude))
                expected = bilinear(source, (px+1)*source.width/2-0.5,
                                    (py+1)*source.height/2-0.5)
                actual = projected.getpixel((x,y))
                error += sum(abs(a-b) for a,b in zip(expected,actual))/3
                count += 1
        return error/count

    results = {}
    for name, fov in [('wide',140),('hdr',140),('narrow',90),('limit',170),('split',140),('rtt',140),('water',140)]:
        regular, panini = image(name+'-regular'), image(name+'-panini')
        assert regular.size == panini.size
        if name == 'split':
            boxes = [(0,0,regular.width//2,regular.height),
                     (regular.width//2,0,regular.width,regular.height)]
        elif name == 'rtt':
            sx, sy = regular.width/800, regular.height/600
            boxes = [tuple(round(v*(sx if i%2==0 else sy)) for i,v in enumerate((200,150,600,450)))]
        else:
            boxes = [(0,0,regular.width,regular.height)]
        errors = [projection_error(regular.crop(box), panini.crop(box), fov) for box in boxes]
        if name == 'rtt':
            # The rendered picture must fill its whole target; sample each
            # quadrant to catch a fixture or backend viewport-size mistake.
            picture = panini.crop(boxes[0])
            for x,y in ((0,0),(1,0),(0,1),(1,1)):
                quadrant = picture.crop((x*picture.width//2,y*picture.height//2,
                                         (x+1)*picture.width//2,(y+1)*picture.height//2))
                assert sum(ImageStat.Stat(quadrant).mean) > 10, 'RTT quadrant was not drawn'
            margin = (0,panini.height//2,round(150*sx),panini.height)
            assert delta(regular.crop(margin),panini.crop(margin)) == 0, 'RTT changed surrounding HUD'
        difference = delta(regular, panini)
        # The RTT is rendered at 400x300 and then enlarged by the HUD. That
        # adds another interpolation compared with the full-resolution ray
        # reference, so allow a small extra RGB error for this case.
        assert max(errors) < (4 if name == 'rtt' else 2.5), (name, 'projection ray error', errors)
        assert difference > 2, (name, 'projection had no effect', difference)
        results[name] = {'ray_error': errors, 'image_difference': difference}
    preview = image('preview-regular')
    assert max(ImageStat.Stat(preview).mean) > 0.1, 'Model preview did not draw'
    assert delta(preview,image('preview-panini')) == 0, 'Panini warped a 3D UI preview'
    regular, panini, gamma, restart = [image(n) for n in
        ('overlay-regular','overlay-panini','scene-gamma','restart')]
    sx, sy = regular.width/800, regular.height/600
    ui = tuple(round(v*(sx if i%2==0 else sy)) for i,v in enumerate((24,24,136,72)))
    assert delta(regular.crop(ui),panini.crop(ui)) <= 1, 'Panini warped HUD'
    assert delta(regular.crop(ui),gamma.crop(ui)) <= 1, 'Scene gamma changed HUD'
    assert delta(gamma,restart) < 1, 'Video restart changed projection'
    config = list(out.rglob('panini-test.cfg'))
    assert config and re.search(r'r_projection\s+"?6',config[0].read_text()), 'Projection not saved'
    (out/'panini-results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps(results,indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('prepare','run','check'))
    parser.add_argument('game',nargs='?',type=Path)
    parser.add_argument('--out',type=Path,default=ROOT/'build/paninidata')
    parser.add_argument('--qcc',type=Path,default=ROOT/'build/all/fteqcc')
    parser.add_argument('--engine',type=Path,default=ROOT/'build/fteqw')
    args = parser.parse_args(); out = args.out.resolve()
    if args.action == 'prepare':
        if not args.game: parser.error('prepare requires the Quake folder containing id1/pak0.pak')
        prepare(args.game,out,args.qcc)
    elif args.action == 'run':
        with (out/'panini.log').open('w') as log:
            subprocess.run([str(args.engine.resolve()),'-nohome','-basedir',str(out),
                '-quake','-window','-width','800','-height','600','-gpu_debug',
                '+vid_conautoscale','0','+vid_conwidth','800','+vid_conheight','600',
                '+exec','gpu-test.cfg'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
                check=True,timeout=100)
        check(out)
    else:
        check(out)


if __name__ == '__main__':
    main()
