"""以既有军事作者与两张已核tiny图准备独立攻城粮源门；不改正式包、美术或冻结march证据。"""
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
ART_HASHES = {'pixel.png': '1c98316a22280de1e7af3ab645d620ce26caa469799dbb11b13c1e8087392288',
              'portrait.png': 'b71e193aacf275c230b35e2e7e2a3f033822d245f0c32ef116807832953b079a'}


def write(path: Path, value: object) -> None:
    """仅保存本门独立作者输入与身份回执；既有工程和玩家文件完全不参与写入。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + '\n')


def digest(path: Path) -> str:
    """按原文件字节冻结资源和测试来源，不把路径或标签当成身份。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def authored_pack(root: Path) -> dict:
    """原军事作者基础上声明同城三军、守军零粮和两种合法军需；不制造运行军团或交付事实。"""
    sys.path.insert(0, str(root))
    spec = importlib.util.spec_from_file_location('siege_supply_military_author', root / 'tests/military_content_fixture.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    pack = module.military_pack()
    for actor in pack['actors']:
        for field in ['military_plan', 'travel_plan', 'appearance_profile', 'social', 'road_conflict', 'road_dialogue_id']: actor.pop(field, None)
        actor['portrait_kind'] = 'merchant'
        if actor['id'].startswith('actor.military.b.'): actor['initial_place'] = 'west.street'
    pack['events'] = []; pack['world_maps'] = []
    for scenario in pack['scenarios']:
        scenario.pop('world_map_id', None); scenario['player_actor'] = 'actor.military.a.commander'
    for table in ['places', 'battlefields', 'personal_battlefields']:
        for row in pack.get(table, []): row['visual_tag'] = 'city'
    for row in pack['troop_types']: row['battle_portrait_kind'] = 'merchant'
    for row in pack['items']:
        if 'battle_portrait_kind' in row: row['battle_portrait_kind'] = 'merchant'
    for row in pack['roles']:
        if row['id'] == 'role.military.b': row['place_id'] = 'west.street'
    for row in pack['offices']: row['command_scope'] = 'army'
    for row in pack['garrisons']:
        row['army_capacity'] = 10000
        if row['id'] == 'garrison.military.b': row['place_id'] = 'west.street'
    for row in pack['strongholds']:
        if row['id'] == 'stronghold.military.b': row['settlement_id'] = 'city.west'
    for row in pack['military_orders']:
        row['target_stronghold_id'] = 'stronghold.military.b'
        row['objective'] = {'kind': 'occupy' if row['id'] == 'order.military.a' else 'relieve'}
    for row in pack['siege_topologies']:
        if row['id'] == 'topology.military.b': row['edges'].append({'from': 'approach', 'to': 'supply'})
    for row in pack['inventories']:
        if row['id'] == 'inventory.military.b': row['goods'] = [item for item in row['goods'] if item['item_id'] != 'item.military.b.food']
    camp_a = next(row for row in pack['garrisons'] if row['id'] == 'garrison.military.a')
    extra_actor = deepcopy(next(row for row in pack['actors'] if row['id'] == 'actor.military.a.commander'))
    extra_actor['id'] = 'actor.military.a.second'; pack['actors'].append(extra_actor)
    extra_unit = deepcopy(camp_a['units'][0]); extra_unit.update(id='unit.military.a.second', commander_actor_id=extra_actor['id'], initial_soldiers=2); camp_a['units'].append(extra_unit)
    for suffix in ['commander', 'provider']:
        actor = deepcopy(next(row for row in pack['actors'] if row['id'] == 'actor.military.a.' + suffix))
        actor['id'] = 'actor.military.c.' + suffix; pack['actors'].append(actor)
    pack['organizations'].append(dict(id='org.military.c', name_key='name.military.fixture', kind='political', leader_actor='actor.military.c.provider'))
    role = deepcopy(next(row for row in pack['roles'] if row['id'] == 'role.military.a'))
    role.update(id='role.military.c', organization_id='org.military.c', initial_actor='actor.military.c.provider'); pack['roles'].append(role)
    office = deepcopy(next(row for row in pack['offices'] if row['id'] == 'office.military.a'))
    office.update(id='office.military.c', organization_id='org.military.c', appointing_role_id=role['id'], garrison_id='garrison.military.c'); pack['offices'].append(office)
    camp = deepcopy(camp_a); camp.update(id='garrison.military.c', organization_id='org.military.c', inventory_id='inventory.military.c', wallet_id='wallet.military.c', food_item_id='item.military.b.food')
    unit = deepcopy(camp['units'][0]); unit.update(id='unit.military.c', commander_actor_id='actor.military.c.commander', initial_soldiers=4); camp['units'] = [unit]; pack['garrisons'].append(camp)
    owner = dict(kind='organization', id='org.military.c')
    pack['inventories'].append(dict(id=camp['inventory_id'], owner=owner, capacity=10000, goods=[dict(item_id=camp['food_item_id'], quantity=1000), dict(item_id='item.spear', quantity=100)]))
    pack['wallets'].append(dict(id=camp['wallet_id'], owner=deepcopy(owner), initial_balance=200))
    for suffix, item in [('spear', 'item.spear'), ('food', camp['food_item_id'])]:
        order = deepcopy(next(row for row in pack['military_orders'] if row['id'] == 'order.military.b'))
        objective = dict(kind='deliver_supply', item_id=item, quantity=1, recipient_organization_id='org.military.b')
        if suffix == 'food': objective['recipient'] = 'army'
        order.update(id='order.military.c.' + suffix, office_id=office['id'], garrison_id=camp['id'], issuer_role_id=role['id'], report_role_id=role['id'], objective=objective); pack['military_orders'].append(order)
    return pack


def prepare(root: Path, output: Path, art: Path, refresh: bool) -> dict:
    """一次新建轻工程，后续只更新本门有标记目录；所有作者输入走真实compose/stage并保留旧证据。"""
    marker = output / 'siege-supply-fixture.json'
    if refresh:
        if not marker.is_file() or json.loads(marker.read_text()).get('kind') != 'siege-supply-sources': raise ValueError('Not this isolated fixture')
    else:
        if output.exists(): raise ValueError('Use a fresh output')
        output.mkdir(parents=True); write(marker, {'kind': 'siege-supply-sources'})
    for name, expected in ART_HASHES.items():
        if digest(art / name) != expected: raise ValueError('Tiny art mismatch: ' + name)
    for name in ['tools', 'schemas', 'game/core']:
        shutil.copytree(root / name, output / name, dirs_exist_ok=True, ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '*.uid'))
    for name in ['rule_capabilities.json', 'module_contracts.json', 'pack_runtime.json', 'presentation.schema.json']:
        target = output / 'game/data' / name; target.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(root / 'game/data' / name, target)
    for name in ['test_siege_supply_sources.gd', 'test_siege_rules.gd', 'campaign_fixture.gd', 'military_fixture.gd']:
        test = output / 'game/tests' / name; test.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(root / 'game/tests' / name, test)
    author = output / 'author'; author.mkdir(exist_ok=True)
    source = authored_pack(root); write(author / 'content.json', source); write(output / 'content/foundation/siege-supply.json', source)
    manifest = json.loads((root / 'tests/fixtures/content_packs/compose/new_packs/base/pack.json').read_text()); manifest['required_capabilities'] = source['required_capabilities']; write(author / 'pack.json', manifest)
    original = json.loads((root / 'game/data/presentation.json').read_text())
    presentation = {'version': 1, 'default_background': original['default_background'], 'default_portrait': original['default_portrait'], 'backgrounds': {key: {'path': 'res://assets/art/pixel.png'} for key in original['backgrounds']}, 'portraits': {key: {'path': 'res://assets/art/portrait.png'} for key in original['portraits']}}
    for target in [author / 'presentation.json', output / 'game/data/presentation.json']: write(target, presentation)
    for name in ART_HASHES:
        for target in [author / 'assets/art' / name, output / 'game/assets/art' / name]: target.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(art / name, target)
    write(output / 'selection.json', {'format': 'HM-PACK-SELECTION', 'format_version': 1, 'packs': [{'id': 'base', 'version': '1.0.0'}]})
    compiler = [*sorted((output / 'tools/content').rglob('*.py')), *sorted((output / 'schemas').rglob('*.json')), output / 'game/data/rule_capabilities.json']
    identity = hashlib.sha256((digest(author / 'content.json') + ''.join(digest(path) for path in compiler)).encode()).hexdigest(); composed = output / 'prepared' / identity
    if not composed.exists(): subprocess.run([sys.executable, str(output / 'tools/content_pipeline.py'), 'compose', '--packs', str(author), '--selection', str(output / 'selection.json'), '--output', str(composed)], cwd=output, check=True)
    subprocess.run([sys.executable, '-c', 'from tools.build import stage_content; from pathlib import Path; stage_content(Path.cwd(),content_sets=[Path(' + repr(str(composed)) + ')],foundation_sets=False)'], cwd=output, check=True)
    pack_digest = digest(composed / 'pack-set.lock.json'); write(output / 'game/data/siege-supply-fixture.json', {'pack_set_digest': pack_digest, 'directory': 'content-sets/' + pack_digest})
    (output / 'game/project.godot').write_text('config_version=5\n[application]\nconfig/name="Siege supply source isolation"\nconfig/siege_supply_isolated=true\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    receipt = {'source_commit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(), 'author_sha256': digest(author / 'content.json'), 'pack_set_digest': pack_digest, 'tiny_art_sha256': ART_HASHES, 'game': str(output / 'game'), 'prepared_sha256': {str(path.relative_to(output)): digest(path) for folder in ['game/core', 'game/tests', 'game/data'] for path in sorted((output / folder).rglob('*')) if path.is_file()}, 'scope': 'Actual Kernel siege food-source, inventory conservation, receipt exactly-once and late candidate rollback; no art/GUI/full storage/migration acceptance'}
    write(output / 'prepare-receipt.json', receipt); write(output / 'receipts' / (hashlib.sha256(json.dumps(receipt, sort_keys=True).encode()).hexdigest() + '.json'), receipt)
    return {key: value for key, value in receipt.items() if key != 'prepared_sha256'}



def prepare_occupation_variant(root: Path, output: Path, art: Path, variant: str = 'occupation') -> dict:
    """在已有tiny工程追加独立开局作者锁；保留旧作者、pin、源码及回执，不重复复制工程。"""
    marker = output / 'siege-supply-fixture.json'
    if not marker.is_file() or json.loads(marker.read_text()).get('kind') != 'siege-supply-sources':
        raise ValueError('Prepare the original siege supply fixture first')
    original = output / 'author'
    pack = json.loads((original / 'content.json').read_text())
    if variant not in ['occupation', 'join-player', 'join-player-foreign']: raise ValueError('Unknown siege variant')
    if variant != 'join-player-foreign':
        pack['organizations'] = [row for row in pack['organizations'] if row['id'] != 'org.military.c']
        pack = json.loads(json.dumps(pack).replace('"org.military.c"', '"org.military.a"'))
        # 原统帅钱包由Factory自动建零余额；显式作者钱包承载经批准的8钱，不写运行state。
        owner = {'kind': 'actor', 'id': 'actor.military.a.commander'}
        wallets = [row for row in pack['wallets'] if row['owner'] == owner]
        if wallets:
            if len(wallets) != 1: raise ValueError('Ambiguous commander wallet')
            wallets[0]['initial_balance'] = 8
        else:
            pack['wallets'].append({'id': 'wallet.military.a.occupation', 'owner': owner, 'initial_balance': 8})
    if variant.startswith('join-player'):
        for scenario in pack['scenarios']: scenario['player_actor'] = 'actor.military.c.commander'
    label = {'occupation': 'siege-occupation', 'join-player': 'siege-join-entry', 'join-player-foreign': 'siege-join-entry-foreign'}[variant]
    author = output / ('author-' + variant)
    write(author / 'content.json', pack)
    for name in ['pack.json', 'presentation.json']:
        shutil.copy2(original / name, author / name)
    for name, expected in ART_HASHES.items():
        if digest(art / name) != expected: raise ValueError('Tiny art mismatch: ' + name)
        target = author / 'assets/art' / name; target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(art / name, target)
    compiler = [*sorted((output / 'tools/content').rglob('*.py')), *sorted((output / 'schemas').rglob('*.json')), output / 'game/data/rule_capabilities.json']
    identity = hashlib.sha256((digest(author / 'content.json') + ''.join(digest(path) for path in compiler)).encode()).hexdigest()
    composed = output / 'prepared' / identity
    if not composed.exists():
        subprocess.run([sys.executable, str(output / 'tools/content_pipeline.py'), 'compose', '--packs', str(author), '--selection', str(output / 'selection.json'), '--output', str(composed)], cwd=output, check=True)
    retained = sorted(str(path.parent) for path in (output / 'prepared').glob('*/pack-set.lock.json'))
    subprocess.run([sys.executable, '-c', 'from tools.build import stage_content; from pathlib import Path; stage_content(Path.cwd(),content_sets=[Path(p) for p in ' + repr(retained) + '],foundation_sets=False)'], cwd=output, check=True)
    pack_digest = digest(composed / 'pack-set.lock.json')
    write(output / ('game/data/' + label + '-fixture.json'), {'variant': variant, 'pack_set_digest': pack_digest, 'directory': 'content-sets/' + pack_digest})
    shutil.copy2(root / 'game/tests/test_siege_start_occupation.gd', output / 'game/tests/test_siege_start_occupation.gd')
    if variant.startswith('join-player'):
        shutil.copytree(root / 'game/ui', output / 'game/ui', dirs_exist_ok=True, ignore=shutil.ignore_patterns('*.uid'))
        shutil.copy2(root / 'game/tests/test_siege_join_entry.gd', output / 'game/tests/test_siege_join_entry.gd')
    receipt = {'variant': variant, 'author_sha256': digest(author / 'content.json'), 'pack_set_digest': pack_digest, 'base_author_sha256': digest(original / 'content.json'), 'compiler_sha256': {str(path.relative_to(output)): digest(path) for path in compiler}, 'scope': ('Original supply author changes only player to C; foreign organization C preserved' if variant == 'join-player-foreign' else 'C joins authored organization A; A personal wallet 8; ' + ('actual Kernel occupation gate' if variant == 'occupation' else 'C is authored player; real navigation Button to Kernel join gate'))}
    write(output / (variant + '-prepare-receipt.json'), receipt)
    write(output / 'receipts' / (hashlib.sha256(json.dumps(receipt, sort_keys=True).encode()).hexdigest() + '.json'), receipt)
    return {key: value for key, value in receipt.items() if key != 'compiler_sha256'}

def main() -> None:
    """调用者提供新输出或明确refresh及已核tiny图；固定Godot独立运行，不自动发布或改全局环境。"""
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument('--root', type=Path, default=ROOT); parser.add_argument('--output', type=Path, required=True); parser.add_argument('--fixture-art', type=Path, required=True); parser.add_argument('--refresh', action='store_true'); parser.add_argument('--variant', choices=['supply', 'occupation', 'join-player', 'join-player-foreign'], default='supply')
    args = parser.parse_args()
    if args.variant != 'supply':
        if not args.refresh: parser.error('occupation appends to an existing isolated project; use --refresh')
        result = prepare_occupation_variant(args.root.resolve(), args.output.absolute(), args.fixture_art.resolve(strict=True), args.variant)
    else:
        result = prepare(args.root.resolve(), args.output.absolute(), args.fixture_art.resolve(strict=True), args.refresh)
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__': main()
