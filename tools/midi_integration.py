#!/usr/bin/env python3
"""Check MIDI music lookup/looping/restart in the real SDL mixer, without game data.

Uses SDL's dummy audio driver and the headless renderer. The engine must have
MIDI enabled and a packaged default SoundFont beside it (or pass --soundfont).
"""
import argparse
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--engine', type=Path, required=True)
    p.add_argument('--soundfont', type=Path)
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='midi-', dir=args.out.resolve()))
    for path in ('id1/music', 'id1/midi', 'fte'):
        (out / path).mkdir(parents=True)
    track = bytes.fromhex('00 c0 00 00 90 3c 64 81 40 80 3c 00 00 ff 2f 00')
    song = b'MThd' + struct.pack('>IHHH', 6, 0, 1, 96) + b'MTrk' + struct.pack('>I', len(track)) + track
    (out / 'id1/midi/loop.mid').write_bytes(song)
    (out / 'id1/music/once.mid').write_bytes(song)
    (out / 'id1/music/track02.mid').write_bytes(song)
    (out / 'id1/music/bad.mid').write_bytes(song[:-1])
    (out / 'id1/music/missing.mid').write_bytes(song)
    def vlq(value):
        result = bytearray([value & 127])
        while value := value >> 7:
            result.insert(0, (value & 127) | 128)
        return result
    # A long silent interval allows seeking past the old mixer's 32-bit sample
    # shift limit without waiting twelve minutes of wall-clock playback.
    longtrack = track[:-4] + vlq(899 * 192) + bytes.fromhex('ff 2f 00')
    (out / 'id1/midi/long.mid').write_bytes(song[:14] + b'MTrk' + struct.pack('>I', len(longtrack)) + longtrack)
    font = str(args.soundfont.resolve()).replace('\\', '/') if args.soundfont else 'soundfonts/FluidR3_GM.sf2'
    if any(c in font for c in ('"', '\n', '\r', '$')):
        raise ValueError('Unsupported character in fixture SoundFont path')
    config = f'''cfg_save_auto 0
plug_loaddefault 0
developer 1
net_enabled 0
music_fade 1
snd_inactive 1
musicvolume 0.5
mastervolume 1
snd_midi_soundfont "{font}"
defer 1 echo MIDI_PHASE_LOOP
defer 1.1 music loop
defer 2 soundinfo
defer 8 echo MIDI_PHASE_ONCE
defer 8.1 music once -
defer 12 echo MIDI_PHASE_NUMERIC
defer 12.1 music 2 -
defer 17 echo MIDI_PHASE_BAD
defer 17.1 music music/bad.mid -
defer 20 echo MIDI_PHASE_MISSING
defer 20.1 snd_midi_soundfont soundfonts/does-not-exist.sf2
defer 20.2 music missing -
defer 23 echo MIDI_PHASE_RESTART
defer 23.1 snd_midi_soundfont "{font}"
defer 23.2 snd_restart
defer 24 music 2 -
defer 28 echo MIDI_PHASE_LONG
defer 28.1 music long -
defer 29.5 music_fforward 750
defer 35 soundinfo
defer 36 echo MIDI_PHASE_DONE
defer 36.1 quit force
'''
    (out / 'id1/midi-test.cfg').write_text(config)
    env = dict(os.environ, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='dummy', ASAN_OPTIONS='detect_leaks=0',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    with (out / 'engine.log').open('w') as log:
        result = subprocess.run([str(args.engine.resolve()), '-nohome', '-basedir', str(out),
            '-quake', '-nostdin', '+vid_renderer', 'headless', '+set', 'plug_loaddefault', '0',
            '+s_device', 'SDL:', '+exec', 'midi-test.cfg'], cwd=out, env=env,
            stdout=log, stderr=subprocess.STDOUT, timeout=60)
    text = (out / 'engine.log').read_text(errors='replace')
    if result.returncode or re.search(r'AddressSanitizer:|runtime error:|Sys_Error|Host_Error', text):
        raise RuntimeError(f'Engine failure: {out / "engine.log"}\n{text[-5000:]}')
    phases = re.split(r'MIDI_PHASE_(LOOP|ONCE|NUMERIC|BAD|MISSING|RESTART|LONG|DONE)\s*\n', text)
    sections = dict(zip(phases[1::2], phases[2::2]))
    def loads(phase, track):
        return sections.get(phase, '').count('MIDI: loaded ' + track)
    assert 'DONE' in sections, f'Fixture did not finish: {out}'
    assert loads('LOOP', 'midi/loop.mid') >= 2, f'Named MIDI did not loop: {out}'
    assert loads('ONCE', 'music/once.mid') == 1, f'One-shot MIDI did not stop: {out}'
    assert loads('NUMERIC', 'music/track02.mid') == 1, f'Numeric MIDI lookup failed: {out}'
    assert 'Invalid MIDI' in sections['BAD'], f'Malformed MIDI not rejected: {out}'
    assert 'SoundFont not found' in sections['MISSING'], f'Missing font not reported: {out}'
    assert loads('RESTART', 'music/track02.mid') == 1, f'Sound restart failed: {out}'
    assert loads('LONG', 'midi/long.mid') == 1, f'Long-track seek failed: {out}'
    assert re.search(r'midi/long.mid[^\n]*active', sections['LONG']), f'Long-track seek stopped playback: {out}'
    report = {'named_lookup_looping': 'passed', 'one_shot': 'passed', 'numeric_track': 'passed',
              'malformed_midi': 'passed', 'missing_soundfont': 'passed', 'sound_restart': 'passed',
              'long_track_seek': 'passed'}
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'MIDI engine checks passed: {out}')


if __name__ == '__main__':
    main()
