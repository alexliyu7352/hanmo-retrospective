"""确认中文注释检查能识别真实职责文档，并不误判程序字符串。"""
from pathlib import Path
import importlib.util


def helper():
    """加载实际检查器，不在测试中实现替代规则。"""
    path = Path(__file__).resolve().parents[1] / 'tools/check_comments.py'
    assert path.is_file(), '中文注释检查器尚未实现'
    spec = importlib.util.spec_from_file_location('comment_check', path)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def test_gd_documentation():
    """具名函数需要中文职责注释，缺失或仅英文时拒绝。"""
    h = helper()
    assert not h.inspect_text('a.gd', '## 模块说明\nextends Node\n## 读取当前结果。\nfunc value(): return "PASS"\n')
    assert h.inspect_text('a.gd', '## Module\nextends Node\nfunc value(): return 1\n')


def test_python_documentation():
    """英文协议字符串不是注释；函数文档必须说明职责。"""
    h = helper()
    assert not h.inspect_text('a.py', '"""模块说明。"""\ndef value():\n    """读取当前结果。"""\n    return "PASS"\n')
    assert h.inspect_text('a.py', '"""Module."""\ndef value():\n    return 1\n')
