"""验证真实 Python 工具；模拟命令仅验证错误传播，不代表游戏运行。"""
from __future__ import annotations
import importlib.util
import json
import os
import pathlib
import subprocess
import sys
import tempfile

import pytest
ROOT = pathlib.Path(__file__).resolve().parents[1]


def module(name: str):
    """直接载入项目工具；文件缺失或导入失败时报告真实错误。"""
    path = ROOT / 'tools' / f'{name}.py'
    assert path.is_file(), f'missing production tool: {path.name}'
    spec = importlib.util.spec_from_file_location(f'hanmo_{name}', path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def test_build_tool_exists():
    """实际构建入口必须存在，不能用测试替身掩盖缺失。"""
    assert (ROOT / 'tools' / 'build.py').is_file()


def test_missing_engine_is_explicit(monkeypatch):
    """明确指定的无效引擎路径必须失败，不能暗中切换到其他程序。"""
    build = module('build')
    monkeypatch.setenv('GODOT_BIN', '/definitely-missing/hanmo-godot')
    with pytest.raises(RuntimeError, match='Godot'):
        build.find_engine()


def test_missing_engine_cli_exits_nonzero(tmp_path):
    """实际命令入口在引擎缺失时返回非零状态，不生成成功记录。"""
    env = dict(os.environ, GODOT_BIN=str(tmp_path / 'absent-engine'))
    result = subprocess.run([sys.executable, str(ROOT/'tools/build.py'), 'test'],
                            env=env, capture_output=True, text=True)
    assert result.returncode != 0 and 'Godot' in result.stderr


def test_command_failure_propagates():
    """外部程序失败时必须传播退出码，不能记作构建成功。"""
    build = module('build')
    with pytest.raises(RuntimeError, match='17'):
        build.run_checked([sys.executable,'-c','raise SystemExit(17)'])


def test_command_success_returns_stdout():
    """命令执行器返回实际标准输出，不能生成虚构结果。"""
    build = module('build')
    assert 'test-only-runner' in build.run_checked([sys.executable,'-c','print("test-only-runner")'])


def test_parser_errors_even_on_zero_exit_are_rejected():
    """引擎即使退出码为零，解析错误诊断仍必须阻止构建成功。"""
    build = module('build')
    with pytest.raises(RuntimeError, match='diagnostic'):
        build.run_checked([sys.executable,'-c','print("SCRIPT ERROR: Parse Error: fixture")'])


def test_font_export_gate(tmp_path):
    """缺少中文字体时必须阻止网页导出，不能依赖不可读的替代字形。"""
    build = module('build')
    with pytest.raises(RuntimeError, match='font'):
        build.require_font(tmp_path)


def test_font_path_must_exist(tmp_path):
    """准备不存在的字体必须报错，不能声称文件已经复制。"""
    fonts = module('prepare_font')
    with pytest.raises(ValueError):
        fonts.prepare(tmp_path/'absent.otf', tmp_path/'game')


def test_nonfont_is_rejected(tmp_path):
    """非字体字节必须拒绝，不能仅凭扩展名认定资源有效。"""
    fonts = module('prepare_font')
    bad = tmp_path/'bad.otf'
    bad.write_bytes(b'not a font')
    with pytest.raises(ValueError):
        fonts.prepare(bad,tmp_path/'game')


def test_service_mime_types():
    """真实服务处理器为引擎和存储脚本返回对应的资源类型。"""
    serve = module('serve')
    assert serve.GameHandler.extensions_map['.wasm'] == 'application/wasm'
    assert serve.GameHandler.extensions_map['.js'] in ['text/javascript','application/javascript']
    assert serve.GameHandler.extensions_map['.pck'] == 'application/octet-stream'


def test_server_defaults_are_loopback():
    """开发服务默认仅监听回环地址，避免向其他主机暴露工作区。"""
    serve = module('serve')
    args = serve.argument_parser().parse_args([])
    assert args.host == '127.0.0.1'
    assert args.port == 8060


def test_actual_server_serves_wasm_bytes(tmp_path):
    """通过回环地址读取真实 HTTP 资源；该结果不代表浏览器或游戏验收。"""
    import functools
    import http.server
    import threading
    import urllib.request
    serve = module('serve')
    (tmp_path/'fixture.wasm').write_bytes(b'\0asm')
    server = http.server.ThreadingHTTPServer(('127.0.0.1',0),
        functools.partial(serve.GameHandler,directory=str(tmp_path)))
    thread = threading.Thread(target=server.serve_forever,daemon=True)
    thread.start()
    try:
        with urllib.request.urlopen(f'http://127.0.0.1:{server.server_port}/fixture.wasm',timeout=3) as reply:
            assert reply.headers.get_content_type() == 'application/wasm'
            assert reply.read() == b'\0asm'
    finally:
        server.shutdown();thread.join(timeout=3);server.server_close()


def test_content_shape_and_ids():
    """检查实际静态内容结构和唯一标识；不替代 Godot 加载器运行。"""
    data = json.loads((ROOT/'game/data/e0.json').read_text())
    assert len(data['nodes']) == 12 and len(data['routes']) == 36 and len(data['items']) == 8
    for field in ['nodes','routes','items']:
        assert len({x['id'] for x in data[field]}) == len(data[field])
    assert data['features']['combat'] is False and data['features']['npc_ai'] is False


def test_content_references_connected():
    """发行静态道路必须端点合法且连通，避免形成不可达据点。"""
    data = json.loads((ROOT/'game/data/e0.json').read_text())
    nodes = {n['id'] for n in data['nodes']}
    visited = {'C01'}
    for _ in nodes:
        for edge in data['routes']:
            assert edge['origin'] in nodes and edge['destination'] in nodes
            assert isinstance(edge['base_steps'],int) and edge['base_steps'] > 0
            if edge['origin'] in visited:
                visited.add(edge['destination'])
    assert visited == nodes


def test_distribution_excludes_fonts_builds_and_local_environment(tmp_path):
    """统一根目录的源码归档保留作者文件，排除发行包、安装环境和缓存。"""
    build = module('build')
    (tmp_path/'game/assets/fonts').mkdir(parents=True)
    (tmp_path/'game/assets/fonts/cjk.otf').write_bytes(b'not-a-distributed-font')
    (tmp_path/'build').mkdir()
    (tmp_path/'build/large.pck').write_bytes(b'fixture')
    (tmp_path/'README.md').write_text('fixture')
    for name in ['backups', 'builds', 'cache', 'config', 'data', 'tmp', 'venv',
                 'tools/godot', 'tools/fonts', 'tools/art-models',
                 'verification/browser-profile', 'verification/profile-run.fixture',
                 'verification/browser-attempt-01']:
        directory = tmp_path/name
        directory.mkdir(parents=True)
        (directory/'local-file').write_bytes(b'local environment')
    for name in ['game/export.pck', 'game/export.wasm', 'game/export.x86_64']:
        (tmp_path/name).write_bytes(b'compiled export')
    for name in ['game/data/author.json', 'tools/build.py', 'artwork/original.png',
                 'handoff/NEXT.md', 'verification/input.json']:
        path = tmp_path/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'author source')
    assert [p.relative_to(tmp_path).as_posix() for p in build.source_files(tmp_path)] == [
        'README.md', 'artwork/original.png', 'game/data/author.json', 'handoff/NEXT.md',
        'tools/build.py', 'verification/input.json']
