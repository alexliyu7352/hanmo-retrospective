"""只验证显式嵌套全局身份的包限定，不把对白节点或剧本覆盖变成新实体定义。"""
from copy import deepcopy
import json
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.content import pack_compose as compose
from tools.content import references
from tools.content.pack_errors import PackError

SCOPE = 'garrisons[*].units'
GLOBAL_ID = r'^(?:[a-z][a-z0-9_-]{0,31}::)?[a-z][a-z0-9_.-]{0,79}(?![\s\S])'


@pytest.fixture
def explicit_contract(monkeypatch):
    """在独有测试中加载中央已约定注解，生产Schema文件保持只读。"""
    schema = deepcopy(compose.SCHEMA)
    unit = schema['$defs']['garrison']['properties']['units']['items']
    unit['properties']['id'] = {'type': 'string', 'pattern': GLOBAL_ID,
                              'x-hanmo-global-definition': SCOPE}

    def declare_reference(rule):
        """仅给既有跨驻军单位引用加全局语义，不扩大其他local-ref。"""
        if isinstance(rule, dict):
            if rule.get('x-hanmo-local-ref') == SCOPE:
                rule.update(pattern=GLOBAL_ID, **{'x-hanmo-global-ref': SCOPE})
            for child in rule.values():
                declare_reference(child)
        elif isinstance(rule, list):
            for child in rule:
                declare_reference(child)

    declare_reference(schema)
    # 旧基线没有剧本初态；局部夹具只声明本片测试用的同形覆盖，不伪造完整初态验收。
    schema['$defs']['scenario']['properties']['initial'] = {
        'type': 'object', 'properties': {'garrisons': {'type': 'array', 'items': {
            'type': 'object', 'properties': {
                'id': {'$ref': '#/$defs/reference_garrisons'},
                'organization_id': {'$ref': '#/$defs/reference_organizations'},
                'units': {'type': 'array', 'items': deepcopy(unit)}},
            'required': ['id', 'organization_id', 'units'], 'additionalProperties': False}}},
        'required': ['garrisons'], 'additionalProperties': False}
    monkeypatch.setattr(references, 'SCHEMA', schema)
    monkeypatch.setattr(compose, 'SCHEMA', schema)
    monkeypatch.setattr(compose, 'PARTIAL_CONTENT_VALIDATOR',
                        compose.StrictValidator(compose._partial_content_schema()))
    return schema


def author_source():
    """使用真实Alpha驻军/计划/局部节点，仅为剧本增加同身份覆盖。"""
    source = json.loads((ROOT / 'content/foundation/alpha_life.json').read_text())
    for scenario in source['scenarios']:
        scenario['initial'] = {'garrisons': [
            {key: deepcopy(row[key]) for key in ('id', 'organization_id', 'units')}
            for row in source['garrisons']]}
    return source


def pack(namespace, dependencies=()):
    """构造资格检查需要的最小已发现身份，文件发现与资源读取不属于本单元测试。"""
    return {'manifest': {'id': namespace, 'namespace': namespace,
                         'dependencies': [{'id': dep} for dep in dependencies]}}


def qualify(source, namespace, selected=None, dependencies=()):
    """始终调用生产限定器，以保留真实字段遍历和直接依赖检查。"""
    current = pack(namespace, dependencies)
    return compose._qualify_content(source, current, 'content.json', selected or {namespace: current})


def unit_ids(source):
    """只取驻军定义的全局单位身份，不重复累计各剧本的覆盖。"""
    return [unit['id'] for camp in source['garrisons'] for unit in camp['units']]


def test_two_packages_and_snapshot_overrides_share_only_own_identity(explicit_contract):
    """不同包同局部单位ID分离，同包多个剧本覆盖保持同一限定身份和原数组顺序。"""
    original = author_source()
    before = deepcopy(original)
    results = {}
    for namespace in ('base', 'addon'):
        result, chains = qualify(original, namespace)
        results[namespace] = result
        assert unit_ids(result) == [namespace + '::' + ident for ident in unit_ids(original)]
        for index, scenario in enumerate(result['scenarios']):
            assert unit_ids(scenario['initial']) == unit_ids(result)
            assert scenario['initial']['garrisons'][0]['id'] == result['garrisons'][0]['id']
            assert chains[('scenarios', index, 'initial', 'garrisons', 0, 'units', 0, 'id')][-1] == unit_ids(result)[0]
        for old, actual in zip(original['actors'], result['actors']):
            if 'military_plan' in old:
                assert actual['military_plan']['unit_ids'] == [namespace + '::' + ident for ident in old['military_plan']['unit_ids']]
        for old, actual in zip(original['dialogues'], result['dialogues']):
            assert [row['id'] for row in actual['nodes']] == [row['id'] for row in old['nodes']]
        for table, field in [('siege_topologies', 'nodes'), ('battlefields', 'cells')]:
            for old, actual in zip(original.get(table, []), result.get(table, [])):
                assert [row.get('id') for row in actual.get(field, [])] == [row.get('id') for row in old.get(field, [])]
        for path, scope, value in references._iter_annotated(original, annotation='x-hanmo-local-ref'):
            if scope == SCOPE:
                continue
            actual = result
            for part in path:
                actual = actual[part]
            assert actual == value
        assert result['texts'] == {namespace + '::' + key: value for key, value in original['texts'].items()}
    assert not set(unit_ids(results['base'])) & set(unit_ids(results['addon']))
    assert original == before
    for namespace in reversed(('base', 'addon')):
        assert qualify(original, namespace)[0] == results[namespace]


@pytest.mark.parametrize('snapshot', [False, True])
def test_nested_definitions_cannot_arrive_prequalified(explicit_contract, snapshot):
    """即使运行Schema接受qualified，作者定义和初态覆盖仍只能写本包local80。"""
    source = author_source()
    target = source['scenarios'][0]['initial'] if snapshot else source
    target['garrisons'][0]['units'][0]['id'] = 'other::unit.stolen'
    with pytest.raises(PackError) as caught:
        qualify(source, 'base')
    assert caught.value.code == 'QualifiedPackDefinition'
    assert caught.value.details['path'].endswith('/units/0/id')


def test_global_unit_reference_requires_direct_dependency(explicit_contract):
    """新注解使用既有跨包依赖门，不因旧local-ref同时存在就跳过权限范围。"""
    source = author_source()
    actor = next(row for row in source['actors'] if 'military_plan' in row)
    actor['military_plan']['unit_ids'][0] = 'addon::unit.shared'
    with pytest.raises(PackError) as caught:
        qualify(source, 'base', {'base': pack('base'), 'addon': pack('addon')})
    assert caught.value.code == 'UndeclaredPackDependency'
    result, _ = qualify(source, 'base', {'base': pack('base'), 'addon': pack('addon')}, ('addon',))
    actual = next(row for row in result['actors'] if row['id'] == 'base::' + actor['id'])
    assert actual['military_plan']['unit_ids'][0] == 'addon::unit.shared'


def test_legacy_merge_preserves_original_unit_id_bytes(explicit_contract):
    """legacy继续走原ID合并分支，不调用新包限定器或把历史单位身份改写。"""
    original = json.loads((ROOT / 'content/foundation/alpha_life.json').read_text())
    entry = 'content/foundation/alpha_life.json'
    legacy = {'manifest': {'id': original['id'], 'namespace': 'alpha_life',
                          'legacy_source': 'alpha_life', 'required_capabilities': original['required_capabilities']},
              'content_paths': [entry], 'files': {entry: json.dumps(original).encode()}}
    merged, _ = compose._merge_content([legacy])
    assert sorted(unit_ids(merged)) == sorted(unit_ids(original))
    assert all('::' not in ident for ident in unit_ids(merged))
