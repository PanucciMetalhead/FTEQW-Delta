#!/usr/bin/env python3
"""Check music remapping, config persistence and SDL mixer playback without game data."""
import argparse
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import wave


def quote(value):
    # FTE's explicit C-style string syntax also protects literal quotes and dollars.
    return '\\"' + value.replace('\\', '\\\\').replace('"', '\\"').replace('$', '\\$') + '"'


def sections(text):
    parts = re.split(r'REMAP_PHASE_(\w+)\s*\n', text)
    return dict(zip(parts[1::2], parts[2::2]))


def mappings(text):
    return {int(n): value for n, value in re.findall(r'^\s+(\d+) -> (.*)$', text, re.M)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--reload-runs', type=int, default=1,
                        help='repeat fresh-process config restoration to check startup/shutdown races')
    args = parser.parse_args()
    if args.reload_runs < 1:
        parser.error('--reload-runs must be positive')
    args.out.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='remap-', dir=args.out.resolve()))
    (out / 'id1/music').mkdir(parents=True)
    (out / 'fte').mkdir()
    named = 'music/with spaces;literal$money.wav'
    for filename in ('music/track05.wav', named):
        with wave.open(str(out / 'id1' / filename), 'wb') as audio:
            audio.setparams((1, 2, 22050, 0, 'NONE', 'not compressed'))
            audio.writeframes(struct.pack('<hh', 1000, -1000) * 22050)

    expected = {n: f'music/slot{n}.wav' for n in range(1, 256)}
    expected.update({1: '1', 2: '5', 3: named, 4: 'music/quoted "track";$name.wav'})
    seed = [f'cd remaptrack {n} {quote(value)}' for n, value in expected.items()]
    # Preserve the original positional syntax and unspecified higher slots.
    seed += ['cd remap 1 5', 'echo REMAP_PHASE_INITIAL', 'cd remap', 'cfg_save saved']
    (out / 'id1/seed.cfg').write_text('\n'.join(seed) + '\n')
    invalid = ['cd remaptrack', 'cd remaptrack 2', 'cd remaptrack 0 bad',
               'cd remaptrack 256 bad', 'cd remaptrack -1 bad',
               'cd remaptrack 2x bad', 'cd remaptrack 999999999999999999999999999999 bad',
               'cd remaptrack 2 bad extra', 'cd remaptrack 2 ' + 'x' * 128]
    (out / 'id1/invalid.cfg').write_text('\n'.join(invalid) + '\n')
    setup = '''cfg_save_auto 0
plug_loaddefault 0
developer 1
net_enabled 0
music_fade 1
snd_inactive 1
musicvolume 0.5
mastervolume 1
'''
    # Schedule each step only after the preceding command buffer has executed.
    # Startup and slow sanitizer frames can otherwise reorder expired timers.
    steps = [
        (0.2, ['exec seed.cfg']),
        (0.2, ['cd remaptrack 2 ""', 'cfg_save_ifmodified cleared',
               'echo REMAP_PHASE_CLEARED', 'cd remap']),
        (1.5, ['exec configs/saved.cfg', 'echo REMAP_PHASE_RESTORED', 'cd remap', 'music 2']),
        (1.5, ['echo REMAP_PHASE_NUMERIC', 'soundinfo', 'cd play 3']),
        (4.6, ['echo REMAP_PHASE_NAMED', 'soundinfo', 'cd loop 3']),
        (1.5, ['echo REMAP_PHASE_LOOP', 'soundinfo', 'cd stop']),
        (0.2, ['echo REMAP_PHASE_STOP', 'soundinfo', 'musicvolume 0', 'cfg_save muted',
               'cd reset', 'cfg_save_ifmodified empty', 'echo REMAP_PHASE_RESET', 'cd remap',
               'cd remaptrack 255 stale', 'exec configs/empty.cfg',
               'echo REMAP_PHASE_EMPTY', 'cd remap']),
        (0.2, ['exec invalid.cfg', 'echo REMAP_PHASE_INVALID', 'cd remap']),
        (0, ['echo REMAP_PHASE_DONE', 'quit force']),
    ]
    for i, (delay, commands) in enumerate(steps):
        if i + 1 < len(steps):
            commands.append(f'defer {delay} exec remap-step{i + 1}.cfg')
        (out / f'id1/remap-step{i}.cfg').write_text('\n'.join(commands) + '\n')
    (out / 'id1/remap-test.cfg').write_text(setup + 'defer 1 exec remap-step0.cfg\n')
    env = dict(os.environ, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='dummy',
               ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')

    def run(config, name):
        logpath = out / (name + '.log')
        with logpath.open('w') as log:
            result = subprocess.run([str(args.engine.resolve()), '-nohome', '-basedir', str(out),
                '-quake', '-nostdin', '+vid_renderer', 'headless', '+set', 'plug_loaddefault', '0',
                '+s_device', 'SDL:', '+exec', config], cwd=out, env=env,
                stdout=log, stderr=subprocess.STDOUT, timeout=40)
        text = logpath.read_text(errors='replace')
        errors = re.findall(r'^.*(?:AddressSanitizer:|runtime error:|Sys_Error|Host_Error|Unknown command).*$', text, re.M)
        if result.returncode or errors:
            diagnostics = '\n'.join(errors)
            raise RuntimeError(f'Engine failure (exit {result.returncode}): {logpath}\n'
                               f'{diagnostics}\n{text[:2500]}\n...\n{text[-5000:]}')
        assert 'Headless: displayless SDL driver; system tray disabled.' in text, f'Dummy video created native tray UI: {logpath}'
        return sections(text)

    phases = run('remap-test.cfg', 'engine')
    assert 'DONE' in phases, f'Fixture did not finish: {out}'
    assert mappings(phases['INITIAL']) == expected, f'Positional/indexed mapping failed: {out}'
    assert mappings(phases['CLEARED']) == {n: v for n, v in expected.items() if n != 2}
    assert mappings(phases['RESTORED']) == expected, f'Config round trip failed: {out}'
    assert re.search(r'music/track05.wav[^\n]*, active\)', phases['NUMERIC']), f'Numeric/nonrecursive lookup failed: {out}'
    for phase in ('NAMED', 'LOOP'):
        assert re.search(re.escape(named) + r'[^\n]*, active\)', phases[phase]), f'{phase} playback failed: {out}'
    assert not re.search(r'music/[^\n]*, active\)', phases['STOP']), f'cd stop failed: {out}'
    for phase in ('RESET', 'EMPTY', 'INVALID'):
        assert not mappings(phases[phase]), f'{phase} did not leave empty mappings: {out}'
    for filename in ('cleared.cfg', 'empty.cfg'):
        assert list(out.rglob(filename)), f'Mapping changes did not trigger cfg_save_ifmodified: {out}'

    # Also replay the saved configuration in a fresh process, without in-memory state.
    (out / 'id1/reload-step.cfg').write_text('exec configs/saved.cfg\n'
        'echo REMAP_PHASE_RELOAD\ncd remap\nquit force\n')
    (out / 'id1/reload.cfg').write_text(setup + 'defer 1 exec reload-step.cfg\n')
    for i in range(args.reload_runs):
        reloaded = run('reload.cfg', 'reload' if i == 0 else f'reload-{i + 1}')
        assert mappings(reloaded['RELOAD']) == expected, f'Fresh-process restore failed: {out}'
    (out / 'results.json').write_text(json.dumps({
        'all_255_slots_and_quoted_paths': 'passed', 'positional_compatibility': 'passed',
        'config_round_trip_and_fresh_process': 'passed', 'save_if_modified': 'passed',
        'numeric_named_looping_and_stop': 'passed', 'reset_and_empty_restore': 'passed',
        'invalid_arguments': 'passed', 'fresh_process_runs': args.reload_runs}, indent=2) + '\n')
    print(f'Music remapping checks passed: {out}')


if __name__ == '__main__':
    main()
