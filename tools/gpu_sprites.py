#!/usr/bin/env python3
"""Prepare/check real explosion sprite masking, fading, blending, and restart.

Prepare with owned Quake data, then run the engine with --out as its basedir
and +exec gpu-test.cfg. Game data stays outside the repository.
"""
import argparse
import collections
import json
import pathlib
import shutil
import subprocess
import re
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
PHASES = ('masked', 'faded', 'blended', 'additive', 'restart')


def prepare(game, out, qcc, openarena=None):
    source = pathlib.Path(game).resolve() / 'id1'
    out = out.resolve()
    if source == (out / 'id1').resolve():
        raise ValueError('Output must be separate from original game data')
    if not (source / 'pak0.pak').is_file():
        raise FileNotFoundError(source / 'pak0.pak')
    (out / 'id1/scripts').mkdir(parents=True, exist_ok=True)
    for name in ('pak0.pak', 'pak1.pak'):
        original = source / name
        target = out / 'id1' / name
        if original.exists() and not target.exists():
            target.symlink_to(original)
    shutil.copyfile(ROOT / 'tools/tests/gpu_sprite.cfg', out / 'id1/gpu-test.cfg')
    shutil.copyfile(ROOT / 'tools/tests/gpu_sprite.shader', out / 'id1/scripts/gpu_sprite.shader')
    if openarena:
        paks = sorted((openarena.resolve() / 'baseoa').glob('*.pk3'), reverse=True)
        shader = texture = None
        for pak in paks:
            with zipfile.ZipFile(pak) as archive:
                if shader is None and 'scripts/oanew.shader' in archive.namelist():
                    text = archive.read('scripts/oanew.shader').decode()
                    start = re.search(r'(?im)^shotgunSmokePuff\s*\{', text)
                    if start:
                        level = 1
                        end = text.index('{', start.start()) + 1
                        while level:
                            if text[end] == '{': level += 1
                            if text[end] == '}': level -= 1
                            end += 1
                        shader = text[start.start():end]
                if texture is None and 'gfx/misc/smokepuff3.tga' in archive.namelist():
                    texture = archive.read('gfx/misc/smokepuff3.tga')
        if shader is None or texture is None:
            raise FileNotFoundError('OpenArena shotgun smoke shader/texture not found')
        (out / 'id1/gfx/misc').mkdir(parents=True, exist_ok=True)
        (out / 'id1/gfx/misc/smokepuff3.tga').write_bytes(texture)
        (out / 'id1/scripts/gpu_shotgun.shader').write_text(shader + '\n')
        config = (out / 'id1/gpu-test.cfg').read_text()
        config = 'set gpu_test_smoke_assets 1\ngamma 1\ncontrast 1\nbrightness 0\nv_contrastboost 1\n' + config
        config = config.replace('defer 25 quit force', '''defer 25 set gpu_test_sprite 0
defer 26 screenshot gpu-smoke-background.png
defer 27 set gpu_test_smoke_alpha 1
defer 28 screenshot gpu-smoke-full.png
defer 29 set gpu_test_smoke_alpha 0.5
defer 30 screenshot gpu-smoke-half.png
defer 31 set gpu_test_smoke_alpha 0.25
defer 32 screenshot gpu-smoke-quarter.png
defer 33 set gpu_test_smoke_alpha 0
defer 34 screenshot gpu-smoke-gone.png
defer 35 quit force''')
        (out / 'id1/gpu-test.cfg').write_text(config)
    subprocess.run([str(qcc.resolve()), '-srcfile', str(ROOT / 'tools/tests/gpu_sprite.qc'),
                    '-o', str(out / 'id1/gpu-sprite.dat')], check=True, cwd=ROOT)


def check(out, log):
    from PIL import Image
    directory = out / 'fte'
    background = Image.open(directory / 'gpu-sprite-background.png').convert('RGB')
    histogram = collections.Counter(background.getdata())
    assert len(histogram) == 1, 'Background capture preceded fixture startup'
    backdrop = background.getpixel((0, 0))
    metrics = {}
    images = {}
    for phase in PHASES:
        image = images[phase] = Image.open(directory / f'gpu-sprite-{phase}.png').convert('RGB')
        assert image.size == background.size, phase
        w, h = image.size
        # The three 56x56 sprite cards project to 0.21875 of the viewport width.
        # Their corners are transparent in every original sprite frame.
        for center in (0.1875, 0.5, 0.8125):
            for x in (center - 0.1, center + 0.1):
                for y in (0.365, 0.635):
                    assert image.getpixel((round(x*w), round(y*h))) == backdrop, (phase, 'opaque border')
        contrast = sum(sum(abs(a-b) for a, b in zip(pixel, backdrop)) for pixel in image.getdata())
        changed = sum(pixel != backdrop for pixel in image.getdata())
        assert changed > w*h*0.01, (phase, 'explosion missing')
        metrics[phase] = {'changed_pixels': changed, 'contrast': contrast}
    ratio = metrics['faded']['contrast'] / metrics['masked']['contrast']
    assert 0.15 < ratio < 0.45, ('incorrect sprite fade', ratio)
    assert images['restart'].tobytes() == images['masked'].tobytes(), 'restart changed masking'
    text = log.read_text()
    assert 'SDL3 GPU renderer: metal' in text or 'SDL3 GPU renderer: vulkan' in text
    assert 'SDL GPU error' not in text and 'Host_Error' not in text
    result = {'sprite_masks_fade_blend_restart': 'passed', 'fade_contrast_ratio': ratio, 'metrics': metrics}
    if (out / 'id1/scripts/gpu_shotgun.shader').exists():
        backdrop = Image.open(directory / 'gpu-smoke-background.png').convert('RGB')
        base = backdrop.getpixel((0, 0))
        smoke = {}
        for phase in ('full', 'half', 'quarter'):
            im = Image.open(directory / f'gpu-smoke-{phase}.png').convert('RGB')
            smoke[phase] = sum(sum(abs(a-b) for a, b in zip(pixel, base)) for pixel in im.getdata())
        assert smoke['full'] > 10000, 'Smoke did not render'
        assert 0.4 < smoke['half']/smoke['full'] < 0.6, smoke
        assert 0.18 < smoke['quarter']/smoke['full'] < 0.32, smoke
        gone = Image.open(directory / 'gpu-smoke-gone.png').convert('RGB')
        assert gone.tobytes() == backdrop.tobytes(), 'Smoke left a residual card'
        result['shotgun_smoke_contrast_ratios'] = {k: v/smoke['full'] for k,v in smoke.items()}
    (out / 'sprite-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    prep = commands.add_parser('prepare')
    prep.add_argument('game', type=pathlib.Path)
    prep.add_argument('--openarena', type=pathlib.Path, help='Also test the installed OpenArena shotgun smoke material')
    prep.add_argument('--qcc', type=pathlib.Path, default=ROOT / 'build/all/fteqcc')
    prep.add_argument('--out', type=pathlib.Path, default=ROOT / 'build/spritedata')
    verify = commands.add_parser('check')
    verify.add_argument('--out', type=pathlib.Path, default=ROOT / 'build/spritedata')
    verify.add_argument('--log', type=pathlib.Path, default=ROOT / 'build/sprite-after.log')
    args = parser.parse_args()
    if args.command == 'prepare':
        prepare(args.game, args.out, args.qcc, args.openarena)
    else:
        check(args.out, args.log)
