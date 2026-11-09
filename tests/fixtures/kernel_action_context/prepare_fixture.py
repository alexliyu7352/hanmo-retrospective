"""复用正式tiny准备器，追加当前行动控制时序门；控制边界使用明确测试替身，不初始化玩家存储。"""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[3]


def prepare(root: Path, output: Path, original_art: Path | None) -> dict:
    """准备隔离core/UI依赖及原图；不改作者玩法或运行完整GUI。"""
    root, output = root.resolve(), output.resolve()
    spec = importlib.util.spec_from_file_location('scene_prepare', root / 'tests/fixtures/m0_scene_context/prepare_fixture.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    report = module.prepare(root, output, original_art)
    for name in ['test_kernel_action_context.gd', 'test_activity_flow_policy.gd']:
        shutil.copy2(root / 'game/tests' / name, output / 'game/tests' / name)
    project = output / 'game/project.godot'
    project.write_text(project.read_text().replace('Hanmo M0 isolated scene context','Hanmo M0 isolated action control'))
    report['scope'] = 'Actual Main control timing with explicit boundary doubles, plus existing procurement begin/one-tick completion through real Kernel and full Schema; no GUI/save or full-suite acceptance'
    return report


def main() -> int:
    """只写新隔离输出，原日志与既有世界保持不变。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--original-art', type=Path)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source_root,args.output,args.original_art),ensure_ascii=False))
    return 0


if __name__ == '__main__': raise SystemExit(main())
