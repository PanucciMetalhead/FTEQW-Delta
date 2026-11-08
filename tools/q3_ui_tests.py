#!/usr/bin/env python3
"""Check Q3 discovery, connections, downloads and pure packages without game data."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r'^(?:static )?[^\n;]+\b' + name + r'\s*\([^;]*?\)\s*\{', source, re.M)
    if not match:
        raise ValueError(f'Missing production function: {name}')
    pos = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[match.start():pos]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/q3-ui-tests')
    parser.add_argument('--cc', default='clang')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'plugins/quake3/clq3_ui.c').read_text()
    master = (ROOT / 'engine/client/net_master.c').read_text()
    start = master.index('//q3/dpm master responses')
    end = master.index('\n#ifdef HAVE_IPV6\n\t\t\tif (!strncmp(s, "qw_slist6', start)
    responses = master[start:end].replace('continue;', 'return;')
    fixture = (ROOT / 'tools/tests/q3_ui.c').read_text()
    generated = fixture.replace('/* ENGINE_FUNCTIONS */', '\n'.join(
        function(source, name) for name in ('Q3_SetKeyCatcher', 'UI_ServerCount', 'UI_ClearPing', 'UI_QueueCommand', 'UI_Init')))
    generated = generated.replace('/* MASTER_RESPONSES */', responses)
    generated = generated.replace('/* MASTER_REQUEST */', '\n'.join(
        function(master, name) for name in ('NetQ3_MasterForSlot', 'NetQ3_GlobalServers_Request')))
    path = out / 'q3-ui-generated.c'
    path.write_text(generated)
    binary = out / 'q3-ui-tests'
    subprocess.run([args.cc, '-std=c17', '-Wall', '-Wextra', '-Werror', '-g', '-O1',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined',
                    str(path), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    parse = (ROOT / 'plugins/quake3/clq3_parse.c').read_text()
    filesystem = (ROOT / 'engine/common/fs.c').read_text()
    fixtures = {
        'systeminfo': (parse, ('CLQ3_InfoValue', 'CLQ3_SystemInfoChanged')),
        'guid': ((ROOT / 'engine/client/cl_main.c').read_text(), ('CL_GUIDString',)),
        'disconnect': (parse, ('CLQ3_Disconnect',)),
        'download': (parse, ('CLQ3_ParseDownload',)),
        'browserinfo': (source, ('UI_AppendServerInfo', 'UI_WriteServerInfo')),
        'packages': (filesystem, ('FS_GetPackHashes', 'FS_GetPackNames')),
        'reject': ('', ()),
        'packet': ('', ()),
        'cvars': ((ROOT / 'plugins/quake3/q3common.c').read_text(), ('VMQ3_Cvar_Register', 'VMQ3_Cvar_Update')),
        'lifecycle': ((ROOT / 'plugins/quake3/clq3_cg.c').read_text(), ('CG_Refresh', 'CG_Stop')),
    }
    for name, (production, names) in fixtures.items():
        generated = (ROOT / f'tools/tests/q3_{name}.c').read_text().replace(
            '/* ENGINE_FUNCTIONS */', '\n'.join(function(production, item) for item in names))
        if name == 'cvars':
            start = production.index('#define MAX_VMQ3_CVARS')
            end = production.index('int VMQ3_Cvar_Register', start)
            generated = generated.replace('/* CVAR_STORAGE */', production[start:end])
        if name == 'download':
            generated = '#define MAXCHUNKSIZE 65536\n' + generated
        if name == 'packet':
            core = (ROOT / 'engine/client/cl_main.c').read_text()
            start = core.index('\tcase CP_QUAKE3:', core.index('void CL_ReadPacket(void)'))
            end = core.index('\n\tcase CP_QUAKEWORLD:', start)
            generated = '#define Q3CLIENT\n' + generated.replace('/* Q3_PACKET */', core[start:end])
        if name == 'reject':
            core = (ROOT / 'engine/client/cl_main.c').read_text()
            start = core.index("\tif (c == 'p')", core.index('// print command from somewhere'))
            end = core.index('\n\tif (c == A2C_PRINT)', start)
            generated = generated.replace('/* Q3_REJECTION */', core[start:end])
        if name == 'packages':
            references = []
            start = 0
            for _ in range(2):
                start = filesystem.index('\t\t\tif (fs_refnames && fs_refcrcs)', start)
                end = filesystem.index('\n\t\t\t}', start) + len('\n\t\t\t}')
                references.append(filesystem[start:end])
                start = end
            generated = generated.replace('/* CHECK_REFERENCES */', references[0])
            generated = generated.replace('/* REBUILD_REFERENCES */', references[1])
        path = out / f'q3-{name}-generated.c'
        path.write_text(generated)
        binary = out / f'q3-{name}-tests'
        subprocess.run([args.cc, '-std=c17', '-Wall', '-Wextra', '-Werror', '-g', '-O1',
                        '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined',
                        str(path), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
