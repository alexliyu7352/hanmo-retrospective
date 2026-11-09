"""只读绑定已核原生stage与当前生产源；作者试玩仍由原preview入口原子发布。"""
from __future__ import annotations

import json
from pathlib import Path
import subprocess

from tools import native_resident_profile as resident
from tools.content.pack_errors import PackError


EXTENSION_LIST = '.godot/extension_list.cfg'


def _snapshot(path: Path, sources: dict) -> bytes:
    """拒绝链接与缺件，冻结普通文件原字节供预览发布前再次核对。"""
    path = resident._regular_path(path)
    if not path.is_file():
        raise PackError('MissingPreviewNativeRuntime', '原生试玩来源缺少必要文件', {'path': str(path)})
    raw = path.read_bytes()
    sources[path] = raw
    return raw


def _mismatch(detail: str) -> None:
    """身份不符必须终止准备，不返回可被误认为运行通过的产物。"""
    raise PackError('PreviewNativeIdentityMismatch', '原生试玩来源与已核生产身份不符', {'error': detail})


def prepare_native_runtime(root: Path, native_stage: Path) -> dict:
    """复用生产校验器，只返回原生字节与固定注册表；调用者负责末尾复核和原子发布。"""
    try:
        root = resident._regular_path(root)
        game = resident._regular_path(native_stage)
        if not game.is_dir():
            raise PackError('MissingPreviewNativeRuntime', '请选择已核原生stage的game目录', {'path': str(game)})
        if game.name != 'game':
            raise PackError('InvalidPreviewNativeRuntime', '原生来源必须是已核stage的game目录')
        stage = game.parent
        sources = {}
        config_raw = _snapshot(stage / 'release-profile.json', sources)
        manifest_raw = _snapshot(stage / resident.MANIFEST, sources)
        receipt_raw = _snapshot(stage / 'native-build-receipt.json', sources)
        library_raw = _snapshot(game / resident.LIBRARY, sources)
        extension_raw = _snapshot(game / resident.EXTENSION, sources)
        _snapshot(root / resident.CONFIG_PATH, sources)
        config = resident._read_config(stage / 'release-profile.json')
        trusted = resident._read_config(root / resident.CONFIG_PATH)
        manifest = json.loads(manifest_raw)
        if (not isinstance(manifest, dict) or manifest.get('format') != resident.FORMAT
                or manifest.get('profile') != resident.PROFILE or manifest.get('status', {}).get('stage') != 'PASS'):
            raise PackError('InvalidPreviewNativeRuntime', '来源缺少已完成的native-resident阶段清单')
        if (manifest.get('configuration') != config or manifest.get('profile_sha256') != resident._sha(config_raw)
                or config['engine'] != trusted['engine']):
            _mismatch('Stage configuration or fixed engine identity differs')
        for relative, raw in {'release-profile.json': config_raw, 'native-build-receipt.json': receipt_raw,
                              'game/' + resident.LIBRARY: library_raw, 'game/' + resident.EXTENSION: extension_raw}.items():
            if manifest.get('files', {}).get(relative) != resident._sha(raw):
                _mismatch('Stage member differs: ' + relative)
        commit = resident._git(root, 'rev-parse', 'HEAD').decode().strip()
        if resident._git(root, 'status', '--porcelain=v1', '--untracked-files=all', '--', *resident.NATIVE_ROOTS):
            _mismatch('Current native production inputs have uncommitted changes')
        native = resident._native_identity(root, commit, game / resident.LIBRARY,
                                           stage / 'native-build-receipt.json', config)
        if native != manifest.get('native'):
            _mismatch('Stage native record differs from original build receipt and production sources')
        template = resident._blob(root, commit, 'native/godot/hanmo_world.gdextension.in')
        descriptor = template.replace(b'@HANMO_BRIDGE_LIBRARY@', ('res://' + resident.LIBRARY).encode())
        if (extension_raw != descriptor or b'@' in descriptor or b'linux.debug.x86_64' not in descriptor
                or b'linux.release' in descriptor):
            _mismatch('Extension differs from the current reviewed debug-only Linux descriptor')
        # 稀疏树可能没有物化原生文件；已有文件也必须参与调用者的末尾字节复核。
        for name in resident._entries(root, commit, resident.NATIVE_ROOTS):
            if (root / name).exists() or (root / name).is_symlink():
                _snapshot(root / name, sources)
        for path, raw in sources.items():
            if resident._regular_path(path).read_bytes() != raw:
                raise PackError('PreviewInputsChanged', '原生试玩来源在准备中发生变化', {'path': str(path)})
        if (resident._git(root, 'rev-parse', 'HEAD').decode().strip() != commit
                or resident._git(root, 'status', '--porcelain=v1', '--untracked-files=all', '--', *resident.NATIVE_ROOTS)):
            raise PackError('PreviewInputsChanged', '原生生产源码在准备中发生变化')
        return {'files': {resident.LIBRARY: library_raw, resident.EXTENSION: extension_raw,
                          EXTENSION_LIST: ('res://' + resident.EXTENSION + '\n').encode()},
                'identity': {'format': 'HM-PACK-PREVIEW-NATIVE/1', 'native': native, 'engine': config['engine'],
                             'source_stage': str(game), 'stage_manifest_sha256': resident._sha(manifest_raw),
                             'profile_sha256': resident._sha(config_raw), 'runtime_verified': False},
                'source_paths': sources}
    except PackError:
        raise
    except RuntimeError as error:
        raise PackError('PreviewNativeIdentityMismatch', '原生试玩来源未通过原生产身份校验',
                        {'error': str(error)}) from error
    except (OSError, ValueError, KeyError, TypeError, AttributeError, subprocess.SubprocessError) as error:
        raise PackError('InvalidPreviewNativeRuntime', '原生试玩来源无法完整读取或校验',
                        {'error': str(error)}) from error
