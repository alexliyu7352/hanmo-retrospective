#!/usr/bin/env python3
"""Create a download only after the actual exported game passed the target smoke."""
import argparse
import hashlib
import json
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    record = json.loads((output / 'preview-path.json').read_text())
    result = json.loads((output / 'smoke-result.json').read_text())
    abi = json.loads((output / 'package-glibc-abi.json').read_text())
    target = json.loads((output / 'target-smoke.json').read_text())
    if (target.get('target') != 'Ubuntu 20.04' or target.get('template_title_exit') != 0
            or target.get('fixed_editor_packed_world_exit') != 0
            or target.get('result_sha256') != hashlib.sha256((output / 'smoke-result.json').read_bytes()).hexdigest()):
        raise RuntimeError('Exact target smoke terminal required')
    if result.get('ok') is not True or result.get('source_commit') != record['source_commit'] or abi.get('glibc_abi_gate') != 'PASS':
        raise RuntimeError('Actual package target smoke and ABI pass required')
    folder = Path(record['folder'])
    if folder.parent != output or not folder.is_dir():
        raise RuntimeError('Unexpected preview folder')
    sys.path.insert(0, str(args.root.resolve()))
    from tools.native_resident_profile import package_profile, _files
    from tools.content_stage import canonical
    preview = {'format': 'HANMO-PLAYABLE-PREVIEW/1', 'source_commit': record['source_commit'],
               'target': 'Linux x86_64 / Ubuntu 20.04 or newer',
               'exported_title_start': 'PASS', 'fixed_editor_same_pck_world_start': 'PASS', 'gui_acceptance': 'NOT_RUN',
               'complete_game_acceptance': 'NOT_RUN', 'smoke': result}
    (folder / 'PREVIEW.json').write_bytes(canonical(preview))
    (folder / 'PREVIEW-README.txt').write_text(
        '汉末开发预览\n\n解压后运行 bash start.sh。无需另装 Godot。\n'
        '已核 Ubuntu 20.04 内导出程序标题启动；另由固定编辑器加载同一PCK及同字节SO进入章节。\n'
        '这是开发预览，完整玩法、GUI和跨版本存档迁移尚未验收。\n'
        '存档按源码版本使用独立目录；请保留原版本与存档。\n'
        '源码：' + record['source_commit'] + '\n', encoding='utf-8')
    manifest = json.loads((folder / 'BUILD.json').read_text())
    manifest['acceptance']['exported_title_start_ubuntu2004'] = 'PASS'
    manifest['acceptance']['fixed_editor_packed_world_start_ubuntu2004'] = 'PASS'
    manifest['files'] = _files(folder)
    manifest['files'].pop('BUILD.json')
    (folder / 'BUILD.json').write_bytes(canonical(manifest))
    archive = package_profile(folder)
    sha = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix('.zip.sha256').write_text(sha + '  ' + archive.name + '\n')
    (output / 'PREVIEW.json').write_bytes(canonical(preview))
    print(archive)


if __name__ == '__main__':
    main()
