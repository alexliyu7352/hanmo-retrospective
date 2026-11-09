"""同一真实资源与变异样本覆盖分层声明、正式准备和拒绝边界。"""
import copy
import json
from pathlib import Path
import pytest
from tools import presentation

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tests/fixtures/presentation'


def fixture(name='layered-v2.json'):
    """读取跨语言共用的字面量清单，不动态维护第二份协议。"""
    return json.loads((FIXTURES / name).read_text())


def mutate(data, patches):
    """按样本JSON指针对独立候选应用有限替换或删除。"""
    data = copy.deepcopy(data)
    for patch in patches:
        parts = [p.replace('~1', '/').replace('~0', '~') for p in patch['path'].split('/')[1:]]
        node = data
        for part in parts[:-1]: node = node[int(part)] if isinstance(node, list) else node[part]
        key = int(parts[-1]) if isinstance(node, list) else parts[-1]
        if patch['op'] == 'remove': del node[key]
        else: node[key] = copy.deepcopy(patch['value'])
    return data


def test_v1_registry_remains_valid():
    """旧源快照及其默认项不需要添加任何分层字段。"""
    data = fixture('legacy-v1.json'); before = copy.deepcopy(data)
    assert presentation.validate_registry(data, ROOT / 'game') == []
    assert data == before


def test_candidate_local_layered_registry_is_valid():
    """组件与引用它的多个方案同一候选提交，不依赖先前生效清单。"""
    data = fixture(); before = copy.deepcopy(data)
    assert presentation.validate_registry(data, ROOT / 'game') == []
    assert data == before


@pytest.mark.parametrize('case', fixture('mutations.json'), ids=lambda c: c['name'])
def test_shared_mutations(case):
    """拒绝跨语言共用的坏候选，并核对语义码和可定位字段路径。"""
    issues = presentation.validate_registry(mutate(fixture(), case['patches']), ROOT / 'game')
    assert issues, case['name']
    if case['code']:
        assert (issues[0]['code'], issues[0]['path']) == (case['code'], case['path']), issues


def test_component_choice_is_not_a_screen_whitelist():
    """两套新方案与场景由Schema注解和数据引用，不增界面资源枚举。"""
    from tools.content.validation import validate_pack, SCHEMA
    data = fixture(); legacy = fixture('legacy-v1.json')
    for group in ('backgrounds', 'portraits'): data[group].update(legacy[group])
    pack = json.loads((ROOT / 'content/foundation/lived_city.json').read_text())
    for id in ['fixture.person', 'fixture.second']:
        pack['actors'][0]['presentation_profile_id'] = id
        pack['places'][0]['scene_template_id'] = 'fixture.room'
        assert validate_pack(pack).ok
        assert presentation.validate_references(data, pack) == []
    assert SCHEMA['$defs']['actor']['properties']['presentation_profile_id']['x-hanmo-presentation'] == 'character_profiles'
    pack['actors'][0]['presentation_profile_id'] = ''
    assert presentation.validate_references(data, pack)


def test_prepare_checks_real_manifest_before_replacing_output(tmp_path, monkeypatch):
    """缺失的必需表现引用在正式prepare替换前拒绝，旧文件字节必须保持。"""
    from tools.content.io import prepare_pack
    pack = json.loads((ROOT / 'content/foundation/lived_city.json').read_text())
    pack['actors'][0]['presentation_profile_id'] = 'does.not.exist'
    source = tmp_path / 'draft.json'; source.write_text(json.dumps(pack))
    output = tmp_path / 'draft.hmcatalog.json'; output.write_bytes(b'old valid generation')
    with pytest.raises(ValueError, match='presentation|表现'):
        prepare_pack(source, output)
    assert output.read_bytes() == b'old valid generation'


def test_actual_invalid_reference_draft_save_reopen_and_prepare(tmp_path):
    """实际HM-CONTENT草稿保存报告无效，重新打开后正式prepare仍保留旧目录。"""
    from tools.content.document import AuthorDocument, DocumentError
    source = tmp_path / 'draft.json'; source.write_bytes((ROOT / 'content/foundation/lived_city.json').read_bytes())
    document = AuthorDocument.open(source)
    document.add_field('actors', 'actor.player', 'presentation_profile_id', 'missing')
    result = document.save()
    assert result['content_valid'] is False
    reopened = AuthorDocument.open(source)
    assert reopened.snapshot()['actors'][0]['presentation_profile_id'] == 'missing'
    assert any(i['code'] == 'UnknownPresentation' for i in reopened.validate()['issues'])
    output = tmp_path / 'draft.hmcatalog.json'; output.write_bytes(b'previous')
    with pytest.raises(DocumentError, match='Repair'):
        reopened.prepare(output)
    assert output.read_bytes() == b'previous'


def svg_game(tmp_path, text):
    """建立只含一个实际SVG资源的旧清单工程，避免生产清单混入测试图。"""
    import shutil
    game = tmp_path / 'game'; (game / 'data').mkdir(parents=True); (game / 'assets/art').mkdir(parents=True)
    shutil.copyfile(ROOT / 'game/data/presentation.schema.json', game / 'data/presentation.schema.json')
    (game / 'project.godot').write_text('config_version=5\n')
    (game / 'assets/art/legacy.svg').write_text(text)
    entry = {'path': 'res://assets/art/legacy.svg'}
    return game, dict(version=1, default_background='legacy', default_portrait='legacy', backgrounds={'legacy': entry}, portraits={'legacy': entry})


def test_legacy_svg_requires_real_fixed_decoder(tmp_path, monkeypatch):
    """合法旧SVG沿固定Godot解码，坏字节和不可用解码器都不能伪装成功。"""
    import os
    if not os.environ.get('GODOT_BIN'): pytest.skip('真实SVG解码测试需要明确固定GODOT_BIN')
    game, data = svg_game(tmp_path, '<svg xmlns="http://www.w3.org/2000/svg" width="8" height="8"><rect width="8" height="8" fill="red"/></svg>')
    assert presentation.validate_registry(data, game) == []
    (game / 'assets/art/legacy.svg').write_text('not an svg')
    assert presentation.validate_registry(data, game)[0]['code'] == 'AssetDecode'
    (game / 'assets/art/legacy.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="8" height="8"><rect width="8" height="8"/></svg>')
    monkeypatch.setenv('GODOT_BIN', str(tmp_path / 'missing-godot'))
    assert presentation.validate_registry(data, game)[0]['code'] == 'SVGDecoderUnavailable'


@pytest.mark.parametrize('case', fixture('raw-cases.json'), ids=lambda c: c['name'])
def test_shared_raw_input_limits(tmp_path, case):
    """两端同样读取重复键、非有限数、超深及超大真实字节输入。"""
    path = tmp_path / 'candidate.json'; path.write_text(case.get('raw', ' ' * case.get('repeat', 0)))
    with pytest.raises((ValueError, RecursionError)):
        presentation.read_registry(path)


@pytest.mark.parametrize('kind', ['fake', 'truncated', 'symlink', 'transparent', 'huge'])
def test_actual_resource_failures(tmp_path, kind):
    """实际图像解码及符号链接边界不得被文件存在或PNG头部检查代替。"""
    import shutil
    from PIL import Image
    game = tmp_path / 'game'; (game / 'data').mkdir(parents=True)
    shutil.copyfile(ROOT / 'game/data/presentation.schema.json', game / 'data/presentation.schema.json')
    shutil.copytree(ROOT / 'game/assets/art/presentation-fixtures', game / 'assets/art/presentation-fixtures')
    data = fixture(); path = game / 'assets/art/presentation-fixtures/child-body.png'
    if kind == 'fake': path.write_bytes(b'not a PNG')
    elif kind == 'truncated': path.write_bytes(path.read_bytes()[:45])
    elif kind == 'transparent': Image.new('RGBA', (8, 8)).save(path)
    elif kind == 'huge':
        import struct, zlib
        raw = bytearray(path.read_bytes()); raw[16:24] = struct.pack('>II', 100000, 100000)
        raw[29:33] = struct.pack('>I', zlib.crc32(raw[12:29])); path.write_bytes(raw)
    else:
        outside = tmp_path / 'outside.png'; shutil.copyfile(path, outside); path.unlink(); path.symlink_to(outside)
    issues = presentation.validate_registry(data, game)
    assert issues
    assert issues[0]['code'] == {'fake': 'AssetDecode', 'truncated': 'AssetDecode', 'symlink': 'AssetPath', 'transparent': 'AlphaBounds', 'huge': 'AssetTooLarge'}[kind]


def test_in_memory_inputs_bounded_before_schema():
    """直接API输入也拒绝非JSON值、非有限数和超过总字节上限的候选。"""
    for value in [float('nan'), float('inf'), object(), 'x' * 1048577]:
        data = fixture(); data['unsupported'] = value
        assert presentation.validate_registry(data, ROOT / 'game')
    data = fixture()
    for n in range(100): data['portraits']['long' + str(n)] = {'path': 'x' * 12000}
    assert presentation.validate_registry(data, ROOT / 'game')[0]['code'] == 'ManifestTooLarge'


def test_real_cli_rejects_broken_registry_before_replace(tmp_path):
    """隔离工程的直接CLI读取本工程坏清单，不能偷用当前工作目录里的有效清单。"""
    import subprocess, sys, importlib.util
    spec = importlib.util.spec_from_file_location('presentation_editor_fixture', FIXTURES / 'editor_project.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    project = tmp_path / 'project'; module.create(project)
    manifest = project / 'game/data/presentation.json'; valid = json.loads(manifest.read_text())
    output = tmp_path / 'old.hmcatalog.json'; output.write_bytes(b'old')
    for raw in [json.dumps(mutate(valid, [{'op': 'remove', 'path': '/components/child.body'}])), '{bad']:
        manifest.write_text(raw)
        result = subprocess.run([sys.executable, str(project / 'tools/content_pipeline.py'), 'prepare', str(project / 'content/foundation/lived_city.json'), '--output', str(output)], cwd=ROOT, capture_output=True, text=True)
        assert result.returncode != 0
        assert 'presentation' in result.stdout + result.stderr
        assert output.read_bytes() == b'old'


@pytest.mark.parametrize('case', fixture('positive-mutations.json'), ids=lambda c: c['name'])
def test_shared_additional_valid_cases(case):
    """合法遮罩组合与多个同类型遮罩按数据选择，不因负向检查全部拒绝。"""
    assert presentation.validate_registry(mutate(fixture(), case['patches']), ROOT / 'game') == []


def test_scene_reference_scale_and_schema_vocabulary():
    """场景尺度与注解一致，发布Schema没有Godot未实现的组合关键字。"""
    data = fixture()
    for table, allowed, denied in [('places', 'fixture.room', 'fixture.world'), ('world_maps', 'fixture.world', 'fixture.city')]:
        assert presentation.validate_references(data, {table: [{'id': 'location', 'scene_template_id': allowed}]}) == []
        assert presentation.validate_references(data, {table: [{'id': 'location', 'scene_template_id': denied}]})[0]['code'] == 'SceneScale'
    schema = json.loads((ROOT / 'game/data/presentation.schema.json').read_text())
    supported = {'$schema', '$defs', '$ref', 'type', 'required', 'additionalProperties', 'properties', 'const', 'enum', 'propertyNames', 'minProperties', 'maxProperties', 'minimum', 'maximum', 'minLength', 'maxLength', 'pattern', 'minItems', 'maxItems', 'items', 'uniqueItems', 'oneOf', 'anyOf', 'x-hanmo-ref'}
    todo = [schema]
    while todo:
        rule = todo.pop(); assert not set(rule) - supported
        if '$ref' in rule: assert rule['$ref'].startswith('#/$defs/')
        for key in ('$defs', 'properties'): todo.extend(rule.get(key, {}).values())
        for key in ('items', 'additionalProperties', 'propertyNames'):
            if isinstance(rule.get(key), dict): todo.append(rule[key])
        for key in ('oneOf', 'anyOf'): todo.extend(rule.get(key, []))


def test_svg_probe_does_not_execute_project_autoload(tmp_path):
    """旧SVG探针不能顺带启动被检查工程的自动加载脚本。"""
    import os
    if not os.environ.get('GODOT_BIN'): pytest.skip('需要固定Godot证明SVG探针隔离')
    game, data = svg_game(tmp_path, '<svg xmlns="http://www.w3.org/2000/svg" width="8" height="8"><rect width="8" height="8"/></svg>')
    marker = tmp_path / 'autoload-ran'
    (game / 'autoload_probe.gd').write_text('extends Node\nfunc _enter_tree():\n var f=FileAccess.open(' + json.dumps(str(marker)) + ',FileAccess.WRITE)\n f.store_string("ran")\n')
    (game / 'project.godot').write_text('config_version=5\n[autoload]\nProbe="*res://autoload_probe.gd"\n')
    assert presentation.validate_registry(data, game) == []
    assert not marker.exists()


@pytest.mark.parametrize('sign', [1, -1], ids=['positive', 'negative'])
def test_large_in_memory_integer_is_a_controlled_diagnostic(sign):
    """仅数百字节可表示的任意精度整数也必须返回稳定码，不能泄漏浮点溢出。"""
    assert presentation.validate_registry({'version': sign * 10 ** 400}, ROOT / 'game') == [presentation.issue('', 'InvalidNumber')]


@pytest.mark.parametrize('sign', ['', '-'], ids=['positive', 'negative'])
def test_large_manifest_integer_preserves_actual_draft_and_output(tmp_path, sign):
    """坏清单真实文件不能阻断HM-CONTENT草稿保存，也不能替换此前正式准备的目录。"""
    import importlib.util
    import subprocess
    import sys
    spec = importlib.util.spec_from_file_location('large_manifest_fixture', FIXTURES / 'editor_project.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    project = tmp_path / 'project'; module.create(project)
    code = '''
import json
from pathlib import Path
from tools.content.document import AuthorDocument, DocumentError
from tools.content.io import prepare_pack
from tools.presentation import read_registry, validate_project_pack
source = Path('content/foundation/lived_city.json')
output = Path('build/previous.hmcatalog.json')
prepare_pack(source, output)
previous = output.read_bytes()
manifest = Path('game/data/presentation.json')
manifest.write_text('{"version":' + SIGN + '1' + '0' * 400 + '}')
document = AuthorDocument.open(source)
document.set_text('name.actor.player', '坏清单下仍保存的作者修改')
saved = document.save()
assert saved['status'] == 'AUTHOR_SAVED' and saved['content_valid'] is False
assert not document.status()['dirty']
report = document.validate()
assert report['status'] == 'INVALID_CONTENT'
assert any(issue['code'] == 'InvalidNumber' for issue in report['issues'])
assert validate_project_pack({})[0]['code'] == 'InvalidNumber'
try:
    read_registry(manifest)
except ValueError as error:
    assert str(error) == 'InvalidNumber'
else:
    raise AssertionError('file reader accepted an oversized integer')
reopened = AuthorDocument.open(source)
assert reopened.snapshot()['texts']['name.actor.player'] == '坏清单下仍保存的作者修改'
try:
    reopened.prepare(output)
except DocumentError as error:
    assert error.code == 'INVALID_CONTENT'
else:
    raise AssertionError('author prepare accepted invalid presentation')
try:
    prepare_pack(source, output)
except ValueError as error:
    assert 'InvalidNumber' in str(error)
else:
    raise AssertionError('direct prepare accepted invalid presentation')
assert output.read_bytes() == previous
print('LARGE_MANIFEST_DRAFT_CHECKS=11 FAILURES=0')
'''
    result = subprocess.run([sys.executable, '-c', 'SIGN = ' + repr(sign) + '\n' + code], cwd=project, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'LARGE_MANIFEST_DRAFT_CHECKS=11 FAILURES=0' in result.stdout
