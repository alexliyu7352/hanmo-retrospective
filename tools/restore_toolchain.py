#!/usr/bin/env python3
"""只恢复固定 Godot Linux/Web 工具链；完整校验后启用，不修改系统或旧安装。"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shlex
import shutil
import stat
import subprocess
import tempfile
import urllib.error
import urllib.request
import zipfile

REPO = Path(__file__).resolve().parents[1]
MANIFEST = REPO / 'tools/toolchain/godot-4.7.2.json'
CHUNK = 1024 * 1024


def _load_manifest() -> dict:
    """读取版本受控的唯一清单，不从下载内容推断或升级版本。"""
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    if (manifest['format'] != 'HANMO-TOOLCHAIN/1' or manifest['version'] != '4.7.2'
            or manifest['editor_build'] != '4.7.2.stable.official.ed1daf0bf'
            or manifest['template_version'] != '4.7.2.stable'
            or manifest['install_directory'] != 'godot-4.7.2'):
        raise RuntimeError('unsupported fixed toolchain manifest')
    return manifest


def _safe_path(path: Path) -> Path:
    """规范化用户路径并拒绝符号链接祖先，避免写入重定向到另一位置。"""
    path = Path(os.path.abspath(path.expanduser()))
    for parent in [path, *path.parents]:
        if parent.is_symlink():
            raise RuntimeError(f'unsafe symlink path: {parent}')
    return path


def _directory(path: Path) -> None:
    """仅创建可写目录；只读或非目录目标报错，不改变既有权限。"""
    path = _safe_path(path)
    nearest = path
    while not nearest.exists():
        nearest = nearest.parent
    if not nearest.is_dir():
        raise RuntimeError(f'not a directory: {nearest}')
    if not nearest.stat().st_mode & 0o222 or not os.access(nearest, os.W_OK):
        raise RuntimeError(f'not writable directory: {nearest}')
    path.mkdir(parents=True, exist_ok=True)
    if not path.is_dir():
        raise RuntimeError(f'not a directory: {path}')


def _verify_file(path: Path, item: dict) -> dict:
    """流式比对真实长度与 SHA256；缺失、链接或未知字节均不能通过。"""
    path = _safe_path(path)
    if not path.is_file():
        raise RuntimeError(f'missing regular file: {path}')
    size = path.stat().st_size
    if size != item['bytes']:
        raise RuntimeError(f'size mismatch: {path}: expected {item["bytes"]}, got {size}')
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(CHUNK), b''):
            digest.update(chunk)
    actual = digest.hexdigest()
    if actual != item['sha256']:
        raise RuntimeError(f'SHA256 mismatch: {path}: expected {item["sha256"]}, got {actual}')
    return {'path': str(path), 'bytes': size, 'sha256': actual}


def _validate_zip(archive: zipfile.ZipFile, item: dict) -> None:
    """全量检查 ZIP 路径、类型、精确成员与 CRC；清单中的其他平台仅校验不解压。"""
    entries = item['entries']
    seen = set()
    for info in archive.infolist():
        name = info.filename
        parts = name.split('/')
        mode = info.external_attr >> 16
        if ('\\' in name or '\x00' in name or name.startswith('/')
                or ':' in name or any(part in ('', '.', '..') for part in parts)):
            raise RuntimeError(f'unsafe archive path: {name}')
        if stat.S_IFMT(mode) not in (0, stat.S_IFREG) or info.is_dir():
            raise RuntimeError(f'unsafe archive type or symlink: {name}')
        if name in seen or name not in entries or info.flag_bits & 1:
            raise RuntimeError(f'unexpected archive entry: {name}')
        if info.file_size != entries[name]['bytes']:
            raise RuntimeError(f'size mismatch in archive member: {name}')
        seen.add(name)
    missing = set(entries) - seen
    if missing:
        raise RuntimeError(f'missing archive entries: {sorted(missing)}')
    corrupt = archive.testzip()
    if corrupt:
        raise RuntimeError(f'CRC mismatch in archive: {corrupt}')



class _OfficialRedirects(urllib.request.HTTPRedirectHandler):
    """约束每个官方重定向；禁止向未列出的 HTTPS 或任何 HTTP 地址发出请求。"""
    def __init__(self, allowed_urls: set[str]) -> None:
        """固定本次归档允许的精确入口与最终对象地址。"""
        super().__init__()
        self.allowed_urls = frozenset(allowed_urls)

    def redirect_request(self, request, response, code, message, headers, new_url):
        """跟随 Location 之前检查精确 HTTPS 白名单，不允许先请求后回到合法地址。"""
        if not new_url.startswith('https://') or new_url not in self.allowed_urls:
            raise RuntimeError(f'official HTTPS redirect rejected before request: {new_url}')
        return super().redirect_request(request, response, code, message, headers, new_url)


def _open_official(item: dict):
    """以受约束标准库 opener 请求固定入口，不使用默认无约束 redirect handler。"""
    url = item['official_url']
    if not url.startswith('https://downloads.godotengine.org/'):
        raise RuntimeError('official source must use fixed Godot HTTPS entry')
    allowed_urls = {url, item.get('official_redirect_url', url)}
    opener = urllib.request.build_opener(_OfficialRedirects(allowed_urls))
    return opener.open(url, timeout=60)


def _acquire(root: Path, item: dict, manifest: dict, source: str, archives: Path | None) -> Path:
    """以独立 .part 保留失败传输，完整身份匹配后定名；既有异名字节绝不覆盖。"""
    target = root / 'downloads' / item['asset']
    _safe_path(target)
    if target.exists():
        _verify_file(target, item)
        return target
    if source == 'local':
        if archives is None:
            raise RuntimeError('local source requires --archives PATH')
        original = archives / item['asset']
        _verify_file(original, item)
    _directory(target.parent)
    descriptor, name = tempfile.mkstemp(prefix=item['asset'] + '.', suffix='.part', dir=target.parent)
    os.close(descriptor)
    partial = Path(name)
    try:
        if source == 'local':
            with original.open('rb') as incoming, partial.open('wb') as outgoing:
                shutil.copyfileobj(incoming, outgoing, CHUNK)
        elif source == 'github':
            gh = shutil.which('gh')
            if gh is None:
                raise RuntimeError('github source requires existing gh and authenticated private repository access')
            result = subprocess.run([gh, 'release', 'download', manifest['release']['tag'],
                                     '--repo', manifest['release']['repo'], '--pattern', item['asset'],
                                     '--output', str(partial), '--clobber'],
                                    capture_output=True, text=True, timeout=3600)
            if result.returncode:
                raise RuntimeError(f'gh download failed ({result.returncode}): {result.stderr.strip()}')
        else:
            with _open_official(item) as incoming, partial.open('wb') as outgoing:
                final_url = incoming.geturl() if hasattr(incoming, 'geturl') else item['official_url']
                allowed_urls = {item['official_url'], item.get('official_redirect_url', item['official_url'])}
                if not final_url.startswith('https://') or final_url not in allowed_urls:
                    raise RuntimeError(f'official HTTPS redirect rejected: {final_url}')
                total = 0
                while chunk := incoming.read(CHUNK):
                    total += len(chunk)
                    if total > item['bytes']:
                        raise RuntimeError(f'size mismatch: official download exceeds {item["bytes"]}')
                    outgoing.write(chunk)
        _verify_file(partial, item)
        # 硬链接创建最终名称具有排他性；并发或用户新增的文件不能被覆盖。
        try:
            os.link(partial, target)
        except FileExistsError:
            _verify_file(target, item)
        partial.unlink()
        return target
    except (RuntimeError, OSError, subprocess.SubprocessError, urllib.error.URLError) as exc:
        raise RuntimeError(f'{source} download failed; retained {partial}: {exc}') from exc


def _environment(root: Path, manifest: dict) -> str:
    """生成 Bash 可 source 入口；源码只取显式 HANMO_ROOT，不读写调用者位置参数。"""
    install = root / manifest['install_directory']
    editor = next(item['install'] for item in manifest['archives']['editor']['entries'].values()
                  if item['install'])
    lines = [
        '# 固定工具链环境；仅影响 source 的当前 shell，不写全局配置。',
        'export HANMO_TOOLCHAIN_ROOT=' + shlex.quote(str(root)),
        'export HANMO_TOOLCHAIN=' + shlex.quote(str(install)),
        'export HANMO_ROOT="${HANMO_ROOT:-}"',
        'export HANMO_GODOT_EDITOR=' + shlex.quote(str(install / editor)),
        'export GODOT_BIN="$HANMO_GODOT_EDITOR"',
        'export XDG_DATA_HOME="$HANMO_TOOLCHAIN/runtime/data"',
        'export XDG_CONFIG_HOME="$HANMO_TOOLCHAIN/runtime/config"',
        'export XDG_CACHE_HOME="$HANMO_TOOLCHAIN/runtime/cache"',
        'export TMPDIR="$HANMO_TOOLCHAIN/runtime/tmp"',
        'export TMP="$TMPDIR"',
        'export TEMP="$TMPDIR"',
        '',
    ]
    return '\n'.join(lines)


def _verify_install(install: Path, root: Path, manifest: dict) -> dict:
    """只读复核编辑器、八项模板与环境入口；未知程序永不在这里执行。"""
    _safe_path(install)
    if not install.is_dir():
        raise RuntimeError(f'missing installation: {install}')
    output = {'status': 'PASS', 'root': str(root), 'build': manifest['editor_build'],
              'env': str(root / manifest['install_directory'] / 'env.sh'),
              'xdg_data_home': str(root / manifest['install_directory'] / 'runtime/data'),
              'templates': []}
    for kind, archive in manifest['archives'].items():
        for name, item in archive['entries'].items():
            if item['install'] is None:
                continue
            relative = PurePosixPath(item['install'])
            if relative.is_absolute() or '..' in relative.parts:
                raise RuntimeError(f'unsafe manifest install path: {relative}')
            path = install / relative
            verified = _verify_file(path, item)
            if item.get('executable') and not os.access(path, os.X_OK):
                raise RuntimeError(f'executable permission missing: {path}')
            if kind == 'editor':
                output['editor'] = str(root / manifest['install_directory'] / relative)
            else:
                verified['name'] = name.split('/')[-1]
                output['templates'].append(verified)
    env = _safe_path(install / 'env.sh')
    if not env.is_file() or env.read_text(encoding='utf-8') != _environment(root, manifest):
        raise RuntimeError(f'env.sh mismatch or missing: {env}')
    template = install / 'runtime/data/godot/export_templates' / manifest['template_version'] / 'version.txt'
    if template.read_text(encoding='utf-8').strip() != manifest['template_version']:
        raise RuntimeError('template version mismatch')
    return output


def verify_toolchain(root: Path) -> dict:
    """只校验固定安装和保留的完整归档，既不下载也不执行二进制。"""
    manifest = _load_manifest()
    root = _safe_path(root)
    result = _verify_install(root / manifest['install_directory'], root, manifest)
    result['archives'] = {kind: _verify_file(root / 'downloads' / item['asset'], item)
                          for kind, item in manifest['archives'].items()}
    return result



def _activate(staging: Path, target: Path) -> None:
    """使用 Linux 排他原子重命名；不支持时失败，不以覆盖性 rename 降级。"""
    library = ctypes.CDLL(None, use_errno=True)
    rename = getattr(library, 'renameat2', None)
    if rename is None:
        raise RuntimeError('atomic no-clobber activation requires Linux renameat2')
    rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
    rename.restype = ctypes.c_int
    if rename(-100, os.fsencode(staging), -100, os.fsencode(target), 1) != 0:
        code = ctypes.get_errno()
        raise RuntimeError(f'atomic activation failed; target preserved: {target}: {os.strerror(code)}')


def restore_toolchain(root: Path, source: str, archives: Path | None = None) -> dict:
    """恢复到显式固定目录；成功安装原子启用，冲突文件保留且匹配安装幂等复用。"""
    if source not in ('local', 'github', 'official'):
        raise RuntimeError(f'unsupported source: {source}')
    manifest = _load_manifest()
    root = _safe_path(root)
    target = root / manifest['install_directory']
    _safe_path(target)
    if target.exists():
        result = verify_toolchain(root)
        result['reused'] = True
        return result
    _directory(root)
    paths = {kind: _acquire(root, item, manifest, source, archives)
             for kind, item in manifest['archives'].items()}
    staging = Path(tempfile.mkdtemp(prefix='.staging-godot-4.7.2-', dir=root))
    try:
        for kind, path in paths.items():
            with zipfile.ZipFile(path) as archive:
                item = manifest['archives'][kind]
                _validate_zip(archive, item)
                for name, entry in item['entries'].items():
                    if entry['install'] is None:
                        continue
                    relative = PurePosixPath(entry['install'])
                    if relative.is_absolute() or '..' in relative.parts:
                        raise RuntimeError(f'unsafe manifest install path: {relative}')
                    destination = staging / relative
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    with archive.open(name) as incoming, destination.open('xb') as outgoing:
                        shutil.copyfileobj(incoming, outgoing, CHUNK)
                    _verify_file(destination, entry)
                    destination.chmod(0o755 if entry.get('executable') else 0o644)
        for name in ('config', 'cache', 'tmp'):
            (staging / 'runtime' / name).mkdir(parents=True, exist_ok=True)
        (staging / 'env.sh').write_text(_environment(root, manifest), encoding='utf-8')
        checked = _verify_install(staging, root, manifest)
        editor = staging / Path(checked['editor']).relative_to(target)
        environment = dict(os.environ, XDG_DATA_HOME=str(staging / 'runtime/data'),
                           XDG_CONFIG_HOME=str(staging / 'runtime/config'),
                           XDG_CACHE_HOME=str(staging / 'runtime/cache'),
                           TMPDIR=str(staging / 'runtime/tmp'))
        result = subprocess.run([str(editor), '--version'], env=environment,
                                capture_output=True, text=True, timeout=30)
        if result.returncode != 0 or result.stdout.strip() != manifest['editor_build']:
            raise RuntimeError(f'editor version mismatch: exit {result.returncode}: {result.stdout.strip()} {result.stderr.strip()}')
        if target.exists() or target.is_symlink():
            raise RuntimeError(f'installation appeared during restore; retained staging: {staging}')
        _activate(staging, target)
        result = verify_toolchain(root)
        result['reused'] = False
        result['executed_version'] = manifest['editor_build']
        return result
    except (RuntimeError, OSError, zipfile.BadZipFile, subprocess.SubprocessError) as exc:
        raise RuntimeError(f'restore failed; retained staging {staging}: {exc}') from exc


def _default_root() -> Path:
    """默认使用显式环境或用户数据目录，不从临时目录或旧 env 推断位置。"""
    if os.environ.get('HANMO_TOOLCHAIN_ROOT'):
        return Path(os.environ['HANMO_TOOLCHAIN_ROOT']).expanduser()
    return Path.home() / '.local/share/hanmo-toolchain'


def main(argv: list[str] | None = None) -> int:
    """提供机器可读 CLI 回执；校验失败非零退出，--check 禁止下载和运行程序。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=_default_root())
    parser.add_argument('--source', choices=('local', 'github', 'official'), default='github')
    parser.add_argument('--archives', type=Path)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args(argv)
    try:
        result = verify_toolchain(args.root) if args.check else restore_toolchain(args.root, args.source, args.archives)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (RuntimeError, OSError, ValueError, KeyError) as exc:
        print(json.dumps({'status': 'FAIL', 'root': str(args.root), 'error': str(exc)}, ensure_ascii=False, indent=2))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
