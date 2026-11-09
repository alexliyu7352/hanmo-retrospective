"""以可信目录句柄保存作者原件；校验来源、保留恢复副本并核对实际写入。"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import stat
import secrets

from .document import DocumentError
from .io import MAX_BYTES, canonical, prepare_pack


class _TrustedDirectory:
    """逐级持有无链接目录FD，路径改名或替换后仍不能把写入转向其他目录。"""
    def __init__(self, path: Path, *, create: bool = False):
        """固定从根到目标父目录的inode链；平台缺少必要原语时明确拒绝。"""
        if not hasattr(os, 'O_DIRECTORY') or not hasattr(os, 'O_NOFOLLOW'):
            raise DocumentError('UNSUPPORTED_SAFE_WRITE', 'This platform lacks directory-relative no-follow writes')
        if '..' in Path(path).parts:
            raise DocumentError('INVALID_DESTINATION', 'Parent directory components must not contain ..')
        self.path = Path(os.path.abspath(path))
        self._fds = []
        self._chain = []
        flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
        try:
            current = os.open(self.path.anchor, flags)
            self._fds.append(current)
            for name in self.path.parts[1:]:
                if create:
                    try:
                        os.mkdir(name, mode=0o700, dir_fd=current)
                    except FileExistsError:
                        pass
                child = os.open(name, flags, dir_fd=current)
                self._fds.append(child)
                self._chain.append((current, name, child))
                current = child
            self.fd = current
            self.check()
        except (OSError, DocumentError) as exc:
            self.close()
            if isinstance(exc, DocumentError):
                raise
            raise DocumentError('SOURCE_CHANGED', 'Parent directory is unavailable or contains a link', path=str(path)) from exc

    def check(self) -> None:
        """逐段核对原路径仍指向持有的目录，拒绝同字节目录或链接替换。"""
        try:
            for parent, name, child in self._chain:
                current = os.stat(name, dir_fd=parent, follow_symlinks=False)
                expected = os.fstat(child)
                if (not stat.S_ISDIR(current.st_mode) or
                        (current.st_dev, current.st_ino) != (expected.st_dev, expected.st_ino)):
                    raise OSError('Parent directory identity changed')
        except OSError as exc:
            raise DocumentError('SOURCE_CHANGED', 'Parent directory identity changed', path=str(self.path)) from exc

    def close(self) -> None:
        """逆序关闭只属于此次操作的目录句柄。"""
        for descriptor in reversed(self._fds):
            os.close(descriptor)
        self._fds.clear()

    def __enter__(self):
        """返回已经固定身份的目录上下文。"""
        return self

    def __exit__(self, *_error):
        """无论保存成功或失败都释放目录句柄。"""
        self.close()

    def unlink(self, name: str) -> None:
        """只在原可信目录清理自有临时文件，不沿当前路径或链接删除外部数据。"""
        try:
            os.unlink(name, dir_fd=self.fd)
        except FileNotFoundError:
            pass


def _read_at(directory: _TrustedDirectory, name: str, *, max_bytes: int = MAX_BYTES) -> bytes:
    """通过目录FD读取有界普通文件，拒绝链接、FIFO及读取期间的路径替换。"""
    directory.check()
    try:
        descriptor = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory.fd)
        with os.fdopen(descriptor, 'rb') as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                raise DocumentError('SOURCE_CHANGED', 'Source is no longer a regular file')
            raw = stream.read(max_bytes + 1)
        if len(raw) > max_bytes:
            raise DocumentError('SOURCE_CHANGED', 'Source now exceeds the author-file size limit')
        directory.check()
        return raw
    except OSError as exc:
        raise DocumentError('SOURCE_CHANGED', 'Source is no longer readable without links') from exc


def _read_bytes(path: Path, *, _max_bytes: int = MAX_BYTES) -> bytes:
    """用固定父目录和无链接打开读取原件，不信任先检查后重解析的路径。"""
    with _TrustedDirectory(path.parent) as directory:
        return _read_at(directory, path.name, max_bytes=_max_bytes)


def _check_source(document) -> bytes:
    """要求原件字节与最后打开或保存的摘要相同，包括格式空白。"""
    raw = _read_bytes(document.path)
    if hashlib.sha256(raw).hexdigest() != document.source_sha256:
        raise DocumentError('SOURCE_CHANGED', 'Source was changed outside this document; reload or save as',
                            path=str(document.path))
    return raw


def _stage(path: Path, data: bytes, mode: int, *, directory: _TrustedDirectory, max_bytes: int = MAX_BYTES) -> str:
    """在持有目录内排他创建暂存文件并核读；路径被替换也不会写入外部。"""
    directory.check()
    name = '.author-' + secrets.token_hex(16) + '.tmp'
    created = False
    try:
        descriptor = os.open(name, os.O_CREAT | os.O_EXCL | os.O_WRONLY | os.O_NOFOLLOW,
                             mode, dir_fd=directory.fd)
        created = True
        with os.fdopen(descriptor, 'wb') as stream:
            os.fchmod(stream.fileno(), mode)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        if _read_at(directory, name, max_bytes=max_bytes) != data:
            raise OSError('Staged author-file readback did not match')
        return name
    except (OSError, DocumentError):
        if created:
            directory.unlink(name)
        raise


def _write_author_bytes(target: Path, data: bytes, *, expected_sha256: str | None = None,
                        before_publish=None, _max_bytes: int = MAX_BYTES) -> str | None:
    """用同一父目录FD保存原件或排他另存，供作者内容与manifest共用。"""
    if type(_max_bytes) is not int or not 0 < _max_bytes <= 64 * 1024 * 1024 or len(data) > _max_bytes:
        raise DocumentError('DOCUMENT_LIMIT', 'Write exceeds the trusted operation byte budget')
    same = expected_sha256 is not None
    previous = '.' + target.name + '.hanmo.previous' if same else None
    temporary = backup = None
    with _TrustedDirectory(target.parent, create=not same) as directory:
        try:
            old = _read_at(directory, target.name, max_bytes=_max_bytes) if same else None
            if same and hashlib.sha256(old).hexdigest() != expected_sha256:
                raise DocumentError('SOURCE_CHANGED', 'Source changed before the trusted write')
            mode = stat.S_IMODE(os.stat(target.name, dir_fd=directory.fd, follow_symlinks=False).st_mode) if same else 0o600
            temporary = _stage(target, data, mode, directory=directory, max_bytes=_max_bytes)
            if before_publish is not None:
                before_publish()
            if same:
                if _read_at(directory, target.name, max_bytes=_max_bytes) != old:
                    raise DocumentError('SOURCE_CHANGED', 'Source changed during staging')
                try:
                    recovery = os.stat(previous, dir_fd=directory.fd, follow_symlinks=False)
                    if not stat.S_ISREG(recovery.st_mode):
                        raise DocumentError('INVALID_DESTINATION', 'Recovery file must be a regular file')
                except FileNotFoundError:
                    pass
                backup = _stage(target.with_name(previous), old, mode, directory=directory, max_bytes=_max_bytes)
                directory.check()
                os.replace(backup, previous, src_dir_fd=directory.fd, dst_dir_fd=directory.fd)
                backup = None
                if _read_at(directory, target.name, max_bytes=_max_bytes) != old:
                    raise DocumentError('SOURCE_CHANGED', 'Source changed before replacement')
                if before_publish is not None:
                    before_publish()
                directory.check()
                os.replace(temporary, target.name, src_dir_fd=directory.fd, dst_dir_fd=directory.fd)
                temporary = None
            else:
                directory.check()
                try:
                    os.link(temporary, target.name, src_dir_fd=directory.fd,
                            dst_dir_fd=directory.fd, follow_symlinks=False)
                except FileExistsError as exc:
                    raise DocumentError('DESTINATION_EXISTS', 'Destination appeared during Save As') from exc
                directory.unlink(temporary)
                temporary = None
            if _read_at(directory, target.name, max_bytes=_max_bytes) != data:
                raise OSError('Saved author-file readback did not match')
        except DocumentError:
            raise
        except OSError as exc:
            raise DocumentError('WRITE_FAILED', 'Author file was not confirmed saved: ' + str(exc), path=str(target)) from exc
        finally:
            for leftover in (temporary, backup):
                if leftover is not None:
                    directory.unlink(leftover)
    return str(target.with_name(previous)) if previous else None


def save_document(document, destination=None) -> dict:
    """保存允许语义未完的草稿；真实文件全部写入确认后才更新文档摘要。"""
    target = document.path if destination is None else Path(destination).absolute()
    if target.suffix != '.json' or target.name.endswith('.hmcatalog.json'):
        raise DocumentError('INVALID_DESTINATION', 'Author files must be .json, not generated catalog files')
    if target.is_symlink():
        raise DocumentError('INVALID_DESTINATION', 'Refusing to overwrite a file symlink')
    same = target == document.path
    if not same and target.exists():
        raise DocumentError('DESTINATION_EXISTS', 'Save As requires a new destination', path=str(target))
    if same:
        _check_source(document)
    report = document.validate()
    if same and not document.status()['dirty']:
        return {'status': 'AUTHOR_SAVED', 'changed': False, 'path': str(target),
                'content_valid': report['status'] == 'STRUCTURE_VALID',
                'runtime_verified': False, 'previous_path': None}
    data = canonical(document.snapshot())
    if len(data) > MAX_BYTES:
        raise DocumentError('DOCUMENT_LIMIT', 'Saved author content exceeds 2 MiB')
    previous = _write_author_bytes(target, data, expected_sha256=document.source_sha256 if same else None)
    document.path = target
    document.source_sha256 = hashlib.sha256(data).hexdigest()
    document._saved_content = data
    return {'status': 'AUTHOR_SAVED', 'changed': True, 'path': str(target),
            'source_sha256': document.source_sha256, 'content_valid': report['status'] == 'STRUCTURE_VALID',
            'previous_path': previous, 'runtime_verified': False}


def prepare_document(document, output: Path) -> dict:
    """仅从已保存且校验通过的精确源快照沿既有管线准备目录。"""
    if document.status()['dirty']:
        raise DocumentError('UNSAVED_CHANGES', 'Save the draft before preparing its content index')
    _check_source(document)
    report = document.validate()
    if report['status'] != 'STRUCTURE_VALID':
        raise DocumentError('INVALID_CONTENT', 'Repair the reported content issues before preparing', report=report)
    try:
        return prepare_pack(document.path, Path(output), expected_source_sha256=document.source_sha256)
    except ValueError as exc:
        code = 'SOURCE_CHANGED' if 'Source changed' in str(exc) else 'PREPARE_FAILED'
        raise DocumentError(code, str(exc)) from exc
