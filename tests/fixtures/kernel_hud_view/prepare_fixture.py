"""复用原真实场景tiny准备器，仅追加HUD专测；不修改作者玩法或复制完整美术。"""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[3]


def prepare(root: Path, output: Path, original_art: Path | None) -> dict:
    """用同一正式stage和原两图准备独立工程；HUD不初始化真实存储或完整Main。"""
    root, output = root.resolve(), output.resolve()
    spec = importlib.util.spec_from_file_location('scene_prepare', root / 'tests/fixtures/m0_scene_context/prepare_fixture.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    report = module.prepare(root, output, original_art)
    shutil.copy2(root / 'game/tests/test_kernel_hud_view.gd', output / 'game/tests/test_kernel_hud_view.gd')
    project = output / 'game/project.godot'
    project.write_text(project.read_text().replace('Hanmo M0 isolated scene context', 'Hanmo M0 isolated HUD view'))
    report['scope'] = 'Actual HUD text/controls and original personal selector; no combat GUI or save acceptance'
    return report


def main() -> int:
    """只接受新隔离输出及原tiny图，不覆盖旧输入/日志。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--original-art', type=Path)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source_root, args.output, args.original_art), ensure_ascii=False))
    return 0


if __name__ == '__main__': raise SystemExit(main())
