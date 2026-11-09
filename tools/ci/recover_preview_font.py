#!/usr/bin/env python3
"""Reuse exact licensed font bytes from this repository's existing playtest asset."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import zipfile

ARCHIVE_SHA = '12d291c6973c9ad2329f712265667e4ace9c37fbe62b16e205a8263ad728daf1'
FONT_SHA = 'b76b0433203017ca80401b2ee0dd69350349871c4b19d504c34dbdd80541690a'
LICENSE_SHA = '849f4ea9c214fa4ac3593b770c699f387534b11ce671264c1b10d85bdcb5997b'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('archive', 'engine', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    args = p.parse_args()
    if sha(args.archive) != ARCHIVE_SHA:
        raise RuntimeError('Published asset digest differs')
    args.output.mkdir(parents=True, exist_ok=True)
    binary = args.output / 'original-package.x86_64'
    license_path = args.output / 'FONT-LICENSE.txt'
    with zipfile.ZipFile(args.archive) as archive:
        binary.write_bytes(archive.read('export/hanmo.x86_64'))
        license_path.write_bytes(archive.read('export/FONT-LICENSE.txt'))
    if sha(license_path) != LICENSE_SHA:
        raise RuntimeError('Published license differs')
    isolated = args.output / 'font-reader'
    isolated.mkdir(exist_ok=True)
    (isolated / 'project.godot').write_text('config_version=5\n')
    font = args.output / 'NotoSansCJK-Regular.ttc'
    script = Path(__file__).with_name('extract_preview_font.gd').resolve()
    environment = dict(os.environ)
    for key, subdir in [('XDG_DATA_HOME', 'data'), ('XDG_CACHE_HOME', 'cache'), ('XDG_CONFIG_HOME', 'config')]:
        directory = args.output.resolve() / 'runtime' / subdir
        directory.mkdir(parents=True, exist_ok=True)
        environment[key] = str(directory)
    subprocess.run([str(args.engine.resolve()), '--headless', '--path', str(isolated.resolve()),
                    '--script', str(script), '--', str(binary.resolve()), str(font.resolve())], check=True, timeout=60, env=environment)
    if sha(font) != FONT_SHA:
        raise RuntimeError('Recovered font differs')
    print('Exact published font and full license recovered; no old Main or SO was loaded.')


if __name__ == '__main__':
    main()
