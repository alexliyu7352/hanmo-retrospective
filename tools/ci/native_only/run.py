#!/usr/bin/env python3
"""只验当前原生生产目标与原K1-1；失败原件保留，不继承历史通过。"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import verify

TERRAIN_COMMIT = '521c49bc88083157722261cb8dd97000d9db9d12'
FIXTURE = 'native/world/tests/resident/fixtures/exact_rest_only_catalog.json'
FIXTURE_SHA = '2840aad48f2993ce55634df727e8b15e8ace8076b47e9deeb139cb0debc10677'
TERRAIN_FILES = {
    'height.u16': (300000, 'bcf62166aaac08a368563a8a727fe2a3cd8622a5c85dca0c7a962b7917bc26db'),
    'land.u8x3': (450000, 'e4203d6b49864b61f4de200a8d9e428a152cf9e6bacdef4888a4a691a33cf8f4'),
    'rivers.u8x2': (300000, 'dfc076c4e63f8cbfb5025a03f2e0d06584252d143815a1e24327497e71cee9b7'),
    'terrain.json': (1756, '964351a7d2699416af0ce524ada7923a1f75ba752e3696673f6fa1472ca53544'),
}


def now():
    """记录本次实际时间，不改历史回执。"""
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def git(root, *args):
    """只查询给定工作树，Git错误保持失败。"""
    return subprocess.check_output(['git', '-C', str(root)] + list(args), text=True).strip()


def require_commit(value):
    """分支和短SHA会漂移，必须提供完整小写提交。"""
    if not re.fullmatch('[0-9a-f]{40}', value):
        raise ValueError('Full lowercase source commit required')


def require_clean(root, expected):
    """源码身份与清洁状态都必须符合请求，禁止归档假身份。"""
    require_commit(expected)
    if git(root, 'rev-parse', 'HEAD') != expected or git(root, 'status', '--porcelain=v1', '--untracked-files=all'):
        raise ValueError('Requested clean immutable source differs')


def verify_tracked_inputs(root, prefixes):
    """按真实Git树逐文件核原字节，稀疏缺件和skip-worktree改动也拒绝。"""
    raw = subprocess.check_output(['git', '-C', str(root), 'ls-tree', '-rz', 'HEAD', '--'] + list(prefixes))
    rows = []
    for entry in raw.split(b'\0'):
        if not entry:
            continue
        metadata, path_raw = entry.split(b'\t', 1)
        mode, kind, expected = metadata.decode().split()
        relative = path_raw.decode('utf-8')
        path = root / relative
        if mode not in ('100644', '100755') or kind != 'blob' or path.is_symlink() or not path.is_file():
            raise ValueError('Missing or unsupported source input: ' + relative)
        data = path.read_bytes()
        actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if actual != expected:
            raise ValueError('Git source bytes differ: ' + relative)
        rows.append({'path': relative, 'git_blob': actual, 'bytes': len(data),
                     'sha256': hashlib.sha256(data).hexdigest()})
    if not rows:
        raise ValueError('Empty required input set')
    return rows


def dependencies(root, cache):
    """固定依赖提交、工作树与锁中字节在编译前后独立重核。"""
    lock = json.loads((root / 'native/dependencies.lock.json').read_text())
    result = {}
    for name in ('flecs', 'json', 'picosha2'):
        spec = lock['libraries'][name]
        directory = cache / 'deps' / name
        require_clean(directory, spec['commit'])
        files = {path: verify.digest(directory / path) for path in spec['files']}
        if files != spec['files']:
            raise ValueError('Locked dependency differs: ' + name)
        result[name] = {'commit': spec['commit'], 'files': files}
    return result


def terrain_inputs(terrain):
    """只接受已经验过的青川四个原件，不生成或替换测试输入。"""
    require_clean(terrain, TERRAIN_COMMIT)
    rows = verify_tracked_inputs(terrain, ['content/world/terrain/map.qingchuan'])
    expected_paths = {'content/world/terrain/map.qingchuan/' + name for name in TERRAIN_FILES}
    if {row['path'] for row in rows} != expected_paths:
        raise ValueError('Original terrain file set differs')
    for row in rows:
        size, digest = TERRAIN_FILES[Path(row['path']).name]
        if row['bytes'] != size or row['sha256'] != digest:
            raise ValueError('Original terrain bytes differ')
    return {'source_commit': TERRAIN_COMMIT, 'files': rows}


def build_commands(root, cache, output):
    """原生与K1使用同一真实CMake树；不添加Godot目标或其他checklist行。"""
    cmake = cache / 'cmake-3.31.12-linux-x86_64/bin/cmake'
    ctest = cmake.with_name('ctest')
    build = cache / 'build'
    return [
        ('configure', [cmake, '-S', root / 'native/world', '-B', build,
            '-G', 'Unix Makefiles', '-C', cache / 'locked-native-only.cmake',
            '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_FLAGS_RELEASE=-O2 -g0',
            '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -g0', '-DCMAKE_C_COMPILER=/usr/bin/gcc-14',
            '-DCMAKE_CXX_COMPILER=/usr/bin/g++-14', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
            '-DHANMO_BUILD_GODOT_BRIDGE=OFF', '-DHANMO_BUILD_RESIDENT_JOURNEY=ON',
            '-DHANMO_RESIDENT_TEST_FAULTS=OFF', '-DHANMO_SOURCE_ARCHIVE=OFF',
            '-DHANMO_ENABLE_CCACHE=OFF', '-DHANMO_ENABLE_PCH=OFF',
            '-DHANMO_BUILD_ACCELERATION_N1_PHASED=OFF',
            '-DFETCHCONTENT_FULLY_DISCONNECTED=ON', '-DFETCHCONTENT_UPDATES_DISCONNECTED=ON',
            '-DHANMO_CHECKLIST_TERRAIN_ROOT=/terrain', '-DPython3_EXECUTABLE=/usr/bin/python3']),
        ('build-native', [cmake, '--build', build, '--target', 'hanmo_native_complete', '--parallel', '1']),
        ('select-K1-1', [ctest, '--test-dir', build, '--show-only=json-v1', '-R', '^checklist_K1_1$']),
        ('checklist_K1_1', [ctest, '--test-dir', build, '-R', '^checklist_K1_1$',
            '--no-tests=error', '--output-on-failure', '--output-junit', output / 'k1.junit.xml']),
    ]


def output_rows(build):
    """七件生产产物必须真实存在；薄归档另核其所有成员，不冒独立发行库。"""
    rows = []
    for relative in sorted(verify.ARTIFACTS):
        path = build / relative
        if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
            raise ValueError('Missing native production output: ' + relative)
        row = {'path': relative, 'bytes': path.stat().st_size, 'sha256': verify.digest(path)}
        with path.open('rb') as stream:
            magic = stream.read(8)
        if relative.endswith('.a'):
            if magic not in (b'!<arch>\n', b'!<thin>\n'):
                raise ValueError('Invalid static archive: ' + relative)
            row['archive_kind'] = 'thin' if magic == b'!<thin>\n' else 'normal'
            members = subprocess.check_output(['ar', 't', str(path)], text=True).splitlines()
            if not members:
                raise ValueError('Empty static archive: ' + relative)
            row['member_count'] = len(members)
            if row['archive_kind'] == 'thin':
                row['thin_members'] = []
                for name in members:
                    member = (path.parent / name).resolve()
                    if build.resolve() not in member.parents or not member.is_file() or member.is_symlink():
                        raise ValueError('Thin archive member escaped or is missing')
                    row['thin_members'].append({'path': str(member.relative_to(build.resolve())),
                        'sha256': verify.digest(member), 'bytes': member.stat().st_size})
        elif not magic.startswith(b'\x7fELF'):
            raise ValueError('Native CLI is not ELF')
        rows.append(row)
    return rows


def main():
    """运行每一步并立即落真实终态；异常不保留先前PASS，也不回填验收。"""
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('root', 'cache', 'output', 'terrain'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--expected-source', required=True)
    args = parser.parse_args()
    root, cache, output, terrain = (getattr(args, key).resolve() for key in ('root', 'cache', 'output', 'terrain'))
    output.mkdir(parents=True, exist_ok=True)
    path = output / 'native-only-receipt.json'
    if path.exists() or (cache / 'build').exists():
        raise ValueError('This run requires fresh receipt and build directories')
    receipt = {'format': 'HANMO-NATIVE-ONLY-CI/1', 'scope': verify.SCOPE,
        'source_commit': args.expected_source, 'status': 'RUNNING', 'started_at': now(),
        'stages': [], 'godot_bridge': 'NOT_RUN', 'other_checklist_rows': 'NOT_RUN',
        'artifact_delivery': 'BUILD_EVIDENCE_ONLY_NOT_REDISTRIBUTABLE_LIBRARIES'}

    def save():
        """单次回执只追加实际阶段，不向历史目录写成功结果。"""
        path.write_text(json.dumps(receipt, indent=2) + '\n')

    def evidence(name, value):
        """本轮证据目录独占命名，已有原件拒绝覆盖。"""
        target = output / name
        with target.open('x') as stream:
            json.dump(value, stream, indent=2)
            stream.write('\n')

    save()
    try:
        require_clean(root, args.expected_source)
        source_before = verify_tracked_inputs(root, ['native', 'tools/ci/native_only'])
        if verify.digest(root / FIXTURE) != FIXTURE_SHA:
            raise ValueError('Accepted common fixture F differs')
        dep_before = dependencies(root, cache)
        terrain_before = terrain_inputs(terrain)
        evidence('source-inputs.json', source_before)
        evidence('dependency-inputs.json', dep_before)
        evidence('terrain-inputs.json', terrain_before)
        receipt['resources_before'] = {'disk_available_bytes': shutil.disk_usage(cache).free,
            'meminfo': Path('/proc/meminfo').read_text()}
        if receipt['resources_before']['disk_available_bytes'] < 8 * 1024**3:
            raise ValueError('Native full build requires at least 8 GiB free disk before starting')
        for name, command in build_commands(root, cache, output):
            command = list(map(str, command))
            stage = {'name': name, 'command': command, 'started_at': now()}
            receipt['stages'].append(stage)
            save()
            timeout = {'configure': 600, 'build-native': 7200, 'select-K1-1': 300,
                       'checklist_K1_1': 7320}[name]
            stage['outer_stage_limit_seconds'] = timeout
            try:
                if name == 'select-K1-1':
                    result = subprocess.run(command, cwd=root, stdout=subprocess.PIPE, text=True, timeout=timeout)
                    (output / 'k1-selection.json').write_text(result.stdout)
                else:
                    result = subprocess.run(command, cwd=root, timeout=timeout)
            except subprocess.TimeoutExpired:
                stage.update(exit_code=124, completed_at=now(), timed_out=True)
                save()
                raise
            stage.update(exit_code=result.returncode, completed_at=now())
            save()
            if result.returncode:
                raise RuntimeError('Stage failed: ' + name)
            if name == 'select-K1-1':
                verify.require_k1_selection(json.loads(result.stdout))
        verify.require_k1_junit(output / 'k1.junit.xml')
        require_clean(root, args.expected_source)
        receipt['inputs_unchanged'] = (source_before == verify_tracked_inputs(root, ['native', 'tools/ci/native_only'])
            and dep_before == dependencies(root, cache) and terrain_before == terrain_inputs(terrain))
        if not receipt['inputs_unchanged']:
            raise ValueError('Inputs changed during build or K1-1')
        receipt['source_commit_after'] = git(root, 'rev-parse', 'HEAD')
        receipt['artifacts'] = output_rows(cache / 'build')
        receipt['evidence'] = {name: verify.digest(output / name) for name in
            ('source-inputs.json', 'dependency-inputs.json', 'terrain-inputs.json',
             'compiler.json', 'k1-selection.json', 'k1.junit.xml')}
        receipt.update(status='PASS', completed_at=now())
        save()
        spec=json.loads((root / 'native/dependencies.lock.json').read_text())['toolchain']['gcc']
        verify.verify(output, cache / 'build', args.expected_source, spec)
    except Exception as error:
        receipt.update(status='FAILED', error=str(error), completed_at=now())
        save()
        raise


if __name__ == '__main__':
    main()
