#!/usr/bin/env python3
"""Run a headless engine startup/shutdown with isolated, data-free game paths."""
import argparse
import io
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import zipfile


def make_pak(members):
    payload, directory = bytearray(), bytearray()
    for name, data in members.items():
        directory += struct.pack('<56sII', name.encode(), 12 + len(payload), len(data))
        payload += data
    return struct.pack('<4sII', b'PACK', 12 + len(payload), len(directory)) + payload + directory


def write_fixture(pak, data):
    pak.parent.mkdir(parents=True, exist_ok=True)
    if pak.exists() and pak.read_bytes() != data:
        raise FileExistsError(f'Refusing to replace non-fixture game data: {pak}')
    pak.write_bytes(data)


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
    def launch(logname, mode, expected_utf8, basedir=out, commands=()):
        with (out / logname).open('w') as log:
            proc = subprocess.run([str(server), '-basedir', str(basedir), '-nohome',
                                   '-nostdin'] + mode + ['+sv_public', '0', '+echo',
                                   'ENGINE_SANITIZER_OK', '+echo', 'ENGINE_UTF8', '$com_parseutf8',
                                   '+echo', 'ENGINE_TICK', '$sv_mintic', *commands,
                                   '+quit', 'force'], cwd=basedir, env=env, stdout=log,
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
    archive = make_pak(members)
    write_fixture(out / 'id1/pak0.pak', archive)
    empty_basedir = out / 'empty'
    (empty_basedir / 'id1').mkdir(parents=True, exist_ok=True)
    launch('server.log', ['-quake'], 0, empty_basedir)
    text = launch('remaster-auto.log', [], 1)
    # The quit command can run before the final initialization banner.
    # Verify the profile's actual settings instead.
    assert re.search(r'ENGINE_TICK\s+0\.016666667\b', text), text
    launch('classic-override.log', ['-quake'], 0)

    basedir = out / 'read-members'
    write_fixture(basedir / 'id1/pak0.pak', make_pak({
        'pak_second.cfg': b'echo PAK_SECOND_OK\n',
        'empty.txt': b'',
        'pak_first.cfg': b'echo PAK_FIRST_OK\n',
    }))
    text = launch('read-members.log', ['-quake'], 0, basedir,
                  ('+exec', 'pak_first.cfg', '+exec', 'pak_second.cfg'))
    assert re.findall(r'^PAK_(?:FIRST|SECOND)_OK$', text, re.MULTILINE) == [
        'PAK_FIRST_OK', 'PAK_SECOND_OK'], text

    directory_length = struct.unpack_from('<I', archive, 8)[0]
    invalid_archives = {
        'short-header': (archive[:8], 'invalid PAK header'),
        'short-directory': (archive[:-1], 'invalid PAK directory'),
        'unaligned-directory': (archive[:8] + struct.pack('<I', directory_length - 1)
                                + archive[12:], 'invalid PAK directory'),
        'member-outside-file': (archive[:-4] + struct.pack('<I', len(archive)),
                                'invalid PAK member range'),
    }
    for name, (data, diagnostic) in invalid_archives.items():
        basedir = out / name
        write_fixture(basedir / 'id1/pak0.pak', data)
        text = launch(name + '.log', [], 0, basedir)
        assert diagnostic in text, text

    pk3 = io.BytesIO()
    with zipfile.ZipFile(pk3, 'w') as package:
        stored = zipfile.ZipInfo('pk3_stored.cfg')
        with package.open(stored, 'w', force_zip64=True) as member:
            member.write(b'echo PK3_STORED_OK\n')
        compressed = zipfile.ZipInfo('pk3_é.cfg')
        package.writestr(compressed, b'echo PK3_DEFLATED_OK\n', zipfile.ZIP_DEFLATED)
        package.comment = b'PK3 smoke fixture'

    for name, prefix in [('pk3', b''), ('prefixed-pk3', b'Package prefix\n')]:
        basedir = out / name
        write_fixture(basedir / 'id1/pak0.pk3', prefix + pk3.getvalue())
        text = launch(name + '.log', ['-quake'], 0, basedir,
                      ('+exec', 'pk3_stored.cfg', '+exec', 'pk3_é.cfg'))
        assert re.findall(r'^PK3_(?:STORED|DEFLATED)_OK$', text, re.MULTILINE) == [
            'PK3_STORED_OK', 'PK3_DEFLATED_OK'], text

    pk3_data = pk3.getvalue()
    footer = pk3_data.rindex(b'PK\x05\x06')
    directory_size = struct.unpack_from('<I', pk3_data, footer + 12)[0]
    truncated_directory = (pk3_data[:footer - 1] + pk3_data[footer:footer + 12]
                           + struct.pack('<I', directory_size - 1) + pk3_data[footer + 16:])
    invalid_pk3s = [
        ('short-pk3', b'PK', 'Failed opening zipfile'),
        ('truncated-pk3-comment', pk3_data[:-1], 'Failed opening zipfile'),
        ('truncated-pk3-directory', truncated_directory, 'appears to be missing its central directory'),
    ]
    for name, data, diagnostic in invalid_pk3s:
        basedir = out / name
        write_fixture(basedir / 'id1/pak0.pk3', data)
        text = launch(name + '.log', ['-quake'], 0, basedir)
        assert diagnostic in text and 'pak0.pk3' in text, text

    (out / 'results.json').write_text(json.dumps({'server_startup_shutdown': 'passed',
                                                'remaster_auto_detection': 'passed',
                                                'explicit_classic_override': 'passed',
                                                'pak_member_reads': 'passed',
                                                'pk3_reads_and_rejection': 'passed',
                                                'invalid_pak_rejection': sorted(invalid_archives)}, indent=2) + '\n')
    print('Sanitized startup/shutdown, remaster detection, classic override and archive validation passed')



if __name__ == '__main__':
    main()
