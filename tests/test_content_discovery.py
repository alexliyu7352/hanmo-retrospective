"""验证增加作者包不再修改构建白名单，错误文件也不能悄悄漏过发布。"""
import importlib.util
from functools import partial
import json
from pathlib import Path
import shutil
import pytest
ROOT = Path(__file__).resolve().parents[1]
TINY_PNG = ROOT / 'game/assets/art/presentation-fixtures/scene.png'


def prepare_root(tmp_path, *, tiny_resources=False):
    """默认复制真实美术；工具夹具可显式缩小无分层引用的PNG，仍走实际校验。"""
    for name in ['content', 'schemas', 'tools']:
        ignore = shutil.ignore_patterns('godot', 'fonts', 'art-models', '__pycache__') if name == 'tools' else None
        shutil.copytree(ROOT/name, tmp_path/name, ignore=ignore)
    (tmp_path/'game').mkdir()
    shutil.copytree(ROOT/'game/core', tmp_path/'game/core')
    (tmp_path/'game/data').mkdir()
    shutil.copyfile(ROOT/'game/data/rule_capabilities.json', tmp_path/'game/data/rule_capabilities.json')
    shutil.copyfile(ROOT/'game/data/module_contracts.json', tmp_path/'game/data/module_contracts.json')
    for name in ('presentation.json', 'presentation.schema.json'):
        shutil.copyfile(ROOT / 'game/data' / name, tmp_path / 'game/data' / name)
    manifest = json.loads((ROOT / 'game/data/presentation.json').read_text())
    protected_paths = set()
    if tiny_resources:
        # 按引用资源的路径保护像素，同一路径的旧别名也不能覆盖分层几何。
        pending = [manifest]
        while pending:
            value = pending.pop()
            if isinstance(value, dict):
                if 'asset_id' in value:
                    group, identifier = value['asset_id'].split('/', 1)
                    protected_paths.add(manifest[group][identifier]['path'])
                pending.extend(value.values())
            elif isinstance(value, list):
                pending.extend(value)
        tiny_png = TINY_PNG.read_bytes()
    for group in ('backgrounds', 'portraits'):
        for entry in manifest[group].values():
            relative = entry['path'].removeprefix('res://')
            target = tmp_path / 'game' / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if tiny_resources and target.suffix == '.png' and entry['path'] not in protected_paths:
                target.write_bytes(tiny_png)
            else:
                shutil.copyfile(ROOT / 'game' / relative, target)
    spec = importlib.util.spec_from_file_location('discovery_build', ROOT/'tools/build.py')
    tool = importlib.util.module_from_spec(spec); spec.loader.exec_module(tool)
    # 旧工具夹具可主动缩图；固定历史binding另由独有真实摘要夹具验证，不能放宽生产pin。
    tool.stage_content = partial(tool.stage_content, foundation_sets=False)
    return tool


def test_extra_pack_is_staged(tmp_path):
    """新增同格式的第四份内容应自动准备，不需给stage_content再加名称分支。"""
    tool = prepare_root(tmp_path)
    folder = tmp_path/'content/foundation'
    pack = json.loads((folder/'two_stories.json').read_text())
    pack['id'] = 'foundation.extra_story'
    (folder/'extra_story.json').write_text(json.dumps(pack))
    tool.stage_content(tmp_path)
    result = tmp_path/'game/data/generated/extra_story.hmcatalog.json'
    assert result.is_file(), '新增作者包未被同一发布管线纳入'
    assert json.loads(result.read_text())['content_id'] == pack['id']
    assert not (result.parent/'runtime_capabilities.hmcatalog.json').exists()


@pytest.mark.parametrize('contents', ['{bad', '{"format":"UNKNOWN"}'])
def test_bad_extra_source_blocks_staging(tmp_path, contents):
    """发布目录内的坏作者文件必须报错；既有非内容能力清单明确跳过。"""
    tool = prepare_root(tmp_path)
    (tmp_path/'content/foundation/extra_story.json').write_text(contents)
    with pytest.raises(RuntimeError):
        tool.stage_content(tmp_path)


def test_default_fixture_preserves_actual_presentation_bytes(tmp_path):
    """默认模式仍复制完整美术、清单和Schema，真实资源消费者不可暗中缩小。"""
    prepare_root(tmp_path)
    for name in ('presentation.json', 'presentation.schema.json'):
        assert (tmp_path / 'game/data' / name).read_bytes() == (ROOT / 'game/data' / name).read_bytes()
    manifest = json.loads((ROOT / 'game/data/presentation.json').read_text())
    for group in ('backgrounds', 'portraits'):
        for entry in manifest[group].values():
            relative = entry['path'].removeprefix('res://')
            assert (tmp_path / 'game' / relative).read_bytes() == (ROOT / 'game' / relative).read_bytes()


def test_tiny_fixture_keeps_registry_and_real_decode_rejection(tmp_path):
    """缩小模式保留全部身份与引用，真实像素解码仍拒绝同路径的截断资源。"""
    from tools import presentation
    tool = prepare_root(tmp_path, tiny_resources=True)
    manifest_path = tmp_path / 'game/data/presentation.json'
    assert manifest_path.read_bytes() == (ROOT / 'game/data/presentation.json').read_bytes()
    assert (tmp_path / 'game/data/presentation.schema.json').read_bytes() == (ROOT / 'game/data/presentation.schema.json').read_bytes()
    manifest = json.loads(manifest_path.read_text())
    assert presentation.validate_registry(manifest, tmp_path / 'game') == []
    sources = tool.discover_author_sources(tmp_path)
    assert sources
    for path in sources:
        pack = json.loads(path.read_text())
        assert pack['format'] == 'HM-CONTENT'
        assert presentation.validate_references(manifest, pack) == []
    relative = manifest['portraits']['merchant']['path'].removeprefix('res://')
    target = tmp_path / 'game' / relative
    tiny = (ROOT / 'game/assets/art/presentation-fixtures/scene.png').read_bytes()
    assert target.read_bytes() == tiny
    target.write_bytes(tiny[:40])
    assert presentation.validate_registry(manifest, tmp_path / 'game') == [
        presentation.issue('/portraits/merchant/path', 'AssetDecode')]


def test_tiny_fixture_preserves_layered_geometry_and_shared_paths(tmp_path, monkeypatch):
    """新增组件、专绘和场景的真实像素及共路径别名不能被旧纹理缩小覆盖。"""
    import copy
    import sys
    from PIL import Image
    from tools import presentation
    source = tmp_path / 'source'; source.mkdir()
    target = tmp_path / 'target'; target.mkdir()
    prepare_root(source)
    data = json.loads((source / 'game/data/presentation.json').read_text())
    layered = json.loads((ROOT / 'tests/fixtures/presentation/layered-v2.json').read_text())
    for group in ('backgrounds', 'portraits', 'components', 'character_profiles', 'scene_templates'):
        data[group].update(copy.deepcopy(layered[group]))
    assets = source / 'game/assets/art/presentation-fixtures'
    shutil.copytree(ROOT / 'game/assets/art/presentation-fixtures', assets, dirs_exist_ok=True)
    protected = {path: (assets / path).read_bytes() for path in
                 ('child-body.png', 'adult-body.png', 'child-garment.png', 'adult-garment.png')}
    for override in data['character_profiles']['fixture.dedicated']['overrides']:
        original = data['portraits'][override['asset_id'].split('/', 1)[1]]
        name = 'override-' + override['id']
        raw = (source / 'game' / original['path'].removeprefix('res://')).read_bytes()
        (assets / (name + '.png')).write_bytes(raw)
        data['portraits'][name] = {'path': 'res://assets/art/presentation-fixtures/' + name + '.png'}
        override['asset_id'] = 'portraits/' + name
        protected[name + '.png'] = raw
    Image.new('RGBA', (8, 8), (190, 40, 20, 255)).save(assets / 'scene.png')
    protected['scene.png'] = (assets / 'scene.png').read_bytes()
    data['portraits']['fixture.shared-child'] = copy.deepcopy(data['portraits']['fixture.child-body'])
    manifest = source / 'game/data/presentation.json'
    manifest.write_text(json.dumps(data))
    monkeypatch.setattr(sys.modules[__name__], 'ROOT', source)
    prepare_root(target, tiny_resources=True)
    assert (target / 'game/data/presentation.json').read_bytes() == manifest.read_bytes()
    for name, raw in protected.items():
        assert (target / 'game/assets/art/presentation-fixtures' / name).read_bytes() == raw
    assert presentation.validate_registry(data, target / 'game') == []


@pytest.mark.parametrize('extension', ['jpg', 'webp', 'svg'])
def test_tiny_fixture_preserves_valid_non_png_resources(tmp_path, monkeypatch, extension):
    """旧纹理新增非PNG格式时保持原文件与真实解码，不能把PNG字节写入其他扩展名。"""
    import sys
    from PIL import Image
    from tools import presentation
    source = tmp_path / 'source'; source.mkdir()
    target = tmp_path / 'target'; target.mkdir()
    prepare_root(source)
    resource = source / 'game/assets/art' / ('fixture.' + extension)
    if extension == 'svg':
        resource.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="8" height="8"><rect width="8" height="8" fill="red"/></svg>')
    else:
        Image.new('RGB', (8, 8), (190, 40, 20)).save(resource)
    manifest = source / 'game/data/presentation.json'
    data = json.loads(manifest.read_text())
    data['portraits']['fixture.non-png'] = {'path': 'res://assets/art/' + resource.name}
    manifest.write_text(json.dumps(data))
    monkeypatch.setattr(sys.modules[__name__], 'ROOT', source)
    prepare_root(target, tiny_resources=True)
    assert (target / 'game/assets/art' / resource.name).read_bytes() == resource.read_bytes()
    assert presentation.validate_registry(data, target / 'game') == []
