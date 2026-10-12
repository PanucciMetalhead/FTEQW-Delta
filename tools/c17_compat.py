#!/usr/bin/env python3
"""Check C17 build flags, endian helpers, and generated gameplay instructions.

Requires a GCC/Clang CMake build with CMAKE_EXPORT_COMPILE_COMMANDS=ON.
Compare unmodified gameplay sources from --baseline-ref using GNU C11 against
the branch's ISO C17 output, with identical compiler and optimization flags.
This is a machine-code comparison, not a complete gameplay/runtime test.
"""
import argparse
import difflib
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
GAMEPLAY = ('pmove.c', 'sv_phys.c', 'world.c', 'mathlib.c')


def command(entry):
    return entry.get('arguments') or shlex.split(entry['command'])


def compile_flags(entry):
    argv = command(entry)
    result = [argv[0]]
    i = 1
    while i < len(argv):
        arg = argv[i]
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            i += 2
            continue
        if arg in ('-c', '-MD', '-MMD') or arg.startswith(('-std=', '-flto')) or arg == entry['file']:
            i += 1
            continue
        result.append(arg)
        i += 1
    return result


def normalized_assembly(path):
    # The source-directory strings differ; executable instructions must not.
    return '\n'.join(line for line in path.read_text().splitlines()
                     if not re.match(r'\s*\.(file|ident|addrsig|addrsig_sym)\b', line))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT/'build/c17')
    parser.add_argument('--out', type=Path, default=ROOT/'build/c17-compat')
    parser.add_argument('--baseline-ref', default='master')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    entries = json.loads((args.build.resolve()/'compile_commands.json').read_text())
    c_entries = [entry for entry in entries if entry['file'].endswith('.c')]
    assert c_entries, 'No C compile commands found'
    for entry in c_entries:
        assert '-std=c17' in command(entry), entry['file']
        assert not any(arg.startswith('-std=gnu') for arg in command(entry)), entry['file']
    results = {'c17_translation_units': len(c_entries), 'gameplay_assembly': {}}
    for name in GAMEPLAY:
        entry = next(entry for entry in c_entries
                     if Path(entry['file']).name == name and 'fteqw.dir/' in entry['command'])
        relative = Path(entry['file']).relative_to(ROOT)
        baseline = subprocess.check_output(['git', 'show', f'{args.baseline_ref}:{relative}'], cwd=ROOT)
        current = Path(entry['file']).read_bytes()
        assert baseline == current, f'{relative}: gameplay source changed; review before updating this guard'
        for dialect, source in [('gnu11', baseline), ('c17', current)]:
            directory = out/dialect
            directory.mkdir(exist_ok=True)
            (directory/name).write_bytes(source)
            subprocess.run([*compile_flags(entry), '-I'+str(Path(entry['file']).parent),
                            '-std='+dialect, '-S', '-g0', str(directory/name),
                            '-o', str(directory/(name+'.s'))], cwd=entry['directory'], check=True)
        before = normalized_assembly(out/'gnu11'/(name+'.s'))
        after = normalized_assembly(out/'c17'/(name+'.s'))
        if before != after:
            (out/(name+'.diff')).write_text('\n'.join(difflib.unified_diff(
                before.splitlines(), after.splitlines(), fromfile='gnu11', tofile='c17'))+'\n')
            raise AssertionError(f'{name}: generated instructions changed; inspect {out/name}.diff')
        results['gameplay_assembly'][str(relative)] = 'identical'
    compiler = command(c_entries[0])[0]
    exe = out/'endian-check'
    subprocess.run([compiler, '-std=c17', '-pedantic-errors', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I'+str(ROOT),
                    str(ROOT/'tools/tests/c17_endian.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    results['endian_sanitizers'] = 'passed'
    (out/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
