#!/usr/bin/env python3
"""窄共同/社会/庄园合同门；只读给定oracle，复用对象文件，不复制资源或运行全矩阵。"""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    """编译唯一生产实现并运行四个有范围声明的真实状态用例。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--json-root', type=Path, required=True)
    parser.add_argument('--oracle', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    args.build.mkdir(parents=True, exist_ok=True)
    production = [
        'storage/common_store.cpp', 'storage/common_codec.cpp',
        'storage/common_validation.cpp', 'storage/common_checkpoint.cpp',
        'checkpoint/ordered_value.cpp', 'domains/social/social_rules.cpp',
        'domains/social/social_codec.cpp', 'domains/estate/estate_rules.cpp',
        'domains/estate/estate_commands.cpp', 'domains/estate/estate_codec.cpp',
        'domains/estate/estate_checkpoint.cpp',
    ]
    common = ['c++', '-std=c++17', '-O0', '-g', '-ffp-contract=off',
              '-I', str(root / 'include'), '-I', str(root / 'src'),
              '-I', str(args.json_root / 'single_include')]
    newest_header = max(p.stat().st_mtime for base in [root / 'include', root / 'src/resident'] for p in base.rglob('*.hpp'))
    objects = []
    for relative in production:
        source = root / 'src/resident' / relative
        obj = args.build / relative.replace('/', '_').replace('.cpp', '.o')
        if not obj.exists() or obj.stat().st_mtime < max(source.stat().st_mtime, newest_header):
            subprocess.run(common + ['-c', str(source), '-o', str(obj)], check=True)
        objects.append(str(obj))
    index = json.loads((args.oracle / 'oracle-index.json').read_text())
    catalog = str(args.oracle / index['expanded_catalog']['file'])
    initial = str(args.oracle / index['initial_state']['file'])
    before = str(args.oracle / index['commands'][0]['after']['file'])
    after = str(args.oracle / index['commands'][1]['after']['file'])
    boundaries = list(dict.fromkeys([index['initial_state']['file']] + [c['after']['file'] for c in index['commands']]))
    for test, inputs in [('state_rng', [str(root / 'tests/resident/state_rng_vectors.json')]), ('state_resources', []), ('state_savepoint', []), ('state_membership', []), ('state_location_order', []), ('state_seed', [catalog, initial]), ('state_receipts', [catalog, initial]),
                         ('state_social', [catalog, before, after]), ('state_estate', [catalog, before, after]),
                         ('state_record_roundtrip', [catalog] + [str(args.oracle / p) for p in boundaries])]:
        target = args.build / test
        subprocess.run(common + [str(root / 'tests/resident' / (test + '.cpp'))] + objects + ['-o', str(target)], check=True)
        subprocess.run([str(target)] + inputs, check=True)


if __name__ == '__main__':
    main()
