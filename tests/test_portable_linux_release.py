"""实跑发行启动器并检查字体发布边界；可执行和字体夹具只用于工具协议，不冒充游戏验收。"""
from pathlib import Path
import importlib.util
import json
import os
import struct
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]


def release_tool():
    """独立加载通用发布模块，测试直接使用当前启动器和导出函数。"""
    sys.path.insert(0, str(ROOT / 'tools'))
    spec = importlib.util.spec_from_file_location('portable_linux_release', ROOT / 'tools/release.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def launch_fixture(tmp_path, settings=None):
    """通过真实 Bash 启动器记录子进程环境；禁止旧实现尝试在 /data 写测试数据。"""
    release = release_tool()
    folder = tmp_path / '发行包 with spaces'; folder.mkdir()
    executable = folder / 'hanmo.x86_64'
    executable.write_text('#!' + sys.executable + '\nimport json,os,sys\nprint(json.dumps({"env":dict(os.environ),"args":sys.argv[1:],"cwd":os.getcwd()}))\n')
    executable.chmod(0o755)
    release.write_launcher(folder)
    guard = tmp_path / 'guard'; guard.mkdir()
    mkdir = guard / 'mkdir'
    mkdir.write_text('#!/bin/sh\nfor path in "$@"; do\n case "$path" in <本地路径>*) echo "forbidden legacy default: $path" >&2; exit 65;; esac\ndone\nexec /bin/mkdir "$@"\n')
    mkdir.chmod(0o755)
    home = tmp_path / 'home'; home.mkdir()
    env = dict(os.environ)
    for key in ['HANMO_STATE_ROOT', 'XDG_DATA_HOME', 'XDG_CONFIG_HOME', 'XDG_CACHE_HOME', 'XDG_STATE_HOME', 'TMPDIR', 'TMP', 'TEMP']:
        env.pop(key, None)
    env.update(HOME=str(home), PATH=str(guard) + os.pathsep + env['PATH'])
    env.update(settings or {})
    result = subprocess.run(['bash', str(folder / 'start.sh'), '--audio-driver', 'Dummy'],
                            cwd=tmp_path, env=env, text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    return json.loads(result.stdout), home, folder


def test_launcher_defaults_to_user_xdg_without_data_mount(tmp_path):
    """只解压即可启动，缺省目录位于当前用户 HOME，不要求开发机的数据盘。"""
    result, home, folder = launch_fixture(tmp_path)
    env = result['env']
    assert env['XDG_DATA_HOME'] == str(home / '.local/share')
    assert env['XDG_CONFIG_HOME'] == str(home / '.config')
    assert env['XDG_CACHE_HOME'] == str(home / '.cache')
    assert env['XDG_STATE_HOME'] == str(home / '.local/state')
    assert env['TMPDIR'] == str(home / '.cache/hanmo/tmp')
    args = result['args']
    assert args[args.index('--log-file') + 1] == str(home / '.local/state/hanmo/hanmo-player.log')
    assert args[-2:] == ['--', '--profile=' + release_tool().CONFIG['save_profile']]
    assert ['--audio-driver', 'Dummy'] == args[args.index('--audio-driver'):args.index('--audio-driver') + 2]
    assert result['cwd'] == str(folder)
    assert Path(env['TMPDIR']).is_dir()


def test_launcher_preserves_explicit_xdg_bases(tmp_path):
    """已有 XDG 配置仍由当前用户指定，不重定向到发布机路径或另一人的存档。"""
    settings = {key: str(tmp_path / key.lower()) for key in
                ['XDG_DATA_HOME', 'XDG_CONFIG_HOME', 'XDG_CACHE_HOME', 'XDG_STATE_HOME']}
    result, _, _ = launch_fixture(tmp_path, settings)
    assert all(result['env'][key] == value for key, value in settings.items())
    assert result['env']['TMPDIR'] == str(Path(settings['XDG_CACHE_HOME']) / 'hanmo/tmp')
    assert result['env']['MESA_SHADER_CACHE_DIR'] == str(Path(settings['XDG_CACHE_HOME']) / 'hanmo/mesa')


def test_state_root_override_preserves_existing_save_layout(tmp_path):
    """显式旧根目录继续使用原 data/config/cache/tmp/logs 布局，不自动搬迁旧档。"""
    state = tmp_path / 'existing state'
    result, home, _ = launch_fixture(tmp_path, {'HANMO_STATE_ROOT': str(state)})
    for key, suffix in [('XDG_DATA_HOME', 'data'), ('XDG_CONFIG_HOME', 'config'),
                        ('XDG_CACHE_HOME', 'cache'), ('TMPDIR', 'tmp')]:
        assert result['env'][key] == str(state / suffix)
    args = result['args']
    assert args[args.index('--log-file') + 1] == str(state / 'logs/hanmo-player.log')
    assert list(home.iterdir()) == []


def configure_export(tmp_path, monkeypatch, prepare=True, *, stage_content=True):
    """默认真实暂存小资源并隔离假字体；仅回执前置拒绝用例可略过无关暂存。"""
    release = release_tool()
    if stage_content:
        from tests.test_content_discovery import prepare_root
        prepare_root(tmp_path, tiny_resources=True).stage_content(tmp_path)
    game = tmp_path / 'game'
    if prepare:
        from prepare_font import prepare as prepare_font
        font = tmp_path / 'fixture.ttc'; font.write_bytes(b'ttcf' + struct.pack('>II', 0x10000, 3) + b'tool-fixture-only')
        license_path = tmp_path / 'LICENSE'; license_path.write_text('仅测试逐字许可保留，不是真实字体授权。\n')
        prepare_font(font, game, license_path, 2)
    monkeypatch.setattr(release.build, 'GAME', game)
    monkeypatch.setattr(release, 'FOLDER', tmp_path / 'releases/hanmo-linux')
    monkeypatch.setattr(release, 'LOGS', tmp_path / 'logs')
    monkeypatch.setattr(release.subprocess, 'check_output', lambda *args, **kwargs: '')
    release.fixture_receipt = {'engine': 'fixture-only', 'format': 'HM-VERIFY/1', 'profile': 'linux',
                               'receipt': str(tmp_path / 'receipt/report.json')}
    monkeypatch.setattr(release.verification, 'require_receipt', lambda *args: release.fixture_receipt)
    monkeypatch.setattr(release.verification, 'record_build_artifacts', lambda *args: release.fixture_receipt)
    calls = []

    def engine_boundary(command, **kwargs):
        """只构造导出层可检验的ELF标头；此夹具不作为有效Godot或可玩包。"""
        calls.append(command)
        if '--' in command:
            from tools.content_stage import MANIFEST, package_files
            stage = json.loads((game / 'data/generated' / MANIFEST).read_text())
            Path(command[-1]).write_text(json.dumps({'format': 'HM-CONTENT-PACKAGE/1', 'files': package_files(tmp_path, stage)}))
            return ''
        if '--export-release' in command:
            Path(command[-1]).write_bytes(b'\x7fELFexplicit packaging fixture, not a real game')
            return ''
        return 'EXPORT_FONT_CHECKS=1 FAILURES=0\n'

    monkeypatch.setattr(release.build, 'run_checked', engine_boundary)
    return release, calls


def test_linux_export_requires_licensed_prepared_font(tmp_path, monkeypatch):
    """没有完整字体准备记录就停止，不能交付依赖用户另装中文字体的默认包。"""
    release, calls = configure_export(tmp_path, monkeypatch, prepare=False)
    with pytest.raises(ValueError, match='CJK font'):
        release.export_native('fixture-engine', release.fixture_receipt)
    assert not calls and not release.FOLDER.exists()


def test_linux_export_preserves_font_license_and_checks(tmp_path, monkeypatch):
    """字体许可随包原样保留，清单反映内嵌字体及其实际前置检查而非旧无字体声明。"""
    release, calls = configure_export(tmp_path, monkeypatch)
    release.export_native('fixture-engine', release.fixture_receipt)
    assert (release.FOLDER / 'FONT-LICENSE.txt').read_bytes() == (tmp_path / 'LICENSE').read_bytes()
    manifest = json.loads((release.FOLDER / 'BUILD.json').read_text())
    assert manifest['font_files_included'] is True
    assert manifest['font']['face_index'] == 2
    assert manifest['font_glyph_checks'] == 1
    assert manifest['storage_default'] == 'XDG base directories'
    assert '--script' in calls[0] and calls[0][-1] == 'res://tests/test_export_font.gd'
    assert '--export-release' in calls[1]


def test_linux_export_preserves_existing_directory(tmp_path, monkeypatch):
    """既有同版本产物不能在重试时被静默覆盖，应选择新发行位置再导出。"""
    release, calls = configure_export(tmp_path, monkeypatch)
    release.FOLDER.mkdir(parents=True)
    prior = release.FOLDER / 'keep.txt'; prior.write_text('earlier artifact')
    with pytest.raises(RuntimeError, match='非空'):
        release.export_native('fixture-engine', release.fixture_receipt)
    assert prior.read_text() == 'earlier artifact' and not calls


def test_linux_font_glyph_failure_stops_before_export(tmp_path, monkeypatch):
    """字形门禁即使进程成功退出，只要包含失败断言也不能生成玩家包。"""
    release, _ = configure_export(tmp_path, monkeypatch)
    monkeypatch.setattr(release.build, 'run_checked', lambda *args, **kwargs: 'EXPORT_FONT_CHECKS=1 FAILURES=1\n')
    with pytest.raises(RuntimeError, match='Missing or failed'):
        release.export_native('fixture-engine', release.fixture_receipt)
    assert not release.FOLDER.exists()


def test_linux_export_preserves_archive_without_extracted_folder(tmp_path, monkeypatch):
    """旧压缩包即使没有解压目录，也属于必须保留的已交付产物。"""
    release, calls = configure_export(tmp_path, monkeypatch)
    release.FOLDER.parent.mkdir(parents=True)
    archive = release.FOLDER.parent / ('hanmo_' + release.CONFIG['version'] + '_linux_x86_64.zip')
    archive.write_bytes(b'previous completed archive')
    with pytest.raises(RuntimeError, match='保留此前产物'):
        release.export_native('fixture-engine', release.fixture_receipt)
    assert archive.read_bytes() == b'previous completed archive' and not calls


@pytest.mark.parametrize('change', ['missing', 'edited'])
def test_linux_repackage_rejects_missing_or_changed_font_license(tmp_path, monkeypatch, change):
    """内嵌字体的二次打包仍核验许可，不能在导出后删改许可再生成缺失授权说明的包。"""
    release, _ = configure_export(tmp_path, monkeypatch)
    release.export_native('fixture-engine', release.fixture_receipt)
    archive = release.FOLDER.parent / ('hanmo_' + release.CONFIG['version'] + '_linux_x86_64.zip')
    original = archive.read_bytes()
    license_path = release.FOLDER / 'FONT-LICENSE.txt'
    if change == 'missing': license_path.unlink()
    else: license_path.write_text('changed license')
    with pytest.raises(RuntimeError, match='许可'):
        release.main(['package', '--platform', 'linux'])
    assert archive.read_bytes() == original


def test_native_cli_checks_font_before_running_suite(tmp_path, monkeypatch):
    """缺失发行字体应在昂贵的完整回归之前被CLI阻止，不改用系统字体继续导出。"""
    release, _ = configure_export(tmp_path, monkeypatch, prepare=False)
    monkeypatch.setattr(release.build, 'find_engine', lambda: 'fixture-engine')

    original = release.build.require_font
    monkeypatch.setattr(release.build, 'require_font', lambda: original(release.build.GAME))
    monkeypatch.setattr(release, 'run_suite', lambda *args: pytest.fail('missing font must stop before suite'))
    with pytest.raises(RuntimeError, match='font'):
        release.main(['export', '--platform', 'linux'])
