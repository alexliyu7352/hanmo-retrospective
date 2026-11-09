"""复用已验真实Scene tiny准备器，补入单人物门面专测及旧caller的只读manifest，不复制大美术。"""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[3]


def prepare(root: Path, output: Path, original_art: Path | None) -> dict:
    """正式stage仍由原fixture负责；旧caller缺图分支只取原manifest，不伪造纹理。"""
    root, output = root.resolve(), output.resolve()
    source = root / 'tests/fixtures/m0_scene_context/prepare_fixture.py'
    spec = importlib.util.spec_from_file_location('m0_scene_prepare', source)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    report = module.prepare(root, output, original_art)
    for name in ['test_kernel_character_input.gd', 'test_character_input_callers.gd']:
        shutil.copy2(root / 'game/tests' / name, output / 'game/tests' / name)
    fixture = output / 'tests/fixtures/character_presentation'; fixture.mkdir(parents=True)
    shutil.copy2(root / 'tests/fixtures/character_presentation/manifest.json', fixture / 'manifest.json')
    project = output / 'game/project.godot'
    project.write_text(project.read_text().replace('Hanmo M0 isolated scene context', 'Hanmo M0 isolated kernel character input'))
    report['scope'] = 'One-person Kernel query and actual conversation caller; descriptor-only failure cases explicitly keep missing textures'
    return report


def main() -> int:
    """参数保持原fixture的隔离输出和原图选择，不初始化真实存储或读取玩家数据。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--original-art', type=Path)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source_root, args.output, args.original_art), ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
