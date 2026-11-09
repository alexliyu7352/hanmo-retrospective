#!/usr/bin/env python3
"""独立复核只验原生层的构建与原K1-1结果；不把日志存在或零测试当通过。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

ARTIFACTS = {'hanmo_resident_journey', 'hanmo_sim', 'libhanmo_resident.a',
             'libhanmo_world.a', 'libhanmo_flecs.a',
             'libhanmo_resident_bridge_native.a', 'libhanmo_resident_storage.a'}
STAGES = ['configure', 'build-native', 'select-K1-1', 'checklist_K1_1']
SCOPE = 'CPP_NATIVE_AND_CHECKLIST_K1_1_ONLY'


def digest(path):
    """流式核实际文件，避免大产物一次读入内存。"""
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def require_k1_selection(selection):
    """选择集必须恰好为原K1-1；不存在或附带另一行均拒绝。"""
    if [row.get('name') for row in selection.get('tests', [])] != ['checklist_K1_1']:
        raise ValueError('Exactly the original checklist_K1_1 must be selected')


def require_k1_junit(path):
    """检查真实CTest的JUnit行，失败、跳过、零行均不能变成通过。"""
    root = ET.parse(path).getroot()
    rows = list(root.iter('testcase'))
    if len(rows) != 1 or rows[0].get('name') != 'checklist_K1_1':
        raise ValueError('JUnit must contain exactly checklist_K1_1')
    if any(list(root.iter(tag)) for tag in ('failure', 'error', 'skipped')):
        raise ValueError('K1-1 failed, errored, or was skipped')
    for suite in root.iter('testsuite'):
        if any(int(suite.get(key, '0')) for key in ('failures', 'errors', 'skipped')):
            raise ValueError('JUnit reports unsuccessful tests')


def require_package_metadata(text, name, version, expected):
    """APT的已验签索引仍须逐项符合锁中的原包文件及摘要。"""
    matches=[]
    for paragraph in text.strip().split('\n\n'):
        row=dict(line.split(': ',1) for line in paragraph.splitlines()
                 if ': ' in line and not line.startswith(' '))
        if row.get('Package')==name and row.get('Version')==version and 'Filename' in row:
            matches.append({'path':row['Filename'],'sha256':row.get('SHA256')})
    if not matches or any(row!=expected for row in matches):
        raise ValueError('Official Debian package metadata differs: '+name)


def require_compiler_record(record, spec):
    """编译器、头文件、共享runtime及官方映像都须符合本次源码锁。"""
    image=spec['container_image']['reference']
    required={'gcc-14','g++-14','gcc-14-base','gcc-14-x86-64-linux-gnu',
              'g++-14-x86-64-linux-gnu','cpp-14','cpp-14-x86-64-linux-gnu',
              'libgcc-14-dev','libstdc++-14-dev','libgcc-s1','libstdc++6'}
    if (not re.fullmatch(r'debian:trixie@sha256:[0-9a-f]{64}',image)
            or image!='debian:trixie@'+spec['container_image']['digest']
            or spec['container_image']['repository']!='docker.io/library/debian'
            or spec['container_image']['platform']!='linux/amd64'
            or spec['acquisition_method']!='official_debian_packages'
            or set(spec['packages'])!=required
            or spec['package_version']!='14.2.0-19'
            or record.get('image')!=image or record.get('platform')!='linux/amd64'
            or record.get('acquisition_method')!='official_debian_packages'
            or not re.fullmatch(r'https://snapshot\.debian\.org/archive/debian/[0-9]{8}T[0-9]{6}Z/',spec['repository'])
            or record.get('toolchain_repository')!=spec['repository']
            or record.get('suite')!='trixie'
            or record.get('archive_keyring')!='/usr/share/keyrings/debian-archive-keyring.gpg'
            or record.get('package_versions')!=spec['packages']
            or not spec['packages'] or any(v!='14.2.0-19' for v in spec['packages'].values())
            or record.get('cxx20_span_bit_cast_and_libstdcxx14_probe')!='PASS'):
        raise ValueError('Official Debian compiler/image evidence differs from lock')
    for name in ('gcc-14','g++-14'):
        first=record.get('tools',{}).get(name,{}).get('version','').splitlines()
        if not first or not re.fullmatch(r'[^ ]+ \(Debian 14\.2\.0-19\) 14\.2\.0',first[0]):
            raise ValueError('Actual Debian compiler version differs')


def verify(output, build, expected, toolchain_spec):
    """仅核本轮固定产物和独立原件；不读取回执自报的任意绝对路径。"""
    if not re.fullmatch('[0-9a-f]{40}', expected):
        raise ValueError('Full lowercase source commit required')
    receipt = json.loads((output / 'native-only-receipt.json').read_text())
    if (receipt.get('format') != 'HANMO-NATIVE-ONLY-CI/1'
            or receipt.get('scope') != SCOPE or receipt.get('status') != 'PASS'
            or receipt.get('source_commit') != expected
            or receipt.get('source_commit_after') != expected
            or receipt.get('inputs_unchanged') is not True
            or receipt.get('godot_bridge') != 'NOT_RUN'
            or receipt.get('other_checklist_rows') != 'NOT_RUN'):
        raise ValueError('Native-only source/scope/terminal receipt mismatch')
    stages = receipt.get('stages', [])
    if ([row.get('name') for row in stages] != STAGES
            or any(type(row.get('exit_code')) is not int or row['exit_code'] != 0 for row in stages)):
        raise ValueError('Original native build and K1-1 stages are incomplete')
    require_k1_selection(json.loads((output / 'k1-selection.json').read_text()))
    require_k1_junit(output / 'k1.junit.xml')
    rows = receipt.get('artifacts', [])
    if len(rows) != len(ARTIFACTS) or {row.get('path') for row in rows} != ARTIFACTS:
        raise ValueError('Native production output set differs')
    for row in rows:
        path = build / row['path']
        if (path.is_symlink() or not path.is_file() or path.stat().st_size <= 0
                or path.stat().st_size != row['bytes'] or digest(path) != row['sha256']):
            raise ValueError('Native production output differs: ' + row['path'])
        with path.open('rb') as stream:
            magic = stream.read(8)
        if row['path'].endswith('.a'):
            kind = 'thin' if magic == b'!<thin>\n' else 'normal' if magic == b'!<arch>\n' else None
            if not kind or row.get('archive_kind') != kind or row.get('member_count', 0) < 1:
                raise ValueError('Static archive shape differs')
        elif not magic.startswith(b'\x7fELF'):
            raise ValueError('Native CLI is not ELF')
        if row.get('archive_kind') == 'thin':
            members = row.get('thin_members', [])
            actual = subprocess.check_output(['ar', 't', str(path)], text=True).splitlines()
            actual_paths = []
            for name in actual:
                member = (path.parent / name).resolve()
                if build.resolve() not in member.parents:
                    raise ValueError('Actual thin member escapes build directory')
                actual_paths.append(str(member.relative_to(build.resolve())))
            if ([item.get('path') for item in members] != actual_paths
                    or len(set(actual_paths)) != len(actual_paths)):
                raise ValueError('Actual thin member inventory differs')
            if not members or len(members) != row.get('member_count'):
                raise ValueError('Thin archive member inventory missing')
            for member in members:
                relative = Path(member['path'])
                target = build / relative
                if (relative.is_absolute() or '..' in relative.parts or target.is_symlink()
                        or not target.is_file() or target.stat().st_size != member['bytes']
                        or digest(target) != member['sha256']):
                    raise ValueError('Thin archive member differs')
    for name in ('source-inputs.json', 'dependency-inputs.json', 'terrain-inputs.json',
                 'compiler.json', 'k1-selection.json', 'k1.junit.xml'):
        if digest(output / name) != receipt.get('evidence', {}).get(name):
            raise ValueError('Original evidence differs: ' + name)
    require_compiler_record(json.loads((output / 'compiler.json').read_text()), toolchain_spec)
    return {'status': 'PASS', 'scope': SCOPE, 'source_commit': expected,
            'native_output_count': len(rows), 'checklist_tests': ['checklist_K1_1'],
            'godot_bridge': 'NOT_RUN', 'other_checklist_rows': 'NOT_RUN',
            'receipt_sha256': digest(output / 'native-only-receipt.json')}


def main():
    """在容器停稳后由runner独立读取本轮构建目录，不改回执。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--expected-source', required=True)
    parser.add_argument('--lock', type=Path, required=True)
    args = parser.parse_args()
    spec=json.loads(args.lock.read_text())['toolchain']['gcc']
    print(json.dumps(verify(args.output, args.build, args.expected_source, spec), indent=2))


if __name__ == '__main__':
    main()
