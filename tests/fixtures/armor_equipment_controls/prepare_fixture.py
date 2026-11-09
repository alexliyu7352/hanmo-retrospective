"""以原护具作者夹具准备真实成长页；只复制界面依赖，不改规则或全量资源。"""
from __future__ import annotations
import argparse
import importlib.util
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[3]


def prepare(root: Path, destination: Path) -> dict:
    """正式compose/stage提供同一护具和公共库存，额外仅加入当前UI和专用入口。"""
    spec = importlib.util.spec_from_file_location('armor_source', root / 'tests/fixtures/skill_study_kernel/prepare_fixture.py')
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    result = helper.prepare(root, root / 'tests/fixtures/content_packs/compose/new_packs/base', destination, armor=True)
    for name in ('game/ui', 'game/storage'):
        shutil.copytree(root / name, destination / name)
    for name in ('test_armor_equipment_controls.gd', 'test_character_summary.gd'):
        shutil.copyfile(root / 'game/tests' / name, destination / 'game/tests' / name)
    target = destination / 'tests/fixtures/character_presentation'
    target.mkdir(parents=True)
    shutil.copyfile(root / 'tests/fixtures/character_presentation/manifest.json', target / 'manifest.json')
    (destination / 'game/project.godot').write_text('config_version=5\n[display]\nwindow/size/viewport_width=1280\nwindow/size/viewport_height=800\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    return result


def main() -> None:
    """只接受新目录，原测试目录与用户存档均不覆盖。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.root.resolve(), args.destination.resolve()), ensure_ascii=False))


if __name__ == '__main__':
    main()
