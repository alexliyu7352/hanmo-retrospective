"""检查原生压缩包边界；可执行夹具只验证打包，不冒充Godot编译或运行。"""
from pathlib import Path
import importlib.util
import sys
import zipfile
import pytest

ROOT = Path(__file__).resolve().parents[1]


def module():
    """读取实际压缩工具，缺失时明确失败，测试不复制另一套打包逻辑。"""
    path = ROOT / 'tools/release_linux.py'
    assert path.is_file(), 'Native release pipeline has not been implemented'
    sys.path.insert(0, str(ROOT / 'tools'))
    spec = importlib.util.spec_from_file_location('release_linux', path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def test_package_keeps_binary_and_launcher(tmp_path):
    """压缩包保留原二进制字节、可执行权限和可随目录移动的启动脚本。"""
    release = module()
    source = tmp_path / 'hanmo-test'
    source.mkdir()
    (source / 'hanmo.x86_64').write_bytes(b'\x7fELF' + b'packing-fixture-not-a-real-build')
    (source / 'hanmo.x86_64').chmod(0o755)
    (source / 'start.sh').write_text('#!/bin/bash\nexit 0\n')
    archive = tmp_path / 'release.zip'
    release.package_directory(source, archive)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert z.read('hanmo-test/hanmo.x86_64').startswith(b'\x7fELF')
        assert z.getinfo('hanmo-test/hanmo.x86_64').external_attr >> 16 & 0o111


def test_package_refuses_font_bytes(tmp_path):
    """阻止未经发行准备的散装字体混入；批准字体只经受校验的Godot资源包内嵌。"""
    release = module()
    source = tmp_path / 'bad'; source.mkdir()
    (source / 'some-font.ttf').write_bytes(b'font fixture')
    with pytest.raises(RuntimeError, match='font'):
        release.package_directory(source, tmp_path / 'bad.zip')


def test_preset_excludes_editor_and_tests():
    """玩家包排除编辑器与测试，但保留经许可准备的中文字体以便解压即玩。"""
    text = (ROOT / 'game/export_presets.cfg').read_text()
    assert 'addons/*' in text
    assert 'tests/*' in text
    assert 'assets/fonts/*' not in text


def test_package_refuses_save_files(tmp_path):
    """玩家导出存档不得因误放到构建目录而进入发行压缩包。"""
    release = module()
    source = tmp_path / 'bad'; source.mkdir()
    (source / 'player.hmsave').write_text('private save')
    with pytest.raises(RuntimeError, match='save'):
        release.package_directory(source, tmp_path / 'bad.zip')
