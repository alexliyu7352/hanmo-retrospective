"""生成可复核的验证身份与原子回执；证据产物不反过来污染源树身份。"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import subprocess
import uuid

EXCLUDED_PARTS = {'.git', '.godot', '.worktrees', '__pycache__', '.pytest_cache', '.local'}
EXCLUDED_ROOTS = {'build', 'builds', 'venv', '.superpowers', 'verification', 'logs',
                  'backups', 'cache', 'config', 'tmp', 'data'}


def sha256_file(path: Path) -> str:
    """流式计算文件摘要，避免把引擎或资源二进制整体装入内存。"""
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def source_identity(root: Path) -> dict:
    """绑定 HEAD 与暂存、未暂存和新增源码；既有证据、缓存与 SDK 不进入差异摘要。"""
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    changed = subprocess.check_output(['git', 'diff', 'HEAD', '--name-only', '-z'], cwd=root).decode().split('\0')
    untracked = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard', '-z'], cwd=root).decode().split('\0')
    names = sorted(name for name in set(changed + untracked) if name and
                   not EXCLUDED_PARTS.intersection(Path(name).parts) and
                   Path(name).parts[0] not in EXCLUDED_ROOTS and
                   not name.startswith(('tools/godot/', 'tools/fonts/', 'tools/art-models/',
                                        'game/data/generated/', 'game/assets/fonts/')))
    digest = hashlib.sha256()
    changes = []
    for name in names:
        path = root / name
        actual = os.readlink(path).encode() if path.is_symlink() else path.read_bytes() if path.is_file() else None
        executable = bool(path.lstat().st_mode & 0o111) if path.exists() or path.is_symlink() else None
        record = {'path': name, 'sha256': hashlib.sha256(actual).hexdigest() if actual is not None else None,
                  'executable': executable, 'symlink': path.is_symlink()}
        changes.append(record)
        digest.update(json.dumps(record, sort_keys=True, ensure_ascii=False).encode() + b'\n')
    return {'commit': commit, 'dirty_diff_sha256': digest.hexdigest(), 'dirty_paths': changes,
            'identity_scope': 'Git 源码/内容/文档；不含 verification/.superpowers/缓存/SDK/准备产物'}


def file_set_identity(root: Path, paths: list[Path]) -> dict:
    """按相对路径排序绑定文件集合；缺文件由调用门禁处理，不借旧摘要宣称成功。"""
    records = [{'path': str(path.relative_to(root)), 'sha256': sha256_file(path)}
               for path in sorted(set(paths)) if path.is_file()]
    payload = json.dumps(records, sort_keys=True, ensure_ascii=False, separators=(',', ':')).encode()
    return {'sha256': hashlib.sha256(payload).hexdigest(), 'files': records}


def schema_identity(root: Path) -> dict:
    """同时绑定作者/表现Schema及受信能力描述，规则边界变化必须使旧回执失效。"""
    paths = list((root / 'schemas').rglob('*.json'))
    paths += list((root / 'game/data').glob('*.schema.json'))
    paths += [root / 'game/data/rule_capabilities.json', root / 'game/data/module_contracts.json']
    return file_set_identity(root, paths)


def catalog_identity(root: Path) -> dict:
    """如实记录当前 legacy 单目录身份；没有 pack-set 时绝不伪造包锁。"""
    pack_sets = list((root / 'content').rglob('*.pack-set.json'))
    if pack_sets:
        return {'mode': 'pack-set', **file_set_identity(root, pack_sets)}
    return {'mode': 'legacy-single-directory', 'directory': 'content/foundation',
            'pack_set': None, **file_set_identity(root, list((root / 'content/foundation').glob('*.json')))}



def runtime_inputs_identity(root: Path) -> dict:
    """分别绑定生成内容、稳定字体/许可与引擎导入配置；后者须在合法导入完成后冻结。"""
    generated = list((root / 'game/data/generated').rglob('*'))
    candidates = list((root / 'game/assets/fonts').rglob('*'))
    fonts = [path for path in candidates if path.suffix != '.import'] + list((root / 'build/font').rglob('*'))
    imports = [path for path in candidates if path.suffix == '.import']
    return {'prepared_catalogs': file_set_identity(root, generated),
            'prepared_fonts': file_set_identity(root, fonts),
            'prepared_font_imports': file_set_identity(root, imports)}


def write_report(path: Path, report: dict) -> None:
    """在唯一运行目录中先写临时文件再原子替换；已存在回执是历史证据，不可覆写。"""
    if path.exists():
        raise RuntimeError('已有验证回执不可覆盖：' + str(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.' + uuid.uuid4().hex + '.tmp')
    try:
        with temporary.open('x', encoding='utf-8') as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write('\n'); stream.flush(); os.fsync(stream.fileno())
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def report_exit_code(report: dict) -> int:
    """仅必需阶段全 PASS 返回零；失败优先为一，阻断或未运行返回二。"""
    required = [report['stages'][name] for name in report['required_stages']]
    if any(stage['status'] == 'FAIL' for stage in required):
        return 1
    return 0 if required and all(stage['status'] == 'PASS' for stage in required) and report['ok'] else 2
