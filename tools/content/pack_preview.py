"""只从受信同源runtime和已核compose代准备隔离试玩工程；不启动进程，不认证玩法。"""
from __future__ import annotations

import ctypes
import hashlib
import os
import sys
from pathlib import Path
import re
import shutil
import tempfile
import uuid

from tools import content_stage as staging
from tools.check_architecture import _imports, _path
from tools.content.io import canonical, read_pack
from tools.content.pack_errors import PackError
from tools.content.packs import _runtime
from tools.content.pack_compose import _artifact_json
from tools.presentation import validate_registry

ROOT = Path(__file__).resolve().parents[2]
SOURCE_MANIFEST = 'preview-source-manifest.json'
OUTPUT_MANIFEST = 'preview-output-manifest.json'
RUNTIME_TREES = ('game/core', 'game/ui', 'game/storage')
# 外貌目录与选件脚本通过JSON读取或动态load访问，必须显式登记，不能依赖静态导入遍历。
RUNTIME_FILES = ('game/main.tscn', 'game/ui/main.gd', 'game/core/world_kernel.gd',
                 'game/core/save_codec.gd', 'game/storage/storage_service.gd', 'game/data/presentation.schema.json',
                 'game/data/accepted_adult_appearances.json', 'game/data/adult_appearance_compositions.json',
                 'game/character_studio/src/studio_session.gd',
                 'game/data/rule_capabilities.json', 'game/data/module_contracts.json',
                 'game/data/pack_runtime.json', 'game/assets/audio/ambience.wav',
                 'game/assets/audio/paper.wav', 'game/assets/audio/LICENSE.json')


def _plain_path(path: Path) -> Path:
    """拒绝目标及任一父目录链接，避免resolve先吞掉越界证据。"""
    path = Path(path).absolute()
    if any(part.is_symlink() for part in (path, *path.parents)):
        raise PackError('ProtectedPreviewPath', '试玩输入与输出不得经过链接')
    return path.resolve()


def _destination(composed: Path, destination: Path, *, native_stage: Path | None = None) -> Path:
    """只接受全新独立目标；作者、compose、源码及已选择原生stage都不得重叠。"""
    destination = _plain_path(destination)
    if destination.exists():
        raise PackError('PreviewDestinationExists', '试玩目标必须是全新目录')
    protected = [ROOT / name for name in ('game', 'content', 'schemas', 'tools', 'tests')]
    protected.append(composed)
    if native_stage is not None:
        protected.append(_plain_path(native_stage).parent)
    if any(destination == path or destination.is_relative_to(path) or path.is_relative_to(destination)
           for path in protected):
        raise PackError('ProtectedPreviewPath', '试玩目标与作者、compose、原生来源或正式工程目录重叠')
    if any((parent / 'pack.json').exists() for parent in destination.parents):
        raise PackError('ProtectedPreviewPath', '试玩目标不得放入作者包目录')
    if not destination.parent.is_dir():
        raise PackError('ProtectedPreviewPath', '试玩目标的独立父目录必须已经存在')
    return destination


def _runtime_sources() -> dict[Path, bytes]:
    """只从固定模块ROOT读取同源脚本/主场景/合同/音频；包内脚本从不成为runtime来源。"""
    paths = {ROOT / name for name in RUNTIME_FILES}
    for name in RUNTIME_TREES:
        directory = _plain_path(ROOT / name)
        if not directory.is_dir():
            raise PackError('MissingPreviewRuntime', '受信runtime源码目录缺失', {'path': name})
        paths.update(path for path in staging.files_in(directory).values() if path.suffix == '.gd')
    paths.update(staging.schema_sources(ROOT).values())
    paths.add(ROOT / 'schemas/pack-set.schema.json')
    result = {}
    pending = sorted(paths)
    while pending:
        path = pending.pop()
        if path in result:
            continue
        _plain_path(path)
        if not path.is_file():
            raise PackError('MissingPreviewRuntime', '受信runtime文件缺失', {'path': str(path)})
        result[path] = path.read_bytes()
        # 复用现架构检查器的注释/字面量识别；动态load仍由正式runtime和包表现校验负责。
        references = []
        if path.suffix == '.gd':
            references, _dynamic = _imports(result[path].decode('utf8'))
        elif path.suffix in {'.tscn', '.tres'}:
            references = re.findall(r'"(res://[^"\n]+)"', result[path].decode('utf8'))
        for reference in references:
            if not reference.startswith('res://') or not _path(reference[6:]):
                raise PackError('ProtectedPreviewPath', 'runtime静态资源路径不是规范res路径',
                                {'source': str(path), 'reference': reference})
            dependency = ROOT / 'game' / reference[6:]
            if dependency not in result:
                pending.append(dependency)
    return result


def _write(path: Path, raw: bytes) -> None:
    """只向本次私有候选树写入原字节，不接管已有目标。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(raw)


def _project(profile: str, catalog: str, scenario: str) -> bytes:
    """生成独立存档的同源主场景配置；保留Main按各剧本选择GD或原生的原分派。"""
    import json
    quote = lambda value: json.dumps(value, ensure_ascii=False)
    return ('config_version=5\n\n[application]\n'
            'config/name="汉末行记 · 内容包试玩"\n'
            'config/version="preview"\n'
            f'config/storage_profile={quote(profile)}\n'
            f'config/default_catalog={quote(catalog)}\n'
            f'config/default_scenario={quote(scenario)}\n'
            'config/use_custom_user_dir=true\n'
            f'config/custom_user_dir_name={quote("hanmo-preview/" + profile)}\n'
            'run/main_scene="res://main.tscn"\n'
            'config/features=PackedStringArray("4.7", "GL Compatibility")\n\n'
            '[display]\nwindow/size/viewport_width=1280\nwindow/size/viewport_height=800\n'
            'window/stretch/mode="canvas_items"\n\n'
            '[rendering]\nrenderer/rendering_method="gl_compatibility"\n').encode('utf8')


def _rename_no_replace(candidate: Path, destination: Path) -> None:
    """使用系统原子拒绝覆盖rename；缺少该原语的平台明确失败，绝不退回exists加replace。"""
    if os.name == 'nt':
        os.rename(candidate, destination)  # Windows rename本身拒绝任何已存在目标。
        return
    if not sys.platform.startswith('linux'):
        raise PackError('PreviewAtomicPublishUnavailable', '此平台未提供已支持的原子不覆盖发布')
    library = ctypes.CDLL(None, use_errno=True)
    rename = getattr(library, 'renameat2', None)
    if rename is None:
        raise PackError('PreviewAtomicPublishUnavailable', '系统未提供renameat2，保留原目录')
    rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
    rename.restype = ctypes.c_int
    # AT_FDCWD=-100，RENAME_NOREPLACE=1；候选与目标在同一父目录，整代不可覆盖发布。
    if rename(-100, os.fsencode(candidate), -100, os.fsencode(destination), 1) != 0:
        error = ctypes.get_errno()
        raise OSError(error, os.strerror(error), str(destination))


def _publish(candidate: Path, destination: Path) -> None:
    """以单次系统原子操作发布新目录；并发出现空目录也必须保留其原inode。"""
    try:
        _rename_no_replace(candidate, destination)
    except FileExistsError as exc:
        raise PackError('PreviewDestinationExists', '试玩目标已由其他操作者创建，未覆盖') from exc


def prepare_preview(composed: Path, destination: Path, *, expected_pack_set_digest: str,
                    native_stage: Path | None = None) -> dict:
    """复用唯一compose复制和原子发布；动态世界须带已核原生，末尾重核全部源。"""
    candidate = None
    try:
        if not isinstance(expected_pack_set_digest, str) or not re.fullmatch('[0-9a-f]{64}', expected_pack_set_digest):
            raise PackError('InvalidPreviewDigest', '试玩必须提供完整精确内容集摘要')
        composed = _plain_path(composed)
        destination = _destination(composed, destination)
        sources = _runtime_sources()
        runtime_snapshot = dict(sources)
        record = staging.composed_set_record(composed, ROOT)
        capabilities = staging.load_contract(ROOT / staging.CAPABILITY)['capabilities']
        rules_api, _available = _runtime({
            'rules_api': _artifact_json(sources[ROOT / 'game/data/pack_runtime.json']),
            'capabilities': [entry['capability_id'] for entry in capabilities]})
        if read_pack(composed / 'pack-set.lock.json')['rules_api'] != rules_api:
            raise PackError('IncompatibleRulesApi', 'compose锁与固定runtime规则API不一致')
        if record['pack_set_digest'] != expected_pack_set_digest:
            raise PackError('PreviewDigestMismatch', 'compose代与请求的精确锁摘要不一致')
        scenarios = read_pack(composed / 'catalog.hmcatalog.json')['catalog']['scenarios']
        if not scenarios:
            raise PackError('MissingPreviewScenario', '试玩内容集没有可选剧本')
        native_runtime = None
        selected_native_stage = None
        if any('dynamic_world' in scenario for scenario in scenarios.values()):
            if not native_stage:
                raise PackError('MissingPreviewNativeRuntime', '动态世界试玩须显式选择已核原生stage/game来源')
            from .preview_native import prepare_native_runtime
            selected_native_stage = Path(native_stage)
            native_runtime = prepare_native_runtime(ROOT, selected_native_stage)
            _destination(composed, destination, native_stage=selected_native_stage)
        candidate = Path(tempfile.mkdtemp(prefix='.pack-preview-', dir=destination.parent))
        game = candidate / 'game'
        for path, raw in sources.items():
            if path.is_relative_to(ROOT / 'game'):
                _write(candidate / path.relative_to(ROOT), raw)
        generated = game / 'data/generated'
        generated.mkdir(parents=True)
        _write(generated / 'content.schema.json', sources[ROOT / 'schemas/content.schema.json'])
        staging.write_schema_index(ROOT, generated)
        declaration = staging.runtime_declaration(staging.load_contract(ROOT / staging.CAPABILITY),
                                                  path=ROOT / staging.CAPABILITY)
        _write(generated / 'runtime_capabilities.json', canonical(declaration))
        copied, memberships = staging.stage_installed_sets(ROOT, generated, candidate / 'no-previous-stage',
                                                           [composed], foundation_sets=False)
        if any(path in sources and sources[path] != raw for path, raw in copied.items()):
            raise PackError('PreviewInputsChanged', '受信合同在准备中发生变化')
        sources.update(copied)
        installed = generated / record['directory']
        if staging.composed_set_record(installed, ROOT) != record:
            raise PackError('PreviewInputsChanged', '复制后内容集与原compose代不一致')
        staging.installed_index_records(generated)
        # 全局标题与包内世界共用同一已核清单，资源按原路径复制，不带默认31图假清单。
        _write(game / 'data/presentation.json', (installed / 'presentation.json').read_bytes())
        for name, path in staging.files_in(installed).items():
            if name.startswith('assets/'):
                _write(game / name, path.read_bytes())
        presentation_errors = validate_registry(read_pack(game / 'data/presentation.json'), game)
        if presentation_errors:
            raise PackError('InvalidPreviewPresentation', '试玩全局表现未通过资源校验', {'issues': presentation_errors})
        catalog = 'res://data/generated/' + record['directory'] + '/catalog.hmcatalog.json'
        if native_runtime is not None:
            for relative, raw in native_runtime['files'].items():
                _write(game / relative, raw)
            if any(path in sources and sources[path] != raw
                   for path, raw in native_runtime['source_paths'].items()):
                raise PackError('PreviewInputsChanged', '原生来源与已冻结受信源不一致')
            sources.update(native_runtime['source_paths'])
        profile = 'pack-preview-' + uuid.uuid4().hex
        _write(game / 'project.godot', _project(profile, catalog, sorted(scenarios)[0]))
        manifest = {'format': 'HM-PACK-PREVIEW-SOURCE/1', 'pack_set_digest': expected_pack_set_digest,
                    'runtime': {path.relative_to(ROOT).as_posix(): hashlib.sha256(raw).hexdigest()
                                for path, raw in sorted(sources.items()) if path.is_relative_to(ROOT)},
                    'composed': {path.relative_to(composed).as_posix(): hashlib.sha256(raw).hexdigest()
                                 for path, raw in sorted(copied.items()) if path.is_relative_to(composed)}}
        if native_runtime is not None:
            manifest['native_runtime'] = native_runtime['identity']
        source_bytes = canonical(manifest)
        _write(candidate / SOURCE_MANIFEST, source_bytes)
        outputs = {name: staging.digest(path) for name, path in staging.files_in(candidate).items()}
        _write(candidate / OUTPUT_MANIFEST, canonical({'format': 'HM-PACK-PREVIEW-OUTPUT/1', 'files': outputs}))
        if native_runtime is not None:
            try:
                fresh_native = prepare_native_runtime(ROOT, selected_native_stage)
            except PackError as exc:
                raise PackError('PreviewInputsChanged', '原生来源或生产身份在准备中发生变化',
                                {'cause': exc.to_dict()}) from exc
            if fresh_native != native_runtime:
                raise PackError('PreviewInputsChanged', '原生来源成员、原字节或身份在准备中发生变化')
        fresh = _runtime_sources()
        if fresh != runtime_snapshot:
            raise PackError('PreviewInputsChanged', '受信runtime成员或原字节在准备中发生变化')
        for path, members in memberships.items():
            if sorted(staging.files_in(path)) != members:
                raise PackError('PreviewInputsChanged', 'compose代成员在准备中发生变化')
        for path, raw in sources.items():
            _plain_path(path)
            try:
                current = path.read_bytes()
            except OSError as exc:
                raise PackError('PreviewInputsChanged', '试玩来源在准备中消失或无法读取',
                                {'path': str(path)}) from exc
            if current != raw:
                raise PackError('PreviewInputsChanged', '试玩来源原字节在准备中发生变化')
        _destination(composed, destination, native_stage=selected_native_stage)
        _publish(candidate, destination)
        result = {'status': 'PREVIEW_PREPARED', 'runtime_verified': False,
                'game_path': str(destination / 'game'), 'catalog_path': catalog,
                'pack_set_digest': expected_pack_set_digest, 'storage_profile': profile,
                'source_manifest_sha256': hashlib.sha256(source_bytes).hexdigest()}
        if native_runtime is not None:
            result['native_runtime'] = native_runtime['identity']
        return result
    except PackError:
        raise
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise PackError('PackPreviewFailed', '隔离试玩准备失败，未覆盖已有目标', {'error': str(exc)}) from exc
    finally:
        if candidate is not None and candidate.exists():
            shutil.rmtree(candidate)
