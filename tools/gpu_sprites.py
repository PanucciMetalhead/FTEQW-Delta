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

ROOT = pathlib.Path(__file__).resolve().parents[1]
PHASES = ('masked', 'faded', 'blended', 'additive', 'restart')


def prepare(game, out, qcc):
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
    (out / 'sprite-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    prep = commands.add_parser('prepare')
    prep.add_argument('game', type=pathlib.Path)
    prep.add_argument('--qcc', type=pathlib.Path, default=ROOT / 'build/all/fteqcc')
    prep.add_argument('--out', type=pathlib.Path, default=ROOT / 'build/spritedata')
    verify = commands.add_parser('check')
    verify.add_argument('--out', type=pathlib.Path, default=ROOT / 'build/spritedata')
    verify.add_argument('--log', type=pathlib.Path, default=ROOT / 'build/sprite-after.log')
    args = parser.parse_args()
    if args.command == 'prepare':
        prepare(args.game, args.out, args.qcc)
    else:
        check(args.out, args.log)
