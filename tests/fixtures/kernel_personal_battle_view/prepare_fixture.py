"""复用正式Alpha与原tiny图准备个人战有限查询门；不启用Native读写或GUI验收。"""
import argparse
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def prepare(output: Path, original_art: Path) -> dict:
    """只复用已有正式compose/stage助手，不复制另一套内容或战斗规则。"""
    spec = importlib.util.spec_from_file_location('personal_prepare', ROOT / 'tests/fixtures/personal_load/prepare_fixture.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    result = module.prepare(output, original_art)
    project = output / 'game/project.godot'
    project.write_text(project.read_text().replace('Personal load isolation', 'Personal battle view isolation').replace('config/personal_load_isolated=true', 'config/personal_battle_view_isolated=true'))
    result['scope'] = 'Finite personal battle queries and real input node behavior; no Native storage, GUI, screenshots or full suite'
    return result


def main() -> int:
    """指定新隔离目录及核验过的原tiny图片路径。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--original-art', type=Path, required=True)
    args = parser.parse_args()
    result = prepare(args.output.resolve(), args.original_art.resolve())
    print(json.dumps({key: value for key, value in result.items() if key != 'prepared_source_sha256'}, ensure_ascii=False))
    return 0


if __name__ == '__main__': raise SystemExit(main())
