"""为旅行纯提案等价门复制精确源码及既有动态人物作者；不改原源或冻结runtime。"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def digest(path: Path) -> str:
    """记录输入原字节身份；目录名不能替代源或库的摘要。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(source: Path, output: Path) -> dict:
    """只接受新隔离目录，正式准备原dynamic fixture，表现只复用已有tiny资源。"""
    if output.exists():
        raise ValueError('Use a new isolated output directory')
    game = output / 'game'
    game.mkdir(parents=True)
    shutil.copytree(source / 'game/core', game / 'core', ignore=shutil.ignore_patterns('*.uid'))
    shutil.copytree(source / 'game/data', game / 'data', ignore=shutil.ignore_patterns('generated'))
    generated = game / 'data/generated'
    generated.mkdir()
    shutil.copy2(source / 'schemas/content.schema.json', generated / 'content.schema.json')
    tests = game / 'tests'
    tests.mkdir()
    shutil.copy2(ROOT / 'game/tests/test_native_travel_schedule.gd', tests / 'test_native_travel_schedule.gd')
    shutil.copy2(source / 'game/tests/companion_fixture.gd', tests / 'companion_fixture.gd')
    tiny = source / 'tests/fixtures/content_packs/compose/new_packs/base/assets/art'
    art = game / 'assets/art'
    art.mkdir(parents=True)
    for name in ['pixel.png', 'portrait.png']:
        shutil.copy2(tiny / name, art / name)
    presentation = json.loads((game / 'data/presentation.json').read_text())
    for row in presentation['backgrounds'].values(): row['path'] = 'res://assets/art/pixel.png'
    for row in presentation['portraits'].values(): row['path'] = 'res://assets/art/portrait.png'
    (game / 'data/presentation.json').write_text(json.dumps(presentation, ensure_ascii=False) + '\n')
    (game / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Native travel schedule isolation"\nconfig/native_travel_schedule_isolated=true\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    for name in ['tools', 'schemas']:
        shutil.copytree(source / name, output / name, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    (output / 'content/foundation').mkdir(parents=True)
    shutil.copy2(source / 'content/foundation/alpha_life.json', output / 'content/foundation/alpha_life.json')
    (output / 'tests').mkdir()
    for name in ['dynamic_actor_content_fixture.py', 'military_content_fixture.py']:
        shutil.copy2(source / 'tests' / name, output / 'tests' / name)
    (output / 'tests/__init__.py').write_text('')
    subprocess.run([sys.executable, '-m', 'tests.dynamic_actor_content_fixture', str(output / 'dynamic')], cwd=output, check=True, stdout=subprocess.PIPE, text=True)
    manifest = output / 'dynamic/dynamic-actor-fixtures.json'
    result = {'game': str(game), 'dynamic_content': str(manifest),
              'source': str(source), 'source_sha256': {name: digest(source / name) for name in [
                  'game/core/travel_schedule.gd', 'game/core/backends/legacy_world_backend.gd',
                  'game/core/world_kernel.gd', 'tests/dynamic_actor_content_fixture.py',
                  'tests/military_content_fixture.py', 'game/tests/test_dynamic_actor_schedule.gd']},
              'scope': 'Original dynamic author fixture and formal adoption/work/travel; no Codec/save matrix, no GUI or full-game claim'}
    (output / 'preparation.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    return result


def main() -> None:
    """源码与新输出由调用者显式给出；不运行导入或覆盖候选库。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source.resolve(strict=True), args.output.resolve()), ensure_ascii=False))


if __name__ == '__main__':
    main()
