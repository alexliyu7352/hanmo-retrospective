#!/usr/bin/env python3
"""仅检查静态文件及引用；不把语法旁检宣称为 GDScript 编译或游戏测试。"""
from __future__ import annotations
import argparse
import ast
from html.parser import HTMLParser
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[1]
EXPECTED_MISSING_REFERENCES = {
    ('game/tests/test_presentation_registry.gd', 'assets/art/missing.png'),
}


class InlineScripts(HTMLParser):
    """收集导出网页内嵌脚本，交给真实 Node 语法检查。"""
    def __init__(self) -> None:
        """记录解析器是否处于内嵌脚本标签中。"""
        super().__init__()
        self.active = False
        self.scripts: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str,str | None]]) -> None:
        """外部脚本地址不作为内嵌 JavaScript 正文解析。"""
        if tag == 'script':
            self.active = 'src' not in dict(attrs)

    def handle_endtag(self, tag: str) -> None:
        """脚本标签闭合时停止收集正文。"""
        if tag == 'script':
            self.active = False

    def handle_data(self, data: str) -> None:
        """只保留内嵌脚本的实际正文。"""
        if self.active:
            self.scripts.append(data)


EXCLUDED_DIRECTORIES = {'.git', '.godot', '.worktrees', '__pycache__', 'build', 'builds', 'venv'}


def recursive_files(folder: Path, suffixes: set[str]) -> list[Path]:
    """受控递归检查业务子目录；已知产物目录和外部符号链接不作为当前工程源码。"""
    files = []
    for current, directories, names in os.walk(folder, followlinks=False):
        directories[:] = sorted(name for name in directories if name not in EXCLUDED_DIRECTORIES
                                and not (Path(current) / name).is_symlink())
        files.extend(Path(current) / name for name in sorted(names)
                     if Path(name).suffix in suffixes and not (Path(current) / name).is_symlink())
    return files


def check(scope: str = 'all') -> dict:
    """检查具体文件与引用，单独声明未执行引擎验证，不以静态通过冒充玩法验收。"""
    if scope not in {'shared', 'web', 'all'}:
        raise ValueError('未知静态检查 scope：' + scope)
    errors: list[str] = []
    blocked: list[str] = []
    godot_files = []; python_files = []
    if scope in {'shared', 'all'}:
        expected = ['game/project.godot','game/main.tscn','game/core/world_kernel.gd',
                    'game/core/save_codec.gd','game/storage/storage_service.gd','game/ui/main.gd',
                    'game/ui/landscape.gd','game/export_presets.cfg']
        for name in expected:
            if not (ROOT/name).is_file():
                errors.append('Missing '+name)
        godot_files = recursive_files(ROOT / 'game', {'.gd'})
        for path in godot_files + recursive_files(ROOT / 'game', {'.tscn'}):
            text = path.read_text('utf-8')
            for match in re.finditer(r'"res://([^"\n]+)"',text):
                ref = match.group(1)
                if ref.startswith('assets/fonts/'):
                    continue  # 字体由用户明确准备，依赖完整性由正式字体门禁检查。
                if (str(path.relative_to(ROOT)), ref) in EXPECTED_MISSING_REFERENCES:
                    continue  # 仅允许这个注册表负向夹具故意缺失，不豁免整份测试文件。
                directory_call = re.search(r'DirAccess\.(?:get_files_at|get_directories_at|open|dir_exists_absolute)\(\s*$', text[:match.start()])
                target = ROOT / 'game' / ref
                exists = target.is_dir() if directory_call or ref.endswith('/') else target.is_file()
                if not exists:
                    errors.append(f'{path.relative_to(ROOT)}: missing reference {ref}')
            if path.suffix == '.gd':
                lines = text.splitlines()
                for index,line in enumerate(lines):
                    if re.match(r'^(?:static )?func ',line) and (index == 0 or not lines[index-1].lstrip().startswith('##')):
                        errors.append(f'{path.name}:{index+1}: missing function responsibility comment')
        python_files = recursive_files(ROOT / 'tools', {'.py'}) + recursive_files(ROOT / 'tests', {'.py'})
        for path in python_files:
            try:
                ast.parse(path.read_text('utf-8'),filename=str(path))
            except SyntaxError as exc:
                errors.append(str(exc))
    if scope in {'web', 'all'}:
        for name in ['web/storage_bridge.js', 'web/shell.html']:
            if not (ROOT / name).is_file():
                errors.append('Missing ' + name)
        shell = InlineScripts()
        if (ROOT / 'web/shell.html').is_file():
            shell.feed((ROOT / 'web/shell.html').read_text('utf-8'))
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'shell.js'
            script.write_text('\n'.join(shell.scripts), encoding='utf-8')
            for path in [ROOT / 'web/storage_bridge.js', script]:
                if not path.is_file():
                    continue
                try:
                    result = subprocess.run(['node', '--check', str(path)], capture_output=True, text=True)
                except OSError as exc:
                    blocked.append('JavaScript 检查依赖缺失：' + str(exc))
                    continue
                if result.returncode:
                    errors.append(result.stderr)
    result = {'scope':scope, 'scope_description':'file references, Python parsing, JavaScript syntax, GDScript function comments ONLY',
              'gdscript_parser':'NOT_PERFORMED_BY_THIS_STATIC_TOOL', 'godot_runtime':'SEE_SEPARATE_RUNTIME_LOGS',
              'godot_source_files':len(godot_files), 'python_files':len(python_files),
              'javascript_syntax': 'NOT_RUN' if scope == 'shared' else 'BLOCKED' if blocked else 'CHECKED',
              'status': 'FAIL' if errors else 'BLOCKED' if blocked else 'PASS',
              'ok':not errors and not blocked,'errors':errors, 'blocked':blocked}
    return result


def main() -> int:
    """写出静态检查的确切范围与结果，不声明游戏运行或图形验收已完成。"""
    global ROOT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--scope', choices=['shared', 'web', 'all'], default='all')
    args = parser.parse_args()
    ROOT = args.root.resolve()
    result = check(scope=args.scope)
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        if args.output.exists():
            raise RuntimeError('已有静态回执不可覆盖：' + str(args.output))
        args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False,indent=2))
    return 0 if result['ok'] else 1 if result['errors'] else 2


if __name__ == '__main__':
    raise SystemExit(main())
