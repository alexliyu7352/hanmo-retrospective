#!/usr/bin/env python3
"""只取得现有锁中的三种C++依赖与CMake；不加载Godot或改动旧bootstrap。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile


def run(*argv):
    """运行指定工具并保留非零失败，不执行来自数据的shell字符串。"""
    return subprocess.check_output(list(map(str, argv)), text=True).strip()


def verify(path, expected):
    """依赖较小且锁中明确列出原字节；不接受只有文件名相同。"""
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError('Locked bytes differ: ' + str(path))


def bootstrap(root, cache):
    """缓存只复用固定提交和已核字节，不包含先前构建或验收结果。"""
    lock = json.loads((root / 'native/dependencies.lock.json').read_text())
    cache.mkdir(parents=True, exist_ok=True)
    for name in ('flecs', 'json', 'picosha2'):
        spec = lock['libraries'][name]
        target = cache / 'deps' / name
        run('git', 'config', '--global', '--add', 'safe.directory', target)
        if not (target / '.git').exists():
            if target.exists():
                raise ValueError('Refuse non-Git dependency cache: ' + str(target))
            run('git', 'init', '-q', target)
            run('git', '-C', target, 'remote', 'add', 'origin', spec['repository'])
            run('git', '-C', target, '-c', 'credential.helper=', 'fetch', '--depth=1', '--no-tags', 'origin', spec['commit'])
            run('git', '-C', target, 'checkout', '--detach', 'FETCH_HEAD')
        if (run('git', '-C', target, 'rev-parse', 'HEAD') != spec['commit']
                or run('git', '-C', target, 'status', '--porcelain', '--untracked-files=all')):
            raise ValueError('Dependency commit or cleanliness differs: ' + name)
        for relative, expected in spec['files'].items():
            verify(target / relative, expected)
    spec = lock['toolchain']['cmake']
    archive = cache / 'cmake.tar.gz'
    if not archive.exists():
        run('curl', '--fail', '--location', '--retry', '3', '--output', archive, spec['archive_url'])
    verify(archive, spec['archive_sha256'])
    directory = cache / ('cmake-' + spec['version'] + '-linux-x86_64')
    if directory.exists():
        raise ValueError('CMake extraction must be fresh; cache only the verified archive')
    if not directory.exists():
        with tarfile.open(archive) as source:
            for entry in source.getmembers():
                destination = (cache / entry.name).resolve()
                if cache.resolve() not in destination.parents or not (entry.isfile() or entry.isdir()):
                    raise ValueError('Unsafe CMake archive member')
            if hasattr(tarfile, 'data_filter'):
                source.extractall(cache, filter='data')
            else:
                source.extractall(cache)
    verify(directory / 'bin/cmake', spec['binary_sha256'])
    entries = {'FLECS': cache / 'deps/flecs', 'JSON': cache / 'deps/json', 'PICOSHA2': cache / 'deps/picosha2'}
    (cache / 'locked-native-only.cmake').write_text(''.join(
        'set(HANMO_%s_ROOT "%s" CACHE PATH "locked native-only")\n' % row
        for row in entries.items()))


def main():
    """命令行只接受显式源码与专属缓存目录。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--cache', type=Path, required=True)
    args = parser.parse_args()
    bootstrap(args.root.resolve(), args.cache.resolve())


if __name__ == '__main__':
    main()
