"""复用原军团短门的正式作者和tiny准备器，仅新增只读活动投影测试入口。"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def prepare(root: Path, output: Path, art: Path) -> dict:
    """只创建新独立工程；原军事作者、Schema与规则不变，不运行旧march全门。"""
    root, output, art = root.resolve(), output.absolute(), art.resolve(strict=True)
    if output.exists() or output == root or any(root / part in output.parents for part in ['game', 'tools', 'tests', 'schemas', 'content']):
        raise ValueError('Choose a new fixture directory outside authored/source paths')
    helper = root / 'tests/fixtures/military_march_kernel/prepare_fixture.py'
    spec = importlib.util.spec_from_file_location('activity_march_fixture', helper)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    original = module.prepare(root, output, art, False)
    test = root / 'game/tests/test_legacy_activity_projection.gd'
    shutil.copy2(test, output / 'game/tests' / test.name)
    project = output / 'game/project.godot'
    project.write_text(project.read_text().replace('config/military_march_isolated=true', 'config/military_march_isolated=true\nconfig/legacy_activity_projection_isolated=true'))
    report = {'kind': 'legacy-activity-projection', 'source_commit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
              'source_identity_kind': 'real Git local candidate; not publication', 'author_sha256': original['author_sha256'],
              'pack_set_digest': original['pack_set_digest'], 'helper_sha256': hashlib.sha256(helper.read_bytes()).hexdigest(),
              'test_sha256': hashlib.sha256(test.read_bytes()).hexdigest(), 'game': str(output / 'game'),
              'scope': 'Only readonly activity/office projection; reuse unchanged march author, no old march suite, UI or save migration'}
    (output / 'activity-preparation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    return report


def main() -> None:
    """调用方明确提供源码、全新输出和已核tiny原图，不下载或修改共享登记。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--original-art', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source_root, args.output, args.original_art), ensure_ascii=False))


if __name__ == '__main__':
    main()
