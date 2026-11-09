"""检查真实本地启动器与失败关闭的测试摘要解析，不模拟游戏编译。"""
from pathlib import Path
import importlib.util
import os
import subprocess
import sys
import pytest
ROOT = Path(__file__).resolve().parents[1]

def module():
    """加载兼容入口并检查其存在；实际发布仍由统一工具负责。"""
    path = ROOT/'tools/release_f05.py'
    assert path.is_file(), 'F0.5 local release helper is missing'
    sys.path.insert(0, str(ROOT/'tools'))
    spec = importlib.util.spec_from_file_location('release_f05', path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

def test_launcher_uses_selected_local_storage(tmp_path):
    """用小型可执行夹具核验目录和存档配置传递，不运行或伪造Godot。"""
    release = module()
    folder = tmp_path/'release with spaces'; folder.mkdir()
    executable = folder/'hanmo.x86_64'
    executable.write_text('#!/bin/sh\nprintf "%s\\n" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$TMPDIR" "$@"\n')
    executable.chmod(0o755)
    release.write_launcher(folder)
    state = tmp_path/'state'; home = tmp_path/'untouched-home'; home.mkdir()
    env = dict(os.environ, HANMO_STATE_ROOT=str(state), HOME=str(home))
    result = subprocess.run(['bash', str(folder/'start.sh'), '--audio-driver', 'Dummy'], env=env, text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    assert str(state/'data') in result.stdout
    assert str(state/'cache') in result.stdout
    assert str(state/'tmp') in result.stdout
    from release import CONFIG
    assert ('--profile='+CONFIG['save_profile']) in result.stdout
    assert '@SAVE_PROFILE@' not in result.stdout
    assert list(home.iterdir()) == []

def test_summary_requires_executed_assertions():
    """没有正数且零失败的断言摘要时，即使退出码零也不能算测试通过。"""
    release = module()
    assert release.assertion_count('PLAYABLE_CHECKS=67 FAILURES=0\n') == 67
    for text in ['nothing ran', 'CHECKS=0 FAILURES=0', 'FOO_CHECKS=20 FAILURES=1', 'SCRIPT ERROR: failed\nFOO_CHECKS=20 FAILURES=0']:
        with pytest.raises(RuntimeError): release.assertion_count(text)
