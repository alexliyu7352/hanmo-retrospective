"""只复用已正式准备的tiny内容，导出指定完整GD源与冻结Bridge到全新live门工程。"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]
LIBRARY_SHA256 = '991c5e09d20b1deb07b15741d839c32259ee888a1af7411d4fe342f9a65546f2'
BASELINE = '91587fed507d7915d35a1e7ac9dc2cfdd5374340'


def sha(path: Path) -> str:
    """工件逐字摘要，不用来源名字冒充身份。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(args: argparse.Namespace) -> dict:
    """源码同一commit导出；作者与锁直接复制，不重组或重写内容版本。"""
    output = args.output.resolve()
    if output.exists():
        raise ValueError('Use a new isolated output directory')
    commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', args.source_commit + '^{commit}'], text=True).strip()
    catalog = args.catalog.resolve()
    index = json.loads(catalog.read_text())
    if index.get('format') != 'HM-CONTENT-INDEX' or args.scenario not in index['catalog']['scenarios']:
        raise ValueError('A formal prepared catalog containing the requested scenario is required')
    if sha(args.library) != LIBRARY_SHA256:
        raise ValueError('Expected frozen f1c907d / 991c Bridge')
    game = output / 'game'
    game.mkdir(parents=True)
    sources = {}
    paths = subprocess.check_output(['git', '-C', str(ROOT), 'ls-tree', '-r', '--name-only', commit, 'game/core', 'game/ui', 'game/storage'], text=True).splitlines()
    for name in paths:
        if not name.endswith('.gd'):
            continue
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(subprocess.check_output(['git', '-C', str(ROOT), 'show', commit + ':' + name]))
        sources[name] = sha(target)
    data = game / 'data'
    data.mkdir()
    for name in ('module_contracts.json', 'rule_capabilities.json', 'pack_runtime.json', 'presentation.schema.json'):
        target = data / name
        target.write_bytes(subprocess.check_output(['git', '-C', str(ROOT), 'show', commit + ':game/data/' + name]))
        sources['game/data/' + name] = sha(target)
    source_game = args.prepared_game.resolve()
    shutil.copy2(source_game / 'data/presentation.json', data / 'presentation.json')
    shutil.copytree(source_game / 'assets', game / 'assets')
    copied = data / 'generated/content-sets' / index['pack_set_digest']
    shutil.copytree(catalog.parent, copied, ignore=shutil.ignore_patterns('.godot'))
    shutil.copy2(source_game / 'data/generated/pack-set.schema.json', data / 'generated/pack-set.schema.json')
    (data / 'generated/content.schema.json').write_bytes(subprocess.check_output(['git', '-C', str(ROOT), 'show', commit + ':schemas/content.schema.json']))
    installed = json.loads((source_game / 'data/generated/installed-content.json').read_text())
    installed['sets'] = [row for row in installed['sets'] if row['pack_set_digest'] == index['pack_set_digest']]
    if len(installed['sets']) != 1:
        raise ValueError('Prepared set must have one genuine installed record')
    (data / 'generated/installed-content.json').write_text(json.dumps(installed, ensure_ascii=False, separators=(',', ':')) + '\n')
    extension = game / 'native_live_growth'
    extension.mkdir()
    shutil.copy2(args.library, extension / 'libhanmo_world_bridge.so')
    (extension / 'hanmo_world.gdextension').write_text('[configuration]\nentry_symbol = "hanmo_world_library_init"\ncompatibility_minimum = "4.7.2"\nreloadable = false\n[libraries]\nlinux.debug.x86_64 = "res://native_live_growth/libhanmo_world_bridge.so"\n')
    (game / 'tests').mkdir()
    shutil.copy2(ROOT / 'game/tests/test_live_native_growth_action.gd', game / 'tests/test_live_native_growth_action.gd')
    relative = 'res://data/generated/content-sets/' + index['pack_set_digest'] + '/' + catalog.name
    fixture = {'source_commit': commit, 'catalog': relative, 'scenario': args.scenario, 'catalog_sha256': sha(catalog), 'pack_set_digest': index['pack_set_digest'], 'library_sha256': LIBRARY_SHA256, 'sources': sources}
    (game / 'data/live-native-growth-fixture.json').write_text(json.dumps(fixture, ensure_ascii=False, indent=2) + '\n')
    settings = ('[development]\nnative_growth_action_enabled=true\nnative_projection_source_commit="' + commit + '"\n') if args.enabled else ''
    (game / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Live native growth isolation"\nconfig/live_native_growth_isolated=true\n[display]\nwindow/size/viewport_width=1280\nwindow/size/viewport_height=720\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n' + settings)
    (output / 'fixture.json').write_text(json.dumps(fixture, ensure_ascii=False, indent=2) + '\n')
    return {'game': str(game), 'source_commit': commit, 'content': index['pack_set_digest'], 'fixture_sha256': sha(output / 'fixture.json')}


def main() -> None:
    """显式路径输入让GUI或专测独立复制；不触碰既有已冻工程。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepared-game', type=Path, required=True)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--scenario', default='base::scenario.alpha_life')
    parser.add_argument('--source-commit', default=BASELINE)
    parser.add_argument('--library', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--enabled', action='store_true')
    print(json.dumps(prepare(parser.parse_args()), ensure_ascii=False))


if __name__ == '__main__':
    main()
