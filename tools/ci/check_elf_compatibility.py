#!/usr/bin/env python3
"""拒绝 Ubuntu 20.04 无法提供的 glibc ABI；目标运行测试仍须另做。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def needed_versions(text):
    """仅读依赖段，库自身导出的版本不能误当最低运行需求。"""
    active = False
    result = set()
    for line in text.splitlines():
        line = line.strip()
        if line.startswith('Version '):
            active = line.startswith('Version needs section')
        if active:
            result.update(re.findall(r'\bName:\s+(\S+)', line))
    return sorted(result)


def glibc_violations(text):
    """数字按分段比较；未知或私有 glibc ABI 拒绝而非默许。"""
    bad = []
    for name in needed_versions(text):
        if not name.startswith('GLIBC_'):
            continue
        suffix = name[len('GLIBC_'):]
        if not re.fullmatch(r'\d+(?:\.\d+)+', suffix) or tuple(map(int, suffix.split('.'))) > (2, 31):
            bad.append(name)
    return bad


def inspect_elf(path):
    """检查每个真实 ELF 的架构和依赖，保留 C++ ABI 供目标环境核验。"""
    def read(*args):
        return subprocess.run(['readelf', *args, str(path)], check=True,
                              capture_output=True, text=True, env={'PATH': '/usr/bin:/bin', 'LC_ALL': 'C'}).stdout
    header = read('--file-header')
    versions = read('--version-info')
    dynamic = read('--dynamic')
    bad = glibc_violations(versions)
    if not re.search(r'Machine:\s+Advanced Micro Devices X86-64', header):
        bad.append('unsupported ELF machine')
    return {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'needed_versions': needed_versions(versions),
            'needed_libraries': re.findall(r'\(NEEDED\).*?\[(.*?)\]', dynamic),
            'violations': bad}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('paths', nargs='+', type=Path)
    args = parser.parse_args()
    rows = []
    for root in args.paths:
        if not root.exists():
            parser.error('missing input: ' + str(root))
        for path in sorted(root.rglob('*')) if root.is_dir() else [root]:
            if path.is_symlink():
                parser.error('materialize package symlink before checking: ' + str(path))
            if path.is_file():
                with path.open('rb') as handle:
                    magic = handle.read(4)
                if magic == b'\x7fELF':
                    rows.append(inspect_elf(path))
    if not rows:
        parser.error('no ELF files found')
    ok = all(not row['violations'] for row in rows)
    print(json.dumps({'glibc_abi_gate': 'PASS' if ok else 'FAIL', 'target': 'Ubuntu 20.04 x86_64',
                      'target_runtime': 'NOT_RUN', 'cpp_abi_runtime_resolution': 'NOT_RUN',
                      'files': rows}, indent=2))
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
