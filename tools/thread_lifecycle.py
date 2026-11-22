#!/usr/bin/env python3
"""Exercise real loader and SDL audio lifecycles without a display or game assets."""

import argparse
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import wave

from physics_integration import ROOT, bsp


def prepare(out, qcc):
    game = out / 'id1'
    (game / 'maps').mkdir(parents=True)
    (game / 'music').mkdir()
    for name in ('thread_a', 'thread_b'):
        (game / 'maps' / f'{name}.bsp').write_bytes(bsp())
    source = (ROOT/'tools/tests/server_benchmark.qc').read_text()
    (game/'thread.qc').write_text(source + '''
void() main = {};
void() SetNewParms = {};
void() SetChangeParms = {};
void() ClientConnect = {};
void() ClientDisconnect = {};
void() PlayerPreThink = {};
void() PlayerPostThink = {};
void() ClientKill = {};
void() PutClientInServer = {
    self.health = 100;
    self.view_ofs = '0 0 22';
    setorigin(self, '0 0 96');
};
''')
    with (out / 'compile.log').open('w') as log:
        subprocess.run([str(qcc), '-srcfile', 'thread.qc',
                        '-o', 'progs.dat'], cwd=game, stdout=log, stderr=subprocess.STDOUT,
                       timeout=30, check=True)
    with wave.open(str(game/'music/track05.wav'), 'wb') as audio:
        audio.setparams((1, 2, 22050, 0, 'NONE', 'not compressed'))
        audio.writeframes(struct.pack('<hh', 1000, -1000) * 44100)
    (game / 'loader.bin').write_bytes(bytes(range(256)) * 4096)
    queue = ['worker_test', 'fs_hash loader.bin', 'fs_hash loader.bin']
    steps = [
        ['map thread_a'],
        ['cd loop 5', *queue, 'worker_status'],
        ['echo THREAD_AUDIO_BEFORE', 'soundinfo', 'snd_restart', 'map thread_b'],
        ['cd loop 5', *queue, 'worker_count 1'],
        ['echo THREAD_AUDIO_AFTER_SOUND_RESTART', 'soundinfo', 'worker_count 4',
         'vid_restart', 'map thread_a'],
        ['cd loop 5', *queue, 'worker_status'],
        ['echo THREAD_AUDIO_AFTER_VIDEO_RESTART', 'soundinfo', *queue, 'echo THREAD_DONE', 'quit force'],
    ]
    for i, commands in enumerate(steps):
        if i + 1 < len(steps):
            commands.append(f'defer 0.5 exec thread-step-{i+1}.cfg')
        (game / f'thread-step-{i}.cfg').write_text('\n'.join(commands) + '\n')
    (game/'thread.cfg').write_text('''cfg_save_auto 0
plug_loaddefault 0
developer 1
net_enabled 0
ruleset_allow_in 1
snd_inactive 1
musicvolume 0.5
mastervolume 1
defer 0.5 exec thread-step-0.cfg
''')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--qcc', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--runs', type=int, default=3)
    args = parser.parse_args()
    if not 1 <= args.runs <= 20:
        parser.error('--runs must be between 1 and 20')
    root = args.out.resolve()
    root.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='run-', dir=root))
    print(f'Thread lifecycle output: {out}', flush=True)
    prepare(out, args.qcc.resolve())
    env = dict(os.environ, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='dummy')
    env['TSAN_OPTIONS'] = ':'.join(filter(None, (env.get('TSAN_OPTIONS'), 'halt_on_error=1')))
    for i in range(args.runs):
        path = out/f'engine-{i+1}.log'
        with path.open('w') as log:
            result = subprocess.run([str(args.engine.resolve()), '-nohome', '-basedir', str(out),
                                     '-quake', '-nostdin', '+vid_renderer', 'headless',
                                     '+set', 'plug_loaddefault', '0', '+s_device', 'SDL:', '+exec', 'thread.cfg'],
                                    cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=35)
        data = path.read_text(errors='replace')
        if result.returncode or re.search(r'ThreadSanitizer:|Host_Error|Sys_Error|Unknown command', data):
            raise RuntimeError(f'Engine failed (exit {result.returncode}). See {path}\n{data[-8000:]}')
        phases = re.split(r'THREAD_AUDIO_\w+\s*\n|THREAD_DONE\b', data)
        if (len(phases) != 5 or any(not re.search(r'music/track05\.wav[^\n]*, active\)', part)
                                   for part in phases[1:4]) or
                len(re.findall(r'SpawnServer: thread_[ab]\b', data)) < 3 or
                data.count('Ping:') < 4 or 'Ping:' not in phases[-1] or
                data.count('4 workers live') < 2 or
                data.count('audio output') < 3 or
                data.count('system tray disabled') < 2):
            raise RuntimeError(f'Lifecycle coverage markers missing. See {path}\n{data[-6000:]}')
    print(f'Loader, map changes, SDL mixer, restarts and shutdown passed in {args.runs} processes.')


if __name__ == '__main__':
    main()
