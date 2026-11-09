"""复用既有采购作者与原tiny图，为只读资产投影准备正常及零预算来源，不改生产内容。"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
ART_SHA = {'pixel.png': '1c98316a22280de1e7af3ab645d620ce26caa469799dbb11b13c1e8087392288',
           'portrait.png': 'b71e193aacf275c230b35e2e7e2a3f033822d245f0c32ef116807832953b079a'}


def prepare(root: Path, output: Path, original_art: Path | None = None) -> dict:
    """只写新独立工程；零预算是明确测试作者变体，随后全部经正式stage与Kernel命令。"""
    root, output = root.resolve(), output.resolve()
    if output.exists() or output == root or any(root / part in output.parents for part in ['game', 'tools', 'tests', 'schemas', 'content']):
        raise ValueError('Choose a new fixture directory outside authored/source paths')
    fixture = root / 'tests/fixtures/content_packs/compose/new_packs/base'
    art = original_art.resolve() if original_art is not None else fixture / 'assets/art'
    images = {}
    for name, expected in ART_SHA.items():
        raw = (art / name).read_bytes()
        if len(raw) != 77 or hashlib.sha256(raw).hexdigest() != expected:
            raise ValueError('Exact original tiny asset required: ' + str(art / name))
        images[name] = raw
    source_bytes = (fixture / 'content.json').read_bytes()
    zero = json.loads(source_bytes)
    zero.update(id='fixture.legacy_asset_zero', version='1.0.0')
    next(row for row in zero['offers'] if row['id'] == 'offer.grain')['budget'] = 0
    output.mkdir(parents=True)
    for name in ['tools', 'schemas', 'game/core']:
        shutil.copytree(root / name, output / name, ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '*.uid', '.godot'))
    data = output / 'game/data'; data.mkdir(parents=True)
    for name in ['rule_capabilities.json', 'module_contracts.json', 'pack_runtime.json', 'presentation.schema.json']:
        shutil.copy2(root / 'game/data' / name, data / name)
    shutil.copy2(fixture / 'presentation.json', data / 'presentation.json')
    assets = output / 'game/assets/art'; assets.mkdir(parents=True)
    for name, raw in images.items(): (assets / name).write_bytes(raw)
    source = output / 'content/foundation'; source.mkdir(parents=True)
    (source / 'procurement.json').write_bytes(source_bytes)
    (source / 'zero_budget.json').write_text(json.dumps(zero, ensure_ascii=False, indent=2) + '\n')
    tests = output / 'game/tests'; tests.mkdir()
    shutil.copy2(root / 'game/tests/test_legacy_asset_projection.gd', tests / 'test_legacy_asset_projection.gd')
    (output / 'game/project.godot').write_text('config_version=5\n[application]\nconfig/name="Legacy readonly asset projection"\nconfig/legacy_asset_projection_isolated=true\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    subprocess.run([sys.executable, '-c', 'from pathlib import Path; from tools import build; build.stage_content(Path.cwd(), foundation_sets=False)'], cwd=output, check=True)
    report = {'project': str(output / 'game'), 'source_sha256': hashlib.sha256(source_bytes).hexdigest(),
              'art_sha256': ART_SHA, 'scope': 'Existing procurement author unchanged; zero-budget explicit author variant; actual Kernel ownership/reservation inputs, no UI/native/save migration'}
    (output / 'preparation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    return report


def main() -> int:
    """调用方明确指定源码和新输出，资源缺失即失败，不下载、覆盖旧运行证据或改共享登记。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--original-art', type=Path)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source_root, args.output, args.original_art), ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
