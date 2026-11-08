#!/usr/bin/env python3
"""Exercise SoundFont picker discovery, input, refresh and persistence.

Uses an isolated executable copy and original fixture files, without modifying
installed fonts or games. Requires a C compiler (macOS/Linux). --gpu captures
the picker in a native window; --game optionally supplies owned Quake artwork.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ADDED = '0 Added ; echo MIDI_INJECTED.sf2'


def check_captures(out, virtual_width=640, virtual_height=480):
    from PIL import Image, ImageChops
    def capture(name):
        matches = list(out.rglob(f'midi-picker-{name}.png'))
        assert len(matches) == 1, (name, matches)
        return Image.open(matches[0]).convert('RGB')
    before = capture('before')
    # Virtual coordinates: title art, heading and instructions,
    # ending above the current-filename row. These must survive list clipping.
    left = (virtual_width - 320) // 2
    top = max(0, (virtual_height - 200) // 2)
    sx, sy = before.width / virtual_width, before.height / virtual_height
    box = tuple(round(v * scale) for v, scale in zip((left, top, left + 320, top + 52),
        (sx, sy, sx, sy)))
    for name in ('refreshed', 'selected', 'empty'):
        later = capture(name)
        assert later.size == before.size
        assert ImageChops.difference(before.crop(box), later.crop(box)).getbbox() is None, \
            f'Picker header disappeared or changed after {name}'
    # Bitmap glyphs round to whole physical pixels. At fractional UI scales,
    # a strlen*8 button width can lose its final letter even with a clear header.
    empty = capture('empty')
    advance = int(8 * sy + 0.5)
    x = int((left + 56) * sx)
    for label, y in (('Rescan Folder', top + 72), ('Back', top + 100)):
        last = x + (len(label) - 1) * advance
        cell = empty.crop((last + 2, int(y * sy) + 2,
                           last + advance - 2, int(y * sy) + advance - 2))
        ink = sum(cell.convert('L').histogram()[65:])
        assert ink > cell.width * cell.height * 0.05, f'Final letter clipped: {label}'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--engine', type=Path, required=True)
    p.add_argument('--font', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--gpu', action='store_true')
    p.add_argument('--virtual-width', type=int, default=640, help='GPU UI width; 360 exercises fractional glyph rounding')
    p.add_argument('--virtual-height', type=int, default=480, help='GPU UI height; use 270 with width 360')
    p.add_argument('--game', type=Path)
    p.add_argument('--menusys', type=Path, help='Also test opening from the QuakeC audio menu')
    p.add_argument('--many', action='store_true', help='Check scrolling with forty extra fonts')
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='picker-', dir=args.out.resolve()))
    (out / 'id1/soundfonts/Directory.sf2').mkdir(parents=True)
    (out / 'soundfonts').mkdir()
    (out / 'fte').mkdir()
    (out / 'id1/music').mkdir()
    (out / 'id1/music/picker.mid').write_bytes(bytes.fromhex(
        '4d546864000000060000000100604d54726b0000001000c00000903c648140803c0000ff2f00'))
    binary = out / args.engine.name
    shutil.copy2(args.engine.resolve(), binary)
    def font(path):
        try:
            path.symlink_to(args.font.resolve())
        except OSError:
            shutil.copyfile(args.font, path)
    font(out / 'soundfonts/FluidR3_GM.sf2')
    font(out / 'id1/soundfonts/A Start.sf2')
    font(out / 'id1/soundfonts/Z Upper.SF2')
    font(out / ('id1/soundfonts/' + ADDED + '.pending'))
    if args.many:
        for i in range(40):
            font(out / f'id1/soundfonts/Test Font {i:02}.sf2')
    (out / 'id1/soundfonts/Ignored.txt').write_text('not a SoundFont')
    if args.game:
        for name in ('pak0.pak', 'pak1.pak'):
            src = args.game.resolve() / 'id1' / name
            if src.is_file():
                try:
                    (out / 'id1' / name).symlink_to(src)
                except OSError:
                    shutil.copyfile(src, out / 'id1' / name)
    if args.menusys:
        shutil.copyfile(args.menusys, out / 'fte/menusys.pk3')
    probe = r'''
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
static pluginputfuncs_t *input;
static void key(void) {
    char name[64], count[32];
    cmdfuncs->Argv(1,name,sizeof(name)); cmdfuncs->Argv(2,count,sizeof(count));
    int code=input->GetKeyCode(name,NULL);
    for(int i=0;i<(*count?atoi(count):1);i++) {
        input->KeyEvent(0,1,code,0); input->KeyEvent(0,0,code,0);
    }
}
static void report(void) {
    char value[2048], line[2200];
    cvarfuncs->GetString("snd_midi_soundfont",value,sizeof(value));
    snprintf(line,sizeof(line),"MIDI_MENU_SELECTION %s\n",value);
    plugfuncs->Print(line);
}
static void add(void) {
    if(rename("id1/soundfonts/0 Added ; echo MIDI_INJECTED.sf2.pending",
              "id1/soundfonts/0 Added ; echo MIDI_INJECTED.sf2"))
        plugfuncs->Print("MIDI_MENU_FIXTURE_FAILED add\n");
}
static void clear(void) {
    if(rename("id1/soundfonts","id1/soundfonts-hidden") ||
       rename("soundfonts","soundfonts-hidden"))
        plugfuncs->Print("MIDI_MENU_FIXTURE_FAILED clear\n");
}
qboolean Plug_Init(void) {
    input=plugfuncs->GetEngineInterface(pluginputfuncs_name,sizeof(*input));
    if(!input || !cmdfuncs->AddCommand("midi_key",key,"Fixture menu input") ||
       !cmdfuncs->AddCommand("midi_report",report,"Fixture font selection") ||
       !cmdfuncs->AddCommand("midi_add",add,"Fixture add font") ||
       !cmdfuncs->AddCommand("midi_clear",clear,"Fixture remove fonts")) return false;
    cmdfuncs->AddText("defer 0.5 exec midi-menu-test.cfg\n",false);
    return true;
}
'''
    (out / 'probe.c').write_text(probe)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-shared', '-fPIC', '-DFTEPLUGIN',
        '-DFTE_LITTLE_ENDIAN' if sys.byteorder == 'little' else '-DFTE_BIG_ENDIAN',
        *['-I' + str(ROOT / folder) for folder in ('plugins', 'engine/common', 'engine/client', 'engine/qclib')],
        str(out / 'probe.c'), str(ROOT / 'plugins/plugin.c'), '-o', str(out / 'fteplug_midiprobe.so')], check=True)
    audio = 'm_audio' if args.menusys else 'menu_audio'
    # Start at A so Up reaches Rescan; after adding/sorting, Up reaches the new 0 font.
    config = f'''snd_midi_soundfont "{(out / 'id1/soundfonts/A Start.sf2').as_posix()}"
music picker
defer 1 {audio}
defer 1.5 menu_midi
defer 2 screenshot midi-picker-before.png
defer 3 midi_add
defer 3.2 midi_key uparrow
defer 3.4 midi_key enter
defer 4 screenshot midi-picker-refreshed.png
defer 5 midi_key uparrow
defer 5.2 midi_key enter
defer 6 midi_report
defer 6.2 cfg_save midi-menu-choice
defer 7 menu_midi
defer 7.5 screenshot midi-picker-selected.png
defer 8 midi_key escape
defer 8.2 screenshot midi-picker-parent.png
defer 9 midi_clear
defer 9.5 menu_midi
defer 10 screenshot midi-picker-empty.png
defer 10.5 midi_key escape
defer 11 echo MIDI_MENU_DONE
defer 11.2 quit force
'''
    # Headless rendering cannot capture pixels.
    if not args.gpu:
        config = '\n'.join(line for line in config.splitlines() if ' screenshot ' not in line) + '\n'
    if args.many:
        config = config.replace('defer 8 midi_key escape', 'defer 7.2 midi_key downarrow 44\ndefer 8 midi_key escape')
    # Engine timers use an unordered list: several independent deadlines that
    # expire during a slow frame can execute in reverse order. Chain one future
    # action at a time, so rescan/navigation/selection always finish in order.
    initial, actions = [], []
    for line in config.splitlines():
        if line.startswith('defer '):
            _, when, command = line.split(' ', 2)
            actions.append((float(when), command))
        else:
            initial.append(line)
    actions.sort(key=lambda action: action[0])
    for i, (when, command) in enumerate(actions):
        step = command + '\n'
        if i + 1 < len(actions):
            delay = actions[i + 1][0] - when
            step += f'defer {delay:.3f} exec midi-menu-step-{i + 1}.cfg\n'
        (out / f'id1/midi-menu-step-{i}.cfg').write_text(step)
    config = '\n'.join(initial) + f'\ndefer {actions[0][0]:.3f} exec midi-menu-step-0.cfg\n'
    (out / 'id1/midi-menu-test.cfg').write_text(config)
    (out / 'id1/midi-menu-start.cfg').write_text('cfg_save_auto 0\nplug_loaddefault 0\n'
        'developer 1\nm_preset_chosen 1\nnet_enabled 0\nsnd_inactive 1\ndefer 1 plug_load midiprobe\n')
    env = dict(os.environ, SDL_AUDIODRIVER='dummy', ASAN_OPTIONS='detect_leaks=0',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    if not args.gpu:
        env['SDL_VIDEODRIVER'] = 'dummy'
    mode = ['-window', '-width', '800', '-height', '600', '+vid_conautoscale', '0',
            '+vid_conwidth', str(args.virtual_width), '+vid_conheight', str(args.virtual_height)] if args.gpu else ['+vid_renderer', 'headless']
    with (out / 'engine.log').open('w') as log:
        result = subprocess.run([str(binary), '-nohome', '-basedir', str(out), '-quake', '-nostdin',
            *mode, '+set', 'plug_loaddefault', '0', '+s_device', 'SDL:', '+exec', 'midi-menu-start.cfg'],
            cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=60)
    text = (out / 'engine.log').read_text(errors='replace')
    assert result.returncode == 0 and 'MIDI_MENU_DONE' in text, f'{out}\n{text[-8000:]}'
    assert not re.search(r'AddressSanitizer:|runtime error:|Sys_Error|Host_Error|MIDI_MENU_FIXTURE_FAILED|Unknown command', text), text
    expected = (out / 'id1/soundfonts' / ADDED).as_posix()
    assert 'MIDI_MENU_SELECTION ' + expected in text, f'Selection failed: {out}'
    extra = 40 if args.many else 0
    assert text.count(f'MIDI SoundFont picker: {3 + extra} files') == 1, out
    assert text.count(f'MIDI SoundFont picker: {4 + extra} files') == 2, out
    assert 'MIDI SoundFont picker: 0 files' in text, out
    assert 'Z Upper.SF2' in text and 'Ignored.txt' not in text, out
    assert not re.search(r'^MIDI_INJECTED', text, re.M), 'Filename executed as console text'
    selected = text.index('MIDI SoundFont selected:')
    assert 'Shutdown SDL sound' in text[selected:], 'Selection did not restart audio'
    assert 'MIDI: loaded music/picker.mid' in text[selected:], 'Music did not reload with the selected font'
    saved = list(out.rglob('midi-menu-choice.cfg'))
    assert len(saved) == 1 and expected in saved[0].read_text(), 'Selected font was not saved'
    if args.menusys:
        assert 'Initialized menu.dat' in text, 'QuakeC menus did not load'
    if args.gpu:
        check_captures(out, args.virtual_width, args.virtual_height)
    (out / 'results.json').write_text(json.dumps({'discovery': 'passed', 'refresh': 'passed',
        'selection': 'passed', 'empty_folder': 'passed', 'persistence': 'passed',
        'filename_escaping': 'passed', 'audio_restart': 'passed'}, indent=2) + '\n')
    print(f'MIDI picker checks passed: {out}')


if __name__ == '__main__':
    main()
