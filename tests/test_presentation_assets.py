"""检查表现清单与内容扩展，不运行第二套游戏规则。"""
import copy
import importlib.util
import json
from pathlib import Path
import pytest

ROOT = Path(__file__).resolve().parents[1]


def helper():
    """载入真实发布检查工具；缺失时明确失败，避免空测试通过。"""
    path = ROOT / 'tools/presentation.py'
    assert path.is_file(), '缺少表现资源检查工具'
    spec = importlib.util.spec_from_file_location('hanmo_presentation', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def manifest():
    """读取实际运行清单，测试副本不覆盖用户资源。"""
    return json.loads((ROOT / 'game/data/presentation.json').read_text())


def test_registry_and_pack_are_valid():
    """现有三份作者内容均须通过同一套表现检查。"""
    h = helper(); data = manifest()
    assert h.validate_registry(data, ROOT / 'game') == []
    for source in (ROOT / 'content/foundation').glob('*.json'):
        pack = json.loads(source.read_text())
        assert h.validate_references(data, pack) == [], source.name


@pytest.mark.parametrize('mutation', ['missing', 'outside', 'default', 'coordinate', 'script'])
def test_bad_registry_is_rejected(mutation):
    """检查路径、缺件、默认ID及坐标，发布前必须报出错误。"""
    h = helper(); data = manifest()
    if mutation == 'missing': data['portraits']['merchant']['path'] = 'res://assets/art/missing.png'
    if mutation == 'outside': data['portraits']['merchant']['path'] = '/etc/passwd'
    if mutation == 'default': data['default_background'] = 'missing'
    if mutation == 'coordinate': data['backgrounds']['hall']['entrance'] = [500, 2]
    if mutation == 'script': data['portraits']['merchant']['path'] = 'res://ui/main.gd'
    assert h.validate_registry(data, ROOT / 'game')


def test_new_art_id_does_not_require_a_schema_enum():
    """新增书斋/学者资源ID应当由数据描述，而不是修改运行代码。"""
    from tools.content.validation import validate_pack
    data = manifest(); data['backgrounds']['study'] = copy.deepcopy(data['backgrounds']['hall'])
    data['portraits']['scholar'] = copy.deepcopy(data['portraits']['official'])
    pack = json.loads((ROOT / 'content/foundation/lived_city.json').read_text())
    next(p for p in pack['places'] if p['id'] == 'west.office')['visual_tag'] = 'study'
    next(p for p in pack['actors'] if p['id'] == 'actor.chen')['portrait_kind'] = 'scholar'
    assert validate_pack(pack).ok, '内容schema仍将表现ID写死为枚举'
    assert helper().validate_references(data, pack) == []
    pack['actors'][0]['portrait_kind'] = 'not_registered'
    issues = helper().validate_references(data, pack)
    assert issues and 'actor.player' in issues[0]['path']


def test_typed_reference_defs_preserve_nested_scene_and_actor_fields():
    """表现引用提到defs后，普通字段和按场景尺度映射的子字段仍须严格校验。"""
    specs = set(helper().reference_specs())
    assert ('actors', 'portrait_kind', 'portraits', 'string') in specs
    assert ('actors', 'presentation_profile_id', 'character_profiles', 'string') in specs
    assert ('places', 'scene_template_ids', 'scene_templates', 'object') in specs
    issues = helper().validate_references({'scene_templates': {}}, {
        'places': [{'id': 'fixture.place', 'scene_template_ids': {'interior': 'missing.template'}}]})
    assert issues and issues[0]['path'] == '/places/fixture.place/scene_template_ids/interior'


def test_namespaced_registry_uses_real_existing_asset_bytes():
    """仅限定资源身份而不改路径或原图，真实注册器仍可解码并检查默认引用。"""
    data = manifest()
    for group, field in [('backgrounds', 'default_background'), ('portraits', 'default_portrait')]:
        data[field] = 'sample::' + data[field]
        data[group] = {'sample::' + key: value for key, value in data[group].items()}
    assert helper().validate_registry(data, ROOT / 'game') == []


def test_presentation_global_references_do_not_qualify_local_age_or_layer_ids():
    """全局组件和profile可限定，局部年龄、服装和场景层ID保持原范围。"""
    from jsonschema import Draft202012Validator
    schema = json.loads((ROOT / 'game/data/presentation.schema.json').read_text())
    defs = schema['$defs']
    for rule in [defs['component']['properties']['id'], defs['profile']['properties']['id'], defs['asset_ref']]:
        value = 'portraits/sample::face' if rule is defs['asset_ref'] else 'sample::item'
        assert Draft202012Validator({'$defs': defs, **rule}).is_valid(value)
    for name in ['variant', 'attire', 'actor_layer', 'image_layer', 'hotspot_layer']:
        assert not Draft202012Validator(defs[name]['properties']['id']).is_valid('sample::local')
    for rule in [defs['component']['properties']['after']['items'], defs['geometry']['properties']['mask_ids']['items']]:
        assert defs[rule['$ref'].split('/')[-1]]['x-hanmo-ref'] == 'components'
