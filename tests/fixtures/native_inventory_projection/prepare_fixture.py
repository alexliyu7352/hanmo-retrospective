"""薄复用既有采购准备器，为可信GDScript导出建立独立工程，不下载依赖或改动原作者。"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def prepare(root: Path, output: Path, original_art: Path) -> dict:
    """保留旧准备器的正式stage和原tiny校验，只添加本片导出入口及真实Git来源身份。"""
    root, output, original_art = root.resolve(), output.resolve(), original_art.resolve(strict=True)
    if output.exists() or output == root or any(
        root / part in output.parents for part in ['game', 'tools', 'tests', 'schemas', 'content', 'native']
    ):
        raise ValueError('Choose a new fixture directory outside authored/source paths')
    helper = root / 'tests/fixtures/legacy_asset_projection/prepare_fixture.py'
    spec = importlib.util.spec_from_file_location('native_inventory_legacy_fixture', helper)
    if spec is None or spec.loader is None:
        raise RuntimeError('Legacy asset fixture helper cannot be loaded')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    original = module.prepare(root, output, original_art)
    exporter = root / 'game/tests/export_native_inventory_projection.gd'
    shutil.copy2(exporter, output / 'game/tests' / exporter.name)
    project = output / 'game/project.godot'
    original_setting = 'config/legacy_asset_projection_isolated=true'
    text = project.read_text(encoding='utf-8')
    if text.count(original_setting) != 1:
        raise RuntimeError('Legacy fixture isolation setting changed; inspect before adapting')
    project.write_text(text.replace(original_setting, original_setting + '\nconfig/native_inventory_projection_isolated=true'), encoding='utf-8')
    report = {
        'format': 'HM-NATIVE-INVENTORY-PREPARATION/1',
        'source_commit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
        'source_identity_kind': 'local Git base plus exact exporter digest; not publication identity',
        'exporter_sha256': hashlib.sha256(exporter.read_bytes()).hexdigest(),
        'helper_sha256': hashlib.sha256(helper.read_bytes()).hexdigest(),
        'source_sha256': original['source_sha256'],
        'art_sha256': original['art_sha256'],
        'game': str(output / 'game'),
        'scope': 'Actual Kernel procurement; separate original Inventory.count oracle; explicit pure Schema boundaries',
    }
    data = json.dumps(report, ensure_ascii=False, indent=2) + '\n'
    (output / 'game/data/native_inventory_export_source.json').write_text(data, encoding='utf-8')
    (output / 'native-inventory-preparation.json').write_text(data, encoding='utf-8')
    return report


def main() -> int:
    """只接受明确源码、新输出和已有原图路径，不隐式执行Godot或native。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--original-art', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source_root, args.output, args.original_art), ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
