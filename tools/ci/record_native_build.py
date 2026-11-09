#!/usr/bin/env python3
"""Run the original native targets and record their actual source, dependencies and exits."""
import argparse
import datetime
import hashlib
import json
import re
import shutil
from pathlib import Path
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def require_source(root, expected):
    """只接完整提交，实际HEAD不等或源码有变立即拒绝，不解析浮动分支。"""
    if not re.fullmatch(r'[0-9a-f]{40}', expected):
        raise RuntimeError('Expected source must be a complete lowercase commit SHA')
    source = git(root, 'rev-parse', 'HEAD')
    if source != expected or git(root, 'status', '--porcelain=v1', '--untracked-files=normal'):
        raise RuntimeError('Actual clean source differs from requested commit')
    return source


def preserve_outputs(build, out):
    """两个target都成功后才保五个真实产物；逐字节复核，不覆盖历史输出。"""
    paths = ['hanmo_resident_journey', 'godot-bridge/libhanmo_world_bridge.so',
             'libhanmo_resident.a', 'libhanmo_world.a', 'libhanmo_flecs.a']
    sources = [build / relative for relative in paths]
    for source in sources:
        if not source.is_file() or source.is_symlink() or (out / source.name).exists():
            raise RuntimeError('Missing native output or existing destination: ' + str(source))
    artifacts = []
    for source in sources:
        before = digest(source)
        target = out / source.name
        shutil.copy2(source, target)
        if digest(target) != before or digest(source) != before:
            raise RuntimeError('Native output changed while preserving: ' + str(source))
        artifacts.append({'path': str(target), 'sha256': before,
                          'bytes': target.stat().st_size})
    return artifacts


def dependencies(root, cache):
    lock = json.loads((root / 'native/dependencies.lock.json').read_text())
    specs = dict(lock['libraries'], **{'godot-cpp': lock['godot_bridge']['binding']})
    result = {}
    for name, spec in specs.items():
        directory = cache / 'deps' / name
        files = {path: digest(directory / path) for path in spec['files']}
        if files != spec['files']:
            raise RuntimeError('Dependency bytes changed: ' + name)
        result[name] = {'root': str(directory), 'files': files}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'cache', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-source', required=True)
    args = parser.parse_args()
    root, cache, out = args.root.resolve(), args.cache.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = require_source(root, args.expected_source)
    before = dependencies(root, cache)
    cmake = cache / 'cmake-3.31.12-linux-x86_64/bin/cmake'
    receipt = {'format': 'HANMO-RECOVERY-NATIVE-BUILD/1', 'source_commit': source,
               'status': 'BUILD_RUNNING', 'started_at': now(), 'stages': [],
               'new_recovery_build': not (cache / 'build/CMakeCache.txt').exists(),
               'historical_artifacts_recreated': False,
               'configuration': {'type': 'Release', 'test_faults': False}}
    path = out / 'native-build-receipt.json'

    def save():
        path.write_text(json.dumps(receipt, indent=2) + '\n')

    commands = [('configure', [cmake, '-S', root / 'native/world', '-B', cache / 'build',
                 '-C', cache / 'locked.cmake', '-DCMAKE_BUILD_TYPE=Release',
                 '-DHANMO_BUILD_GODOT_BRIDGE=ON', '-DHANMO_BUILD_RESIDENT_JOURNEY=ON',
                 '-DHANMO_RESIDENT_TEST_FAULTS=OFF', '-DCMAKE_C_COMPILER_LAUNCHER=ccache',
                 '-DCMAKE_CXX_COMPILER_LAUNCHER=ccache'])]
    commands += [('build-' + target, [cmake, '--build', cache / 'build', '--target', target,
                                     '--parallel', '2'])
                 for target in ('hanmo_resident_journey', 'hanmo_world_bridge')]
    save()
    for name, command in commands:
        stage = {'name': name, 'command': list(map(str, command)), 'started_at': now()}
        receipt['stages'].append(stage)
        save()
        result = subprocess.run(stage['command'], cwd=root)
        stage.update(exit_code=result.returncode, completed_at=now())
        save()
        if result.returncode:
            receipt.update(status='BUILD_FAILED', completed_at=now())
            save()
            raise SystemExit(result.returncode)
    try:
        after = dependencies(root, cache)
        receipt.update(source_commit_after=git(root, 'rev-parse', 'HEAD'),
                       source_status_after=git(root, 'status', '--porcelain=v1', '--untracked-files=normal'),
                       dependencies=after, dependencies_unchanged_during_build=before == after)
        if receipt['source_commit_after'] != source or receipt['source_status_after'] or before != after:
            raise RuntimeError('Source or dependency changed during build')
        lock = json.loads((root / 'native/dependencies.lock.json').read_text())
        editor = cache / 'Godot_v4.7.2-stable_linux.x86_64'
        if digest(editor) != lock['godot_bridge']['sdk']['engine_sha256']:
            raise RuntimeError('Actual locked editor differs')
        receipt['engine'] = {'path': str(editor), 'sha256': digest(editor)}
        receipt['bridge_identity'] = json.loads((cache / 'build/godot-bridge/resident_bridge_identity.json').read_text())
        receipt['artifacts'] = preserve_outputs(cache / 'build', out)
        identity_files = []
        for original in [cache / 'build/godot-bridge/resident_bridge_identity.json',
                         root / 'native/dependencies.lock.json', cache / 'locked.cmake']:
            target = out / original.name
            if target.exists():
                raise RuntimeError('Existing native identity output: ' + str(target))
            shutil.copy2(original, target)
            if digest(original) != digest(target):
                raise RuntimeError('Native identity copy mismatch')
            identity_files.append({'path': str(target), 'sha256': digest(target),
                                   'bytes': target.stat().st_size})
        receipt['identity_files'] = identity_files
        receipt['requested_source_commit'] = args.expected_source
        require_source(root, args.expected_source)
        receipt.update(status='BUILD_COMPLETE_RUNTIME_NOT_RUN', completed_at=now())
        save()
    except Exception as error:
        receipt.update(status='BUILD_FAILED', error=str(error), completed_at=now())
        save()
        raise



if __name__ == '__main__':
    main()
