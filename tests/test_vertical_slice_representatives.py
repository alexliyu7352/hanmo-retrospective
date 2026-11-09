"""最小真实代表组仅改变隔离清单；同脸换衣和未覆盖年龄边界均保留可追溯证据。"""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/presentation/vertical_slice.json'


def helper():
    """复用正式作者准备器，避免另建可绕过校验的fixture装配路径。"""
    spec = importlib.util.spec_from_file_location('representative_slice', FIXTURE.with_name('prepare_vertical_slice.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_representatives_reuse_registered_pixels_and_preserve_structural_controls():
    """退回几何专绘/成人战斗/场景会失败；儿童不得借用成人战斗身体。"""
    spec = json.loads(FIXTURE.read_text())
    assets = spec['representative_art']
    actual = helper().manifest(ROOT, spec)
    production = json.loads((ROOT / 'game/data/presentation.json').read_text())
    dedicated = actual['character_profiles'][assets['dedicated']['profile']]
    assert dedicated['mode'] == 'handcrafted' and dedicated['age_domain'] == [16, 40]
    assert spec['actors']['actor.shen']['profile'] == dedicated['id']
    assert len(dedicated['overrides']) == 1
    assert dedicated['overrides'][0]['asset_id'] == 'portraits/sender'
    battle = actual['character_profiles']['fixture.battle']
    assert len(battle['overrides']) == 1 and battle['overrides'][0]['ages'] == [16, 100]
    assert battle['overrides'][0]['asset_id'] == 'portraits/battle_sword'
    assert battle['age_variants'][0]['proportion'] == 'child'
    for profile in (dedicated, battle):
        for binding in profile['overrides']:
            group, asset_id = binding['asset_id'].split('/')
            assert actual[group][asset_id] == production[group][asset_id]
            with Image.open(ROOT / 'game' / actual[group][asset_id]['path'].removeprefix('res://')) as image:
                bounds = image.getbbox()
                assert binding['geometry']['source_size'] == list(image.size)
                assert binding['geometry']['alpha_bounds'] == [bounds[0], bounds[1], bounds[2] - bounds[0], bounds[3] - bounds[1]]
    for scene in actual['scene_templates'].values():
        if not scene['id'].startswith('slice.'):
            continue
        background = scene['layers'][0]
        assert background['asset_id'] == assets['backgrounds'][scene['view_kind']]
        assert scene['logical_canvas'] == background['geometry']['source_size']
        group, asset_id = background['asset_id'].split('/')
        assert actual[group][asset_id] == production[group][asset_id]
        assert len([layer for layer in scene['layers'] if layer['kind'] == 'actor']) == 3
        assert len([layer for layer in scene['layers'] if layer['kind'] == 'hotspot']) == 2
    assert 'structural_granary' in assets['limits']
    assert 'child_battle_art' in assets['limits']


def test_author_clothing_pair_changes_only_attire_and_keeps_original_ab(tmp_path):
    """作者原始A/B保留；新增两配置除服装及必要模板ID外，完整作者内容相同。"""
    spec = json.loads(FIXTURE.read_text())
    variants = {row['id']: row for row in spec['variants'] + spec['representative_variants']}
    assert variants['a']['face_shape'] == 'round' and variants['a']['attire_style'] == 'plain'
    assert variants['b']['face_shape'] == 'angular' and variants['b']['attire_style'] == 'formal'
    assert spec['clothing_pair'] == ['cloth_plain', 'cloth_formal']
    left, right = (variants[key] for key in spec['clothing_pair'])
    assert left['actor_id'] == right['actor_id'] == 'actor.carter'
    assert left['face_shape'] == right['face_shape']
    assert left['actor_positions'] == right['actor_positions']
    prepared = helper().prepare(tmp_path / 'project', representatives=True)
    results = {row['id']: row for row in prepared['variants']}
    authored = []
    for key in spec['clothing_pair']:
        row = results[key]
        assert row['saved'] == 'AUTHOR_SAVED' and row['reopened_equal'] and row['prepared'] == 'AUTHORING_ONLY'
        source = json.loads(Path(row['author_path']).read_text())
        catalog = json.loads(Path(row['catalog_path']).read_text())
        assert catalog['source_sha256'] == row['author_sha256']
        authored.append(source)
    before, after = authored
    first = next(row for row in before['actors'] if row['id'] == left['actor_id'])['appearance_definition']
    second = next(row for row in after['actors'] if row['id'] == right['actor_id'])['appearance_definition']
    assert first['base_features'] == second['base_features']
    assert first['attire_style'] == 'plain' and second['attire_style'] == 'formal'
    normalized = deepcopy(after)
    next(row for row in normalized['actors'] if row['id'] == right['actor_id'])['appearance_definition']['attire_style'] = 'plain'
    normalized = json.loads(json.dumps(normalized).replace('slice.cloth_formal.', 'slice.cloth_plain.'))
    assert normalized == before
    assert prepared['age_transition']['status'] == 'NOT_RUN'
    age = prepared['age_boundary']
    assert age['saved'] == 'AUTHOR_SAVED' and age['reopened_equal'] and age['prepared'] == 'AUTHORING_ONLY'
    source = json.loads(Path(age['author_path']).read_text())
    player = next(row for row in source['actors'] if row['id'] == 'actor.player')
    assert player['initial_age_years'] == 15 and len(source['actors']) == 4
    assert player['appearance_definition']['base_features'] == spec['base_features']


def test_default_prepare_still_emits_only_ab(tmp_path):
    """未显式请求代表测试时保持旧两配置入口，避免破坏现有严格回执门。"""
    prepared = helper().prepare(tmp_path / 'project')
    assert [row['id'] for row in prepared['variants']] == ['a', 'b']
    assert prepared['clothing_pair'] == []
