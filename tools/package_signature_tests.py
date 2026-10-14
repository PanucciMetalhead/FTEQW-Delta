#!/usr/bin/env python3
"""Check actual package trust/gate bodies in release and debug configurations.

Runs ASan/UBSan fixtures with injected provider outcomes. --gnutls additionally
uses the engine's actual GnuTLS verifier and ephemeral RSA signing credentials;
it requires GnuTLS development files and pkg-config, with no network or assets.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
from parser_sanitizer_tests import ROOT, function


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='clang')
    parser.add_argument('--out', type=Path, default=ROOT / 'build/package-tests')
    parser.add_argument('--gnutls', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'results.json').unlink(missing_ok=True)
    engine = 'engine/client/m_download.c'
    constants = '\n'.join(re.findall(r'^#define DPF_[^\n]+', (ROOT / engine).read_text(), re.M))
    constants += '\n' + re.search(r'^\s*#define ENGINEWEBSITE[^\n]+', (ROOT / 'engine/common/bothdefs.h').read_text(), re.M)[0]
    bodies = [function('engine/common/common.c', name) for name in
              ('dehex', 'Base64_Encode', 'Base64_Decode', 'Base64_EncodeBlock', 'Base64_DecodeBlock', 'Base16_DecodeBlock')]
    bodies += [function(engine, name) for name in
               ('PM_HashIsValid', 'PM_DecodeSignature', 'PM_ValidateAuthenticity', 'PM_SignatureOkay')]
    template = (ROOT / 'tools/tests/package_signatures.c').read_text()
    source = template.replace('/* ENGINE_FLAGS */', constants).replace('/* ENGINE_FUNCTIONS */', '\n\n'.join(bodies))
    if args.gnutls:
        source = source.replace('/* ENGINE_CRYPTO */', function('engine/common/net_ssl_gnutls.c', 'GNUTLS_VerifyHash'))
        crypto_flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'gnutls'], text=True))
    generated = out / 'packages-generated.c'
    generated.write_text(source)
    env = dict(os.environ)
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    if sys.platform.startswith('linux'):
        options = env.get('ASAN_OPTIONS', '')
        env['ASAN_OPTIONS'] = (options + ':' if options else '') + 'detect_leaks=1'
    modes = {'release': [], 'debug': ['-D_DEBUG']}
    if args.gnutls:
        modes.update({'gnutls-release': ['-DPACKAGE_REAL_CRYPTO'] + crypto_flags,
                      'gnutls-debug': ['-D_DEBUG', '-DPACKAGE_REAL_CRYPTO'] + crypto_flags})
    results = {}
    for name, extra in modes.items():
        exe = out / name
        cmd = [args.cc, '-std=c17', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
               '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
               '-fno-omit-frame-pointer', str(generated), '-o', str(exe)] + extra
        with (out / f'{name}-build.log').open('w') as log:
            subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
        with (out / f'{name}.log').open('w') as log:
            subprocess.run([str(exe)], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=60, env=env)
        results[name] = 'passed'
        print(name + ': ' + (out / f'{name}.log').read_text().strip())
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
