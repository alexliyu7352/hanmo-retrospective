"""正式compose/stage军团行军Kernel短门：同源军事作者、显式训练初值及两张已核验tiny图。"""
from __future__ import annotations
import argparse
from copy import deepcopy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
ART_HASHES = {
    'pixel.png': '1c98316a22280de1e7af3ab645d620ce26caa469799dbb11b13c1e8087392288',
    'portrait.png': 'b71e193aacf275c230b35e2e7e2a3f033822d245f0c32ef116807832953b079a',
}


def write(path: Path, value: object) -> None:
    """只写本门隔离输入或身份回执，不改生产作者、美术和玩家存档。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + '\n')


def digest(path: Path) -> str:
    """按原字节核验输入，不把路径、mtime或部分历史SHA冒充完整发行身份。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def authored_pack(root: Path) -> dict:
    """复用正式两军作者生成器，仅作者层增加短门地图、旁部训练和真实初值。"""
    sys.path.insert(0, str(root))
    spec = importlib.util.spec_from_file_location('march_military_fixture', root / 'tests/military_content_fixture.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    pack = module.military_pack()
    for actor in pack['actors']:
        for field in ['military_plan', 'travel_plan', 'appearance_profile', 'social', 'road_conflict', 'road_dialogue_id']:
            actor.pop(field, None)
        actor['portrait_kind'] = 'merchant'
    pack['events'] = []
    for table in ['places', 'battlefields', 'personal_battlefields']:
        for row in pack.get(table, []): row['visual_tag'] = 'city'
    for row in pack['troop_types']: row['battle_portrait_kind'] = 'merchant'
    for row in pack['battlefields']: row['spawns'].append({'side': 0, 'position': [0, 2]})
    for row in pack['items']:
        if 'battle_portrait_kind' in row: row['battle_portrait_kind'] = 'merchant'
    for index, settlement in enumerate(pack['settlements']): settlement['map_xy'] = [100 + index * 700, 300]
    pack['settlements'].append(dict(id='city.march_waypoint', kind='city', map_xy=[421, 320], entry_place='march.waypoint', name_key='name.military.fixture'))
    pack['places'].append(dict(id='march.waypoint', settlement_id='city.march_waypoint', parent_place=None, visual_tag='city', passages=['march.square'], admission=[], name_key='name.military.fixture'))
    pack['places'].append(dict(id='march.square', settlement_id='city.march_waypoint', parent_place='march.waypoint', visual_tag='city', passages=['march.waypoint'], admission=[], name_key='name.military.fixture'))
    pack['routes'].append(dict(id='road.march_waypoint', origin='city.west', destination='city.march_waypoint', duration_ticks=1, risk_bp=0))
    pack['routes'].append(dict(id='road.march_waypoint_return', origin='city.march_waypoint', destination='city.west', duration_ticks=1, risk_bp=0))
    normal = dict(id='map.march', name_key='name.military.fixture', width=1000, height=600, cell_size=20,
                  cells=[{'terrain': 'plain'} for _ in range(1500)], budgets=dict(foot=1000, army=2000, boat=1000),
                  money_per_tick=3, fatigue_per_tick=1, visual_tag='city')
    crawl = deepcopy(normal); crawl['id'] = 'map.crawl'; crawl['budgets']['army'] = 1
    blocked = deepcopy(normal); blocked['id'] = 'map.blocked'
    for index, cell in enumerate(blocked['cells']):
        if index % 50 == 15: cell['blocked_flag'] = 'march.closed'
    mixed = deepcopy(normal); mixed['id'] = 'map.mixed'; mixed['budgets']['army'] = 964
    for y in range(14, 17):
        for x in range(20, 23): mixed['cells'][y * 50 + x] = {'terrain': 'forest' if x == 20 else 'road'}
    mixed['cells'][14 * 50 + 21] = {'terrain': 'mountain'}
    waypoint = deepcopy(mixed); waypoint['id'] = 'map.mixed_waypoint'; waypoint['budgets']['army'] = 2240
    pack['world_maps'] = [normal, crawl, blocked, mixed, waypoint]; pack['flags']['march.closed'] = False
    pack['scenarios'] = [dict(id='scenario.' + name, name_key='name.military.fixture',
                              player_actor='actor.military.a.commander', seed_text='march-kernel-v1',
                              world_map_id=map_id, **({} if name == 'noopt' else {'military_condition_model': 'terrain_march_v1'}))
                         for name, map_id in [('march', 'map.march'), ('noopt', 'map.march'), ('crawl', 'map.crawl'), ('blocked', 'map.blocked'), ('mixed', 'map.mixed'), ('mixed_waypoint', 'map.mixed_waypoint')]]
    for row in pack['offices']: row['command_scope'] = 'army'
    for row in pack['military_objectives']: row['max_rounds'] = 1
    for row in pack['garrisons']:
        row['army_capacity'] = 10000
        for unit in row['units']: unit.update(initial_training=10, initial_fatigue=7)
    for row in pack['inventories']:
        if row['id'].startswith('inventory.military.') and row['id'] != 'inventory.military.private':
            row['capacity'] = 100000
            for good in row['goods']: good['quantity'] = 10000
    for row in pack['wallets']:
        if row['id'].startswith('wallet.military.'): row['initial_balance'] = 10000
    army_a = next(row for row in pack['garrisons'] if row['id'] == 'garrison.military.a')
    commander = deepcopy(next(row for row in pack['actors'] if row['id'] == 'actor.military.a.commander'))
    commander['id'] = 'actor.military.a.cap'; pack['actors'].append(commander)
    capped = deepcopy(army_a['units'][0]); capped.update(id='unit.military.a.cap', commander_actor_id=commander['id'], initial_soldiers=2, initial_fatigue=99)
    army_a['units'].append(capped)
    for row in pack['military_orders']:
        row['target_stronghold_id'] = 'stronghold.military.b'; row['deadline_ticks'] = 500
        row['objective'] = {'kind': 'occupy' if row['id'] == 'order.military.a' else 'relieve'}
    # 两位独立教官分别使用真实公职/组织/钱包；绝不把随军部队当作可训练闲部。
    for label in ['c', 'd']:
        base = 'military.' + label
        for suffix in ['commander', 'provider']:
            actor = deepcopy(next(row for row in pack['actors'] if row['id'] == 'actor.military.a.' + suffix))
            actor.update(id='actor.' + base + '.' + suffix, initial_place='west.office'); pack['actors'].append(actor)
        pack['organizations'].append(dict(id='org.' + base, name_key='name.military.fixture', kind='political', leader_actor='actor.' + base + '.provider'))
        role = deepcopy(next(row for row in pack['roles'] if row['id'] == 'role.military.a'))
        role.update(id='role.' + base, organization_id='org.' + base, initial_actor='actor.' + base + '.provider', place_id='west.office', services=['office_accept', 'office_resign']); pack['roles'].append(role)
        office = deepcopy(next(row for row in pack['offices'] if row['id'] == 'office.military.a'))
        office.update(id='office.' + base, organization_id='org.' + base, appointing_role_id=role['id'], garrison_id='garrison.' + base); pack['offices'].append(office)
        owner = dict(kind='organization', id='org.' + base)
        for suffix in ['', '.service']: pack['wallets'].append(dict(id='wallet.' + base + suffix, owner=deepcopy(owner), initial_balance=1000))
        pack['inventories'].append(dict(id='inventory.' + base, owner=owner, capacity=100000, goods=[dict(item_id='item.military.a.food', quantity=10000)]))
        camp = deepcopy(army_a); camp.update(id='garrison.' + base, organization_id='org.' + base, place_id='west.office', inventory_id='inventory.' + base, wallet_id='wallet.' + base,
                                         training_wallet_id='wallet.' + base + '.service', training_fatigue_per_tick=4)
        unit = deepcopy(camp['units'][0]); unit.update(id='unit.' + base, commander_actor_id='actor.' + base + '.commander', initial_soldiers=4, initial_fatigue=0)
        camp['units'] = [unit]; pack['garrisons'].append(camp)
    return pack


def refresh(root: Path, output: Path) -> None:
    """只增量替换本门自己的轻量代码与合同输入；不删除既有运行、缓存、日志或任何外部工程。"""
    for name in ['tools', 'schemas', 'game/core']:
        shutil.copytree(root / name, output / name, dirs_exist_ok=True, ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '*.uid'))
    data = output / 'game/data'; data.mkdir(parents=True, exist_ok=True)
    for name in ['rule_capabilities.json', 'module_contracts.json', 'pack_runtime.json', 'presentation.schema.json']:
        shutil.copy2(root / 'game/data' / name, data / name)
    tests = output / 'game/tests'; tests.mkdir(parents=True, exist_ok=True)
    for name in ['test_military_march_kernel.gd']:
        if (root / 'game/tests' / name).exists(): shutil.copy2(root / 'game/tests' / name, tests / name)


def prepare(root: Path, output: Path, art: Path, refresh_only: bool) -> dict:
    """准备唯一新工程或有标记的本门轻量增量；全部目录均经正式compose/stage生成。"""
    marker = output / 'march-fixture.json'
    if refresh_only:
        if not marker.is_file() or json.loads(marker.read_text()).get('kind') != 'military-march-kernel': raise ValueError('Not this isolated fixture')
    else:
        if output.exists(): raise ValueError('Use a fresh output directory')
        output.mkdir(parents=True)
        write(marker, {'kind': 'military-march-kernel'})
    for name, expected in ART_HASHES.items():
        if digest(art / name) != expected: raise ValueError('Tiny art mismatch: ' + name)
    refresh(root, output)
    (output / 'game/project.godot').write_text('config_version=5\n[application]\nconfig/name="Military march seam isolation"\nconfig/military_march_isolated=true\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    fixture = root / 'tests/fixtures/content_packs/compose/new_packs/base'
    author = output / 'author'; author.mkdir(exist_ok=True)
    manifest = json.loads((fixture / 'pack.json').read_text()); source = authored_pack(root)
    manifest['required_capabilities'] = source['required_capabilities']; write(author / 'pack.json', manifest)
    write(author / 'content.json', source)
    original = json.loads((root / 'game/data/presentation.json').read_text())
    presentation = {'version': 1, 'default_background': original['default_background'], 'default_portrait': original['default_portrait'],
                    'backgrounds': {key: {'path': 'res://assets/art/pixel.png'} for key in original['backgrounds']},
                    'portraits': {key: {'path': 'res://assets/art/portrait.png'} for key in original['portraits']}}
    write(author / 'presentation.json', presentation); write(output / 'game/data/presentation.json', presentation)
    for name in ART_HASHES:
        for target in [author / 'assets/art' / name, output / 'game/assets/art' / name]:
            target.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(art / name, target)
    write(output / 'content/foundation/march.json', source)
    write(output / 'selection.json', {'format': 'HM-PACK-SELECTION', 'format_version': 1, 'packs': [{'id': 'base', 'version': '1.0.0'}]})
    # 只在作者或正式编译器变更时重做很小的内容目录，源码同步不会重复复制大型stage。
    compile_identity = hashlib.sha256((digest(author / 'content.json') + ''.join(digest(path) for path in sorted((output / 'tools/content').rglob('*.py'))) + ''.join(digest(path) for path in sorted((output / 'schemas').rglob('*.json'))) + ''.join(digest(path) for path in sorted((output / 'game/data').glob('*.json')) if path.name != 'military-march-fixture.json')).encode()).hexdigest()
    build_root = output / 'prepared' / compile_identity
    if not build_root.exists():
        subprocess.run([sys.executable, str(output / 'tools/content_pipeline.py'), 'compose', '--packs', str(author), '--selection', str(output / 'selection.json'), '--output', str(build_root)], cwd=output, check=True)
    subprocess.run([sys.executable, '-c', 'from tools.build import stage_content; from pathlib import Path; stage_content(Path.cwd(),content_sets=[Path(' + repr(str(build_root)) + ')],foundation_sets=False)'], cwd=output, check=True)
    write(output / 'game/data/military-march-fixture.json', {'pack_set_digest': digest(build_root / 'pack-set.lock.json'), 'directory': 'content-sets/' + digest(build_root / 'pack-set.lock.json')})
    (output / 'game/project.godot').write_text('config_version=5\n[application]\nconfig/name="Military march seam isolation"\nconfig/military_march_isolated=true\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    receipt = {'source_commit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
               'source_identity_kind': 'synthetic local partial recovery, never publication', 'author_sha256': digest(author / 'content.json'),
               'pack_set_digest': digest(build_root / 'pack-set.lock.json'), 'tiny_art_sha256': ART_HASHES,
               'game': str(output / 'game'), 'prepared_sha256': {str(path.relative_to(output)): digest(path) for folder in ['game/core', 'game/tests', 'game/data'] for path in sorted((output / folder).rglob('*')) if path.is_file()},
               'scope': 'Actual Kernel terrain army march seam; explicit tiny test presentation; no GUI, SDK, full suite or A4 acceptance'}
    identity = hashlib.sha256(json.dumps(receipt, sort_keys=True).encode()).hexdigest()
    write(output / 'receipts' / (identity + '.json'), receipt); write(output / 'prepare-receipt.json', receipt)
    return {key: value for key, value in receipt.items() if key != 'prepared_sha256'}


def main() -> None:
    """调用者指定已核tiny资源与隔离目录；Godot必须另用固定4.7.2运行并保存独立日志。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT); parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fixture-art', type=Path, required=True); parser.add_argument('--refresh', action='store_true')
    args = parser.parse_args()
    print(json.dumps(prepare(args.root.resolve(), args.output.absolute(), args.fixture_art.resolve(strict=True), args.refresh), ensure_ascii=False))


if __name__ == '__main__': main()
