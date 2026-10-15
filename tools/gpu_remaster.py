#!/usr/bin/env python3
"""Prepare/run/check isolated remaster content tests with an owned installation.

Original game data is read only. Model tests inspect compatible MD5 skeletons
and animations, verify original-model fallback for mismatched pairs, compare
replacement/fallback rendering, and check lighting/restart.
Campaign tests exercise the supplied gamecode, map changes and save/load.
Requires Python/Pillow; preparation also requires fteqcc.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CAMPAIGNS = {'id1': 'e1m1', 'hipnotic': 'hip1m1', 'rogue': 'r1m1',
             'dopa': 'e5m1', 'mg1': 'mge1m1', 'mg3': 'map1'}
OPTIONAL_BUILTINS = {'ex_draw_point', 'ex_draw_line', 'ex_draw_arrow', 'ex_draw_ray',
                    'ex_draw_circle', 'ex_draw_bounds', 'ex_draw_worldtext',
                    'ex_draw_sphere', 'ex_draw_cylinder', 'ex_bot_movetopoint',
                    'ex_bot_followentity'}
CAPTURES = '''defer 1 screenshot remaster-background.png
defer 2 set gpu_remaster_models 1
defer 3 screenshot remaster-md5.png
defer 4 set gpu_remaster_models 2
defer 5 screenshot remaster-replacement.png
defer 6 set gpu_remaster_models 1
defer 6.1 set gpu_remaster_frame 30
defer 7 screenshot remaster-pose30.png
defer 8 r_fullbright 0
defer 9 screenshot remaster-lit.png
defer 9.1 gpu_info
defer 10 vid_restart
defer 15 screenshot remaster-restart.png
defer 15.1 gpu_info
defer 16 echo REMASTER_MODE $com_parseutf8 $sv_mintic
defer 17 quit force
'''


class Pak:
    def __init__(self, path):
        self.path = path
        self.entries = {}
        size = path.stat().st_size
        with path.open('rb') as f:
            magic, offset, length = struct.unpack('<4sII', f.read(12))
            if magic != b'PACK' or length % 64 or offset < 12 or offset + length > size:
                raise ValueError(f'Invalid PAK directory: {path}')
            f.seek(offset)
            for _ in range(length // 64):
                name, start, count = struct.unpack('<56sII', f.read(64))
                name = name.split(b'\0', 1)[0].decode('ascii')
                if start + count > size:
                    raise ValueError(f'Invalid PAK member: {path}:{name}')
                self.entries[name] = (start, count)

    def read(self, name):
        start, count = self.entries[name]
        with self.path.open('rb') as f:
            f.seek(start)
            return f.read(count)


def link(source, target):
    if target.exists() or target.is_symlink():
        raise FileExistsError(target)
    try:
        target.symlink_to(source, target_is_directory=source.is_dir())
    except OSError:
        if source.is_dir():
            shutil.copytree(source, target)
        else:
            shutil.copyfile(source, target)


def prepare(game, out, qcc, model_game='id1'):
    game = game.resolve()
    if game == out or game in out.parents or out in game.parents:
        raise ValueError('Output must be separate from the owned installation')
    if out.exists() and any(out.iterdir()):
        raise FileExistsError(f'Use an empty output directory: {out}')
    out.mkdir(parents=True, exist_ok=True)
    base = Pak(game / 'id1/pak0.pak')
    models = {}
    model_files = {name: base for name in base.entries}
    if model_game != 'id1':
        extra = Pak(game / model_game / 'pak0.pak')
        model_files.update({name: extra for name in extra.entries})
    for name in sorted(model_files):
        if not name.endswith('.md5mesh'):
            continue
        pak = model_files[name]
        text = pak.read(name).decode()
        bones = int(re.search(r'\bnumJoints\s+(\d+)', text)[1])
        anim = name.removesuffix('.md5mesh') + '.md5anim'
        frames = 0
        if anim in model_files:
            animation = model_files[anim].read(anim).decode()
            frames = int(re.search(r'\bnumFrames\s+(\d+)', animation)[1])
            animbones = int(re.search(r'\bnumJoints\s+(\d+)', animation)[1])
        compatible = anim not in model_files or bones == animbones
        models[name] = {'bones': bones, 'frames': frames, 'compatible': compatible}
    if not models:
        raise ValueError('No MD5 models found in the supplied base PAK')
    campaigns = [name for name in CAMPAIGNS if (game / name / 'pak0.pak').is_file()]
    actors = ('ogre_rocket', 'shambler_blood', 'dog_explosive') if model_game == 'mg3' else ('player', 'soldier', 'dog')
    manifest = {'source': str(game), 'models': models, 'campaigns': campaigns,
                'model_game': model_game, 'replacement_model': f'progs/{actors[0]}.md5mesh'}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    generated = out / 'generated'
    generated.mkdir()
    lines = ['void() inspect_all_models = {']
    for name, info in models.items():
        anim = name.removesuffix('.md5mesh') + '.md5anim'
        if not info['compatible']:
            lines.append(f'inspect_model("{name.removesuffix(".md5mesh")}.mdl", "", "");')
            continue
        last = f'{anim}_{info["frames"]-1}' if info['frames'] else ''
        beyond = f'{anim}_{info["frames"]}' if info['frames'] else ''
        lines.append(f'inspect_model("{name}", "{last}", "{beyond}");')
    lines.append('};')
    (generated / 'remaster-model-list.qh').write_text('\n'.join(lines) + '\n')
    fixture = (ROOT / 'tools/tests/gpu_remaster.qc').read_text()
    for original, actor in zip(('player', 'soldier', 'dog'), actors):
        fixture = fixture.replace(f'progs/{original}.', f'progs/{actor}.')
    for name, info in models.items():
        if not info['compatible']:
            fixture = fixture.replace(name, name.removesuffix('.md5mesh') + '.mdl')
    (generated / 'gpu_remaster.qc').write_text(fixture)
    with (out / 'qcc.log').open('w') as log:
        subprocess.run([str(qcc.resolve()), '-srcfile', str(generated / 'gpu_remaster.qc'),
                        '-o', str(generated / 'remaster-test.dat')], cwd=generated,
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)

    def assets(case, campaign='id1'):
        for folder in dict.fromkeys(('id1', campaign)):
            (case / folder).mkdir(parents=True)
            for pak in sorted((game / folder).glob('pak*.pak')):
                link(pak, case / folder / pak.name)
            music = game / folder / 'music'
            if music.is_dir():
                link(music, case / folder / 'music')
        (case / 'fte').mkdir()

    for mode, replacement in (('modern', 'md5mesh'), ('classic', '')):
        case = out / mode
        assets(case, model_game)
        (case / 'id1/scripts').mkdir()
        shutil.copyfile(ROOT / 'tools/tests/gpu_sprite.shader', case / 'id1/scripts/gpu_sprite.shader')
        shutil.copyfile(generated / 'remaster-test.dat', case / 'id1/remaster-test.dat')
        (case / 'id1/remaster-captures.cfg').write_text(CAPTURES)
        (case / 'id1/remaster-test.cfg').write_text(f'''cfg_save_auto 0
ruleset_allow_in 1
m_preset_chosen 1
con_notifytime 0
developer 1
sv_public 0
fps_preset faithful
r_replacemodels "{replacement}"
r_fullbright 1
r_fb_models 0
r_dynamic 0
sv_csqc_progname remaster-test.dat
set gpu_remaster_models 0
set gpu_remaster_frame 0
map {CAMPAIGNS[model_game]}
''')
    for name in campaigns:
        case = out / 'campaigns' / name
        assets(case, name)
        pak = Pak(game / name / 'pak0.pak')
        firstmap = CAMPAIGNS[name]
        if f'maps/{firstmap}.bsp' not in pak.entries:
            raise ValueError(f'Missing campaign test map: {name}/{firstmap}')
        (case / name / 'remaster-test.cfg').write_text(f'''cfg_save_auto 0
m_preset_chosen 1
con_notifytime 0
developer 1
sv_public 0
fps_preset faithful
map start
defer 9 god
defer 10 screenshot remaster-start.png
defer 11 save remaster-check
defer 12 map {firstmap}
defer 18 screenshot remaster-firstmap.png
defer 19 gpu_info
defer 20 load remaster-check
defer 27 screenshot remaster-restored.png
defer 28 echo REMASTER_CAMPAIGN_CLIENT_OK
defer 29 quit force
''')
    print(f'Prepared {len(models)} MD5 models and {len(campaigns)} campaigns in {out}', flush=True)


def inspect_log(case, filename, graphical=True):
    text = (case / filename).read_text(errors='replace')
    assert not re.search(r'Host_Error|Sys_Error|Bad builtin|runaway loop|invalid string offset|'
                         r'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|'
                         r'GPU.*Validation Error|SDL3 GPU error|(?<!\.)MD5(?:MESH|ANIM):|'
                         r'Error: filename too long', text, re.I), case
    unknown = set(re.findall(r'Unknown ssqc builtin: (\S+)', text))
    assert unknown <= OPTIONAL_BUILTINS, (case, unknown - OPTIONAL_BUILTINS)
    assert 'Server ended' in text, case
    if graphical:
        expected = 'metal' if sys.platform == 'darwin' else 'vulkan'
        assert f'SDL3 GPU renderer: {expected}' in text, case
    return text, sorted(unknown)


def check(out, profile):
    from PIL import Image, ImageChops, ImageStat
    manifest = json.loads((out / 'manifest.json').read_text())
    results = {}

    def picture(case, name):
        paths = list(case.rglob(f'remaster-{name}.png'))
        assert len(paths) == 1, (case, name, paths)
        return Image.open(paths[0]).convert('RGB')

    def delta(a, b):
        assert a.size == b.size
        return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3

    if profile in ('models', 'all'):
        pictures = {}
        for mode in ('modern', 'classic'):
            case = out / mode
            text, optional = inspect_log(case, 'models.log')
            assert 'Quake Re-Release Initialized' in text, 'Portable installation was not auto-detected'
            actual = {name: int(bones) for name, bones in re.findall(r'REMASTER_MODEL (\S+) bones=(\d+)', text)}
            expected = {(name if info.get('compatible', True) else name.removesuffix('.md5mesh') + '.mdl'):
                        (info['bones'] if info.get('compatible', True) else 0)
                        for name, info in manifest['models'].items()}
            assert actual == expected, (mode, actual, expected)
            animations = {name: (int(last), int(beyond)) for name, last, beyond in
                          re.findall(r'REMASTER_ANIM (\S+) last=(-?\d+) beyond=(-?\d+)', text)}
            expected_animations = {name: (info['frames']-1, -1) for name, info in manifest['models'].items() if info['frames'] and info.get('compatible', True)}
            assert animations == expected_animations, (mode, animations, expected_animations)
            replacement = manifest['models'][manifest.get('replacement_model', 'progs/player.md5mesh')]
            replacement_bones = replacement['bones'] if mode == 'modern' and replacement.get('compatible', True) else 0
            if mode == 'modern':
                for name, info in manifest['models'].items():
                    if not info.get('compatible', True):
                        assert f'Skipping MD5 replacement {name}: incompatible mesh/animation joint counts' in text, name
            assert f'REMASTER_REPLACEMENT player bones={replacement_bones}' in text
            assert re.search(r'REMASTER_MODE\s+1\s+0\.016', text), 'Remaster defaults not applied'
            assert text.count('renderer initialized') >= 2, 'Video restart missing'
            frames = {name: picture(case, name) for name in
                      ('background', 'md5', 'replacement', 'pose30', 'lit', 'restart')}
            assert delta(frames['background'], frames['md5']) > 1, 'Models did not render'
            assert delta(frames['md5'], frames['pose30']) > .2, 'Animation poses did not change'
            assert delta(frames['pose30'], frames['lit']) > .05, 'Model lighting did not change'
            assert delta(frames['lit'], frames['restart']) < .1, 'Restart changed the fixed scene'
            difference = delta(frames['md5'], frames['replacement'])
            assert difference < .1 if mode == 'modern' else difference > .2, (mode, difference)
            pictures[mode] = frames
            results[mode] = {'models': len(actual), 'animations': len(animations),
                             'mdl_fallbacks': [name.removesuffix('.md5mesh') + '.mdl'
                                               for name, info in manifest['models'].items()
                                               if not info.get('compatible', True)],
                             'rendering_lighting_restart': 'passed', 'optional_unimplemented_builtins': optional}
        assert delta(pictures['modern']['md5'], pictures['classic']['md5']) < .1, 'Direct MD5 rendering depends on replacement preference'
    if profile in ('campaigns', 'all'):
        for name in manifest['campaigns']:
            case = out / 'campaigns' / name
            text, optional = inspect_log(case, 'campaign.log')
            assert f'Loaded progs {name}/progs.dat' in text, (name, 'wrong gamecode')
            assert 'REMASTER_CAMPAIGN_CLIENT_OK' in text, name
            assert f'SpawnServer: {CAMPAIGNS[name]}' in text and text.count('SpawnServer: start') >= 2, name
            assert not re.search(r"Couldn't (?:open|load).*remaster-check|Can't save|ERROR.*sav", text, re.I), name
            saves = [p for p in case.rglob('*remaster-check*') if p.name != 'remaster-test.cfg']
            assert saves, (name, 'save missing')
            for capture in ('start', 'firstmap', 'restored'):
                image = picture(case, capture)
                assert max(ImageStat.Stat(image).stddev) > 5, (name, capture, 'blank frame')
            results[name] = {'gamecode_maps_save_load': 'passed', 'optional_unimplemented_builtins': optional}
    (out / f'{profile}-results.json').write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps(results, indent=2), flush=True)
    return results


def run(out, engine, profile, server=None):
    manifest = json.loads((out / 'manifest.json').read_text())
    (out / f'{profile}-results.json').unlink(missing_ok=True)
    tasks = []
    if profile in ('models', 'all'):
        model_game = manifest.get('model_game', 'id1')
        modeargs = ['-quake_rerel', '-game', model_game] if model_game != 'id1' else []
        tasks += [(out / mode, 'models.log', modeargs) for mode in ('modern', 'classic')]
    if profile in ('campaigns', 'all'):
        tasks += [(out / 'campaigns' / name, 'campaign.log', ['-quake_rerel'] +
                   (['-game', name] if name != 'id1' else [])) for name in manifest['campaigns']]
    for index, (case, logfile, modeargs) in enumerate(tasks):
        # Models intentionally use automatic detection, including exports without QuakeEX.kpf.
        with (case / logfile).open('w') as log:
            subprocess.run([str(engine.resolve()), '-nohome', '-basedir', str(case),
                            '-window', '-width', '800', '-height', '600', '-port', str(28860+index),
                            '-gpu_debug'] + modeargs + ['+exec', 'remaster-test.cfg'], cwd=ROOT,
                           stdout=log, stderr=subprocess.STDOUT, check=True, timeout=75)
        print(f'Completed {case.name}: {case / logfile}', flush=True)
    if server and profile in ('campaigns', 'all'):
        for index, name in enumerate(manifest['campaigns']):
            case = out / 'campaigns' / name
            args = [str(server.resolve()), '-nohome', '-nostdin', '-basedir', str(case),
                    '-quake_rerel', '-port', str(28880+index)]
            if name != 'id1':
                args += ['-game', name]
            args += ['+sv_public', '0', '+developer', '1', '+map', 'start', '+defer', '2',
                     'map', CAMPAIGNS[name], '+defer', '4', 'echo', 'REMASTER_SERVER_OK',
                     '+defer', '5', 'quit', 'force']
            with (case / 'server.log').open('w') as log:
                subprocess.run(args, cwd=case, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
            text, _ = inspect_log(case, 'server.log', graphical=False)
            assert 'REMASTER_SERVER_OK' in text and f'Loaded progs {name}/progs.dat' in text, name
            print(f'Headless campaign passed: {name}', flush=True)
    check(out, profile)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare', 'run', 'check'))
    parser.add_argument('game', nargs='?', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--model-game', choices=tuple(CAMPAIGNS), default='id1',
                        help='Include this campaign’s MD5 assets in model checks (mg3 also renders its new monsters)')
    parser.add_argument('--qcc', type=Path, default=ROOT / 'build/all/fteqcc')
    parser.add_argument('--engine', type=Path, default=ROOT / 'build/fteqw')
    parser.add_argument('--server', type=Path)
    parser.add_argument('--profile', choices=('models', 'campaigns', 'all'), default='all')
    args = parser.parse_args()
    out = args.out.resolve()
    if args.action == 'prepare':
        if not args.game:
            parser.error('prepare requires the owned installation directory')
        prepare(args.game, out, args.qcc, args.model_game)
    elif args.action == 'run':
        run(out, args.engine, args.profile, args.server)
    else:
        check(out, args.profile)


if __name__ == '__main__':
    main()
