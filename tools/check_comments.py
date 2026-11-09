"""检查本轮维护文件的中文职责注释；不改写源码、不处理第三方许可。"""
import argparse
import ast
import io
import json
from pathlib import Path
import re
import subprocess
import tokenize


def chinese(text: str) -> bool:
    """判断说明是否包含中文；协议名、API名可以与中文共同出现。"""
    return bool(re.search(r'[\u3400-\u9fff]', text))


def inspect_text(name: str, text: str) -> list[dict]:
    """检查模块和具名定义的职责注释，程序字符串不参与语言判断。"""
    issues = []
    if name.endswith('.py'):
        tree = ast.parse(text, filename=name)
        for node in ast.walk(tree):
            if isinstance(node, (ast.Module, ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
                if not chinese(ast.get_docstring(node) or ''):
                    issues.append({'file': name, 'line': getattr(node, 'lineno', 1), 'message': '缺少中文职责文档'})
        comments = [(t.start[0], t.string) for t in tokenize.generate_tokens(io.StringIO(text).readline) if t.type == tokenize.COMMENT]
    else:
        lines = text.splitlines()
        comments = [(i+1, line.strip()) for i, line in enumerate(lines) if line.lstrip().startswith('#')]
        if not any(chinese(line) for line in lines[:6] if line.lstrip().startswith('##')):
            issues.append({'file': name, 'line': 1, 'message': '缺少中文模块说明'})
        for i, line in enumerate(lines):
            if re.match(r'^\s*(static\s+)?func\s+\w+', line):
                previous = i - 1
                while previous >= 0 and (not lines[previous].strip() or lines[previous].lstrip().startswith('@')):
                    previous -= 1
                if previous < 0 or not lines[previous].lstrip().startswith('##') or not chinese(lines[previous]):
                    issues.append({'file': name, 'line': i+1, 'message': '具名函数缺少中文职责注释'})
    for line, comment in comments:
        if comment.startswith('#!') or comment.startswith(('# type:', '# noqa', '# pylint:')):
            continue
        if re.search('[A-Za-z]', comment) and not chinese(comment):
            issues.append({'file': name, 'line': line, 'message': '说明性注释仍为纯英文'})
    return issues


def changed_paths(root: Path, base: str) -> list[str]:
    """读取指定基线之后的源码变更，既检查已提交文件也检查新文件。"""
    tracked = subprocess.check_output(['git', 'diff', '--name-only', '--diff-filter=ACMR', '-z', base], cwd=root)
    new = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard', '-z'], cwd=root)
    names = set((tracked + new).decode().split('\0'))
    return sorted(p for p in names if p.endswith(('.gd', '.py')) and p.startswith(('game/', 'tools/', 'tests/')) and '/.godot/' not in p)


def main() -> int:
    """输出可供本地构建读取的结果；有缺失注释时返回非零状态。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    paths = changed_paths(root, args.base)
    issues = []
    for path in paths:
        issues.extend(inspect_text(path, (root/path).read_text()))
    print(json.dumps({'ok': not issues, 'checked_files': paths, 'issues': issues}, ensure_ascii=False, indent=2))
    return 1 if issues else 0


if __name__ == '__main__':
    raise SystemExit(main())
