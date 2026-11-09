"""用固定真实Godot覆盖源图解码、导入缓存和原始图片缺席的实际PCK路线。"""
import json
import os
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]


def engine():
    """没有明确引擎时标为未执行；正式门禁必须提供固定Godot环境。"""
    value = os.environ.get('GODOT_BIN')
    if not value: pytest.skip('需要明确固定GODOT_BIN执行真实资源与导出测试')
    assert subprocess.check_output([value, '--version'], text=True).strip() == '4.7.2.stable.official.ed1daf0bf'
    return value


def run(command, log):
    """保存实际子进程输出并要求成功退出，断言属于被调用探针自身。"""
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    log.write_text(result.stdout + result.stderr)
    assert result.returncode == 0, log.read_text()
    assert 'SCRIPT ERROR:' not in result.stderr, log.read_text()
    return result.stdout


def project(folder):
    """构造最小真实资源工程，仅复制生产校验器和同一JSON/PNG夹具。"""
    game = folder / 'game'; game.mkdir(parents=True)
    (game / 'project.godot').write_text('config_version=5\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    for relative in ['ui/presentation_registry.gd', 'ui/presentation_contract.gd', 'core/strict_json.gd', 'core/content_shape.gd', 'data/presentation.schema.json']:
        destination = game / relative; destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / 'game' / relative, destination)
    shutil.copytree(ROOT / 'game/assets/art/presentation-fixtures', game / 'assets/art/presentation-fixtures', ignore=shutil.ignore_patterns('*.import', '*.md'))
    from PIL import Image
    data = json.loads((ROOT / 'tests/fixtures/presentation/layered-v2.json').read_text())
    folder = game / 'assets/art/presentation-fixtures'
    with Image.open(folder / 'child-body.png') as image:
        image.convert('RGB').save(folder / 'probe.jpg')
        image.save(folder / 'probe.webp', lossless=True)
    (folder / 'probe.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="8" height="8"><rect width="8" height="8" fill="red"/></svg>')
    for extension in ('jpg', 'webp', 'svg'):
        data['portraits']['fixture.' + extension] = {'path': 'res://assets/art/presentation-fixtures/probe.' + extension}
    (game / 'data/presentation.json').write_text(json.dumps(data))
    return game


def test_actual_decode_failure_and_exported_import_route(tmp_path):
    """坏源图不能靠旧导入缓存通过，且正式PCK中没有原PNG时仍可加载旧与新描述。"""
    binary = engine(); game = project(tmp_path)
    probe = game / 'resource_probe.gd'
    probe.write_text('''## 隔离实际资源验证：故障只写临时工程，生产资源完全不动。
extends SceneTree
## 返回实际载入结果，失败保留旧对象和快照。
func _init() -> void:
    var registry = load("res://ui/presentation_registry.gd").new()
    var valid = load("res://core/strict_json.gd").parse_text(FileAccess.get_file_as_string("res://data/presentation.json")).value
    var checks: int = 0
    assert(registry.configure(valid).ok); checks += 1
    for extension in ["jpg", "webp"]:
        var candidate: Dictionary = valid.duplicate(true)
        candidate.components["child.body"].asset_id = "portraits/fixture." + extension
        if extension == "jpg": candidate.components["child.body"].geometry.alpha_bounds = [0,0,8,8]
        assert(registry.configure(candidate).ok); checks += 1
    assert(registry.configure(valid).ok); checks += 1
    var before: Dictionary = registry.describe()
    var texture: Texture2D = registry.texture("portraits", "fixture.child")
    var path: String = "res://assets/art/presentation-fixtures/child-body.png"
    var original: PackedByteArray = FileAccess.get_file_as_bytes(path)
    for broken in ["not a png".to_utf8_buffer(), original.slice(0,45)]:
        var file = FileAccess.open(path, FileAccess.WRITE); file.store_buffer(broken); file.close()
        var rejected: Dictionary = registry.configure(valid)
        assert(not rejected.ok and rejected.code == "AssetDecode"); checks += 1
        assert(registry.describe() == before and registry.texture("portraits", "fixture.child") == texture); checks += 1
    var file = FileAccess.open(path, FileAccess.WRITE); file.store_buffer(original); file.close()
    assert(registry.configure(valid).ok); checks += 1
    var outside: String = ProjectSettings.globalize_path("res://").path_join("../outside.png")
    file = FileAccess.open(outside, FileAccess.WRITE); file.store_buffer(original); file.close()
    assert(DirAccess.remove_absolute(ProjectSettings.globalize_path(path)) == OK)
    assert(DirAccess.open(path.get_base_dir()).create_link(outside, path.get_file()) == OK)
    assert(registry.configure(valid).get("code") == "AssetPath"); checks += 1
    assert(registry.describe() == before and registry.texture("portraits", "fixture.child") == texture); checks += 1
    DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
    file = FileAccess.open(path, FileAccess.WRITE); file.store_buffer(original); file.close()
    var svg_path: String = "res://assets/art/presentation-fixtures/probe.svg"
    var svg: PackedByteArray = FileAccess.get_file_as_bytes(svg_path)
    file = FileAccess.open(svg_path, FileAccess.WRITE); file.store_string("not an svg"); file.close()
    assert(registry.configure(valid).get("code") == "AssetDecode"); checks += 1
    file = FileAccess.open(svg_path, FileAccess.WRITE); file.store_buffer(svg); file.close()
    assert(registry.configure(valid).ok); checks += 1
    print("RESOURCE_PROBE_CHECKS=", checks, " FAILURES=0")
    quit(0)
''')
    run([binary, '--headless', '--path', str(game), '--editor', '--quit'], tmp_path / 'import.log')
    output = run([binary, '--headless', '--path', str(game), '--script', 'res://resource_probe.gd'], tmp_path / 'resource.log')
    assert 'RESOURCE_PROBE_CHECKS=13 FAILURES=0' in output
    (game / 'export_presets.cfg').write_text('[preset.0]\nname="Linux"\nplatform="Linux"\nrunnable=true\nexport_filter="all_resources"\ninclude_filter="data/*.json"\nexclude_filter="resource_probe.gd"\nscript_export_mode=2\n[preset.0.options]\nbinary_format/architecture="x86_64"\ntexture_format/s3tc_bptc=true\n')
    package = tmp_path / 'fixture.pck'
    run([binary, '--headless', '--path', str(game), '--export-pack', 'Linux', str(package)], tmp_path / 'export.log')
    assert package.is_file()
    empty = tmp_path / 'empty'; empty.mkdir(); (empty / 'project.godot').write_text('config_version=5\n')
    runner = empty / 'probe.gd'
    runner.write_text('''## 空工程只从实际PCK读取资源，不能回退源工程PNG。
extends SceneTree
## 校验原始PNG缺席、导入纹理有效及分层几何仍可装载。
func _init() -> void:
    assert(ProjectSettings.load_resource_pack(OS.get_cmdline_user_args()[0]))
    var path: String = "res://assets/art/presentation-fixtures/child-body.png"
    assert(not FileAccess.file_exists(path))
    assert(ResourceLoader.exists(path))
    var registry = load("res://ui/presentation_registry.gd").new()
    var result: Dictionary = registry.load_from_path("res://data/presentation.json")
    assert(result.ok, str(result))
    assert(registry.texture("portraits", "fixture.child") != null)
    assert(registry.ids("character_profiles").has("fixture.second"))
    var legacy: Dictionary = registry.describe(); legacy.version = 1
    for group in ["components", "character_profiles", "scene_templates"]: legacy.erase(group)
    assert(registry.configure(legacy).ok)
    assert(registry.texture("portraits", "") == registry.texture("portraits", "fixture.child"))
    assert(registry.texture("portraits", "fixture.svg") != null)
    print("PACKAGE_PRESENTATION_CHECKS=9 FAILURES=0")
    quit(0)
''')
    output = run([binary, '--headless', '--path', str(empty), '--script', str(runner), '--', str(package)], tmp_path / 'package.log')
    assert 'PACKAGE_PRESENTATION_CHECKS=9 FAILURES=0' in output
