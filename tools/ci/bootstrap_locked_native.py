#!/usr/bin/env python3
"""CI 只取得已锁依赖；缓存每次核原 commit/文件，不覆盖有改动的目录。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import zipfile


def run(*args, cwd=None):
    return subprocess.run(list(map(str, args)), cwd=cwd, check=True,
                          stdout=subprocess.PIPE, text=True).stdout.strip()


def verify(path, expected):
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise RuntimeError('locked SHA256 mismatch: ' + str(path))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--cache', type=Path, required=True)
    a = p.parse_args()
    root, cache = a.root.resolve(), a.cache.resolve()
    cache.mkdir(parents=True, exist_ok=True)
    lock = json.loads((root / 'native/dependencies.lock.json').read_text())
    specs = dict(lock['libraries'], **{'godot-cpp': lock['godot_bridge']['binding']})
    for name, spec in specs.items():
        target = cache / 'deps' / name
        # CI 缓存由 runner 持有；仅信任这四个已锁定目录，仍逐文件核验。
        run('git', 'config', '--global', '--add', 'safe.directory', target)
        if not (target / '.git').exists():
            if target.exists():
                raise RuntimeError('refusing non-Git cache: ' + str(target))
            run('git', 'init', '-q', target)
            run('git', '-C', target, 'remote', 'add', 'origin', spec['repository'])
            run('git', '-C', target, '-c', 'credential.helper=', 'fetch', '--depth=1', '--no-tags', 'origin', spec['commit'])
            run('git', '-C', target, 'checkout', '--detach', 'FETCH_HEAD')
        if run('git', '-C', target, 'rev-parse', 'HEAD') != spec['commit']:
            raise RuntimeError('cached commit mismatch: ' + name)
        if run('git', '-C', target, 'status', '--porcelain', '--untracked-files=all'):
            raise RuntimeError('dirty dependency: ' + name)
        for relative, expected in spec['files'].items():
            verify(target / relative, expected)
    cmake = lock['toolchain']['cmake']
    archive = cache / 'cmake.tar.gz'
    if not archive.exists():
        run('curl', '--fail', '--location', '--retry', '3', '--output', archive, cmake['archive_url'])
    verify(archive, cmake['archive_sha256'])
    cmake_root = cache / ('cmake-' + cmake['version'] + '-linux-x86_64')
    if not cmake_root.exists():
        # 固定官方归档已核 SHA；仍拒绝出缓存目录的路径和链接。
        with tarfile.open(archive) as source:
            for entry in source.getmembers():
                destination = (cache / entry.name).resolve()
                if cache not in destination.parents or entry.issym() or entry.islnk():
                    raise RuntimeError('unexpected CMake archive entry: ' + entry.name)
            source.extractall(cache)
    verify(cmake_root / 'bin/cmake', cmake['binary_sha256'])
    editor_zip = cache / 'godot-editor.zip'
    verify(editor_zip, 'cadd3204e728a35d3f13adb7fd0d7902636b79f6b95c40c265eb73b6c35329e4')
    editor = cache / 'Godot_v4.7.2-stable_linux.x86_64'
    if not editor.exists():
        with zipfile.ZipFile(editor_zip) as source:
            editor.write_bytes(source.read(editor.name))
        editor.chmod(0o755)
    verify(editor, lock['godot_bridge']['sdk']['engine_sha256'])
    if run(editor, '--headless', '--version') != lock['godot_bridge']['sdk']['version']:
        raise RuntimeError('unexpected engine version')
    sdk = cache / 'sdk'
    sdk.mkdir(exist_ok=True)
    for flag, name in [('--dump-extension-api', 'extension_api.json'),
                       ('--dump-gdextension-interface', 'gdextension_interface.h'),
                       ('--dump-gdextension-interface-json', 'gdextension_interface.json')]:
        if not (sdk / name).exists():
            run(editor, '--headless', flag, cwd=sdk)
        verify(sdk / name, lock['godot_bridge']['sdk']['files'][name])
    entries = {'FLECS': cache/'deps/flecs', 'JSON': cache/'deps/json',
               'PICOSHA2': cache/'deps/picosha2', 'GODOT_CPP': cache/'deps/godot-cpp', 'GODOT_API': sdk}
    (cache / 'locked.cmake').write_text(''.join(
        'set(HANMO_%s_ROOT "%s" CACHE PATH "locked")\n' % (key, value)
        for key, value in entries.items()))
    print('Locked dependency and SDK bytes verified; target runtime acceptance still pending.')


if __name__ == '__main__':
    main()
