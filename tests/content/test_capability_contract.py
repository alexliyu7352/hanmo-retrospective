"""对受信能力描述和跨语言运行边界做真实拒绝测试，不把声明匹配当运行证明。"""
import copy
import hashlib
import importlib
import json
from pathlib import Path
import pytest
from tools.content.io import canonical, check_target, prepare_pack, read_pack
from tools.content.validation import validate_pack, VALIDATOR

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = ROOT / 'game/data/rule_capabilities.json'
SOURCES = sorted(p for p in (ROOT / 'content/foundation').glob('*.json')
                 if json.loads(p.read_text()).get('format') == 'HM-CONTENT')


def contract_api():
    """缺失实现以明确断言失败，避免导入错误掩盖红测试目标。"""
    assert (ROOT / 'tools/content/capability_contract.py').is_file(), 'trusted capability contract is missing'
    return importlib.import_module('tools.content.capability_contract')


@pytest.mark.parametrize('source', SOURCES, ids=lambda p: p.stem)
def test_all_authored_sources_match_static_runtime_declaration(source):
    """当前五份合法作者包均保持兼容，静态匹配不产生运行认证。"""
    report = check_target(read_pack(source), read_pack(ROOT / 'content/foundation/runtime_capabilities.json'))
    assert report['declaration_match'] is True, report
    assert report['runtime_verified'] is False


@pytest.mark.parametrize('table,field,value,code', [
    ('events', 'trigger', 'on_tick', 'UNSUPPORTED_EVENT_TRIGGER'),
    ('events', 'trigger', 'after_action', 'UNSUPPORTED_EVENT_TRIGGER'),
    ('events', 'on_unavailable', 'defer', 'UNSUPPORTED_EVENT_POLICY'),
    ('offers', 'on_unavailable', 'cancel_without_reward', 'UNSUPPORTED_TASK_POLICY'),
])
def test_future_schema_values_are_editable_but_not_executable(tmp_path, table, field, value, code):
    """结构允许未来枚举，准备器拒绝未实现语义并精确定位原记录。"""
    pack = read_pack(ROOT / 'content/foundation/two_stories.json')
    pack[table][0][field] = value
    assert VALIDATOR.is_valid(pack)
    rejected = validate_pack(pack, 'variant.json')
    matches = [issue for issue in rejected.issues if issue.code == code]
    assert len(matches) == 1, rejected.to_dict()
    assert matches[0].path == f'variant.json#/{table}/0/{field}'
    assert matches[0].record_id == pack[table][0]['id']
    source = tmp_path / 'variant.json'
    source.write_bytes(canonical(pack))
    with pytest.raises(ValueError, match=code):
        prepare_pack(source, tmp_path / 'variant.hmcatalog.json')


def test_trusted_descriptor_has_real_test_associations_and_no_script_capability():
    """十八项能力来自单一受信描述，每项关联唯一登记的真实运行测试。"""
    from tools.verify import TESTS
    api = contract_api()
    contract = api.load_contract(CONTRACT)
    entries = contract['capabilities']
    assert len(entries) == 18
    assert {item['capability_id'] for item in entries} == set(read_pack(ROOT / 'content/foundation/alpha_life.json')['required_capabilities'])
    assert all(item['test_ids'] and set(item['test_ids']) <= set(TESTS) for item in entries)
    assert all(item['support_status'] == 'implemented/unverified' for item in entries)
    assert not any('script' in item['capability_id'] for item in entries)
    for source in SOURCES:
        assert not api.check_supported_semantics(read_pack(source), contract)


@pytest.mark.parametrize('mutation', ['duplicate', 'module', 'tests', 'unknown_test', 'script'])
def test_invalid_trusted_descriptor_is_rejected(tmp_path, mutation):
    """重复能力、未知模块、缺失或虚构测试关联以及任意脚本能力都不能发布。"""
    api = contract_api()
    contract = api.load_contract(CONTRACT)
    if mutation == 'duplicate': contract['capabilities'].append(copy.deepcopy(contract['capabilities'][0]))
    if mutation == 'module': contract['capabilities'][0]['module_id'] = 'unknown.module'
    if mutation == 'tests': contract['capabilities'][0]['test_ids'] = []
    if mutation == 'unknown_test': contract['capabilities'][0]['test_ids'] = ['test_nonexistent']
    if mutation == 'script': contract['capabilities'][0]['capability_id'] = 'arbitrary.script'
    path = tmp_path / 'contract.json'
    path.write_bytes(canonical(contract))
    with pytest.raises(ValueError): api.load_contract(path)


def test_prepared_catalog_binds_contract_bytes(tmp_path):
    """描述摘要和Schema摘要同时绑定准备物，静态成功仍明确标记未认证。"""
    result = prepare_pack(ROOT / 'content/foundation/two_stories.json', tmp_path / 'ready.hmcatalog.json')
    assert CONTRACT.is_file(), 'trusted descriptor is missing'
    assert result.get('rule_capabilities_sha256') == hashlib.sha256(CONTRACT.read_bytes()).hexdigest()
    assert result['runtime_verified'] is False


def test_unverified_declaration_still_matches_statically():
    """能力声明只需ABI与集合匹配，不能要求调用者伪称运行通过。"""
    pack = read_pack(ROOT / 'content/foundation/two_stories.json')
    runtime = {'format': 'HM-RUNTIME-CAPABILITIES', 'content_abi': 1,
               'runtime_verified': False, 'capabilities': pack['required_capabilities']}
    assert check_target(pack, runtime)['declaration_match'] is True


@pytest.mark.parametrize('mutation', ['selector', 'semantic_field', 'semantic_value', 'historical_verified'])
def test_descriptor_cannot_silently_drift_from_schema_or_evidence(tmp_path, mutation):
    """描述拼错字段或以历史测试冒充当前认证时，加载须失败而非静默放过。"""
    api = contract_api()
    contract = api.load_contract(CONTRACT)
    if mutation == 'selector': contract['capabilities'][0]['schema_selectors'] = [{'path': 'actors.*.not_a_field'}]
    if mutation == 'semantic_field': contract['capabilities'][3]['supported_semantics'][0]['field'] = 'missing_policy'
    if mutation == 'semantic_value': contract['capabilities'][3]['supported_semantics'][0]['values'] = ['run_external_script']
    if mutation == 'historical_verified': contract['capabilities'][0]['support_status'] = 'verified'
    path = tmp_path / 'contract.json'
    path.write_bytes(canonical(contract))
    with pytest.raises(ValueError): api.load_contract(path)


@pytest.mark.parametrize('source', SOURCES, ids=lambda p: p.stem)
def test_selectors_match_author_arrays_and_prepared_indexes(tmp_path, source):
    """相同选择器同时适配作者数组和运行索引，不因空可选表扩大所需能力。"""
    api = contract_api()
    pack = read_pack(source)
    output = tmp_path / 'ready.hmcatalog.json'
    prepare_pack(source, output)
    indexed = read_pack(output)['catalog']
    assert api.required_capabilities(pack) == api.required_capabilities(indexed)
    assert api.required_capabilities(pack) <= set(pack['required_capabilities'])


@pytest.mark.parametrize('field', ['battlefields', 'troop_types', 'military_objectives', 'offices', 'garrisons', 'military_orders', 'strongholds', 'siege_topologies'])
def test_military_table_selector_is_not_lost(field):
    """每种军事表独立触发声明；这里只测试选择器，不把最小数据当完整合法包。"""
    api = contract_api()
    assert 'military' not in api.required_capabilities({field: []})
    assert 'military' in api.required_capabilities({field: [{'id': 'fixture'}]})


@pytest.mark.parametrize('service', ['office_accept', 'office_resign', 'military_order', 'military_report'])
def test_military_service_selector_is_not_lost(service):
    """无军事表的军事服务仍需声明同一能力。"""
    assert 'military' in contract_api().required_capabilities({'roles': [{'services': [service]}]})


def test_runtime_manifest_is_reproducibly_generated_from_trusted_descriptor():
    """检入的兼容清单与构建生成逻辑一致，不能成为第二个可编辑真值。"""
    assert read_pack(ROOT / 'content/foundation/runtime_capabilities.json') == contract_api().runtime_declaration()


def test_contract_is_part_of_verification_identity():
    """改变规则描述必须改变验证身份，即使作者Schema没有变化。"""
    from tools.verification_report import schema_identity
    assert 'game/data/rule_capabilities.json' in {item['path'] for item in schema_identity(ROOT)['files']}


@pytest.mark.parametrize('capability_id', ['events', 'multi_actor'])
@pytest.mark.parametrize('mutation', ['object', 'string', 'null', 'missing'])
def test_supported_semantics_requires_explicit_array(tmp_path, capability_id, mutation):
    """空对象、空字符串、null及缺字段都不能悄悄移除能力的语义边界。"""
    api = contract_api()
    contract = api.load_contract(CONTRACT)
    entry = next(item for item in contract['capabilities'] if item['capability_id'] == capability_id)
    if mutation == 'missing':
        del entry['supported_semantics']
    else:
        entry['supported_semantics'] = {'object': {}, 'string': '', 'null': None}[mutation]
    path = tmp_path / 'malformed-contract.json'
    path.write_bytes(canonical(contract))
    with pytest.raises(ValueError, match='InvalidCapabilitySemantics'):
        api.load_contract(path)


def test_godot_rejects_malformed_contract_with_matching_digests(tmp_path):
    """在隔离真工程中重算两份摘要，证明坏描述不能放行尚未实现的on_tick。"""
    import os
    import shutil
    from tools.build import run_checked
    engine = os.environ.get('GODOT_BIN')
    if not engine:
        pytest.skip('需要项目固定Godot工具链运行真实目录边界')
    game = tmp_path / 'game'
    shutil.copytree(ROOT / 'game/core', game / 'core')
    generated = game / 'data/generated'
    generated.mkdir(parents=True)
    shutil.copyfile(ROOT / 'schemas/content.schema.json', generated / 'content.schema.json')
    (game / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Contract boundary fixture"\n')
    contract = contract_api().load_contract(CONTRACT)
    next(item for item in contract['capabilities'] if item['capability_id'] == 'events')['supported_semantics'] = {}
    contract_bytes = canonical(contract)
    (game / 'data/rule_capabilities.json').write_bytes(contract_bytes)
    catalog_path = tmp_path / 'probe.hmcatalog.json'
    prepare_pack(ROOT / 'content/foundation/two_stories.json', catalog_path)
    header = read_pack(catalog_path)
    header['catalog']['events']['event.letter']['trigger'] = 'on_tick'
    header['catalog_sha256'] = hashlib.sha256(canonical(header['catalog'])).hexdigest()
    header['rule_capabilities_sha256'] = hashlib.sha256(contract_bytes).hexdigest()
    (generated / 'probe.hmcatalog.json').write_bytes(canonical(header))
    (game / 'probe.gd').write_text('''## 隔离工程运行真实加载器，仅输出结构化拒绝结果。
extends SceneTree
const Catalog = preload("res://core/content_catalog.gd")
## 重新计算过合法摘要的内容仍必须接受描述结构核验。
func _init() -> void:
	print("CONTRACT_BOUNDARY_RESULT=" + JSON.stringify(Catalog.read_prepared("res://data/generated/probe.hmcatalog.json")))
	quit(0)
''')
    run_checked([engine, '--headless', '--path', str(game), '--editor', '--quit'], log=tmp_path / 'godot-import.log')
    output = run_checked([engine, '--headless', '--path', str(game), '--script', 'res://probe.gd'], log=tmp_path / 'godot-contract-boundary.log')
    result = json.loads(next(line.split('=', 1)[1] for line in output.splitlines() if line.startswith('CONTRACT_BOUNDARY_RESULT=')))
    assert result.get('ok') is False, result
    assert result.get('code') == 'InvalidCapabilitySemantics', result
