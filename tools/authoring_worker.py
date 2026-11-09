#!/usr/bin/env python3
"""本地作者及声明包工作进程；沿同一行协议服务工坊，不执行游戏。"""
import json
from pathlib import Path
import sys


def main() -> int:
    """加载既有作者依赖并从本地标准流处理严格JSON行，缺依赖时明确失败。"""
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    try:
        from tools.content.editor_protocol import run_stream
    except ImportError as exc:
        print(json.dumps({'protocol': 'HM-AUTHOR/1', 'id': None, 'ok': False,
                          'runtime_verified': False, 'document': None,
                          'error': {'code': 'DEPENDENCY_MISSING', 'message': str(exc),
                                    'details': {'install': 'python -m pip install -r requirements-content.txt'}}}))
        return 2
    return run_stream(sys.stdin.buffer, sys.stdout)


if __name__ == '__main__':
    raise SystemExit(main())
