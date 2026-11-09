"""为旧军政提案oracle准备原作者与精确生产源，隔离表现资源且不触碰默认工程。"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]


def digest(path: Path) -> str:
    """保存输入字节身份，源码提交与实际脚本同时可核验。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def copy_script_closure(source: Path, game: Path, names: list[str]) -> None:
    """只复制原测试静态引用闭包；所有生产core已另行原样复制。"""
    pending = list(names)
    seen = set()
    while pending:
        name = pending.pop()
        if name in seen:
            continue
        seen.add(name)
        original = source / 'game' / name
        target = game / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
        for child in re.findall(r'res://(tests/[^"\s]+\.gd)', original.read_text()):
            pending.append(child)


def prepare(source: Path, output: Path) -> dict:
    """新目录内正式编译原dynamic军事变体和lived_city，不新增计划、战果或军队。"""
    if output.exists() or output == source or source in output.parents:
        raise ValueError('Use a new isolated output outside the source worktree')
    game = output / 'game'
    game.mkdir(parents=True)
    shutil.copytree(source / 'game/core', game / 'core', ignore=shutil.ignore_patterns('*.uid'))
    shutil.copytree(source / 'game/data', game / 'data', ignore=shutil.ignore_patterns('generated'))
    generated = game / 'data/generated'
    generated.mkdir()
    shutil.copy2(source / 'schemas/content.schema.json', generated / 'content.schema.json')
    copy_script_closure(source, game, ['tests/test_dynamic_actor_schedule.gd', 'tests/test_campaign_return.gd'])
    shutil.copy2(ROOT / 'game/tests/test_native_military_schedule.gd', game / 'tests/test_native_military_schedule.gd')
    for path in (ROOT / 'tests/fixtures/native_military_schedule').glob('*.gd'):
        shutil.copy2(path, game / 'tests' / path.name)
    art = game / 'assets/art'
    art.mkdir(parents=True)
    for name in ('pixel.png', 'portrait.png'):
        shutil.copy2(source / 'tests/fixtures/content_packs/compose/new_packs/base/assets/art' / name, art / name)
    presentation = json.loads((game / 'data/presentation.json').read_text())
    for row in presentation['backgrounds'].values():
        row['path'] = 'res://assets/art/pixel.png'
    for row in presentation['portraits'].values():
        row['path'] = 'res://assets/art/portrait.png'
    (game / 'data/presentation.json').write_text(json.dumps(presentation, ensure_ascii=False) + '\n')
    (game / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Military schedule oracle isolation"\nconfig/native_military_schedule_isolated=true\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    for name in ('tools', 'schemas'):
        shutil.copytree(source / name, output / name, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    (output / 'content/foundation').mkdir(parents=True)
    for name in ('alpha_life.json', 'lived_city.json'):
        shutil.copy2(source / 'content/foundation' / name, output / 'content/foundation' / name)
    (output / 'tests').mkdir()
    for name in ('dynamic_actor_content_fixture.py', 'military_content_fixture.py'):
        shutil.copy2(source / 'tests' / name, output / 'tests' / name)
    (output / 'tests/__init__.py').write_text('')
    # 仍调用原作者函数和正式编译口；仅省略本门不运行的两套行旅作者生成。
    code = '''import json
from pathlib import Path
from tests.dynamic_actor_content_fixture import dynamic_actor_pack
from tools.content.io import prepare_pack
out=Path("military");out.mkdir()
source=out/"dynamic-actor-military.json"
source.write_text(json.dumps(dynamic_actor_pack("military"),ensure_ascii=False,indent=2)+"\\n")
catalog=out/"dynamic-actor-military.hmcatalog.json"
receipt=prepare_pack(source,catalog)
(out/"fixtures.json").write_text(json.dumps({"military":{"catalog":str(catalog.resolve()),"catalog_sha256":receipt["catalog_sha256"]}},indent=2)+"\\n")
prepared=out/"lived_city.hmcatalog.json"
prepare_pack(Path("content/foundation/lived_city.json"),prepared)
Path("game/data/generated/lived_city.hmcatalog.json").write_bytes(prepared.read_bytes())
'''
    result = subprocess.run([sys.executable, '-c', code], cwd=output, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    result = {'game': str(game), 'dynamic_content': str(output / 'military/fixtures.json'),
              'source': str(source), 'source_commit': subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip(),
              'source_sha256': {name: digest(source / name) for name in (
                  'game/core/military_schedule.gd', 'game/core/backends/legacy_world_backend.gd',
                  'game/tests/test_dynamic_actor_schedule.gd', 'game/tests/test_campaign_return.gd',
                  'tests/dynamic_actor_content_fixture.py', 'tests/military_content_fixture.py')},
              'scope': 'Original military proposals and bounded real-command sources; no Codec matrix, native wire, GUI or full-game claim'}
    (output / 'preparation.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    return result


def main() -> None:
    """显式旧源及新目录；准备不进行Godot导入，也不装载native扩展。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source.resolve(strict=True), args.output.resolve()), ensure_ascii=False))


if __name__ == '__main__':
    main()
