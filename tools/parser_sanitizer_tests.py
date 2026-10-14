#!/usr/bin/env python3
"""Exercise actual engine parsers and archive scans under ASan/UBSan.

No game data, network connections, or GPU are required. Fixtures supply only
host/error services and minimal input structures; parser bodies come from the
current engine source, not copied implementations. Requires GCC or Clang.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def function(path, name):
    source = (ROOT / path).read_text()
    # Ignore comments and quoted literals when finding a function's closing brace.
    pattern = r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    masked = re.sub(pattern, lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), source, flags=re.S)
    # A declaration prefix contains type/qualifier names and pointer stars,
    # never the parentheses of an if/call expression with the same name.
    match = re.search(r'^[ \t]*[A-Za-z_][\w \t*]*\b' + re.escape(name) + r'\s*\([^;{}]*\)\s*\{', masked, re.M)
    if not match:
        raise ValueError(f'Cannot find {name} in {path}')
    start = match.start()
    end = masked.index('{', start) + 1
    depth = 1
    while depth and end < len(masked):
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    if depth:
        raise ValueError(f'Unterminated body: {name}')
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='clang')
    parser.add_argument('--out', type=Path, default=ROOT / 'build/parser-tests')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'results.json').unlink(missing_ok=True)
    cases = {
        'messages': [('engine/common/common.c', name) for name in
                     ('SZ_Clear', 'SZ_GetSpace', 'MSG_WriteRawBytes', 'MSG_WriteRawBits',
                      'MSG_WriteBits', 'MSG_ReadRawBytes', 'MSG_ReadRawBits', 'MSG_ReadBits')],
        'strings': [('engine/common/common.c', 'COM_ParseCString')],
        'escapes': [('plugins/quake3/botlib/l_script.c', 'PS_ReadEscapeCharacter')],
        'numbers': [('engine/common/common.c', 'Q_atoi')],
        'archives': [('engine/common/fs.c', name) for name in
                     ('FS_FindWildDataFiles', 'FS_SortWildDataFiles', 'FS_LoadWildDataFiles')],
    }
    results = {}
    env = dict(os.environ)
    # These isolated fixtures own/free their allocations, so leak checking stays
    # enabled where the host runtime supports it. Compiler shutdown is separate.
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    if sys.platform.startswith('linux'):
        # These fixtures free all their allocations; override the engine's
        # process-lifetime-cache exemption and exercise LeakSanitizer too.
        options = env.get('ASAN_OPTIONS', '')
        env['ASAN_OPTIONS'] = (options + ':' if options else '') + 'detect_leaks=1'
    for name, bodies in cases.items():
        template = (ROOT / f'tools/tests/parser_{name}.c').read_text()
        generated = out / f'{name}-generated.c'
        generated.write_text(template.replace('/* ENGINE_FUNCTIONS */',
                                              '\n\n'.join(function(*body) for body in bodies)))
        exe = out / name
        compile_cmd = [args.cc, '-std=c17', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
                       '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                       '-fno-omit-frame-pointer', str(generated), '-o', str(exe)]
        with (out / f'{name}-build.log').open('w') as log:
            subprocess.run(compile_cmd, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
        with (out / f'{name}.log').open('w') as log:
            subprocess.run([str(exe)], stdout=log, stderr=subprocess.STDOUT,
                           check=True, timeout=30, env=env)
        results[name] = 'passed'
        print((out / f'{name}.log').read_text().strip())
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
