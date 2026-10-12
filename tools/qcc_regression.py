#!/usr/bin/env python3
"""Check FTEQCC help/diagnostics and execute optimization fixtures in qcvm."""
import argparse
import json
import shutil
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, default=ROOT/'build/all/fteqcc')
    parser.add_argument('--vm', type=Path, default=ROOT/'build/all/qcvm')
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--out', type=Path, default=ROOT/'build/qcc-tests')
    args = parser.parse_args()
    compiler, vm, out = args.compiler.resolve(), args.vm.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    def run(argv, success=True):
        proc = subprocess.run([str(a) for a in argv], cwd=out, text=True, encoding='utf-8',
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        assert (proc.returncode == 0) == success, (argv, proc.returncode, proc.stdout)
        return proc.stdout
    def compile_case(source, flags=(), success=True, name='case'):
        (out/'case.qc').write_bytes(source.encode())
        output = out/'case.dat'
        if output.exists(): output.unlink()
        log = run([compiler, '-srcfile', 'case.qc', '-o', 'case.dat', *flags], success)
        (out/f'{name}.log').write_text(log, encoding='utf-8')
        assert output.exists() == success, log
        return log
    for topic, marker in [(None, 'Usage:'), ('warnings', 'uninitialized'),
                          ('optimizations', '-Ooverlaptemps'), ('flags', '-Fwasm'),
                          ('targets', '-Tq1'), ('manual', 'Exit status')]:
        option = '--help' if topic is None else '--manual' if topic == 'manual' else '--help='+topic
        assert marker in run([compiler, option]), option
        request = subprocess.run([str(compiler), option], cwd=out, capture_output=True, text=True, encoding='utf-8')
        assert marker in request.stdout and not request.stderr, 'help must use stdout'
    assert 'Unknown help topic' in run([compiler, '--help=unknown'], False)
    assert 'Invalid diagnostics format' in run([compiler, '--diagnostics=invalid'], False)
    assert not (out/'progs.dat').exists(), 'help must not emit bytecode'
    for option in ['-src', '-srcfile', '-o', '-I', '-max_regs']:
        assert 'Missing value' in run([compiler, option], False)
    invalid = 'float first = 1;\nfloat second = ;\n'
    rich = compile_case(invalid, success=False, name='error-rich')
    plain = compile_case(invalid, ['--diagnostics=plain'], False, 'error-plain')
    msvc = compile_case(invalid, ['-Fmsvcstyle'], False, 'error-msvc')
    assert '| float second = ;' in rich and '^' in rich, rich
    assert '|' not in plain and '^' not in plain, plain
    assert 'case.qc(2)' in msvc and '|' not in msvc, msvc
    assert 'case.qc:2: error' in plain
    assert '1 error occurred' in rich, rich
    warn = 'float(float x) sample = { local float y; if (x) y = 1; return y; };\n'
    log = compile_case(warn, ['-Wuninitialized'], name='warning')
    assert 'F302' in log and 'uninitialised' in log, log
    muted = compile_case(warn, ['-Wno-uninitialized'], name='warning-muted')
    assert 'F302' not in muted, muted
    promoted = compile_case(warn, ['-Werror-uninitialized'], False, 'warning-error')
    assert 'werror F302' in promoted, promoted
    compile_case(invalid.replace('\n', '\r\n').replace('float second', '\tfloat second'), success=False, name='crlf-tabs')
    # A macro's synthetic filename must not produce an unrelated source caret.
    macro = '#define BROKEN ;\nfloat value = BROKEN\n'
    macro_log = compile_case(macro, success=False, name='macro')
    assert 'macro' in macro_log, macro_log
    (out/'broken.qh').write_text(invalid)
    assert 'broken.qh' in compile_case('#include "broken.qh"\n', success=False, name='include')
    unicode = 'float first = 1; /* 世界 */ float broken = ;\n'
    ulog = compile_case(unicode, success=False, name='unicode')
    assert '世界' in ulog and '^' not in ulog, ulog
    for length in [0, 1, 239, 240, 241, 1000]:
        snippet = ' '*length + 'float broken = ;\n'
        compile_case(snippet, success=False, name=f'clip-{length}')
    longline = 'float first = 1;\n' + ' '*300 + 'float second = ;\n'
    longlog = compile_case(longline, success=False, name='long-line')
    assert '| ' in longlog and '^' in longlog and max(map(len,longlog.splitlines())) < 500
    # Byte-for-byte comparison uses identical physical paths and output options.
    fixture = (ROOT/'tools/tests/qcc_runtime.qc').read_bytes()
    (out/'runtime.qc').write_bytes(fixture)
    results = {}
    for level in range(4):
        options = ['-srcfile','runtime.qc','-o','runtime.dat','-Tq1',f'-O{level}','-Wnone']
        if args.baseline:
            run([args.baseline.resolve(), *options])
            baseline = (out/'runtime.dat').read_bytes()
        log = run([compiler, *options])
        (out/f'O{level}.log').write_text(log, encoding='utf-8')
        blob = (out/'runtime.dat').read_bytes()
        if args.baseline:
            assert blob == baseline, f'O{level}: bytecode changed'
        runtime = run([vm, 'runtime.dat'])
        (out/f'O{level}-vm.log').write_text(runtime, encoding='utf-8')
        assert 'QCC_RUNTIME_OK' in runtime and 'FAIL ' not in runtime, runtime
        header = struct.unpack_from('<15i', blob)
        assert header[0] == 6, header
        results[f'O{level}'] = {'bytes': len(blob), 'statements': header[3],
                               'globals': header[13], 'runtime': 'passed',
                               'baseline_bytecode': 'identical' if args.baseline else 'not checked'}
        shutil.copyfile(out/'runtime.dat', out/f'O{level}.dat')
    reported = run([compiler, '-srcfile', 'runtime.qc', '-o', 'runtime.dat', '-Tq1', '-O3', '-Wnone', '--opt-report'])
    assert 'Optimization report' in reported and 'Folded constant expressions:' in reported
    assert (out/'runtime.dat').read_bytes() == blob, 'report must not alter bytecode'
    assert results['O1']['statements'] < results['O0']['statements'], results
    (out/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    print(json.dumps(results, indent=2))
    print('Help, source excerpts, warning controls, and VM execution passed')

if __name__ == '__main__': main()
