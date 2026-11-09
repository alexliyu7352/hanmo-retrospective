"""从既有分层夹具运行时生成语义绑定边界，两个校验端共用全部候选。"""
import copy
import json
import os
from pathlib import Path
import subprocess

import pytest

from tools import presentation

ROOT = Path(__file__).resolve().parents[1]
PROFILE = '/character_profiles/fixture.person'
VARIANT = PROFILE + '/age_variants/0'
ATTIRE = PROFILE + '/attire_sets/0/bindings/0'
FEATURES = ('face_shape', 'eyes', 'brows', 'nose', 'mouth', 'hair', 'beard', 'skin_palette', 'build')


def fixture():
    """只加载原有完整清单；新增图层都是本次候选中的运行时副本。"""
    return json.loads((ROOT / 'tests/fixtures/presentation/layered-v2.json').read_text())


def variant(data):
    """定位日常儿童变体，不更改同夹具的战斗或专绘方案。"""
    return data['character_profiles']['fixture.person']['age_variants'][0]


def component(data, id, slot='body'):
    """复用真实儿童PNG和几何；测试只改变注册组件身份与解剖槽。"""
    value = copy.deepcopy(data['components']['child.body'])
    value.update(id=id, slot=slot)
    data['components'][id] = value
    return value


def bind(data, id='bound.body', features=None, slot='body', binding_id='choice'):
    """登记替换项和语义条件；不隐式添加被替换的默认槽。"""
    value = component(data, id, slot)
    variant(data).setdefault('feature_bindings', []).append({
        'id': binding_id, 'features': {'build': 'stocky'} if features is None else features, 'component_id': id})
    return value


def combinatorial_fixture(options):
    """按每槽候选数构造互斥条件，证明上限基于组合而非绑定条目数。"""
    data = fixture()
    for slot, count in zip(('body', 'face', 'hair', 'beard'), options):
        if slot != 'body':
            component(data, 'default.' + slot, slot)
            variant(data)['components'].append('default.' + slot)
        for n in range(count - 1):
            bind(data, f'bound.{slot}.{n}', {slot if slot in ('hair', 'beard') else 'build': f'value{n}'}, slot, f'{slot}.{n}')
    return data


def cases():
    """覆盖协议形状、互斥、现有槽、完整组合及复杂度上限，保留两端稳定诊断。"""
    results = []

    def add(name, data, code='', path='', ok=False):
        """记录独立候选及预期；空错误码只要求两端均拒绝形状错误。"""
        results.append(dict(name=name, data=data, ok=ok, code=code, path=path))

    data = fixture(); add('v01_unmodified', data, ok=True)
    data = fixture(); variant(data)['feature_bindings'] = []; add('empty_composite', data, ok=True)
    for slot in ('body', 'face', 'hair', 'beard'):
        data = fixture()
        if slot != 'body':
            component(data, 'default.' + slot, slot); variant(data)['components'].append('default.' + slot)
        bind(data, slot=slot); add('replace_' + slot, data, ok=True)
    for feature in FEATURES:
        data = fixture(); bind(data, features={feature: 'round'}); add('feature_' + feature, data, ok=True)
    data = fixture(); bind(data, features={feature: 'a0._-' for feature in FEATURES}); add('all_nine_features', data, ok=True)
    data = fixture(); bind(data, features={'build': 'a' * 64}); add('token_maximum', data, ok=True)
    data = fixture(); bind(data, features={'build': 'a'}); add('token_minimum', data, ok=True)
    for name, features in [('empty', {}), ('unknown', {'weight': 'round'}), ('uppercase', {'build': 'Round'}),
                           ('path', {'hair': 'res://hair'}), ('long', {'build': 'a' * 65}),
                           ('newline', {'build': 'round\n'}), ('unicode', {'build': '圆'}),
                           ('leading_digit', {'build': '1round'}), ('numeric', {'build': 1}),
                           ('empty_token', {'build': ''}), ('non_object', ['round'])]:
        data = fixture(); bind(data, features=features); add('shape_' + name, data)
    for field in ('id', 'features', 'component_id'):
        data = fixture(); bind(data); del variant(data)['feature_bindings'][0][field]; add('missing_' + field, data)
    data = fixture(); bind(data); variant(data)['feature_bindings'][0]['slot'] = 'body'; add('explicit_slot_rejected', data)
    data = fixture(); variant(data)['feature_bindings'] = {}; add('binding_list_type', data)
    data = fixture(); bind(data); variant(data)['feature_bindings'][0]['component_id'] = 'missing'; add('missing_component', data, 'MissingComponent', VARIANT + '/feature_bindings/0/component_id')
    for slot in ('garment', 'headwear', 'accessory', 'mask'):
        data = fixture(); bind(data, slot=slot); add('forbidden_' + slot, data, 'AnatomySlot', VARIANT + '/feature_bindings/0/component_id')
    data = fixture(); bind(data, slot='face'); add('missing_default_slot', data, 'FeatureBindingSlot', VARIANT + '/feature_bindings/0/component_id')
    data = fixture(); bind(data); component(data, 'other.body'); variant(data)['components'].append('other.body'); add('duplicate_default_slot', data, 'DuplicateSlot', VARIANT + '/components')
    data = fixture(); bind(data); bind(data, 'bound.other', {'build': 'slim'}); add('duplicate_binding_id', data, 'RecordIdentity', VARIANT + '/feature_bindings/1/id')
    for name, left, right in [('same', {'build': 'stocky'}, {'build': 'stocky'}),
                              ('disjoint_keys', {'build': 'stocky'}, {'eyes': 'narrow'}),
                              ('subset', {'build': 'stocky'}, {'build': 'stocky', 'eyes': 'narrow'})]:
        data = fixture(); bind(data, features=left); bind(data, 'bound.other', right, binding_id='other'); add('overlap_' + name, data, 'FeatureBindingOverlap', VARIANT + '/feature_bindings/1/features')
    data = fixture(); bind(data); bind(data, 'bound.other', {'build': 'slim'}, binding_id='other'); add('same_slot_exclusive', data, ok=True)
    data = fixture(); bind(data, features={'build': 'stocky', 'eyes': 'narrow'}); bind(data, 'bound.other', {'build': 'stocky', 'eyes': 'wide'}, binding_id='other'); add('conjunction_exclusive', data, ok=True)
    data = fixture(); bind(data); component(data, 'default.hair', 'hair'); variant(data)['components'].append('default.hair'); bind(data, 'bound.hair', {'build': 'stocky'}, 'hair', 'other'); add('different_slots_overlap_allowed', data, ok=True)
    data = fixture(); bind(data); grown = data['character_profiles']['fixture.person']['age_variants'][1]; grown['feature_bindings'] = [{'id': 'choice', 'features': {'build': 'stocky'}, 'component_id': 'adult.body'}]; add('ids_are_variant_local', data, ok=True)
    for bindings in ([], [{'id': 'choice', 'features': {'build': 'stocky'}, 'component_id': 'child.body'}]):
        data = fixture(); data['character_profiles']['fixture.dedicated']['age_variants'][0]['feature_bindings'] = bindings; add('handcrafted_' + str(len(bindings)), data, 'HandcraftedComposition', '/character_profiles/fixture.dedicated/age_variants/0/feature_bindings')
    for key, value, code in [('body_basis', 'other', 'IncompatibleBody'), ('proportion', 'adult', 'IncompatibleBody'),
                              ('incompatible_tags', ['child'], 'IncompatibleTags'), ('compatible_tags', ['missing'], 'IncompatibleTags'),
                              ('after', ['adult.body'], 'MissingCompositionDependency')]:
        data = fixture(); value_component = bind(data); value_component['compatible_tags'] = []; value_component[key] = value; add('candidate_' + key, data, code, ATTIRE)
    for key, value in [('view', 'avatar'), ('pose', 'walk'), ('facing', 'left'), ('logical_canvas', [9, 9])]:
        data = fixture(); bind(data)['geometry'][key] = value; add('candidate_geometry_' + key, data, 'IncompatibleGeometry', ATTIRE)
    data = fixture(); bind(data)['geometry']['source_size'] = [9, 9]; add('candidate_pixels', data, 'SourceSize', '/components/bound.body/geometry/source_size')
    data = fixture(); bind(data); data['components']['child.garment']['after'] = ['child.body']; add('dependency_on_replaced_default', data, 'MissingCompositionDependency', ATTIRE)
    data = fixture(); bind(data); data['components']['child.body']['compatible_tags'] = ['absent']; add('default_still_validated', data, 'IncompatibleTags', ATTIRE)
    data = combinatorial_fixture([2, 1, 2, 1]); data['components']['bound.body.0']['incompatible_tags'] = ['bound_hair']; data['components']['bound.hair.0']['tags'].append('bound_hair'); add('cross_slot_combination_invalid', data, 'IncompatibleTags', ATTIRE)
    data = fixture(); bind(data)['tags'].append('bound_body'); other = copy.deepcopy(data['character_profiles']['fixture.person']['attire_sets'][0]); other['id'] = 'other'; other['bindings'][0]['components'] = ['other.garment']; data['character_profiles']['fixture.person']['attire_sets'].append(other); component(data, 'other.garment', 'garment')['incompatible_tags'] = ['bound_body']; add('every_attire_validated', data, 'IncompatibleTags', PROFILE + '/attire_sets/1/bindings/0')
    data = fixture(); bind(data)['geometry']['mask_ids'] = ['child.mask']; component(data, 'child.mask', 'mask'); add('unselected_mask_dependency', data, 'MissingCompositionDependency', ATTIRE)
    data = fixture(); bind(data)['geometry']['mask_ids'] = ['child.mask']; component(data, 'child.mask', 'mask'); variant(data)['components'].append('child.mask'); add('selected_mask_preserved', data, ok=True)
    data = fixture(); bind(data)['after'] = ['bound.body']; add('bound_component_cycle', data, 'CompositionCycle', ATTIRE)
    data = combinatorial_fixture([4, 4, 4, 4]); add('exactly_256', data, ok=True)
    data = combinatorial_fixture([5, 4, 4, 4]); add('over_256', data, 'FeatureCombinationLimit', VARIANT + '/feature_bindings')
    data = fixture()
    for n in range(64): bind(data, 'child.body', {'build': 'value' + str(n)}, binding_id='choice' + str(n))
    add('reused_component_deduplicated', data, ok=True)
    data = copy.deepcopy(data); variant(data)['feature_bindings'].append({'id': 'overflow', 'features': {'build': 'overflow'}, 'component_id': 'child.body'}); add('over_64_bindings', data)
    return results


@pytest.mark.parametrize('case', cases(), ids=lambda value: value['name'])
def test_feature_bindings_python(case):
    """每个候选保持源形状，拒绝时对可稳定语义诊断核对码和JSON指针。"""
    before = copy.deepcopy(case['data'])
    issues = presentation.validate_registry(case['data'], ROOT / 'game')
    assert bool(not issues) == case['ok'], issues
    if case['code']: assert (issues[0]['code'], issues[0]['path']) == (case['code'], case['path']), issues
    assert case['data'] == before


def test_feature_bindings_godot_shared_cases(tmp_path):
    """固定真实Godot读取同一批运行时候选，验证公开入口、原子快照与跨语言语义一致。"""
    engine = os.environ.get('GODOT_BIN')
    if not engine: pytest.skip('需要明确固定GODOT_BIN执行分层语义绑定两端校验')
    assert subprocess.check_output([engine, '--version'], text=True).strip() == '4.7.2.stable.official.ed1daf0bf'
    source = tmp_path / 'feature-cases.json'; source.write_text(json.dumps(cases()))
    result = subprocess.run([engine, '--headless', '--audio-driver', 'Dummy', '--path', str(ROOT / 'game'),
                             '--script', 'res://tests/test_feature_bindings.gd', '--', str(source)],
                            capture_output=True, text=True, timeout=120)
    (tmp_path / 'feature-bindings-godot.log').write_text(result.stdout + result.stderr)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'SCRIPT ERROR:' not in result.stderr, result.stderr
    assert 'FEATURE_BINDING_CHECKS=' in result.stdout and 'FAILURES=0' in result.stdout
