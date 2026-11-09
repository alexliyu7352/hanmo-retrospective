"""真实候选接缝只作用隔离表现清单，保护作者语义、几何边界与生产资源。"""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tests/fixtures/presentation'


def helper():
    """加载专用准备器，不把测试装配逻辑放入生产类。"""
    spec = importlib.util.spec_from_file_location('real_art_slice', FIXTURES / 'prepare_vertical_slice.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_real_art_reuses_frozen_layers_and_keeps_authored_semantics():
    """恢复几何半身、双body叠画、年龄串衣或篡改候选几何均应被拒绝。"""
    spec = json.loads((FIXTURES / 'vertical_slice.json').read_text())
    source_spec = deepcopy(spec)
    art = spec['character_art']
    candidate_path = ROOT / art['manifest']
    candidate_bytes = candidate_path.read_bytes()
    candidate = json.loads(candidate_bytes)
    old = json.loads((ROOT / 'tests/fixtures/character_presentation/manifest.json').read_text())
    actual = helper().manifest(ROOT, spec)
    target = actual['character_profiles'][art['target_profile']]
    original = candidate['character_profiles'][art['source_profile']]
    assert spec == source_spec and candidate_path.read_bytes() == candidate_bytes
    assert target['age_variants'] == original['age_variants']
    assert target['age_domain'] == [8, 40] and target['child_max_age'] == 15
    assert target['battle_profile'] == old['character_profiles']['fixture.person']['battle_profile']
    assert target['identity_group'] == old['character_profiles']['fixture.person']['identity_group']
    attire = {row['id']: row['bindings'] for row in target['attire_sets']}
    assert attire['formal'] == attire['travel'] and attire['plain'] != attire['formal']
    for variant in target['age_variants']:
        assert len(variant['components']) == 1
        assert len(variant['feature_bindings']) == 2
        for binding in variant['feature_bindings']:
            assert set(binding['features']) == {'face_shape'}
            assert actual['components'][binding['component_id']]['slot'] == 'body'
        for rows in attire.values():
            bound = next(row for row in rows if row['body_basis'] == variant['body_basis'])
            assert len(bound['components']) == 1
            assert actual['components'][bound['components'][0]]['slot'] == 'garment'
    for component_id, component in candidate['components'].items():
        assert actual['components'][component_id] == component
        texture = candidate['portraits'][component['asset_id'].split('/', 1)[1]]['path']
        with Image.open(ROOT / 'game' / texture.removeprefix('res://')) as image:
            assert image.mode == 'RGBA' and list(image.size) == component['geometry']['source_size']
            bounds = image.getbbox()
            assert [bounds[0], bounds[1], bounds[2] - bounds[0], bounds[3] - bounds[1]] == component['geometry']['alpha_bounds']
            assert image.getextrema()[3][0] == 0
    assert actual['character_profiles']['fixture.dedicated.a'] == old['character_profiles']['fixture.dedicated.a']
    for scene in actual['scene_templates'].values():
        if scene['id'].startswith('slice.'):
            assert scene['ui_safe_areas'] == [[0, 0, 1, 1]]
            for layer in scene['layers']:
                if layer['kind'] == 'actor':
                    assert layer['extent'] == spec['actor_extent'] and layer['after'] == ['granary.1', 'granary.2']


def test_real_art_prepares_two_normal_author_configurations(tmp_path):
    """两作者配置真实保存重开prepare；fixed/generated、formal与完整特征不变。"""
    module = helper()
    spec = json.loads((FIXTURES / 'vertical_slice.json').read_text())
    production = ROOT / 'game/data/presentation.json'
    before = production.read_bytes()
    prepared = module.prepare(tmp_path / 'project')
    assert production.read_bytes() == before
    assert prepared['character_art']['manifest_sha256'] == module.digest(ROOT / spec['character_art']['manifest'])
    for variant, result in zip(spec['variants'], prepared['variants'], strict=True):
        assert result['id'] == variant['id'] and result['saved'] == 'AUTHOR_SAVED'
        assert result['reopened_equal'] and result['prepared'] == 'AUTHORING_ONLY'
        content = json.loads(Path(result['author_path']).read_text())
        catalog = json.loads(Path(result['catalog_path']).read_text())
        assert catalog['source_sha256'] == result['author_sha256']
        for actor_id, options in spec['actors'].items():
            actor = next(row for row in content['actors'] if row['id'] == actor_id)
            appearance = actor['appearance_definition']
            assert actor['presentation_profile_id'] == options['profile']
            assert actor['initial_age_years'] == options['age']
            expected = deepcopy(spec['base_features'])
            if 'generated_candidates' in options:
                assert appearance['kind'] == 'generated'
                assert appearance['candidates'] == {key: options['generated_candidates'].get(key, [value]) for key, value in expected.items()}
            else:
                assert appearance['kind'] == 'fixed'
                if actor_id == variant['actor_id']:
                    expected['face_shape'] = variant['face_shape']
                assert appearance['base_features'] == expected
            assert appearance['attire_style'] == (variant['attire_style'] if actor_id == variant['actor_id'] else 'plain')
