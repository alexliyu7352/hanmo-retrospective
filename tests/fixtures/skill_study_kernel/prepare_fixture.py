"""以两张既有tiny图和正式compose/stage准备普通课程Kernel门，不复制完整资源。"""
import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def prepare(root: Path, fixture: Path, destination: Path, etiquette: bool = False, bargain: bool = False, trade: bool = False, armor: bool = False, military: bool = False, drill: bool = False, personal: bool = False) -> dict:
    """所有生产输入逐字复制到新测试根，仅变更明确课程作者夹具和最小测试工程配置。"""
    military = military or drill
    destination.mkdir(parents=True, exist_ok=False)
    for name in ('tools', 'schemas', 'game/core'):
        shutil.copytree(root / name, destination / name, ignore=shutil.ignore_patterns('__pycache__'))
    data = destination / 'game/data'; data.mkdir(parents=True)
    for name in ('rule_capabilities.json', 'module_contracts.json', 'pack_runtime.json', 'presentation.schema.json'):
        shutil.copyfile(root / 'game/data' / name, data / name)
    shutil.copyfile(fixture / 'presentation.json', data / 'presentation.json')
    shutil.copytree(fixture / 'assets', destination / 'game/assets')
    author = destination / 'author'; shutil.copytree(fixture, author)
    source = json.loads((author / 'content.json').read_text())
    source['required_capabilities'].append('cultivation')
    next(row for row in source['actors'] if row['id'] == 'actor.player')['initial_place'] = 'west.office'
    role = next(row for row in source['roles'] if row['id'] == 'role.office')
    role['services'].append('study_skill')
    role['skill_courses'] = [{'skill_id': 'rites', 'mode': 'facility', 'wallet_id': 'wallet.office'}]
    if etiquette:
        question = json.loads((root / 'tests/fixtures/etiquette_challenge/definition.json').read_text())
        source['etiquette_challenges'] = [question]
        source['texts'].update(json.loads((root / 'tests/fixtures/etiquette_challenge/texts.json').read_text()))
        role['skill_courses'][0]['etiquette_challenge_id'] = question['id']
    if bargain:
        question = json.loads((root / 'tests/fixtures/bargain_challenge/definition.json').read_text())
        source['bargain_challenges'] = [question]
        source['texts'].update(json.loads((root / 'tests/fixtures/bargain_challenge/texts.json').read_text()))
        role['skill_courses'] = [{'skill_id': 'commerce', 'mode': 'facility', 'wallet_id': 'wallet.office', 'challenge': {'kind': 'bargain', 'id': question['id'], 'context': {'budget': 800, 'quantity': 10, 'relation_discount_bp': 0}}}]
    if trade:
        question = json.loads((root / 'tests/fixtures/bargain_challenge/definition.json').read_text())
        source['bargain_challenges'] = [question]
        source['texts'].update(json.loads((root / 'tests/fixtures/bargain_challenge/texts.json').read_text()))
        shop = source['shops'][0]
        shop['bargaining'] = {'challenge_id': question['id'], 'condition_sources': [{'condition_id': row['id'], 'source': 'stock_covers_quantity' if row['kind'] == 'supply' else 'cash_payment'} for row in question['conditions']]}
        next(row for row in source['actors'] if row['id'] == 'actor.player')['initial_place'] = shop['place_id']
    if armor:
        source['required_capabilities'].append('personal_combat')
        for item_id in ['item.coat', 'item.public_coat']:
            source['items'].append({'id': item_id, 'kind': 'commodity', 'base_price': 10, 'weight': 1, 'name_key': 'name.item.grain', 'armor': {'slot': 'body', 'defense': 6}})
        player = next(row for row in source['actors'] if row['id'] == 'actor.player')
        player['initial_place'] = source['shops'][0]['place_id']
        player['initial_goods'] = [{'item_id': 'item.coat', 'quantity': 1}]
        player['combat'] = {'weapon_id': '', 'learned': [], 'armor_id': 'item.coat'}
        next(row for row in source['inventories'] if row['id'] == source['shops'][0]['inventory_id'])['goods'].append({'item_id': 'item.public_coat', 'quantity': 2})
    if personal:
        source['required_capabilities'].append('personal_combat')
        source['items'].append({'id': 'item.spear', 'kind': 'commodity', 'name_key': 'name.item.grain', 'base_price': 20, 'weight': 2, 'weapon': {'category': 'spear', 'minimum_range': 40, 'maximum_range': 180, 'arc_degrees': 100, 'damage': 12, 'stamina_cost': 8, 'recovery_rounds': 0, 'move_distance': 90}})
        source['personal_skills'] = []
        for identifier, kind, extra in [('skill.area', 'strike', {'requires_stationary': True, 'range_modifier': {'maximum_bonus': 20}, 'area': {'shape': 'sector', 'arc_degrees': 90, 'max_targets': 3}}), ('skill.guard', 'guard', {'guard_facing': 'front'}), ('skill.heart', 'heart', {'defend_recovery_bonus': 5})]:
            source['personal_skills'].append(dict(id=identifier, name_key='name.item.grain', description_key='name.item.grain', kind=kind, weapons=['spear'], stamina_cost=8, power_bp=10000, defense_bp=5000, move_bonus=0, status='none', status_rounds=0, practice_ticks=2, **extra))
        source['personal_battlefields'] = [{'id': 'field.study', 'name_key': 'name.item.grain', 'width': 600, 'height': 400, 'visual_tag': 'city', 'obstacles': [], 'spawns': [{'side': 0, 'position': [200, 200], 'heading': 0}, {'side': 1, 'position': [310, 200], 'heading': 180}], 'exits': [{'side': 0, 'position': [40, 200], 'radius': 30}, {'side': 1, 'position': [560, 200], 'radius': 30}]}]
        source['personal_objectives'] = [{'id': 'objective.study', 'name_key': 'name.item.grain', 'kind': 'sparring', 'max_rounds': 4, 'injury_on_defeat': 0, 'allow_withdrawal': True, 'hit_model': 'skill_v1', 'stamina_model': 'round_v1', 'ai_model': 'scored_v1', 'initial_stamina': 70}]
        for person in source['actors']:
            if person['id'] not in ['actor.player', 'actor.chen']: continue
            person['initial_place'] = 'west.office'; person['skills']['martial'] = 650
            person['initial_goods'] = [{'item_id': 'item.spear', 'quantity': 1}]
            person['combat'] = {'weapon_id': 'item.spear', 'learned': [{'skill_id': row['id'], 'mastery': 80} for row in source['personal_skills']]}
        role['services'].append('duel_start')
        role['sparring'] = {'battlefield_id': 'field.study', 'objective_id': 'objective.study', 'fee': 0, 'wallet_id': 'wallet.office'}
    if military:
        source['required_capabilities'].append('military')
        role['services'].extend(['office_accept', 'office_resign'])
        owner = {'kind': 'organization', 'id': 'org.office'}
        source['wallets'].append({'id': 'wallet.camp', 'owner': owner.copy(), 'initial_balance': 500})
        source['inventories'].append({'id': 'inventory.camp', 'owner': owner.copy(), 'capacity': 1000, 'goods': [{'item_id': 'item.grain', 'quantity': 100}]})
        source['troop_types'] = [{'id': 'troop.study', 'name_key': 'name.item.grain', 'category': 'infantry', 'movement': 2, 'minimum_range': 1, 'maximum_range': 1, 'attack': 35, 'defense': 10, 'accuracy': 75, 'tactic_capacity': 8, 'movement_costs': {'plain': 1, 'forest': 2, 'hill': 2, 'water': 0, 'wall': 0}, 'tactics': [], 'battle_portrait_kind': 'merchant'}]
        source['garrisons'] = [{'id': 'garrison.study', 'name_key': 'name.item.grain', 'place_id': 'west.office', 'organization_id': 'org.office', 'inventory_id': 'inventory.camp', 'wallet_id': 'wallet.camp', 'food_item_id': 'item.grain', 'ration_per_person': 1, 'army_capacity': 20, 'training_wallet_id': 'wallet.office', 'training_fatigue_per_tick': 4, 'units': [{'id': 'unit.study', 'troop_type_id': 'troop.study', 'commander_actor_id': 'actor.player', 'initial_soldiers': 6, 'equipment': [{'item_id': 'item.grain', 'quantity_per_person': 1}], 'initial_training': 10, 'initial_fatigue': 0}]}]
        source['offices'] = [{'id': 'office.study', 'name_key': 'name.item.grain', 'organization_id': 'org.office', 'appointing_role_id': 'role.office', 'garrison_id': 'garrison.study', 'command_scope': 'army'}]
    if drill:
        question = json.loads((root / 'tests/fixtures/drill_challenge/definition.json').read_text())
        source['drill_challenges'] = [question]
        source['texts'].update(json.loads((root / 'tests/fixtures/drill_challenge/texts.json').read_text()))
        source['garrisons'][0]['training_drill_id'] = question['id']
    raw = (json.dumps(source, ensure_ascii=False, indent=2) + '\n').encode()
    (author / 'content.json').write_bytes(raw)
    manifest = json.loads((author / 'pack.json').read_text()); manifest['required_capabilities'].append('cultivation')
    if armor or personal: manifest['required_capabilities'].append('personal_combat')
    if military: manifest['required_capabilities'].append('military')
    (author / 'pack.json').write_text(json.dumps(manifest, ensure_ascii=False) + '\n')
    flat = destination / 'content/foundation'; flat.mkdir(parents=True)
    (flat / 'procurement.json').write_bytes(raw)
    selection = destination / 'selection.json'
    selection.write_text(json.dumps({'format': 'HM-PACK-SELECTION', 'format_version': 1, 'packs': [{'id': 'base', 'version': '1.0.0'}]}))
    composed = destination / 'composed'
    subprocess.run([sys.executable, str(destination / 'tools/content_pipeline.py'), 'compose', '--packs', str(author), '--selection', str(selection), '--output', str(composed)], cwd=destination, check=True)
    subprocess.run([sys.executable, '-c', 'from tools.build import stage_content; from pathlib import Path; stage_content(Path.cwd(),content_sets=[Path("composed")],foundation_sets=False)'], cwd=destination, check=True)
    game = destination / 'game'
    (game / 'project.godot').write_text('config_version=5\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    tests = game / 'tests'; tests.mkdir()
    runner = 'test_personal_models_kernel.gd' if personal else 'test_military_drill_kernel.gd' if drill else 'test_military_training_kernel.gd' if military else 'test_armor_kernel.gd' if armor else 'test_trade_bargain_kernel.gd' if trade else 'test_skill_study_kernel.gd'
    shutil.copyfile(root / 'game/tests' / runner, tests / runner)
    lock = (composed / 'pack-set.lock.json').read_bytes()
    import hashlib
    identity = hashlib.sha256(lock).hexdigest()
    return {'game': str(game), 'catalog': 'res://data/generated/content-sets/' + identity + '/catalog.hmcatalog.json', 'pack_set_digest': identity, 'runtime_verified': False}


def main() -> None:
    """只准备指定新目录并打印明确路径；引擎门由外层单独执行。"""
    parser = argparse.ArgumentParser(); parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--fixture', type=Path, required=True); parser.add_argument('--destination', type=Path)
    choice = parser.add_mutually_exclusive_group()
    choice.add_argument('--etiquette', action='store_true')
    choice.add_argument('--bargain', action='store_true')
    choice.add_argument('--trade', action='store_true')
    choice.add_argument('--armor', action='store_true')
    choice.add_argument('--military', action='store_true')
    choice.add_argument('--drill', action='store_true')
    choice.add_argument('--personal', action='store_true')
    args = parser.parse_args()
    destination = args.destination or Path(tempfile.mkdtemp(prefix='hanmo-study-kernel-parent-')) / 'project'
    print(json.dumps(prepare(args.root.resolve(), args.fixture.resolve(), destination, args.etiquette, args.bargain, args.trade, args.armor, args.military, args.drill, args.personal), ensure_ascii=False))


if __name__ == '__main__':
    main()
