"""独立验证原生发行配置与目录；收集本模块不依赖 Playwright 或 Chromium。"""
import json
import os
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]


def test_godot_forwards_release_profile_and_preserves_native_paths(tmp_path):
    """真实 Godot 读取同一发行设置并测试命令行覆盖；参数不能伪造 profile，原生目录不回退。"""
    script = tmp_path / 'profile_probe.gd'
    script.write_text('''## 只读验证发行存储配置；不创建人物世界或写入存档。
extends SceneTree
const Storage=preload("res://storage/storage_service.gd")
## 输出正式适配器形成的请求及原生目录，测试据此检查跨端配置来源。
func _init() -> void:
    if "--legacy-setting" in OS.get_cmdline_user_args(): ProjectSettings.set_setting("application/config/storage_profile","")
    var service=Storage.new()
    service.configure({})
    var args={"slot":"manual-1","profile":"forged-profile","nested":{"untouched":true}}
    var before=args.duplicate(true)
    print("PROFILE_PROBE="+JSON.stringify({"args":service._web_arguments(args),"unchanged":args==before,
        "directory":service.native.directory,"legacy":ProjectSettings.globalize_path("user://f02-1/saves"),
        "configured":ProjectSettings.get_setting("application/config/storage_profile","")}))
    service.free()
    quit()
''', encoding='utf-8')
    engine = os.environ.get('GODOT_BIN', '<本地路径>')
    for args, override in [([], None), (['--profile=tests-release-isolation'], 'tests-release-isolation'),
                           (['--profile=player-m01b'], 'player-m01b'), (['--profile='], None),
                           (['--legacy-setting'], ''),
                           (['--legacy-setting', '--profile=tests-release-isolation'], 'tests-release-isolation')]:
        result = subprocess.run([engine, '--headless', '--path', str(ROOT / 'game'), '--script', str(script), '--', *args],
            cwd=ROOT, text=True, capture_output=True, timeout=30, check=True)
        assert 'SCRIPT ERROR:' not in result.stdout + result.stderr
        output = next(line.removeprefix('PROFILE_PROBE=') for line in result.stdout.splitlines() if line.startswith('PROFILE_PROBE='))
        actual = json.loads(output)
        expected = actual['configured'] if override is None else override
        assert actual['args']['profile'] == expected and actual['unchanged']
        assert actual['args']['slot'] == 'manual-1' and actual['args']['nested'] == {'untouched': True}
        if expected:
            assert actual['directory'].endswith('/profiles/' + expected + '/saves')
            assert actual['directory'] != actual['legacy']
        else:
            assert actual['directory'] == actual['legacy']
