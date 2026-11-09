"""同源场景绑定样本约束作者Schema、Python语义和真实Godot公开入口。"""
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess

import pytest
from jsonschema import Draft202012Validator
from tools import presentation

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tests/fixtures/presentation'
CASES = json.loads((FIXTURES / 'scene-binding-cases.json').read_text())


def fixture():
    """保持旧静态场景字面量不变，变异只作用于独立副本。"""
    return json.loads((FIXTURES / 'layered-v2.json').read_text())


def mutate(data, patches):
    """共享有限JSON指针用例只替换或删除字段，不生成运行世界对象。"""
    data = copy.deepcopy(data)
    for patch in patches:
        parts = patch['path'].split('/')[1:]
        node = data
        for part in parts[:-1]:
            node = node[int(part)] if isinstance(node, list) else node[part]
        key = int(parts[-1]) if isinstance(node, list) else parts[-1]
        if patch['op'] == 'remove': del node[key]
        else: node[key] = copy.deepcopy(patch['value'])
    return data


@pytest.mark.parametrize('case', CASES['registry'], ids=lambda c: c['name'])
def test_shared_scene_binding_registry(case):
    """合法静态/绑定层通过，谓词越界和无能力热点拒绝并保留来源路径。"""
    errors = presentation.validate_registry(mutate(fixture(), case['patches']), ROOT / 'game')
    assert (not errors) == case['ok'], errors
    if case.get('code'):
        assert (errors[0]['code'], errors[0]['path']) == (case['code'], case['path'])


@pytest.mark.parametrize('case', CASES['selectors'], ids=lambda c: c['name'])
def test_shared_scene_selector_capabilities(case):
    """运行时公开能力校验对缺形状、未来命令和多余参数均明确失败。"""
    result = presentation.scene_selector_valid(case['selector'], case['target_ref'])
    assert result['ok'] == case['ok'], result
    if not case['ok']: assert result['code'] == case['code']


@pytest.mark.parametrize('case', CASES['references'], ids=lambda c: c['name'])
@pytest.mark.parametrize('dictionary_rows', [False, True])
def test_shared_scene_reference_scales(case, dictionary_rows):
    """源包数组与运行目录字典均按映射键精确匹配尺度，保留旧字段行为。"""
    record = case['record']
    rows = {record['id']: record} if dictionary_rows else [record]
    errors = presentation.validate_references(fixture(), {'places': rows})
    assert (not errors) == case['ok'], errors
    if not case['ok']: assert errors[0]['code'] == case['code']


def test_content_schema_accepts_only_scene_view_map():
    """作者场所映射独立有界，只接受当前三种尺度与已有ID词法。"""
    schema = json.loads((ROOT / 'schemas/content.schema.json').read_text())
    rule = schema['$defs']['place']['properties']['scene_template_ids']
    assert 'x-hanmo-presentation' not in rule
    assert all(child['x-hanmo-presentation'] == 'scene_templates' for child in rule['properties'].values())
    validator = Draft202012Validator(rule)
    assert validator.is_valid({'world': 'fixture.world', 'city': 'fixture.city', 'interior': 'fixture.room'})
    for value in [{'battle': 'fixture.world'}, {'city': ''}, {'city': 2}, {'city': 'x' * 81}, []]:
        assert not validator.is_valid(value)


def test_godot_shared_scene_binding_contract(tmp_path):
    """隔离工程复制真实生产校验器与同源JSON，只跑本合同及原子拒绝检查。"""
    binary = os.environ.get('GODOT_BIN')
    if not binary: pytest.skip('需要固定GODOT_BIN核对双端合同')
    assert subprocess.check_output([binary, '--version'], text=True).strip() == '4.7.2.stable.official.ed1daf0bf'
    game = tmp_path / 'game'
    game.mkdir()
    (game / 'project.godot').write_text('config_version=5\n')
    for path in ['ui/presentation_registry.gd', 'ui/presentation_contract.gd', 'core/strict_json.gd', 'core/content_shape.gd', 'data/presentation.schema.json', 'tests/test_scene_binding_contract.gd']:
        target = game / path
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / 'game' / path, target)
    (game / 'data/generated').mkdir()
    shutil.copyfile(ROOT / 'schemas/content.schema.json', game / 'data/generated/content.schema.json')
    shutil.copytree(ROOT / 'game/assets/art/presentation-fixtures', game / 'assets/art/presentation-fixtures', ignore=shutil.ignore_patterns('*.import', '*.md'))
    shutil.copytree(FIXTURES, tmp_path / 'tests/fixtures/presentation')
    imported = subprocess.run([binary, '--headless', '--path', str(game), '--editor', '--quit'], capture_output=True, text=True, timeout=45)
    assert imported.returncode == 0 and 'SCRIPT ERROR:' not in imported.stderr, imported.stdout + imported.stderr
    result = subprocess.run([binary, '--headless', '--path', str(game), '--script', 'res://tests/test_scene_binding_contract.gd'], capture_output=True, text=True, timeout=90)
    output = result.stdout + result.stderr
    (tmp_path / 'scene-binding-godot.log').write_text(output)
    assert result.returncode == 0 and 'SCRIPT ERROR:' not in output, output
    assert 'FAILURES=0' in output, output
