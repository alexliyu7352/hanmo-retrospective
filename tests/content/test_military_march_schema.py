"""地形行军只在明确剧本启用；正式作者门按有效驻军初态核验，不修改发布内容。"""
from copy import deepcopy
import json
from pathlib import Path

import pytest

from tools.content.io import canonical
from tools.content.military_links import check_military
from tools.content.pack_compose import _qualify_content
from tools.content.scenarios import register_schema, resolve
from tools.content.validation import SCHEMA, SCHEMA_BYTES, TABLES, StrictValidator, validate_pack

ROOT = Path(__file__).resolve().parents[2]
MODEL = 'terrain_march_v1'
PROFILE = {'initial_training': 47, 'initial_fatigue': 19}


def snapshot_pack():
    """在既有双年代测试副本添两处驻军；仅西部初态显式写测试条件，基础原型不补值。"""
    pack = json.loads((ROOT / 'tests/fixtures/content_packs/second_scenario/two_eras.json').read_text())
    alpha = json.loads((ROOT / 'content/foundation/alpha_life.json').read_text())
    troop = deepcopy(alpha['troop_types'][0])
    troop['tactics'] = []
    pack['troop_types'] = [troop]
    pack['texts'][troop['name_key']] = '行军合同测试兵种'
    pack['required_capabilities'].append('military')
    pack['garrisons'] = []
    for number, side in enumerate(('west', 'east')):
        unit = {'id': 'unit.' + side, 'troop_type_id': troop['id'],
                'commander_actor_id': 'actor.partner', 'initial_soldiers': 8,
                'equipment': [{'item_id': 'item.grain', 'quantity_per_person': 1}]}
        camp = {'id': 'garrison.' + side, 'name_key': 'name.camp.' + side,
                'place_id': side + '.square', 'organization_id': 'org.' + side,
                'inventory_id': 'inventory.' + side, 'wallet_id': 'wallet.' + side,
                'food_item_id': 'item.grain', 'ration_per_person': 1,
                'army_capacity': 40, 'units': [unit]}
        pack['texts'][camp['name_key']] = '行军合同测试驻军'
        pack['garrisons'].append(camp)
        scenario = pack['scenarios'][number]
        scenario['enabled']['garrisons'] = [camp['id']]
        scenario['initial']['garrisons'] = [{key: deepcopy(camp[key]) for key in ('id', 'organization_id', 'units')}]
    pack['scenarios'][0]['military_condition_model'] = MODEL
    pack['scenarios'][0]['initial']['garrisons'][0]['units'][0].update(PROFILE)
    pack['scenarios'][1].pop('world_map_id')
    return pack


def effective(pack, scenario_id='scenario.west'):
    """通过真实Snapshot投影后构造语义入口，不在测试中替代初态覆盖规则。"""
    index = {table: {row['id']: deepcopy(row) for row in pack.get(table, [])} for table in TABLES}
    index.update({field: deepcopy(pack[field]) for field in ('texts', 'flags', 'required_capabilities')})
    catalog = {'ok': True, 'data': index, 'schema_sha256': register_schema(SCHEMA_BYTES)['schema_sha256']}
    result = resolve(catalog, scenario_id)
    assert result['ok'], result
    data = result['catalog']['data']
    view = {table: list(data[table].values()) for table in TABLES}
    view.update({field: data[field] for field in ('texts', 'flags', 'required_capabilities')})
    return view, data


def test_mixed_scenarios_validate_only_effective_profiles_without_mutation():
    """一个剧本启用而另一个保留无配置道路模式；未启用原型不应被要求补值。"""
    pack = snapshot_pack()
    before = canonical(pack)
    report = validate_pack(pack, 'march')
    assert report.ok, report.to_dict()
    west, _ = effective(pack)
    east, _ = effective(pack, 'scenario.east')
    assert {key: west['garrisons'][0]['units'][0][key] for key in PROFILE} == PROFILE
    assert all(key not in east['garrisons'][0]['units'][0] for key in PROFILE)
    assert all(key not in unit for camp in pack['garrisons'] for unit in camp['units'] for key in PROFILE)
    assert canonical(pack) == before


def test_snapshot_initial_profiles_do_not_inherit_base_prototype_values():
    """初态漏写不能从原型已有条件补齐；同一错误仍定位到启用剧本。"""
    pack = snapshot_pack()
    pack['garrisons'][0]['units'][0].update(PROFILE)
    unit = pack['scenarios'][0]['initial']['garrisons'][0]['units'][0]
    for field in PROFILE:
        unit.pop(field)
    report = validate_pack(pack, 'march')
    assert any(issue.code == 'MILITARY_TRAINING_PROFILE' and
               issue.path.startswith('march#/scenarios/0/initial/garrisons') for issue in report.issues), report.to_dict()


@pytest.mark.parametrize('field', ['initial_training', 'initial_fatigue'])
def test_partial_effective_profile_is_rejected(field):
    """任一条件缺失都必须拒绝，不用零或训练默认数替作者决定初态。"""
    pack = snapshot_pack()
    del pack['scenarios'][0]['initial']['garrisons'][0]['units'][0][field]
    assert 'MILITARY_TRAINING_PROFILE' in {issue.code for issue in validate_pack(pack).issues}


@pytest.mark.parametrize('field', ['initial_training', 'initial_fatigue'])
@pytest.mark.parametrize('value', [-1, 101, True, 47.0, '47', None])
def test_profile_schema_rejects_wrong_types_and_bounds(field, value):
    """基础与显式初态都沿同一严格整数Schema，拒绝布尔、浮点、空值与越界。"""
    pack = snapshot_pack()
    pack['scenarios'][0]['initial']['garrisons'][0]['units'][0][field] = value
    report = validate_pack(pack)
    assert any(issue.code == 'SCHEMA' and issue.path.endswith('/' + field) for issue in report.issues)
    rule = SCHEMA['$defs']['garrison']['properties']['units']['items']
    unit = deepcopy(pack['scenarios'][0]['initial']['garrisons'][0]['units'][0])
    assert not StrictValidator({'$defs': SCHEMA['$defs'], **rule}).is_valid(unit)


@pytest.mark.parametrize('value', [0, 100])
def test_explicit_profile_endpoints_are_preserved(value):
    """零和满值均是合法明确作者值，校验不能改写成内部默认值。"""
    pack = snapshot_pack()
    unit = pack['scenarios'][0]['initial']['garrisons'][0]['units'][0]
    unit.update({field: value for field in PROFILE})
    assert validate_pack(pack).ok
    view, _ = effective(pack)
    assert all(view['garrisons'][0]['units'][0][field] == value for field in PROFILE)


@pytest.mark.parametrize('model', ['legacy_v1', 'rest_v1', '', None, False, 1, {}, []])
def test_unknown_model_is_rejected_by_schema_and_semantics(model):
    """只接受唯一已批准模型，缺省通过字段不存在表达，不能用另一个枚举冒充。"""
    pack = snapshot_pack()
    pack['scenarios'][0]['military_condition_model'] = model
    assert 'SCHEMA' in {issue.code for issue in validate_pack(pack).issues}
    view, data = effective(pack)
    issues = check_military(view, data)
    assert any(code == 'MILITARY_CONDITION_MODEL' and path == ('scenarios', 0, 'military_condition_model')
               for code, path, _ in issues)


def test_opt_in_without_terrain_map_rejects_legacy_road_activation():
    """已有道路和其他剧本地图都不能替本剧本提供实际地形来源。"""
    pack = snapshot_pack()
    del pack['scenarios'][0]['world_map_id']
    report = validate_pack(pack, 'march')
    assert any(issue.code == 'MILITARY_CONDITION_MAP' and issue.path == 'march#/scenarios/0/world_map_id'
               for issue in report.issues), report.to_dict()


def test_unknown_map_keeps_existing_reference_error():
    """真正不存在的地图仍由原引用首错拦截，不能把错误引用当可用地形。"""
    pack = snapshot_pack()
    pack['scenarios'][0]['world_map_id'] = 'map.missing'
    assert any(issue.code == 'REFERENCE' and issue.path.endswith('/world_map_id')
               for issue in validate_pack(pack).issues)


@pytest.mark.parametrize('field', ['initial_training', 'initial_fatigue'])
@pytest.mark.parametrize('value', [True, 19.0, '19', None, -1, 101])
def test_semantic_boundary_independently_rejects_bad_effective_profiles(field, value):
    """直接语义边界也拒绝错误条件，不依赖JSON数字转换或Schema先行替它修复。"""
    pack = snapshot_pack()
    pack['scenarios'][0]['initial']['garrisons'][0]['units'][0][field] = value
    view, data = effective(pack)
    assert 'MILITARY_TRAINING_PROFILE' in {issue[0] for issue in check_military(view, data)}


def test_no_opt_in_keeps_profiles_optional_and_source_unchanged():
    """删去显式开关与测试初值后，两个剧本继续原合同，校验不注入模型或条件。"""
    pack = snapshot_pack()
    del pack['scenarios'][0]['military_condition_model']
    unit = pack['scenarios'][0]['initial']['garrisons'][0]['units'][0]
    for field in PROFILE:
        del unit[field]
    before = canonical(pack)
    assert validate_pack(pack).ok
    assert canonical(pack) == before


def test_qualified_scenario_keeps_model_literal_and_profile_projection():
    """正式限定器只改明确ID，模型枚举原样保留且快照仍找到真实限定驻军。"""
    source = snapshot_pack()
    declaration = {'manifest': {'id': 'march.fixture', 'namespace': 'march', 'dependencies': []}}
    pack, _ = _qualify_content(source, declaration, 'content.json', {'march': declaration})
    assert pack['scenarios'][0]['military_condition_model'] == MODEL
    report = validate_pack(pack)
    assert report.ok, report.to_dict()
    view, _ = effective(pack, 'march::scenario.west')
    assert view['garrisons'][0]['units'][0]['id'] == 'march::unit.west'
    assert all(view['garrisons'][0]['units'][0][field] == PROFILE[field] for field in PROFILE)


def test_profile_diagnostic_maps_sorted_initials_back_to_original_author_order():
    """剧本、驻军和部队顺序都与有效视图不同，错误仍指向作者缺值的原始单元。"""
    pack = snapshot_pack()
    west = pack['scenarios'][0]
    first = west['initial']['garrisons'][0]
    unit = first['units'][0]
    unit['id'] = 'unit.z'
    second = deepcopy(unit)
    second.update(id='unit.a', commander_actor_id='actor.player')
    first['units'].append(second)
    for field in PROFILE:
        del unit[field]
    actor = deepcopy(pack['actors'][1])
    actor['id'] = 'actor.extra'
    pack['actors'].append(actor)
    west['enabled']['actors'].append(actor['id'])
    initial_actor = deepcopy(west['initial']['actors'][1])
    initial_actor['actor_id'] = actor['id']
    west['initial']['actors'].append(initial_actor)
    camp = deepcopy(pack['garrisons'][0])
    camp['id'] = 'garrison.a'
    camp['units'][0].update(id='unit.extra', commander_actor_id=actor['id'], **PROFILE)
    for table, field in [('wallets', 'wallet_id'), ('inventories', 'inventory_id')]:
        asset = deepcopy(pack[table][0])
        asset['id'] += '.extra'
        camp[field] = asset['id']
        pack[table].append(asset)
        west['enabled'][table].append(asset['id'])
        west['initial'][table].append(deepcopy(asset))
    pack['garrisons'].append(camp)
    west['enabled']['garrisons'].append(camp['id'])
    west['initial']['garrisons'].append({key: deepcopy(camp[key]) for key in ('id', 'organization_id', 'units')})
    pack['scenarios'].reverse()
    report = validate_pack(pack, 'march')
    assert [(issue.code, issue.path, issue.record_id) for issue in report.issues] == [
        ('MILITARY_TRAINING_PROFILE', 'march#/scenarios/1/initial/garrisons/0/units/0', 'scenario.west')]


def test_legacy_snapshotless_scenario_requires_each_available_unit_profile():
    """未使用新初态组的旧剧本可明确启用，但全体实际驻军必须由作者成对给值。"""
    source = snapshot_pack()
    view, _ = effective(source)
    pack = {**source, **view}
    for actor in pack['actors']:
        if actor.get('combat') == {}:
            actor.pop('combat')
    for field in ('playable_actor_ids', 'start_date', 'enabled', 'initial'):
        del pack['scenarios'][0][field]
    assert validate_pack(pack).ok
    for field in PROFILE:
        del pack['garrisons'][0]['units'][0][field]
    report = validate_pack(pack, 'march')
    assert [(issue.code, issue.path) for issue in report.issues] == [
        ('MILITARY_TRAINING_PROFILE', 'march#/garrisons/0/units/0')]
    del pack['scenarios'][0]['military_condition_model']
    assert validate_pack(pack).ok


def test_model_still_requires_military_capability_without_available_units():
    """仅显式模型也声明军事能力，不能用空驻军绕过既有授权入口。"""
    pack = json.loads((ROOT / 'tests/fixtures/content_packs/second_scenario/two_eras.json').read_text())
    pack['scenarios'][0]['military_condition_model'] = MODEL
    assert 'military' not in pack['required_capabilities']
    assert 'MILITARY_CAPABILITY' in {issue.code for issue in validate_pack(pack).issues}


def test_duplicate_unit_diagnostic_still_targets_missing_profile_occurrence():
    """重复身份本来即非法，但缺条件的第二记录不能误指第一条合法条件。"""
    pack = snapshot_pack()
    units = pack['scenarios'][0]['initial']['garrisons'][0]['units']
    duplicate = deepcopy(units[0])
    duplicate['commander_actor_id'] = 'actor.player'
    for field in PROFILE:
        del duplicate[field]
    units.append(duplicate)
    report = validate_pack(pack, 'march')
    assert any(issue.code == 'MILITARY_UNIT_DUPLICATE' for issue in report.issues)
    assert [issue.path for issue in report.issues if issue.code == 'MILITARY_TRAINING_PROFILE'] == [
        'march#/scenarios/0/initial/garrisons/0/units/1']
