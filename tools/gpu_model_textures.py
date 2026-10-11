#!/usr/bin/env python3
"""Prepare/check model replacement images in QuakeWorld-compatible paths.

Prepare with owned Quake data. Run each case with its directory as -basedir,
-quake -game qw, and +exec gpu-test.cfg. Original game data is never changed.
"""
import argparse
import json
import pathlib
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
CASES = ('compat', 'direct', 'disabled', 'missing')


def prepare(game, out, qcc):
    from PIL import Image
    source = game.resolve() / 'id1'
    out = out.resolve()
    if source == (out / 'id1').resolve() or source in out.parents:
        raise ValueError('Output must be separate from original game data')
    if not (source / 'pak0.pak').is_file():
        raise FileNotFoundError(source / 'pak0.pak')
    out.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(qcc.resolve()), '-srcfile', str(ROOT / 'tools/tests/gpu_model_textures.qc'),
                    '-o', str(out / 'gpu-model.dat')], check=True, cwd=ROOT)
    cfg = (ROOT / 'tools/tests/gpu_model_textures.cfg').read_text()
    for case in CASES:
        directory = out / case
        if directory.exists():
            raise FileExistsError(f'Use a new output directory: {directory}')
        (directory / 'id1/scripts').mkdir(parents=True)
        (directory / 'qw').mkdir()
        for name in ('pak0.pak', 'pak1.pak'):
            original = source / name
            if original.exists():
                (directory / 'id1' / name).symlink_to(original)
        shutil.copyfile(out / 'gpu-model.dat', directory / 'id1/gpu-model.dat')
        shutil.copyfile(ROOT / 'tools/tests/gpu_sprite.shader', directory / 'id1/scripts/gpu_sprite.shader')
        (directory / 'id1/gpu-test.cfg').write_text(
            cfg.replace('gl_load24bit 1', 'gl_load24bit 0') if case == 'disabled' else cfg)
        if case != 'missing':
            textures = directory / 'qw/textures/models'
            textures.mkdir(parents=True)
            # Non-power-of-two sizes matching these original MDL skins.
            Image.new('RGB', (232, 156), (0, 255, 0)).save(textures / 'g_rock2_0.tga')
            Image.new('RGB', (308, 121), (255, 0, 255)).save(textures / 'quaddama_0.png')
        if case == 'direct':
            # Explicit FTE paths retain priority over compatibility paths.
            progs = directory / 'qw/progs'
            progs.mkdir()
            Image.new('RGB', (232, 156), (0, 255, 255)).save(progs / 'g_rock2.mdl_0.tga')
            Image.new('RGB', (308, 121), (255, 255, 0)).save(progs / 'quaddama.mdl_0.png')
    print(out)


def check(out):
    from PIL import Image
    metrics, images = {}, {}
    for case in CASES:
        directory = out / case
        log = (out / f'{case}.log').read_text()
        assert 'SDL3 GPU renderer: metal' in log or 'SDL3 GPU renderer: vulkan' in log, case
        assert 'SDL GPU error' not in log and 'Host_Error' not in log, case
        background = Image.open(directory / 'fte/gpu-model-background.png').convert('RGB')
        assert len(background.getcolors(background.width * background.height)) == 1, (case, 'fixture startup')
        image = images[case] = Image.open(directory / 'fte/gpu-model-replacements.png').convert('RGB')
        restart = Image.open(directory / 'fte/gpu-model-restart.png').convert('RGB')
        assert image.size == background.size and restart.tobytes() == image.tobytes(), (case, 'restart')
        pixels = list(image.getdata())
        counts = {
            'green': sum(g > 2*r and g > 2*b and g > 30 for r,g,b in pixels),
            'magenta': sum(r > 2*g and b > 2*g and r > 30 and b > 30 for r,g,b in pixels),
            'cyan': sum(g > 2*r and b > 2*r and g > 30 and b > 30 for r,g,b in pixels),
            'yellow': sum(r > 2*b and g > 2*b and r > 30 and g > 30 for r,g,b in pixels),
        }
        if case in ('compat', 'direct'):
            expected = ('green', 'magenta') if case == 'compat' else ('cyan', 'yellow')
            for color in expected:
                assert counts[color] > image.width * image.height * 0.005, (case, color, counts)
        else:
            assert counts['green'] == 0 and counts['magenta'] == 0, (case, counts)
        metrics[case] = counts
    assert images['disabled'].tobytes() == images['missing'].tobytes(), 'replacement switch bypassed'
    result = {'model_texture_paths_formats_disable_restart': 'passed', 'pixels': metrics}
    (out / 'model-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    prep = commands.add_parser('prepare')
    prep.add_argument('game', type=pathlib.Path)
    prep.add_argument('--qcc', type=pathlib.Path, default=ROOT / 'build/all/fteqcc')
    prep.add_argument('--out', type=pathlib.Path, default=ROOT / 'build/modeldata')
    verify = commands.add_parser('check')
    verify.add_argument('--out', type=pathlib.Path, default=ROOT / 'build/modeldata')
    args = parser.parse_args()
    if args.command == 'prepare':
        prepare(args.game, args.out, args.qcc)
    else:
        check(args.out)
