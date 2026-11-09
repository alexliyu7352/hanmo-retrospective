"""复用真实场景tiny准备器，明确补上同图world模板和作者映射，供有限场景门面调用测试。"""
import argparse
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]


def prepare(root: Path, output: Path, original_art: Path | None) -> dict:
    """仅扩隔离fixture的三尺度映射并重新正式stage；不改生产作者，不下载或复制大资源。"""
    root, output = root.resolve(), output.resolve()
    spec = importlib.util.spec_from_file_location('scene_prepare', root / 'tests/fixtures/m0_scene_context/prepare_fixture.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    report = module.prepare(root, output, original_art)
    path = output / 'game/data/presentation.json'
    manifest = json.loads(path.read_text())
    world = deepcopy(manifest['scene_templates']['fixture.scene.city'])
    world.update(id='fixture.scene.world', view_kind='world')
    manifest['scene_templates'][world['id']] = world
    path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    path = output / 'content/foundation/procurement.json'
    author = json.loads(path.read_text())
    for place in author['places']: place['scene_template_ids']['world'] = world['id']
    path.write_text(json.dumps(author, ensure_ascii=False, indent=2) + '\n')
    subprocess.run([sys.executable, '-c', 'from pathlib import Path; from tools import build; build.stage_content(Path.cwd(), foundation_sets=False)'], cwd=output, check=True)
    shutil.copy2(root / 'game/tests/test_kernel_scene_view.gd', output / 'game/tests/test_kernel_scene_view.gd')
    project = output / 'game/project.godot'
    project.write_text(project.read_text().replace('Hanmo M0 isolated scene context', 'Hanmo M0 isolated scene view'))
    report['scope'] = 'Actual Host finite-view boundary and two production callers; no GUI or save acceptance'
    return report


def main() -> int:
    """要求新隔离输出及原tiny图，不覆旧证据或读取玩家存储。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--original-art', type=Path)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source_root, args.output, args.original_art), ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
