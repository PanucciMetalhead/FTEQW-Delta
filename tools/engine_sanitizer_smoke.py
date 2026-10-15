#!/usr/bin/env python3
"""Run a headless engine startup/shutdown with isolated, data-free game paths."""
import argparse
import json
import os
from pathlib import Path
import re
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    server, out = args.server.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'results.json').unlink(missing_ok=True)
    (out / 'id1').mkdir(exist_ok=True)
    env = dict(os.environ)
    # Process-lifetime engine caches are not yet a shutdown-ownership audit.
    # Invalid accesses and undefined behavior must still terminate the process.
    env['ASAN_OPTIONS'] = 'detect_leaks=0'
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    def launch(logname, mode, expected_utf8):
        with (out / logname).open('w') as log:
            proc = subprocess.run([str(server), '-basedir', str(out), '-nohome',
                                   '-nostdin'] + mode + ['+sv_public', '0', '+echo',
                                   'ENGINE_SANITIZER_OK', '+echo', 'ENGINE_UTF8', '$com_parseutf8',
                                   '+echo', 'ENGINE_TICK', '$sv_mintic',
                                   '+quit', 'force'], cwd=out, env=env, stdout=log,
                                  stderr=subprocess.STDOUT, timeout=30)
        text = (out / logname).read_text(errors='replace')
        assert proc.returncode == 0, f'Server exited {proc.returncode}; inspect {out / logname}\n{text}'
        assert 'ENGINE_SANITIZER_OK' in text, 'Startup did not reach the command fixture'
        assert re.search(r'ENGINE_UTF8\s+' + str(expected_utf8) + r'\b', text), text
        assert not re.search(r'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:', text), text
        return text

    # A tiny synthetic PAK supplies only the identifying member names. It is
    # never used for models/gamecode; this exercises real startup mode selection.
    members = {'mapdb.json': b'{}', 'localization/loc_english.txt': b'm_test = "Test"\n',
               'progs/player.md5mesh': b'MD5Version 10\n'}
    payload, directory = bytearray(), bytearray()
    for name, data in members.items():
        directory += struct.pack('<56sII', name.encode(), 12+len(payload), len(data))
        payload += data
    archive = struct.pack('<4sII', b'PACK', 12+len(payload), len(directory)) + payload + directory
    pak = out / 'id1/pak0.pak'
    if pak.exists() and pak.read_bytes() != archive:
        raise FileExistsError(f'Refusing to replace non-fixture game data: {pak}')
    launch('server.log', ['-quake'], 0)
    pak.write_bytes(archive)
    text = launch('remaster-auto.log', [], 1)
    # The quit command can run before the final initialization banner.
    # Verify the profile's actual settings instead.
    assert re.search(r'ENGINE_TICK\s+0\.016666667\b', text), text
    launch('classic-override.log', ['-quake'], 0)
    (out / 'results.json').write_text(json.dumps({'server_startup_shutdown': 'passed',
                                                'remaster_auto_detection': 'passed',
                                                'explicit_classic_override': 'passed'}, indent=2) + '\n')
    print('Sanitized server startup/shutdown, portable remaster detection and explicit classic override passed')



if __name__ == '__main__':
    main()
