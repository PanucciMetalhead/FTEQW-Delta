#!/usr/bin/env python3
"""Fetch or stage the pinned MIT-licensed FluidR3 GM release with its notices.

Network access happens only with --download. For offline packaging use --source.
--out is the soundfonts directory beside the engine, e.g. build/soundfonts.
"""
import argparse
import hashlib
from pathlib import Path
import shutil
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
URL = 'https://github.com/pianobooster/fluid-soundfont/releases/download/v3.1/FluidR3_GM.sf2'
SHA256 = '74594e8f4250680adf590507a306655a299935343583256f3b722c48a1bc1cb0'
SIZE = 148398306


def verify(path):
    if path.stat().st_size != SIZE:
        raise ValueError(f'Incorrect FluidR3 GM file size: {path}')
    with path.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest() if hasattr(hashlib, 'file_digest') else None
        if digest is None:
            stream.seek(0)
            sha = hashlib.sha256()
            for block in iter(lambda: stream.read(1024 * 1024), b''):
                sha.update(block)
            digest = sha.hexdigest()
    if digest != SHA256:
        raise ValueError(f'Incorrect FluidR3 GM checksum: {path}')


def stage(source, out):
    verify(source)
    out.mkdir(parents=True, exist_ok=True)
    destination = out / 'FluidR3_GM.sf2'
    if source.resolve() != destination.resolve():
        # An interrupted copy must never appear as the final asset.
        with tempfile.NamedTemporaryFile(dir=out, delete=False) as temp:
            temporary = Path(temp.name)
        try:
            shutil.copyfile(source, temporary)
            verify(temporary)
            temporary.chmod(0o644)
            temporary.replace(destination)
        finally:
            temporary.unlink(missing_ok=True)
    notices = out / 'FluidR3-GM'
    notices.mkdir(exist_ok=True)
    for name in ('COPYING', 'README', 'SOURCE.txt'):
        src = ROOT / 'dist/soundfonts/FluidR3-GM' / name
        if src.resolve() != (notices / name).resolve():
            shutil.copyfile(src, notices / name)
    print(f'FluidR3 GM 3.1 staged: {destination}\nSHA-256: {SHA256}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--download', action='store_true', help='Explicitly download FluidR3 GM 3.1')
    source.add_argument('--source', type=Path, help='Existing unmodified FluidR3_GM.sf2')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if args.download:
        with tempfile.TemporaryDirectory(prefix='fte-fluidr3-') as folder:
            downloaded = Path(folder) / 'FluidR3_GM.sf2'
            with urllib.request.urlopen(URL, timeout=60) as response, downloaded.open('wb') as output:
                total = 0
                while block := response.read(1024 * 1024):
                    total += len(block)
                    if total > SIZE:
                        raise ValueError('FluidR3 download exceeds expected size')
                    output.write(block)
            stage(downloaded, args.out)
    else:
        stage(args.source, args.out)


if __name__ == '__main__':
    main()
