#!/usr/bin/env python3
"""Exercise whole-image and scene-only gamma with opaque/translucent UI and RTT.

Use prepare with owned Quake data, then run and check. All game/config/output
files live in an isolated test directory; original game data is never modified.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def prepare(game, out, qcc):
    source = game.resolve() / 'id1'
    if not (source / 'pak0.pak').is_file():
        raise FileNotFoundError(source / 'pak0.pak')
    if out.exists():
        raise FileExistsError(f'Use a new output directory: {out}')
    if source == out or source in out.parents:
        raise ValueError('Output must be separate from original game data')
    (out / 'id1/scripts').mkdir(parents=True)
    (out / 'fte').mkdir()
    for name in ('pak0.pak', 'pak1.pak'):
        if (source / name).exists():
            try:
                (out / 'id1' / name).symlink_to(source / name)
            except OSError:
                shutil.copyfile(source / name, out / 'id1' / name)
    shutil.copyfile(ROOT / 'tools/tests/gpu_rtt.shader', out / 'id1/scripts/gpu_rtt.shader')
    subprocess.run([str(qcc.resolve()), '-srcfile', str(ROOT / 'tools/tests/gpu_gamma.qc'),
                    '-o', str(out / 'id1/gpu-gamma.dat')], cwd=ROOT, check=True)
    commands = ['ruleset_allow_in 1', 'cfg_save_auto 0', 'm_preset_chosen 1',
                'con_notifytime 0', 'sv_public 0', 'r_bloom 0', 'r_fxaa 0',
                'r_hdr_framebuffer 0', 'r_shadow_realtime_world 0',
                'r_shadow_realtime_dlight 0', 'r_dynamic 0',
                'gamma 1', 'contrast 1', 'brightness 0', 'v_contrastboost 1',
                'vid_sceneonlygamma 0', 'set gpu_gamma_overlay 1',
                'sv_csqc_progname gpu-gamma.dat', 'map start', 'defer 5 pause']
    time = 6

    def capture(name, *settings):
        nonlocal time
        for i, setting in enumerate(settings):
            commands.append(f'defer {time + i / 10:g} {setting}')
        commands.append(f'defer {time + 1:g} screenshot gamma-{name}.png')
        time += 2

    for hdr in (0, 1):
        prefix = f'hdr{hdr}'
        capture(prefix + '-neutral', f'r_hdr_framebuffer {hdr}', 'gamma 1',
                'contrast 1', 'brightness 0', 'vid_sceneonlygamma 0', 'set gpu_gamma_overlay 1')
        capture(prefix + '-full', 'gamma 0.65', 'contrast 1.2', 'brightness 0.04')
        capture(prefix + '-scene', 'vid_sceneonlygamma 1')
        capture(prefix + '-background', 'set gpu_gamma_overlay 0')
        capture(prefix + '-restored', 'set gpu_gamma_overlay 1', 'vid_sceneonlygamma 0')
    capture('restart', 'vid_sceneonlygamma 1', 'vid_restart')
    capture('menu-adjusted', 'menu_video')
    capture('menu-neutral', 'gamma 1', 'contrast 1', 'brightness 0')
    capture('legacy-off', 'closemenu', 'gamma 0.65', 'contrast 1.2', 'brightness 0.04',
            'vid_hardwaregamma 0', 'vid_reload')
    capture('console-only-adjusted', 'disconnect')
    capture('console-only-neutral', 'gamma 1', 'contrast 1', 'brightness 0')
    commands += [f'defer {time:g} cfg_save gamma-scene-test', f'defer {time + 1:g} quit force']
    (out / 'id1/gpu-test.cfg').write_text('\n'.join(commands) + '\n')


def check(out):
    from PIL import Image, ImageChops, ImageStat
    text = (out / 'gamma.log').read_text(errors='replace')
    assert 'SDL3 GPU renderer: metal' in text or 'SDL3 GPU renderer: vulkan' in text
    assert not re.search(r'SDL GPU error|Host_Error|Sys_Error|Bad builtin|Validation Error', text, re.I)

    def image(name):
        matches = list(out.rglob(f'gamma-{name}.png'))
        assert len(matches) == 1, (name, matches)
        return Image.open(matches[0]).convert('RGB')

    def delta(a, b, box):
        return sum(ImageStat.Stat(ImageChops.difference(a.crop(box), b.crop(box))).mean) / 3

    results = {}
    # UI positions are in logical console pixels, not physical framebuffer pixels.
    # The test explicitly fixes logical dimensions to 800x600 during run.
    for hdr in (0, 1):
        prefix = f'hdr{hdr}'
        neutral, full, scene, background, restored = [image(prefix + '-' + n)
            for n in ('neutral', 'full', 'scene', 'background', 'restored')]
        assert len({im.size for im in (neutral, full, scene, background, restored)}) == 1
        sx, sy = scene.width / 800, scene.height / 600
        def box(x0, y0, x1, y1):
            return tuple(round(v * (sx if i % 2 == 0 else sy)) for i, v in enumerate((x0, y0, x1, y1)))
        opaque = box(24, 24, 136, 72)
        translucent = box(168, 24, 280, 72)
        rtt = box(312, 24, 424, 136)
        rtt_later = box(456, 24, 568, 136)
        world = box(100, 160, 700, 520)
        assert delta(neutral, scene, opaque) <= 1, (prefix, 'opaque UI changed')
        assert delta(neutral, scene, rtt) <= 1, (prefix, 'offscreen UI changed')
        assert delta(neutral, scene, rtt_later) <= 1, (prefix, 'later offscreen UI changed')
        assert neutral.getpixel((round(350*sx),round(50*sy))) != neutral.getpixel((round(500*sx),round(50*sy))), 'RTT fixture did not repaint'
        assert delta(neutral, full, opaque) > 5, (prefix, 'full-image gamma ineffective')
        assert delta(neutral, scene, world) > 3, (prefix, 'scene gamma ineffective')
        assert delta(full, scene, world) <= 1, (prefix, 'world correction differs')
        assert delta(full, restored, opaque) <= 1, (prefix, 'toggle off did not restore')
        # A half-transparent overlay must blend its unchanged source color with
        # the corrected background, rather than gamma-correcting the blend.
        source = neutral.getpixel((round(64*sx), round(40*sy)))
        actual = scene.crop(translucent)
        bg = background.crop(translucent)
        expected = Image.blend(bg, Image.new('RGB', bg.size, source), 0.5)
        alpha_error = sum(ImageStat.Stat(ImageChops.difference(actual, expected)).mean) / 3
        assert alpha_error < 2, (prefix, 'translucent compositing', alpha_error)
        results[prefix] = {'opaque_ui_difference': delta(neutral, scene, opaque),
                          'rtt_ui_difference': delta(neutral, scene, rtt),
                          'world_gamma_difference': delta(neutral, scene, world),
                          'translucent_error': alpha_error}
    reference = image('hdr1-scene')
    assert delta(reference, image('restart'), (0,0,reference.width,reference.height)) < 1
    assert delta(reference, image('legacy-off'), (0,0,reference.width,reference.height)) < 1
    a, b = image('menu-adjusted'), image('menu-neutral')
    # The world behind a translucent menu still changes, and its slider
    # handles move when their values change. Compare opaque gray label pixels.
    sx, sy = a.width / 800, a.height / 600
    label = tuple(round(v * (sx if i % 2 == 0 else sy)) for i,v in enumerate((300,164,448,176)))
    pairs = list(zip(a.crop(label).getdata(), b.crop(label).getdata()))
    glyphs = [(x,y) for x,y in pairs if max(y)-min(y) <= 1 and min(y) >= 60]
    assert len(glyphs) >= 50, 'Menu label was not visible'
    assert all(max(abs(u-v) for u,v in zip(x,y)) <= 1 for x,y in glyphs), 'Menu label colors changed'
    a, b = image('console-only-adjusted'), image('console-only-neutral')
    # Disconnect automatically opens the console. Its history changes because
    # taking the first screenshot adds a line; compare the untouched backdrop.
    backdrop = (0, a.height//2 + 32, a.width, a.height)
    assert a.crop(backdrop).tobytes() == b.crop(backdrop).tobytes(), 'Console-only colors changed'
    from collections import Counter
    def text_color(im):
        colors = Counter(c for c in im.crop((0,0,im.width,im.height//2)).getdata()
                         if max(c)-min(c) <= 1 and min(c) >= 60)
        return colors.most_common(1)[0][0]
    assert text_color(a) == text_color(b), 'Console font colors changed'
    configs = list(out.rglob('gamma-scene-test.cfg'))
    assert configs and 'vid_sceneonlygamma' in configs[0].read_text(), 'Setting not saved'
    (out / 'gamma-results.json').write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps(results, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare', 'run', 'check'))
    parser.add_argument('game', nargs='?', type=Path)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/gammadata')
    parser.add_argument('--qcc', type=Path, default=ROOT / 'build/all/fteqcc')
    parser.add_argument('--engine', type=Path, default=ROOT / 'build/fteqw')
    args = parser.parse_args()
    out = args.out.resolve()
    if args.action == 'prepare':
        if not args.game:
            parser.error('prepare requires the Quake folder containing id1/pak0.pak')
        prepare(args.game, out, args.qcc)
    elif args.action == 'run':
        with (out / 'gamma.log').open('w') as log:
            subprocess.run([str(args.engine.resolve()), '-nohome', '-basedir', str(out),
                '-quake', '-window', '-width', '800', '-height', '600', '-gpu_debug',
                '+vid_conautoscale', '0', '+vid_conwidth', '800', '+vid_conheight', '600',
                '+exec', 'gpu-test.cfg'], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                check=True, timeout=100)
        check(out)
    else:
        check(out)


if __name__ == '__main__':
    main()
