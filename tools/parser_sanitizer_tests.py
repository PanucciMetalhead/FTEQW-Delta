#!/usr/bin/env python3
"""Exercise actual engine parsers and archive scans under ASan/UBSan.

No game data, network connections, or GPU are required. Fixtures supply only
host/error services and minimal input structures; parser bodies come from the
current engine source, not copied implementations. Requires GCC or Clang.
"""
import argparse
import hashlib
import hmac
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def function(path, name, occurrence=0):
    source = (ROOT / path).read_text()
    # Ignore comments and quoted literals when finding a function's closing brace.
    pattern = r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    masked = re.sub(pattern, lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), source, flags=re.S)
    # A declaration prefix contains type/qualifier names and pointer stars,
    # never the parentheses of an if/call expression with the same name.
    matches = list(re.finditer(r'^[ \t]*[A-Za-z_][\w \t*]*\b' + re.escape(name) + r'\s*\([^;{}]*\)\s*\{', masked, re.M))
    if not matches:
        raise ValueError(f'Cannot find {name} in {path}')
    match = matches[occurrence]
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
                      'MSG_WriteBits', 'MSG_ReadRawBytes', 'MSG_ReadRawBits', 'MSG_ReadBits',
                      'MSG_ReadLong', 'MSG_ReadDouble')],
        'strings': [('engine/common/common.c', 'COM_ParseCString')],
        'escapes': [('plugins/quake3/botlib/l_script.c', 'PS_ReadEscapeCharacter')],
        'numbers': [('engine/common/common.c', 'Q_atoi')],
        'archives': [('engine/common/fs.c', name) for name in
                     ('FS_FindWildDataFiles', 'FS_SortWildDataFiles', 'FS_LoadWildDataFiles')],
        'viewangles': [('engine/client/cl_ents.c', 'CLFTE_EncodeViewAngle')],
        'particle_types': [],
        'md5': [('engine/common/common.c', 'COM_ParseType')] + [('engine/common/com_mesh.c', name) for name in
                ('Mod_MD5JointCount', 'Mod_LoadMD5MeshModel')],
        # Select the real PACKAGE_TEXWAD implementation, after its disabled stub.
        'wads': [('engine/client/wad.c', 'Wad_NextDownload', -1)],
        'remaster': [('engine/common/fs.c', 'FS_QuakeRereleasePak')],
        'hashes': [('engine/common/sha1.c', 'hashfunc_terminate_uint')],
        'pickups': [('engine/client/zqtp.c', 'TP_CheckPickupSound')],
        'stats': [('engine/client/cl_parse.c', 'CL_SetStat_Internal')],
        'poses': [('engine/common/com_mesh.c', 'Q1MDL_AllocPoses')],
        'poses_server': [('engine/common/com_mesh.c', 'Q1MDL_AllocPoses')],
        'headers': [],
        'byteswap': [('engine/common/common.c', name) for name in ('ShortSwap', 'LongSwap', 'I64Swap')],
        'cookies': [],
        'gc': [('engine/qclib/initlib.c', 'PR_QCGC_Snapshot')],
        'sizes': [('engine/common/common.c', name) for name in ('COM_EncodeSize', 'COM_DecodeSize')],
        'zone': [('engine/common/zone.c', name) for name in ('ZG_Free', 'ZG_FreeGroup')],
        'zone_debug': [('engine/common/zone.c', name) for name in ('ZG_Free', 'ZG_FreeGroup')],
        'mesh': [('engine/gl/gl_model.c', 'ModQ1_AllocSurfaceMesh')],
        'mesh_wide': [('engine/gl/gl_model.c', 'ModQ1_AllocSurfaceMesh')],
        'normalmap': [('engine/client/image.c', 'Image_GenerateNormalMap')],
        'extents': [('engine/common/gl_q2bsp.c', 'CalcSurfaceExtents')],
        'audio': [('engine/client/snd_mem.c', 'SND_ResampleStream')],
        'worker_lifecycle': [('engine/common/common.c', name) for name in
                             ('COM_WorkerSync_WorkerStopped', 'COM_WorkerThread',
                              'COM_StartWorkerThread', 'COM_WorkerFullSync', 'COM_DestroyWorkerThread')],
        'music_seek': [('engine/client/snd_dma.c', 'S_Music_Seek')],
        'palette': [('engine/client/renderer.c', 'R_GenPaletteLookup')],
        'md4': [('engine/common/sha1.c', name) for name in
                ('hashfunc_terminate_uint', 'CalcHashInt', 'CalcHash')],
        'sha1': [('engine/common/sha1.c', name) for name in
                 ('SHA1Transform', 'SHA1Init', 'SHA1Update', 'SHA1Final', 'CalcHash', 'memxor', 'CalcHMAC')],
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
        template = (ROOT / f'tools/tests/parser_{name.removesuffix("_debug").removesuffix("_wide").removesuffix("_server")}.c').read_text()
        if name == 'viewangles':
            macro = re.search(r'^#define ANGLE2SHORT[^\n]+',
                              (ROOT / 'engine/common/protocol.h').read_text(), re.M)[0]
            template = template.replace('/* ENGINE_ANGLE_SCALE */', macro)
        if name == 'particle_types':
            for func, start_token, end_token, marker in (
                ('PScript_RunParticleEffectState', 'if (typenum >= FALLBACKBIAS', 'if (!ptype->loaded)', 'PARTICLE'),
                ('PScript_ParticleTrail', 'if (type >= FALLBACKBIAS', 'if (!ptype->loaded)', 'TRAIL'),
                ('PScript_Query', 'if (typenum < 0', 'if (body == 0)', 'QUERY')):
                source = function('engine/client/p_script.c', func)
                declaration = re.search(r'part_type_t \*ptype[^;]*;', source)[0]
                start = source.index(start_token)
                end = source.index(end_token)
                template = template.replace(f'/* ENGINE_{marker}_LOOKUP */', declaration + '\n' + source[start:end])
        if name == 'hashes':
            # Check the actual ZIP byte-order macro too, rather than duplicating
            # a corrected decoder in the fixture.
            macro = re.search(r'^#define LittleU4FromPtr[^\n]+',
                              (ROOT / 'engine/common/fs_zip.c').read_text(), re.M)[0]
            template = template.replace('/* ENGINE_ZIP_ENDIAN */', macro)
            gzip_size = re.search(r'unsigned int isize = [^;\n]+;',
                                  (ROOT / 'engine/common/fs_zip.c').read_text())[0]
            template = template.replace('/* ENGINE_GZIP_SIZE */', gzip_size)
        if name == 'palette':
            gloss = re.search(r'glossval \|= 0x01000000u? \* [^;\n]+;',
                              (ROOT / 'engine/client/r_2d.c').read_text())[0]
            template = template.replace('/* ENGINE_GLOSS_ALPHA */', gloss)
        if name == 'headers':
            source = (ROOT / 'engine/common/net_chan.c').read_text()
            statements = re.findall(r'^\s*w[12] = chan->(?:outgoing_sequence|incoming_sequence) \| [^;]+;', source, re.M)
            assert len(statements) == 2
            template = template.replace('/* ENGINE_HEADERS */', '\n'.join(statements))
        if name == 'cookies':
            source = (ROOT / 'engine/server/sv_main.c').read_text()
            start = source.index('unsigned int timebits;')
            end = source.index(';', source.index('newcl->realip_ping =', start)) + 1
            template = template.replace('/* ENGINE_COOKIE */', source[start:end])
            number = re.search(r'host_client->realip_num = [^;]+;',
                               (ROOT / 'engine/server/sv_user.c').read_text())[0]
            template = template.replace('/* ENGINE_REALIP_NUMBER */', number)
        if name == 'gc':
            context = re.search(r'struct qcgccontext_s\s*\{.*?\};',
                                (ROOT / 'engine/qclib/initlib.c').read_text(), re.S)[0]
            template = template.replace('/* ENGINE_GC_CONTEXT */', context)
        if name in ('zone', 'zone_debug'):
            source = (ROOT / 'engine/common/zone.c').read_text()
            start = source.index('typedef struct zonegroupblock_s')
            end = source.index('void ZG_Free(', start)
            template = template.replace('/* ENGINE_ZONE_ALLOCATOR */', source[start:end])
        if name in ('mesh', 'mesh_wide', 'poses', 'poses_server'):
            source = (ROOT / 'engine/common/mathlib.h').read_text()
            start = source.index('typedef float vec_t;')
            end = source.index('typedef\tint\tfixed4_t;', start)
            template = template.replace('/* ENGINE_VECTOR_TYPES */', source[start:end])
            mesh = re.search(r'typedef struct mesh_s\s*\{.*?\} mesh_t;',
                             (ROOT / 'engine/gl/gl_model.h').read_text(), re.S)[0]
            template = template.replace('/* ENGINE_MESH_TYPE */', mesh)
        if name in ('poses', 'poses_server'):
            source = (ROOT / 'engine/common/com_mesh.h').read_text()
            start = source.index('typedef struct', source.index('#ifdef NONSKELETALMODELS'))
            end = source.index('} galiaspose_t;', start) + len('} galiaspose_t;')
            template = template.replace('/* ENGINE_POSE_TYPE */', source[start:end])
            vbo = re.search(r'typedef union vboarray_s\s*\{.*?\} vboarray_t;',
                            (ROOT / 'engine/client/merged.h').read_text(), re.S)[0]
            template = template.replace('/* ENGINE_VBO_TYPE */', vbo)
        if name == 'audio':
            source = (ROOT / 'engine/client/snd_mem.c').read_text()
            start = source.index('#define LINEARUPSCALE(')
            end = source.index('// SND_ResampleStream:', start)
            template = template.replace('/* ENGINE_AUDIO_MACROS */', source[start:end])
        if name == 'md4':
            source = (ROOT / 'engine/common/common.h').read_text()
            end = source.index('} hashfunc_t;') + len('} hashfunc_t;')
            start = source.rfind('typedef struct', 0, end)
            template = template.replace('/* ENGINE_HASH_TYPES */', source[start:end])
            source = (ROOT / 'engine/common/md4.c').read_text().replace('#include "quakedef.h"', '')
            template = template.replace('/* ENGINE_MD4_SOURCE */', source)
        if name == 'sha1':
            source = (ROOT / 'engine/common/sha1.c').read_text()
            start = source.index('/* #define SHA1HANDSOFF')
            end = source.index('/* Hash a single', start)
            padding = '\n'.join(re.findall(r'^#define (?:IPAD|OPAD) [^\n]+', source, re.M))
            template = template.replace('/* ENGINE_SHA1_TYPES */', source[start:end] + padding)

            def bytes_for(size, seed):
                data = bytearray()
                for _ in range(size):
                    seed = (seed*1664525+1013904223) & 0xffffffff
                    data.append(seed >> 24)
                return bytes(data)

            vectors = []
            for size in (0, 1, 3, 55, 56, 63, 64, 65, 127, 128, 129, 8193, 1000000):
                digest = hashlib.sha1(bytes_for(size, 17)).hexdigest()
                vectors.append(f'{{{size}, 17, "{digest}"}},')
            template = template.replace('/* SHA1_REFERENCE_CASES */', '\n'.join(vectors))
            vectors = []
            for size in (0, 1, 20, 64, 65, 128, 200):
                digest = hmac.new(bytes_for(size, 99), bytes_for(127, 42), hashlib.sha1).hexdigest()
                vectors.append(f'{{{size}, "{digest}"}},')
            template = template.replace('/* HMAC_REFERENCE_CASES */', '\n'.join(vectors))
        generated = out / f'{name}-generated.c'
        generated.write_text(template.replace('/* ENGINE_FUNCTIONS */',
                                              '\n\n'.join(function(*body) for body in bodies)))
        exe = out / name
        compile_cmd = [args.cc, '-std=c17', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
                       '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                       '-fno-omit-frame-pointer', str(generated), '-o', str(exe), '-lm']
        if name == 'poses_server':
            compile_cmd.append('-DSERVERONLY')
        if name == 'mesh_wide':
            compile_cmd.append('-DTEST_WIDE_INDEX')
        if name == 'zone_debug':
            compile_cmd.append('-D_DEBUG')
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
