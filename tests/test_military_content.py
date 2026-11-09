"""军事作者包沿既有准备、Schema、引用、撤销和 Godot 纯规则走同一验证入口。"""
from copy import deepcopy
import json
from pathlib import Path
import subprocess

import pytest

from tools import build
from tests.military_content_fixture import military_pack
from tools.content import validation
from tools.content.document import AuthorDocument
from tools.content.io import prepare_pack
from tools.content.military_links import TABLES, check_military
from tools.content.references import iter_references

ROOT = Path(__file__).resolve().parents[1]


def write_pack(tmp_path, pack):
    """保存独立作者草稿，真实 prepare 会重新读取相同字节并生成目录摘要。"""
    source = tmp_path / 'military.json'
    source.write_text(json.dumps(pack, ensure_ascii=False), encoding='utf-8')
    return source


def index_of(pack):
    """构造只读索引供直接语义测试，使用 Schema 已声明的全部可选表。"""
    return {**{table: {row['id']: row for row in pack.get(table, [])} for table in validation.TABLES}, 'texts': pack['texts'], 'flags': pack['flags'], 'required_capabilities': pack['required_capabilities']}


def test_real_prepare_two_configurations_and_legacy(tmp_path):
    """两军定义由原 Alpha 管线准备成功；旧作者包的缺失军事表和目录摘要不被空表污染。"""
    pack = military_pack(); before = deepcopy(pack)
    source = write_pack(tmp_path, pack); output = tmp_path / 'military.hmcatalog.json'
    receipt = prepare_pack(source, output)
    result = json.loads(output.read_text())
    assert receipt['catalog_sha256'] == result['catalog_sha256']
    assert result['catalog']['battlefields']['field.military.a']['objective_id'] == 'goal.military.a'
    assert result['catalog']['garrisons']['garrison.military.b']['ration_per_person'] == 2
    assert result['catalog']['offices']['office.military.b']['command_scope'] == 'army'
    assert pack == before
    legacy = ROOT / 'content/foundation/lived_city.json'; raw = legacy.read_bytes()
    prepare_pack(legacy, output)
    assert not set(TABLES) & json.loads(output.read_text())['catalog'].keys()
    assert legacy.read_bytes() == raw


CASES = [
    (('garrisons', 0, 'inventory_id'), 'inventory.military.private', 'MILITARY_PUBLIC_ASSET'),
    (('garrisons', 1, 'units', 0, 'id'), 'unit.military.a', 'MILITARY_UNIT_DUPLICATE'),
    (('garrisons', 1, 'units', 0, 'commander_actor_id'), 'actor.military.a.commander', 'MILITARY_COMMANDER'),
    (('garrisons', 0, 'food_item_id'), 'message.family_letter', 'REFERENCE'),
    (('garrisons', 0, 'food_item_id'), 'item.family_letter', 'MILITARY_COMMODITY'),
    (('garrisons', 0, 'units', 0, 'equipment', 0, 'item_id'), 'item.family_letter', 'MILITARY_EQUIPMENT'),
    (('siege_topologies', 0, 'rules', 'engine_item_id'), 'item.family_letter', 'MILITARY_COMMODITY'),
    (('siege_topologies', 0, 'rules', 'repair_materials', 0, 'item_id'), 'item.family_letter', 'MILITARY_MATERIAL'),
    (('garrisons', 0, 'units', 0, 'equipment', 0, 'item_id'), 'item.no_such_weapon', 'REFERENCE'),
    (('offices', 0, 'command_scope'), 'all_world', 'SCHEMA'),
    (('offices', 0, 'garrison_id'), 'garrison.military.b', 'MILITARY_OFFICE_ORGANIZATION'),
    (('military_orders', 0, 'issuer_role_id'), 'role.military.b', 'MILITARY_ROLE_AUTHORITY'),
    (('military_orders', 0, 'garrison_id'), 'garrison.military.b', 'MILITARY_ORDER_GARRISON'),
    (('military_orders', 0, 'target_stronghold_id'), 'stronghold.military.b', 'MILITARY_SUPPLY_AUTHORITY'),
    (('strongholds', 0, 'settlement_id'), 'village.east', 'MILITARY_STRONGHOLD_GARRISON'),
    (('strongholds', 0, 'public_wallet_ids'), ['wallet.military.b'], 'MILITARY_PUBLIC_ASSET'),
    (('siege_topologies', 0, 'edges', 0, 'to'), 'missing_node', 'MILITARY_NODE_REFERENCE'),
    (('siege_topologies', 0, 'edges', 1, 'blocker_id'), 'wall', 'MILITARY_BLOCKER'),
    (('siege_topologies', 0, 'nodes', 1, 'max_hp'), 0, 'MILITARY_STRUCTURE_HP'),
    (('siege_topologies', 0, 'supply_node_ids'), ['inner'], 'MILITARY_SUPPLY_PORT'),
    (('siege_topologies', 0, 'nodes', 0, 'battlefield_id'), 'field.absent', 'REFERENCE'),
    (('battlefields', 0, 'cells', 1), {'q': 0, 'r': 0, 'terrain': 'plain'}, 'MILITARY_HEX_DUPLICATE'),
    (('battlefields', 0, 'spawns', 1, 'position'), [99, 99], 'MILITARY_HEX_ENTRY'),
    (('battlefields', 0, 'spawns', 1, 'side'), 0, 'MILITARY_HEX_SIDES'),
    (('battlefields', 0, 'objective_id'), 'goal.missing', 'REFERENCE'),
    (('troop_types', 0, 'minimum_range'), 2, 'MILITARY_TROOP_RANGE'),
    (('troop_types', 0, 'tactics', 0, 'kind'), 'volley', 'MILITARY_TACTIC_TROOP'),
    (('troop_types', 0, 'tactics', 0, 'point_cost'), 9, 'MILITARY_TACTIC_COST'),
    (('troop_types', 0, 'tactics', 0, 'move_bonus'), 1, 'MILITARY_TACTIC_EFFECT'),
    (('troop_types', 0, 'movement_costs', 'water'), -1, 'SCHEMA'),
    (('garrisons', 0, 'units', 0, 'initial_soldiers'), True, 'SCHEMA'),
]


def change(pack, path, value):
    """只对当前用例副本修改一个明确作者字段，不能绕过真实准备器的拒绝入口。"""
    owner = pack
    for key in path[:-1]: owner = owner[key]
    owner[path[-1]] = deepcopy(value)


@pytest.mark.parametrize('path,value,code', CASES, ids=lambda value: str(value))
def test_invalid_definitions_have_author_location(tmp_path, path, value, code):
    """坏引用、兵装来源和城防语义由真实 prepare 拒绝，诊断保留可导航作者路径。"""
    pack = military_pack(); change(pack, path, value); before = deepcopy(pack)
    source = write_pack(tmp_path, pack)
    report = validation.validate_pack(pack, str(source))
    assert not report.ok
    matching = [issue for issue in report.issues if issue.code == code]
    assert matching, report.to_dict()
    assert all(issue.path.startswith(str(source) + '#/') and (issue.record_id or code == 'SCHEMA') for issue in matching)
    with pytest.raises(ValueError, match='Invalid content'):
        prepare_pack(source, tmp_path / 'bad.hmcatalog.json')
    assert pack == before


def test_local_topology_and_explicit_foreign_supply():
    """局部节点可跨两套城防同名，独立边断开仍拒绝；外组织供给只有模板明示才可通过。"""
    pack = military_pack()
    pack['military_orders'][0]['target_stronghold_id'] = 'stronghold.military.b'
    pack['military_orders'][0]['objective']['recipient_organization_id'] = 'org.military.b'
    assert validation.validate_pack(pack).ok
    pack['siege_topologies'][0]['edges'] = pack['siege_topologies'][0]['edges'][:3]
    assert any(i.code == 'MILITARY_TOPOLOGY_DISCONNECTED' for i in validation.validate_pack(pack).issues)


def test_known_foreign_container_cannot_be_relabelled():
    """公库所属组织相同也不能把已在外城的商店钱柜放入本城清单。"""
    pack = military_pack(); wallet = pack['garrisons'][0]['wallet_id']
    shop = deepcopy(pack['shops'][0]); shop.update(id='shop.military.other', wallet_id=wallet, inventory_id=pack['garrisons'][0]['inventory_id'], place_id='east.entrance', representative_roles=[])
    pack['shops'].append(shop)
    issues = check_military(pack, index_of(pack))
    assert any(code == 'MILITARY_ASSET_PLACE' for code, _, _ in issues)


def test_private_container_separation_survives_wrong_owner_declaration():
    """即使草稿同时改了容器 owner，也不能将已列私庄的同一库存兼作驻军来源。"""
    pack = military_pack(); camp = pack['garrisons'][0]
    private = next(row for row in pack['inventories'] if row['id'] == 'inventory.military.private')
    private['owner'] = dict(kind='organization', id=camp['organization_id'])
    camp['inventory_id'] = private['id']
    assert any(code == 'MILITARY_PUBLIC_ASSET' for code, _, _ in check_military(pack, index_of(pack)))


def test_missing_capability_and_orphan_service_are_diagnosed():
    """军事服务不可无来源出现，实际机制缺少能力声明也会阻断准备。"""
    pack = military_pack(); pack['required_capabilities'].remove('military')
    assert any(i.code == 'MILITARY_CAPABILITY' for i in validation.validate_pack(pack).issues)
    pack = military_pack(); pack['military_orders'] = []
    for actor in pack['actors']: actor.pop('military_plan', None)
    assert any(i.code == 'MILITARY_SERVICE_SOURCE' for i in validation.validate_pack(pack).issues)


def test_unfunded_garrison_remains_real_and_reports_later():
    """军营可以真实缺粮少钱，作者校验不凭空补足初始军需，也不误当作语义损坏。"""
    pack = military_pack()
    for camp in pack['garrisons']:
        next(row for row in pack['inventories'] if row['id'] == camp['inventory_id'])['goods'] = []
        next(row for row in pack['wallets'] if row['id'] == camp['wallet_id'])['initial_balance'] = 0
    assert validation.validate_pack(pack).ok


def test_schema_metadata_and_existing_author_history(tmp_path):
    """新表引用、局部节点注解和表现标记沿原 Schema；编辑仍保留原文档撤销与保存。"""
    pack = military_pack(); source = write_pack(tmp_path, pack); doc = AuthorDocument.open(source)
    before = doc.snapshot()
    doc.set_value('garrisons', 'garrison.military.a', '/units/0/initial_soldiers', 9)
    assert doc.record('garrisons', 'garrison.military.a')['units'][0]['initial_soldiers'] == 9
    doc.undo(); assert doc.snapshot() == before
    doc.redo(); doc.save(); assert AuthorDocument.open(source).snapshot() == doc.snapshot()
    refs = list(iter_references(pack))
    assert (('battlefields', 0, 'objective_id'), 'military_objectives', 'goal.military.a') in refs
    assert (('siege_topologies', 0, 'nodes', 0, 'battlefield_id'), 'battlefields', 'field.military.a') in refs
    assert not any(path[-1] == 'blocker_id' for path, _, _ in refs)
    defs = validation.SCHEMA['$defs']
    assert defs['military_battlefield']['properties']['visual_tag']['x-hanmo-presentation'] == 'backgrounds'
    assert defs['troop_type']['properties']['battle_portrait_kind']['x-hanmo-presentation'] == 'portraits'
    assert defs['siege_topology']['properties']['attacker_entry_id']['x-hanmo-local-ref'] == 'nodes'
    assert defs['actor']['properties']['military_plan']['properties']['unit_ids']['items']['x-hanmo-local-ref'] == 'garrisons[*].units'
    assert 'initial_age_years' in defs['actor']['properties']
    assert 'family' in defs['role']['properties'] and 'family_adopt' in defs['role']['properties']['services']['items']['enum']
    assert not set(TABLES) & set(validation.SCHEMA['required'])


def test_default_objectives_all_supported_kinds():
    """默认军事目标逐项采用已实现条件，护送单位留给真实运行名单，不能写进作者模板。"""
    pack = military_pack()
    for kind in ['repel', 'hold', 'breakthrough', 'escort', 'cover_retreat']:
        goal = dict(id='goal.military.a', name_key='name.military.fixture', kind=kind, side=0, max_rounds=8, allow_withdrawal=True)
        if kind == 'hold': goal.update(point=[1, 1], hold_rounds=2)
        if kind in ('breakthrough', 'cover_retreat'): goal['required_units'] = 1
        pack['military_objectives'][0] = goal
        assert validation.validate_pack(pack).ok, kind
    pack['military_objectives'][0]['allow_withdrawal'] = False
    assert any(i.code == 'MILITARY_OBJECTIVE_EXIT' for i in validation.validate_pack(pack).issues)


def plan_cases(pack):
    """根据临时新增人物真实索引生成军政计划坏字段，不依赖 Alpha 既有人物数量。"""
    number = next(i for i, actor in enumerate(pack['actors']) if actor['id'] == 'actor.military.a.commander')
    prefix = ('actors', number, 'military_plan')
    return [
        (prefix + ('order_id',), 'order.absent', 'REFERENCE'),
        (prefix + ('order_id',), 'order.military.b', 'MILITARY_PLAN_ORDER'),
        (prefix + ('unit_ids',), ['unit.military.b'], 'MILITARY_PLAN_UNIT'),
        (prefix + ('unit_ids',), ['unit.absent'], 'MILITARY_PLAN_UNIT'),
        (prefix + ('unit_ids',), ['unit.military.a', 'unit.military.b'], 'MILITARY_PLAN_SCOPE'),
        (prefix + ('money',), -1, 'SCHEMA'),
        (('scenarios', 0, 'player_actor'), 'actor.military.a.commander', 'MILITARY_PLAN_ACTOR'),
    ]


def test_military_plan_uses_real_actor_units_and_permissions(tmp_path):
    """自动军政计划受真人、实际部队、公职与模板约束，未知引用和双重权限保留字段定位。"""
    pack = military_pack()
    for path, value, code in plan_cases(pack):
        changed = deepcopy(pack); change(changed, path, value)
        issues = validation.validate_pack(changed).issues
        assert any(issue.code == code for issue in issues), (path, issues)
    actor = next(row for row in pack['actors'] if row['id'] == 'actor.military.a.commander')
    actor['military_plan']['unit_ids'] = ['unit.military.b']
    assert any(code == 'MILITARY_PLAN_SCOPE' for code, _, _ in check_military(pack, index_of(pack)))
    pack = military_pack(); actor = next(row for row in pack['actors'] if row['id'] == 'actor.military.a.commander')
    actor['travel_plan'] = deepcopy(next(row['travel_plan'] for row in pack['actors'] if 'travel_plan' in row))
    assert any(code == 'MILITARY_PLAN_ACTOR' for code, _, _ in check_military(pack, index_of(pack)))
    actor.pop('travel_plan')
    role = next(row for row in pack['roles'] if row['id'] == 'role.military.a')
    role['place_id'] = 'west.office'
    next(row for row in pack['actors'] if row['id'] == role['initial_actor'])['initial_place'] = role['place_id']
    assert any(code == 'MILITARY_PLAN_PRESENCE' for code, _, _ in check_military(pack, index_of(pack)))
    actor['initial_place'] = role['place_id']
    assert validation.validate_pack(pack).ok, '同城官署与军营分开时，人物可先在官署实际受任'


def test_godot_uses_prepared_author_definitions(tmp_path):
    """真实准备目录交给 Godot 同时验证 Schema 和纯军事规则，坏定义跨语言一致拒绝。"""
    pack = military_pack(); output = tmp_path / 'military.hmcatalog.json'
    prepare_pack(write_pack(tmp_path, pack), output)
    scenarios = []
    for path, value, _ in CASES + plan_cases(pack):
        changed = deepcopy(pack); change(changed, path, value)
        scenarios.append({'name': '/'.join(map(str, path)), 'data': index_of(changed), 'valid': False})
    separate = deepcopy(pack)
    for actor in separate['actors']:
        if actor['id'] in ('actor.military.a.commander', 'actor.military.a.provider'): actor['initial_place'] = 'west.office'
    next(role for role in separate['roles'] if role['id'] == 'role.military.a')['place_id'] = 'west.office'
    scenarios.append({'name': '同城官署和军营分离', 'data': index_of(separate), 'valid': True})
    for kind in ('hold', 'breakthrough', 'escort', 'cover_retreat'):
        changed = deepcopy(pack); goal = changed['military_objectives'][0]; goal['kind'] = kind
        if kind == 'hold': goal.update(point=[1, 1], hold_rounds=2)
        if kind in ('breakthrough', 'cover_retreat'): goal['required_units'] = 1
        scenarios.append({'name': '默认目标/' + kind, 'data': index_of(changed), 'valid': True})
    payload = {'catalog_path': str(output), 'schema': validation.SCHEMA, 'valid': json.loads(output.read_text())['catalog'], 'cases': scenarios}
    source = tmp_path / 'godot-military.json'; source.write_text(json.dumps(payload, ensure_ascii=False), encoding='utf-8')
    result = subprocess.run([build.find_engine(), '--headless', '--path', str(ROOT / 'game'), '--script', 'res://tests/test_military_content.gd', '--', str(source)], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0 and 'FAILURES=0' in result.stdout and 'SCRIPT ERROR' not in result.stderr, result.stdout + result.stderr
