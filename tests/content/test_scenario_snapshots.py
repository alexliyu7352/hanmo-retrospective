"""通过真实作者目录验证纯剧本视图；这些检查不替代Factory/Kernel的运行接线。"""
import copy
import importlib
import json
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.content.validation import TABLES, SCHEMA_BYTES
from tools.content.io import canonical

FIXTURE = ROOT / 'tests/fixtures/content_packs/second_scenario/two_eras.json'


def api():
    """先确认生产模块存在，再调用同一解析入口，不用测试伪实现代替功能。"""
    assert (ROOT / 'tools/content/scenarios.py').exists(), 'ScenarioSnapshot纯解析尚未实现'
    return importlib.import_module('tools.content.scenarios')


def catalog(source=FIXTURE):
    """把真实HM-CONTENT定义索引化，保留不可被剧本视图改写的整体身份。"""
    pack = json.loads(source.read_text())
    data = {table: {row['id']: row for row in pack.get(table, [])} for table in TABLES}
    data.update({field: pack[field] for field in ('texts', 'flags', 'required_capabilities')})
    return {'ok': True, 'data': data, 'content_id': pack['id'], 'content_version': pack['version'],
            'content_digest': 'a' * 64, 'pack_set_digest': 'b' * 64,
            'schema_sha256': api().register_schema(SCHEMA_BYTES)['schema_sha256'],
            'pack_set': {'format': 'HM-PACK-SET', 'format_version': 1, 'catalog_sha256': 'a' * 64}}


def scenario(source, name='west'):
    """只缩短测试对真实作者剧本的定位，不替代解析或世界初始化。"""
    return source['data']['scenarios']['scenario.' + name]


def test_resolve_two_eras_is_detached_and_retains_whole_content_identity():
    """同一人物定义在不同年代的有效年龄不同，来源和整体锁保持原样。"""
    source = catalog()
    before = canonical(source)
    west = api().resolve(source, 'scenario.west')
    east = api().resolve(source, 'scenario.east')
    assert west['ok'] and east['ok'], (west, east)
    for result, age in [(west, 24), (east, 44)]:
        assert result['catalog']['data']['actors']['actor.player']['initial_age_years'] == age
        for key in ('content_id', 'content_version', 'content_digest', 'pack_set', 'pack_set_digest'):
            assert result['catalog'][key] == source[key]
        assert result['catalog']['data']['active_static_actor_ids'] == [
            'actor.deceased', 'actor.partner', 'actor.player']
    assert canonical(source) == before
    west['catalog']['data']['actors']['actor.player']['attributes']['martial'] = 99
    west['initial']['actors'][0]['skills']['transport'] = 77
    assert canonical(source) == before
    assert east['catalog']['data']['actors']['actor.player']['attributes']['martial'] == 60


def test_definition_closure_preserves_uninstantiated_family_prototype():
    """岗位引用使原型定义存在，但不会把原型添加到真人激活列表。"""
    result = api().resolve(catalog(), 'scenario.west')
    assert result['ok'], result
    data = result['catalog']['data']
    assert 'actor.prototype' in data['actors']
    assert 'actor.prototype' not in data['active_static_actor_ids']
    assert 'actor.prototype' not in [row['actor_id'] for row in result['initial']['actors']]
    assert data['actors']['actor.deceased']['initial_age_years'] == 24
    dead = next(row for row in result['initial']['actors'] if row['actor_id'] == 'actor.deceased')
    assert dead['alive'] is False and dead['available'] is False


def test_selected_actor_keeps_identity_and_default_is_explicit():
    """可控角色选择只改变选中的人物引用，不能交换或改写作者ID。"""
    source = catalog()
    chosen = api().resolve(source, 'scenario.west', 'actor.partner')
    default = api().resolve(source, 'scenario.west')
    assert chosen['ok'] and default['ok']
    assert chosen['initial']['player_actor_id'] == 'actor.partner'
    assert default['initial']['player_actor_id'] == 'actor.player'
    assert chosen['catalog']['data']['scenarios']['scenario.west']['player_actor'] == 'actor.partner'
    assert source['data']['scenarios']['scenario.west']['player_actor'] == 'actor.player'
    assert set(chosen['catalog']['data']['actors']) == set(default['catalog']['data']['actors'])


@pytest.mark.parametrize('field', ['playable_actor_ids', 'start_date', 'enabled', 'initial'])
def test_partial_snapshot_group_is_never_legacy(field):
    """缺失新字段组中的任何成员都必须明确失败，不能静默回退全人物开局。"""
    source = catalog()
    del scenario(source)[field]
    before = canonical(source)
    result = api().resolve(source, 'scenario.west')
    assert result['ok'] is False and result['code'] == 'PartialScenarioSnapshot'
    assert result['path'].startswith('/scenarios/scenario.west')
    assert canonical(source) == before


@pytest.mark.parametrize('person', ['actor.prototype', 'actor.deceased', 'actor.missing'])
def test_unlisted_dead_or_unknown_player_is_rejected(person):
    """不能把仅原型、已故人物或不存在人物作为可控角色开局。"""
    result = api().resolve(catalog(), 'scenario.west', person)
    assert result['ok'] is False and result['code'] == 'InvalidPlayableActor'


@pytest.mark.parametrize('field,value', [('year', 0), ('day_of_year', 0),
                                        ('day_of_year', 361), ('half_day', 2), ('year', True)])
def test_calendar_date_is_strict_bounded_integer(field, value):
    """历史日期不接受越界或布尔值充当整数，也不与世界tick混写。"""
    source = catalog()
    scenario(source)['start_date'][field] = value
    result = api().resolve(source, 'scenario.west')
    assert result['ok'] is False and result['code'] == 'InvalidScenarioStartDate'


def test_missing_activated_actor_initial_is_an_author_error():
    """显式激活缺初态人物必须失败；保留原型不意味着允许漏建真人。"""
    source = catalog()
    scenario(source)['initial']['actors'].pop(1)
    result = api().resolve(source, 'scenario.west')
    assert result['ok'] is False and result['code'] == 'MissingScenarioInitial'


def test_legacy_view_keeps_all_actors_ages_and_definition_order():
    """旧目录不排序改写既有实例分配顺序，只附稳定激活列表。"""
    source = catalog(ROOT / 'content/foundation/procurement.json')
    before = canonical(source)
    result = api().resolve(source, 'scenario.procurement')
    assert result['ok'], result
    assert result['initial']['legacy'] is True
    assert result['catalog']['data']['active_static_actor_ids'] == sorted(source['data']['actors'])
    assert list(result['catalog']['data']['actors']) == list(source['data']['actors'])
    for ident, actor in source['data']['actors'].items():
        assert result['catalog']['data']['actors'][ident] == actor
    assert canonical(source) == before


def test_unknown_scenario_does_not_mutate_catalog():
    """错误选择不会更改输入目录，也不会伪造默认剧本。"""
    source = catalog()
    before = canonical(source)
    result = api().resolve(source, 'scenario.missing')
    assert result == {'ok': False, 'code': 'UnknownScenario', 'path': '/scenarios/scenario.missing'}
    assert canonical(source) == before


def test_other_map_is_definition_only_and_never_an_active_place():
    """原型旧地点保留合法定义，但不能令另一地图的据点进入当前活动世界。"""
    result = api().resolve(catalog(), 'scenario.east')
    assert result['ok'], result
    data = result['catalog']['data']
    assert list(data['settlements']) == ['city.east']
    assert list(data['world_maps']) == ['map.east']
    assert set(data['places']) == {'east.office', 'east.square'}
    assert data['actors']['actor.prototype']['initial_place'] == 'west.square'
    assert 'west.square' in data['definitions']['places']
    assert 'city.west' in data['definitions']['settlements']
    assert 'west.square' not in data['active_entity_ids']['places']


def test_different_organization_and_asset_initials_use_existing_records():
    """第二剧本的真实公产初值由明确初态决定，不是复制第一剧本后换显示标题。"""
    west = api().resolve(catalog(), 'scenario.west')['catalog']['data']
    east = api().resolve(catalog(), 'scenario.east')['catalog']['data']
    assert list(west['organizations']) == ['org.west']
    assert list(east['organizations']) == ['org.east']
    assert west['wallets']['wallet.west']['initial_balance'] == 300
    assert east['wallets']['wallet.east']['initial_balance'] == 1700
    assert west['inventories']['inventory.west']['goods'][0]['quantity'] == 25
    assert east['inventories']['inventory.east']['goods'][0]['quantity'] == 75


def test_definition_and_initial_enumeration_order_does_not_change_new_view():
    """新模式无序定义、启用集合与初态记录逆序后产生完全相同的确定视图。"""
    source = catalog()
    reversed_source = copy.deepcopy(source)
    for table in TABLES:
        reversed_source['data'][table] = dict(reversed(list(source['data'][table].items())))
    for entry in reversed_source['data']['scenarios'].values():
        entry['playable_actor_ids'].reverse()
        for ids in entry['enabled'].values():
            ids.reverse()
        for rows in entry['initial'].values():
            rows.reverse()
    before = canonical(reversed_source)
    first = api().resolve(source, 'scenario.east')
    second = api().resolve(reversed_source, 'scenario.east')
    assert first['ok'] and second['ok']
    assert canonical(first) == canonical(second)
    assert canonical(reversed_source) == before
    for table in TABLES:
        assert list(first['catalog']['data'][table]) == sorted(first['catalog']['data'][table])


@pytest.mark.parametrize('table', ['wallets', 'inventories', 'roles', 'relations'])
def test_explicit_runtime_assets_require_initial_rows(table):
    """启用真实资产和岗位但漏掉初态必须失败，不从别年代定义悄悄补齐。"""
    source = catalog()
    scenario(source)['initial'][table] = []
    result = api().resolve(source, 'scenario.west')
    assert result['ok'] is False and result['code'] == 'MissingScenarioInitial'


def test_referenced_wallet_is_not_implicitly_activated():
    """岗位引用只形成定义依赖，不能暗中启用作者未选择的钱包。"""
    source = catalog()
    scenario(source)['enabled']['wallets'] = []
    scenario(source)['initial']['wallets'] = []
    result = api().resolve(source, 'scenario.west')
    assert result['ok'] is False and result['code'] == 'ScenarioDependencyNotEnabled'
    assert result['path'] == '/roles/role.west/family/wallet_id'


def test_explicit_prototype_definition_does_not_need_actor_initial():
    """定义集合可以显式保留原型，激活仍只由initial.actors决定。"""
    source = catalog()
    scenario(source)['enabled']['actors'].append('actor.prototype')
    result = api().resolve(source, 'scenario.west')
    assert result['ok'], result
    assert 'actor.prototype' not in result['catalog']['data']['active_static_actor_ids']


def test_duplicate_or_unknown_initial_is_rejected_not_last_writer_wins():
    """重复初态与未启用记录都有明确诊断，不容忍隐式覆盖或自建ID。"""
    source = catalog()
    scenario(source)['initial']['actors'].append(copy.deepcopy(scenario(source)['initial']['actors'][0]))
    result = api().resolve(source, 'scenario.west')
    assert result['code'] == 'DuplicateScenarioInitial'
    source = catalog()
    scenario(source)['initial']['wallets'][0]['id'] = 'wallet.east'
    result = api().resolve(source, 'scenario.west')
    assert result['code'] == 'MissingScenarioInitial'


def test_unknown_typed_reference_is_rejected_without_activation():
    """引用注解指向的缺失原型有准确路径，普通字符串不会被猜测为引用。"""
    source = catalog()
    source['data']['roles']['role.west']['family']['prototype_id'] = 'actor.missing'
    result = api().resolve(source, 'scenario.west')
    assert result['code'] == 'UnknownScenarioReference'
    assert result['path'] == '/roles/role.west/family/prototype_id'


def test_unknown_schema_never_uses_the_current_schema_silently():
    """Schema身份必须匹配已经登记的原字节，不可拿当前版本解释另一个身份。"""
    source = catalog()
    source['schema_sha256'] = 'f' * 64
    result = api().resolve(source, 'scenario.west')
    assert result == {'ok': False, 'code': 'ScenarioReferenceRulesMissing', 'path': '/schema_sha256'}


def test_registered_schema_hash_is_of_exact_bytes_and_stays_out_of_catalog():
    """Schema缓存按原字节摘要区分，不能canonical重写后伪装确切历史Schema。"""
    import hashlib
    compact = json.dumps(json.loads(SCHEMA_BYTES)).encode()
    result = api().register_schema(compact)
    assert result['schema_sha256'] == hashlib.sha256(compact).hexdigest()
    assert result['schema_sha256'] != api().register_schema(SCHEMA_BYTES)['schema_sha256']
    source = catalog()
    source['schema_sha256'] = result['schema_sha256']
    resolved = api().resolve(source, 'scenario.west')
    assert resolved['ok'], resolved
    assert 'schema' not in resolved['catalog'] and 'reference_rules' not in resolved['catalog']


def test_python_and_pinned_godot_produce_identical_complete_views(tmp_path):
    """真正执行Godot纯模块并逐字段对照Python结果，既不mock引擎也不搭假state。"""
    import os
    import subprocess
    binary = os.environ.get('GODOT_BIN')
    if not binary:
        pytest.skip('GODOT_BIN未提供；独立Godot局部门另行运行')
    output = tmp_path / 'scenario-views.json'
    source = catalog()
    cases = [{'catalog': source, 'scenario_id': 'scenario.west'},
             {'catalog': source, 'scenario_id': 'scenario.east'},
             {'catalog': source, 'scenario_id': 'scenario.east', 'player_actor_id': 'actor.partner'},
             {'catalog': catalog(ROOT / 'content/foundation/procurement.json'), 'scenario_id': 'scenario.procurement'}]
    full, entry = full_domain_snapshot()
    cases.append({'catalog': full, 'scenario_id': entry['id']})
    reverse = copy.deepcopy(full)
    for inventory in reverse['data']['scenarios'][entry['id']]['initial']['inventories']:
        inventory['goods'].reverse()
    for garrison in reverse['data']['scenarios'][entry['id']]['initial']['garrisons']:
        garrison['units'].reverse()
    cases.append({'catalog': reverse, 'scenario_id': entry['id']})
    bad = copy.deepcopy(source)
    scenario(bad)['initial']['offer_ids'] = []
    cases.append({'catalog': bad, 'scenario_id': 'scenario.west'})
    input_path = tmp_path / 'scenario-cases.json'
    input_path.write_text(json.dumps(cases, ensure_ascii=False) + '\n')
    run = subprocess.run([binary, '--headless', '--path', str(ROOT / 'game'), '--script',
                          'res://tests/test_scenario_snapshot.gd', '--', '--resolve-cases', str(input_path), str(output)],
                         capture_output=True, text=True, timeout=45)
    assert run.returncode == 0 and 'SCRIPT ERROR' not in run.stdout + run.stderr, run.stdout + run.stderr
    expected = [api().resolve(case['catalog'], case['scenario_id'], case.get('player_actor_id', '')) for case in cases]
    assert json.loads(output.read_text()) == expected



def full_domain_snapshot():
    """把现有真实alpha_life作者目录写成显式初态，覆盖冻结的庄园与军政表而不造state。"""
    source = catalog(ROOT / 'content/foundation/alpha_life.json')
    data = source['data']
    entry = data['scenarios']['scenario.alpha_life']
    entry.update(playable_actor_ids=[entry['player_actor']],
                 start_date={'year': 204, 'day_of_year': 1, 'half_day': 0},
                 enabled={table: list(data[table]) for table in (*TABLES, 'texts', 'flags')})
    appearance = scenario(catalog())['initial']['actors'][0]['appearance_definition']
    initial = {'actors': [], 'offer_ids': list(data['offers'])}
    for ident, actor in data['actors'].items():
        initial['actors'].append({'actor_id': ident, 'place_id': actor['initial_place'],
                                  'alive': True, 'available': True,
                                  'age_years': actor.get('initial_age_years', 24),
                                  'attributes': copy.deepcopy(actor['attributes']),
                                  'skills': copy.deepcopy(actor['skills']),
                                  'combat': copy.deepcopy(actor.get('combat', {})),
                                  'appearance_definition': copy.deepcopy(actor.get('appearance_definition', appearance))})
    for table, fields in api().INITIAL_FIELDS.items():
        initial[table] = []
        for ident, definition in data[table].items():
            if table == 'roles':
                row = {'role_id': ident, 'actor_id': definition['initial_actor'], 'enabled': True}
            else:
                row = {key: copy.deepcopy(definition[key]) for key in fields}
            initial[table].append(row)
    entry['initial'] = initial
    return source, entry


def test_existing_estate_and_military_shapes_project_without_new_appointment_dsl():
    """庄园/驻军复用真实现有字段，公职和据点只确认ID，不偷建任职或占领事实。"""
    source, entry = full_domain_snapshot()
    estate = entry['initial']['estates'][0]
    estate['initial_facilities']['farm'] += 1
    garrison = entry['initial']['garrisons'][0]
    garrison['units'][0]['initial_soldiers'] += 1
    result = api().resolve(source, entry['id'])
    assert result['ok'], result
    data = result['catalog']['data']
    assert data['estates'][estate['id']]['initial_facilities'] == estate['initial_facilities']
    expected_unit = garrison['units'][0]
    actual_unit = next(row for row in data['garrisons'][garrison['id']]['units'] if row['id'] == expected_unit['id'])
    assert actual_unit['initial_soldiers'] == expected_unit['initial_soldiers']
    assert result['initial']['offices'] and all(set(row) == {'id'} for row in result['initial']['offices'])
    assert result['initial']['strongholds'] and all(set(row) == {'id'} for row in result['initial']['strongholds'])
    for table in ('offices', 'strongholds'):
        assert data[table] == source['data'][table]
    assert 'state' not in result and 'facts' not in result['initial']


@pytest.mark.parametrize('table', ['estates', 'garrisons', 'offices', 'strongholds'])
def test_enabled_domain_record_cannot_omit_initial(table):
    """每个启用领域记录都需要对应初态，不借定义存在掩盖遗漏。"""
    source, entry = full_domain_snapshot()
    entry['initial'][table].pop()
    result = api().resolve(source, entry['id'])
    assert result['ok'] is False and result['code'] == 'MissingScenarioInitial'


def test_initial_cannot_inject_new_office_holder_fields():
    """没有冻结来源事实的任职字段不属于本次初态合同，不能作为通用override偷偷接受。"""
    source, entry = full_domain_snapshot()
    entry['initial']['offices'][0]['holder_actor_id'] = entry['player_actor']
    result = api().resolve(source, entry['id'])
    assert result['ok'] is False and result['code'] == 'InvalidScenarioInitial'


@pytest.mark.parametrize('speaker', ['$provider', 'actor.partner'])
def test_dialogue_literal_speaker_is_not_an_actor_reference(speaker):
    """复用Schema联合分支的常量优先规则，不能把合法对白角色常量当缺失人物。"""
    source = catalog()
    source['data']['dialogues']['dialog.literal'] = {
        'id': 'dialog.literal', 'context_role': 'role.west', 'entry': 'hello',
        'nodes': [{'id': 'hello', 'kind': 'say', 'speaker': speaker, 'text_key': 'name.child', 'next': 'end'},
                  {'id': 'end', 'kind': 'end'}]}
    source['data']['actors']['actor.partner']['dialogue_id'] = 'dialog.literal'
    result = api().resolve(source, 'scenario.west')
    assert result['ok'], result
    assert result['catalog']['data']['dialogues']['dialog.literal']['nodes'][0]['speaker'] == speaker
    assert speaker not in result['catalog']['data']['actors'] if speaker.startswith('$') else True


def test_enabled_offer_without_explicit_initial_cannot_silently_disappear():
    """显式启用的机会必须给齐initial.offer_ids，不可静默裁掉真实内容并返回成功。"""
    source = catalog()
    scenario(source)['initial']['offer_ids'] = []
    result = api().resolve(source, 'scenario.west')
    assert result == {'ok': False, 'code': 'MissingScenarioInitial',
                      'path': '/scenarios/scenario.west/initial/offer_ids/offer.west'}


def test_explicit_offer_and_literal_dialogue_remain_in_the_selected_view():
    """真实机会保留原有条件与受理结构，合法对白角色常量仍完整原样保留。"""
    result = api().resolve(catalog(), 'scenario.west')
    assert result['ok'], result
    data = result['catalog']['data']
    assert list(data['offers']) == data['active_entity_ids']['offers'] == ['offer.west']
    assert result['initial']['offer_ids'] == ['offer.west']
    assert data['dialogues']['dialog.west']['nodes'][0]['speaker'] == '$provider'


def test_fixture_uses_real_shared_record_shapes_and_declared_capabilities():
    """新字段组以外仍是现有Schema记录，能力声明完整；整体剧本语义留给中央接线验证。"""
    from tools.content.validation import VALIDATOR
    from tools.content.links import required_capabilities
    pack = json.loads(FIXTURE.read_text())
    assert required_capabilities(pack) <= set(pack['required_capabilities'])
    for entry in pack['scenarios']:
        for field in api().SNAPSHOT_FIELDS:
            del entry[field]
    assert not list(VALIDATOR.iter_errors(pack))


def test_unordered_initial_goods_and_garrison_units_have_stable_order():
    """物资和编制记录按各自稳定ID排序，避免输入枚举改变未来lot/部队遍历；语义数组不动。"""
    source, entry = full_domain_snapshot()
    reversed_source = copy.deepcopy(source)
    reverse_initial = reversed_source['data']['scenarios'][entry['id']]['initial']
    for inventory in reverse_initial['inventories']:
        inventory['goods'].reverse()
    for garrison in reverse_initial['garrisons']:
        garrison['units'].reverse()
        for unit in garrison['units']:
            unit['equipment'].reverse()
    for estate in reverse_initial['estates']:
        estate['resident_actor_ids'].reverse()
        estate['initial_duties'].reverse()
    first = api().resolve(source, entry['id'])
    second = api().resolve(reversed_source, entry['id'])
    assert first['ok'] and second['ok'], (first, second)
    assert canonical(first) == canonical(second)
    original_plan = next((actor.get('travel_plan') for actor in source['data']['actors'].values()
                          if actor.get('travel_plan')), None)
    if original_plan:
        assert original_plan in [actor.get('travel_plan') for actor in first['catalog']['data']['actors'].values()]


def actor_initial_goods_catalog():
    """在真实双年代夹具中加入第二商品与相异原型库存，明确区分基础和选中初货。"""
    source = catalog()
    salt = copy.deepcopy(source['data']['items']['item.grain'])
    salt.update(id='item.salt', name_key='name.item.salt')
    source['data']['items'][salt['id']] = salt
    source['data']['texts'][salt['name_key']] = '盐'
    for actor in source['data']['actors'].values():
        actor['initial_goods'] = [{'item_id': 'item.salt', 'quantity': 8},
                                  {'item_id': 'item.grain', 'quantity': 9}]
    player = next(row for row in scenario(source, 'east')['initial']['actors']
                  if row['actor_id'] == 'actor.player')
    player['initial_goods'] = [{'item_id': 'item.salt', 'quantity': 2},
                               {'item_id': 'item.grain', 'quantity': 3}]
    return source


def test_actor_initial_goods_project_defaults_and_keep_inactive_definition():
    """选中人物只用显式初货，缺省空袋；未激活原型保留完整基础定义和原货物顺序。"""
    source = actor_initial_goods_catalog()
    before = canonical(source)
    result = api().resolve(source, 'scenario.east')
    assert result['ok'], result
    rows = {row['actor_id']: row for row in result['initial']['actors']}
    expected = [{'item_id': 'item.grain', 'quantity': 3}, {'item_id': 'item.salt', 'quantity': 2}]
    data = result['catalog']['data']
    assert rows['actor.player']['initial_goods'] == data['actors']['actor.player']['initial_goods'] == expected
    assert rows['actor.partner']['initial_goods'] == data['actors']['actor.partner']['initial_goods'] == []
    assert data['actors']['actor.prototype'] == source['data']['actors']['actor.prototype']
    assert data['definitions']['actors']['actor.prototype'] == source['data']['actors']['actor.prototype']
    assert 'item.salt' in data['items'] and 'item.salt' in data['definitions']['items']
    assert canonical(source) == before
    rows['actor.player']['initial_goods'][0]['quantity'] = 100
    assert data['actors']['actor.player']['initial_goods'] == expected
    assert canonical(source) == before


def test_actor_initial_goods_order_empty_and_legacy_remain_deterministic():
    """新货物逆序仍得到相同完整结果，显式空不继承，旧目录的人物和货物枚举不变。"""
    source = actor_initial_goods_catalog()
    first = api().resolve(source, 'scenario.east')
    player = next(row for row in scenario(source, 'east')['initial']['actors']
                  if row['actor_id'] == 'actor.player')
    player['initial_goods'].reverse()
    assert canonical(api().resolve(source, 'scenario.east')) == canonical(first)
    player['initial_goods'] = []
    empty = api().resolve(source, 'scenario.east')
    assert empty['ok'] and empty['catalog']['data']['actors']['actor.player']['initial_goods'] == []
    legacy = catalog(ROOT / 'content/foundation/alpha_life.json')
    before = canonical(legacy)
    result = api().resolve(legacy, 'scenario.alpha_life')
    assert result['ok'] and result['initial']['legacy']
    assert list(result['catalog']['data']['actors']) == list(legacy['data']['actors'])
    assert result['catalog']['data']['actors'] == legacy['data']['actors']
    assert canonical(legacy) == before


@pytest.mark.parametrize('goods,code', [
    (None, 'InvalidScenarioActorInitial'),
    ([{'item_id': 'item.grain', 'quantity': True}], 'InvalidScenarioActorInitial'),
    ([{'item_id': 'item.grain', 'quantity': 101}], 'InvalidScenarioActorInitial'),
    ([{'item_id': 'item.grain', 'quantity': 1, 'owner': 'actor.player'}], 'InvalidScenarioInitial'),
    ([{'item_id': 'item.grain', 'quantity': 1}] * 2, 'DuplicateScenarioInitial'),
])
def test_actor_initial_goods_invalid_values_fail_without_mutation(goods, code):
    """初货严格沿现有物品记录，不接受布尔数量、任意覆盖或悄悄合并重复项。"""
    source = actor_initial_goods_catalog()
    player = next(row for row in scenario(source, 'east')['initial']['actors']
                  if row['actor_id'] == 'actor.player')
    player['initial_goods'] = goods
    before = canonical(source)
    result = api().resolve(source, 'scenario.east')
    assert not result['ok'] and result['code'] == code
    assert '/initial/actors/actor.player/initial_goods' in result['path']
    assert canonical(source) == before


def test_actor_initial_goods_unknown_reference_and_unapproved_actor_field_fail():
    """新初货引用沿原Schema闭包定位；允许initial_goods不等于开放任意人物字段覆盖。"""
    source = actor_initial_goods_catalog()
    player = next(row for row in scenario(source, 'east')['initial']['actors']
                  if row['actor_id'] == 'actor.player')
    player['initial_goods'] = [{'item_id': 'item.missing', 'quantity': 1}]
    result = api().resolve(source, 'scenario.east')
    assert not result['ok'] and result['code'] == 'UnknownScenarioReference'
    assert result['path'].endswith('/initial_goods/0/item_id')
    player['initial_goods'] = []
    player['goods_override'] = []
    result = api().resolve(source, 'scenario.east')
    assert not result['ok'] and result['code'] == 'InvalidScenarioInitial'
