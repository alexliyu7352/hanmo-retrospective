"""验证静态引用检查能区分真实缺失、目录调用和唯一的故意损坏测试资源。"""
from pathlib import Path
import json
import os
import subprocess
import sys

import pytest

from tools import check_source


@pytest.fixture
def project(tmp_path, monkeypatch):
    """构造最小文件树供真实静态检查使用，不伪造其检查结果。"""
    for name in ['game/project.godot', 'game/main.tscn', 'game/core/world_kernel.gd',
                 'game/core/save_codec.gd', 'game/storage/storage_service.gd', 'game/ui/main.gd',
                 'game/ui/landscape.gd', 'game/export_presets.cfg', 'web/storage_bridge.js', 'web/shell.html']:
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('', encoding='utf-8')
    (tmp_path / 'game/tests').mkdir()
    monkeypatch.setattr(check_source, 'ROOT', tmp_path)
    return tmp_path


def test_directory_reference_without_trailing_slash_is_checked_as_directory(project):
    """DirAccess 目录参数可不带末尾斜杠，但目录真实缺失时仍必须报告。"""
    path = project / 'game/tests/test_directory.gd'
    path.write_text('var files = DirAccess.get_files_at("res://data/generated")\n')
    directory = project / 'game/data/generated'; directory.mkdir(parents=True)
    assert check_source.check(scope='shared')['ok']
    directory.rmdir()
    assert 'data/generated' in '\n'.join(check_source.check(scope='shared')['errors'])


def test_directory_cannot_satisfy_a_resource_load(project):
    """加载资源仍须是文件，不能因同名目录存在而绕过检查。"""
    (project / 'game/data/generated').mkdir(parents=True)
    (project / 'game/tests/test_directory.gd').write_text('var item = load("res://data/generated")\n')
    assert 'data/generated' in '\n'.join(check_source.check(scope='shared')['errors'])


def test_known_missing_asset_is_only_allowed_in_its_negative_test(project):
    """仅注册表指定的坏路径夹具可缺失，生产源码与其他测试引用同名文件照常失败。"""
    path = project / 'game/tests/test_presentation_registry.gd'
    path.write_text('var broken = "res://assets/art/missing.png"\n')
    assert check_source.check(scope='shared')['ok']
    (project / 'game/ui/main.gd').write_text(path.read_text())
    assert 'game/ui/main.gd: missing reference assets/art/missing.png' in check_source.check(scope='shared')['errors']


def test_unrelated_missing_reference_in_negative_test_still_fails(project):
    """故意损坏测试不是整份文件豁免，其余拼错的资源必须继续阻止门禁。"""
    (project / 'game/tests/test_presentation_registry.gd').write_text('var broken = "res://assets/art/typo.png"\n')
    assert 'assets/art/typo.png' in '\n'.join(check_source.check(scope='shared')['errors'])


def test_recursive_python_syntax_failure_blocks(project):
    """业务工具任意深度的语法错误必须阻断，不能被仅检查顶层的 glob 漏过。"""
    path = project / 'tools/content/nested/bad.py'; path.parent.mkdir(parents=True)
    path.write_text('def broken(:\n')
    result = check_source.check(scope='shared')
    assert not result['ok']
    assert 'bad.py' in '\n'.join(result['errors'])


@pytest.mark.parametrize('excluded', ['.git', '.godot', '.worktrees', '__pycache__', 'build', 'builds', 'venv'])
def test_recursive_python_ignores_only_controlled_artifacts(project, excluded):
    """已知缓存或产物子目录不纳入语法门禁，其余作者工具与测试目录仍递归覆盖。"""
    path = project / 'tools' / excluded / 'bad.py'; path.parent.mkdir(parents=True)
    path.write_text('def broken(:\n')
    good = project / 'tests/content/nested/good.py'; good.parent.mkdir(parents=True)
    good.write_text('"""业务测试职责。"""\n')
    result = check_source.check(scope='shared')
    assert result['ok'], result['errors']
    assert result['python_files'] == 1


@pytest.mark.parametrize('source_error', [False, True])
def test_missing_node_has_distinct_dependency_status(project, monkeypatch, source_error):
    """静态检查只把明确的 Node 启动缺失列为依赖；真实语法错误仍单独失败。"""
    def run(*args, **kwargs):
        """静态工具的 JavaScript 子进程边界抛缺依赖，不替换 Python 或资源检查。"""
        raise FileNotFoundError('node executable fixture')
    monkeypatch.setattr(check_source.subprocess, 'run', run)
    if source_error:
        path = project / 'tools/content/bad.py'; path.parent.mkdir(parents=True); path.write_text('def broken(:\n')
    result = check_source.check()
    assert result['status'] == ('FAIL' if source_error else 'BLOCKED')
    assert not result['ok']
    assert result['blocked']
    assert bool(result['errors']) == source_error
    assert result['javascript_syntax'] == 'BLOCKED'


@pytest.mark.parametrize('source_error', [False, True])
def test_missing_node_cli_exits_two_unless_source_error(project, source_error):
    """真实 Python CLI 在空 PATH 中无法启动 Node；纯依赖退出二，附带语法错误仍退出一。"""
    if source_error:
        path = project / 'tools/content/bad.py'; path.parent.mkdir(parents=True); path.write_text('def broken(:\n')
    tool = Path(check_source.__file__)
    result = subprocess.run([sys.executable, str(tool), '--root', str(project)],
                            env=dict(os.environ, PATH=''), capture_output=True, text=True)
    report = json.loads(result.stdout)
    assert result.returncode == (1 if source_error else 2)
    assert report['status'] == ('FAIL' if source_error else 'BLOCKED')
    assert report['blocked']
    assert bool(report['errors']) == source_error



@pytest.mark.parametrize('source_error', [False, True])
def test_shared_scope_has_no_web_or_node_dependency(project, monkeypatch, source_error):
    """Linux 共享静态检查不用 Node/Web 文件，坏 Python 仍以真实语法失败阻止通过。"""
    import inspect
    assert 'scope' in inspect.signature(check_source.check).parameters, '静态检查缺少 scope'
    (project / 'web/storage_bridge.js').unlink(); (project / 'web/shell.html').unlink()
    monkeypatch.setattr(check_source.subprocess, 'run', lambda *args, **kwargs: pytest.fail('共享检查不能启动 Node'))
    if source_error:
        path = project / 'tests/content/bad.py'; path.parent.mkdir(parents=True); path.write_text('def bad(:\n')
    result = check_source.check(scope='shared')
    assert result['javascript_syntax'] == 'NOT_RUN'
    assert result['status'] == ('FAIL' if source_error else 'PASS')
    assert not result['blocked'] and result['ok'] is not source_error


@pytest.mark.parametrize('failure', ['missing_file', 'bad_js', 'missing_node'])
def test_web_scope_preserves_real_failures(project, monkeypatch, failure):
    """Web 必需文件、JavaScript 语法与明确缺 Node 的分类分开，不能借延期放行。"""
    import inspect
    assert 'scope' in inspect.signature(check_source.check).parameters, '静态检查缺少 scope'
    if failure == 'missing_file':
        (project / 'web/shell.html').unlink()
        monkeypatch.setattr(check_source.subprocess, 'run', lambda *args, **kwargs:
                            subprocess.CompletedProcess(args[0], 0, '', ''))
    if failure == 'bad_js':
        (project / 'web/storage_bridge.js').write_text('const broken = ;')
        monkeypatch.setattr(check_source.subprocess, 'run', lambda *args, **kwargs:
                            subprocess.CompletedProcess(args[0], 1, '', 'SyntaxError: bad JavaScript fixture'))
    if failure == 'missing_node':
        def run(*args, **kwargs):
            """只模拟 Node 启动缺失，真实文件和静态解析保持原检查。"""
            raise FileNotFoundError('node fixture')
        monkeypatch.setattr(check_source.subprocess, 'run', run)
    result = check_source.check(scope='web')
    assert result['status'] == ('BLOCKED' if failure == 'missing_node' else 'FAIL')
    assert not result['ok']
    if failure == 'missing_file':
        assert 'Missing web/shell.html' in result['errors']
        assert not result['blocked']



def test_shared_cli_runs_with_empty_path(project):
    """共享 CLI 在没有 Node 可查找的 PATH 中仍通过，回执明确 JavaScript 没运行。"""
    result = subprocess.run([sys.executable, str(Path(check_source.__file__)), '--root', str(project), '--scope', 'shared'],
                            env=dict(os.environ, PATH=''), capture_output=True, text=True)
    report = json.loads(result.stdout)
    assert result.returncode == 0 and report['status'] == 'PASS'
    assert report['javascript_syntax'] == 'NOT_RUN' and not report['blocked']


def test_shared_web_scope_fixtures_never_launch_real_node():
    """共享套件中的Web分类夹具必须隔离Node启动，外层禁止启动证明缺文件和语法分支也可独立运行。"""
    code = '''import subprocess
import sys
import pytest
original = subprocess.run

def guarded(command, *args, **kwargs):
    """阻止夹具意外调用真实Node；其他进程保持原有边界。"""
    if command and command[0] == 'node':
        raise AssertionError('共享工具测试不能启动真实Node')
    return original(command, *args, **kwargs)

subprocess.run = guarded
sys.exit(pytest.main(['tests/test_source_check.py::test_web_scope_preserves_real_failures', '-q']))
'''
    result = subprocess.run([sys.executable, '-c', code], cwd=Path(__file__).resolve().parents[1],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
