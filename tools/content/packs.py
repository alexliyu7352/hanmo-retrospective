"""读取声明包的不可变快照并解析显式选择；不编译内容、不执行脚本或发布产物。

发现记录固定含manifest/root/manifest_path/manifest_bytes/files/file_paths及三类摘要。
Path与原始bytes只留在内存；锁和来源图只能使用规范相对路径与摘要。
resolve返回依赖优先的packs、rules_api、required_capabilities、runtime_verified。
"""
from __future__ import annotations

from collections import defaultdict
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat

from .capability_contract import load_contract
from .io import _read_snapshot, canonical
from .pack_errors import PackError
from .validation import ROOT, StrictValidator, _bounded

PACK_SCHEMA = json.loads((ROOT / 'schemas/pack.schema.json').read_text(encoding='utf8'))
PACK_SET_SCHEMA = json.loads((ROOT / 'schemas/pack-set.schema.json').read_text(encoding='utf8'))
PACK_VALIDATOR = StrictValidator(PACK_SCHEMA)
SELECTION_VALIDATOR = StrictValidator({**PACK_SET_SCHEMA, 'oneOf': [{'$ref': '#/$defs/selection'}]})
VERSION = re.compile(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\Z')
MAX_MEMBERS = 4096
MAX_RESOURCE_BYTES = 64 * 1024 * 1024
MAX_TOTAL_BYTES = 256 * 1024 * 1024
RESOURCE_EXTENSIONS = {
    'data': {'.json'},
    'image': {'.png', '.webp', '.jpg', '.jpeg', '.svg'},
    'audio': {'.wav', '.ogg', '.mp3'},
    'font': {'.ttf', '.otf'},
}
FORBIDDEN_EXTENSIONS = {'.gd', '.gdc', '.gdshader', '.cs', '.dll', '.so', '.dylib', '.exe',
                        '.pck', '.zip', '.tar', '.gz', '.bz2', '.xz', '.7z', '.rar',
                        '.py', '.js', '.sh', '.bat', '.ps1', '.tscn', '.tres', '.godot'}
LEGACY_SOURCES = {name: ROOT / 'content/foundation' / (name + '.json') for name in
                  ('procurement', 'two_stories', 'lived_city', 'alpha_life', 'journey_life')}
LEGACY_PRESENTATION = ROOT / 'game/data/presentation.json'


def _sha(raw: bytes) -> str:
    """摘要只依赖既已冻结的字节，不重读源文件。"""
    return hashlib.sha256(raw).hexdigest()


def _version(value: str) -> tuple[int, int, int]:
    """比较规范三段整数版本；不接受隐式最新、前导零或表达式。"""
    if not isinstance(value, str) or len(value) > 96 or not VERSION.fullmatch(value):
        raise PackError('InvalidPackVersion', '版本须为规范三段非负整数', {'version': value})
    return tuple(int(part) for part in value.split('.'))


def _schema(value: dict, validator, code: str, source: str) -> None:
    """共享既有结构边界和严格整数校验，并稳定输出所有Schema定位。"""
    if not _bounded(value):
        raise PackError(code, 'JSON结构超过既有深度或节点边界', {'source': source})
    errors = sorted(validator.iter_errors(value), key=lambda error: (str(list(error.absolute_path)), error.message))
    if errors:
        raise PackError(code, '包声明不符合Schema', {'source': source, 'issues': [
            {'path': '/' + '/'.join(str(p).replace('~', '~0').replace('/', '~1') for p in error.absolute_path),
             'message': error.message[:600]} for error in errors]})


def _validate_manifest(manifest: dict, source: str) -> None:
    """检查声明范围和唯一依赖名，引用存在性留待所选集合解析。"""
    _schema(manifest, PACK_VALIDATOR, 'InvalidPackManifest', source)
    for field in ('rules_api', 'dependencies', 'conflicts'):
        ranges = [manifest[field]] if field == 'rules_api' else manifest[field]
        seen = set()
        for requirement in ranges:
            if _version(requirement['min_version']) >= _version(requirement['max_version_exclusive']):
                raise PackError('InvalidVersionRange', '版本范围必须非空', {'source': source, 'field': field, **requirement})
            if requirement['id'] in seen:
                raise PackError('DuplicatePackRequirement', '同一字段不能重复声明包ID', {'source': source, 'field': field, 'id': requirement['id']})
            seen.add(requirement['id'])


def _relative(value: str, source: str) -> str:
    """接受单一规范相对路径，拒绝跨平台绝对路径、逃逸和别名。"""
    path_schema = StrictValidator(PACK_SCHEMA['$defs']['path'])
    if not isinstance(value, str) or not path_schema.is_valid(value):
        raise PackError('InvalidPackPath', '包路径必须为规范相对路径', {'source': source, 'path': value})
    return value


def _contained(root: Path, relative: str) -> Path:
    """逐段拒绝符号链接和非普通文件，避免声明路径跳出包根。"""
    _relative(relative, str(root))
    path = root
    for part in PurePosixPath(relative).parts:
        path = path / part
        if path.is_symlink():
            raise PackError('InvalidPackPath', '包路径不得包含符号链接', {'path': relative})
    if not path.resolve().is_relative_to(root.resolve()):
        raise PackError('InvalidPackPath', '包路径逃逸根目录', {'path': relative})
    if path.exists() and not stat.S_ISREG(path.stat().st_mode):
        raise PackError('InvalidPackPath', '入口和资源须为普通文件', {'path': relative})
    return path


def _read_json(path: Path) -> tuple[dict, bytes]:
    """沿既有2MiB、深32和20万节点读取器冻结JSON，重复键一律拒绝。"""
    try:
        return _read_snapshot(path)
    except ValueError as exc:
        raise PackError('InvalidPackJson', str(exc), {'source': str(path)}) from exc


def _read_resource(path: Path, relative: str) -> bytes:
    """按实际读取字节限制资源，不仅依赖可能过时的stat大小。"""
    try:
        with path.open('rb') as stream:
            raw = stream.read(MAX_RESOURCE_BYTES + 1)
    except OSError as exc:
        raise PackError('MissingPackResource', '无法读取声明资源', {'path': relative}) from exc
    if len(raw) > MAX_RESOURCE_BYTES:
        raise PackError('PackResourceLimit', '单资源超过64MiB', {'path': relative})
    return raw


def _walk_error(error: OSError) -> None:
    """目录读取失败必须成为显式诊断，不能静默跳过包或成员。"""
    raise PackError('PackReadError', '无法枚举包目录', {'source': str(error.filename), 'error': str(error)}) from error


def _check_members(root: Path) -> None:
    """导入目录只做有界成员与脚本检查；不解压、不执行或复制任何成员。"""
    count = total = 0
    for folder, directories, files in os.walk(root, followlinks=False, onerror=_walk_error):
        for name in sorted(directories + files):
            path = Path(folder) / name
            relative = path.relative_to(root).as_posix()
            count += 1
            if count > MAX_MEMBERS:
                raise PackError('PackMemberLimit', '包超过4096个成员', {'source': str(root)})
            if path.is_symlink():
                raise PackError('InvalidPackPath', '包目录不得包含链接', {'path': relative})
            if path.suffix.lower() in FORBIDDEN_EXTENSIONS:
                raise PackError('UnsupportedPackResource', '包不得包含脚本、引擎包或嵌套归档', {'path': relative})
            if path.is_file():
                total += path.stat().st_size
            elif not path.is_dir():
                raise PackError('InvalidPackPath', '包只能包含目录和普通文件', {'path': relative})
            if total > MAX_TOTAL_BYTES:
                raise PackError('PackTotalLimit', '包成员总量超过256MiB', {'source': str(root)})


def _digest_files(files: dict[str, bytes], paths: list[str]) -> str:
    """规范路径和原字节摘要组成有序清单；路径迁移不会改变身份。"""
    return _sha(canonical([{'path': path, 'sha256': _sha(files[path])} for path in sorted(paths)]))


def _legacy_resources(presentation: dict, files: dict, file_paths: dict) -> list[str]:
    """复用既有表现路径边界冻结旧显式资产；不解码图像或运行引擎。"""
    try:
        from tools.presentation import _asset_path, _asset_bytes
    except ModuleNotFoundError:
        from presentation import _asset_path, _asset_bytes
    resources = set()
    try:
        for group in ('backgrounds', 'portraits'):
            for asset in presentation[group].values():
                path = _asset_path(ROOT / 'game', asset['path'])
                logical = path.relative_to(ROOT).as_posix()
                if logical not in resources:
                    files[logical] = _asset_bytes(path)
                    file_paths[logical] = path
                    resources.add(logical)
    except (KeyError, OSError, ValueError) as exc:
        raise PackError('InvalidLegacyResource', '旧表现显式资源无法完整冻结', {'source': str(LEGACY_PRESENTATION), 'error': str(exc)}) from exc
    return sorted(resources)


def _snapshot(manifest_path: Path) -> dict:
    """读取一次manifest及全部声明输入；后续解析只使用本快照。"""
    root = manifest_path.parent
    _check_members(root)
    manifest, raw = _read_json(manifest_path)
    _validate_manifest(manifest, str(manifest_path))
    files, file_paths = {}, {}
    content_paths, resource_paths, missing_resources = [], [], []
    if 'legacy_source' in manifest:
        source = LEGACY_SOURCES[manifest['legacy_source']]
        content, content_raw = _read_json(source)
        if manifest['id'] != content['id'] or manifest['version'] != content['version']:
            raise PackError('LegacyIdentityMismatch', '旧包身份须沿用原内容ID和版本', {'id': manifest['id'], 'expected_id': content['id'], 'expected_version': content['version']})
        presentation, presentation_raw = _read_json(LEGACY_PRESENTATION)
        for path, source_raw in ((source, content_raw), (LEGACY_PRESENTATION, presentation_raw)):
            logical = path.relative_to(ROOT).as_posix()
            files[logical], file_paths[logical] = source_raw, path
            content_paths.append(logical)
        resource_paths = _legacy_resources(presentation, files, file_paths)
    else:
        for field in ('content_entries', 'presentation_entries'):
            for relative in sorted(manifest[field]):
                path = _contained(root, relative)
                if path.suffix != '.json':
                    raise PackError('InvalidPackPath', '内容和表现入口必须为JSON文件', {'path': relative, 'field': field})
                if relative == 'pack.json' or relative in file_paths:
                    raise PackError('DuplicatePackPath', '同一路径不能担任多个包入口', {'path': relative})
                files[relative] = _read_json(path)[1]
                file_paths[relative] = path
                content_paths.append(relative)
        for resource in sorted(manifest['resources'], key=lambda item: item['path']):
            relative = resource['path']
            path = _contained(root, relative)
            if path.suffix not in RESOURCE_EXTENSIONS[resource['kind']]:
                raise PackError('UnsupportedPackResource', '资源类型与受支持扩展名不匹配', {'path': relative, 'kind': resource['kind']})
            if relative == 'pack.json' or relative in file_paths:
                raise PackError('DuplicatePackPath', '资源路径不得重复或占用入口', {'path': relative})
            file_paths[relative] = path
            if not path.exists():
                if resource['required']:
                    raise PackError('MissingPackResource', '必需资源不存在', {'path': relative})
                missing_resources.append(relative)
                continue
            resource_raw = _read_json(path)[1] if resource['kind'] == 'data' else _read_resource(path, relative)
            if _sha(resource_raw) != resource['sha256']:
                raise PackError('PackResourceDigestMismatch', '资源字节不匹配声明摘要', {'path': relative, 'expected_sha256': resource['sha256'], 'actual_sha256': _sha(resource_raw)})
            files[relative] = resource_raw
            resource_paths.append(relative)
    if sum(map(len, files.values())) + len(raw) > MAX_TOTAL_BYTES:
        raise PackError('PackTotalLimit', '冻结输入总量超过256MiB', {'source': str(root)})
    return {'manifest': manifest, 'root': root, 'manifest_path': manifest_path,
            'manifest_bytes': raw, 'files': files, 'file_paths': file_paths,
            'content_paths': sorted(content_paths), 'resource_paths': sorted(resource_paths),
            'missing_resources': sorted(missing_resources),
            'manifest_sha256': _sha(raw), 'content_sha256': _digest_files(files, content_paths),
            'resources_sha256': _digest_files(files, resource_paths)}


def _identity(record: dict) -> tuple[str, str, str]:
    """同ID同版本仅在manifest和全部声明输入摘要一致时允许去重。"""
    return tuple(record[field] for field in ('manifest_sha256', 'content_sha256', 'resources_sha256'))


def _deduplicate(records: list[dict]) -> list[dict]:
    """拒绝相同身份的不同字节副本，相同副本按物理路径稳定择一。"""
    found = {}
    for record in sorted(records, key=lambda item: str(item['manifest_path'])):
        manifest = record['manifest']
        key = manifest['id'], manifest['version']
        if key in found and _identity(found[key]) != _identity(record):
            raise PackError('PackIdentityConflict', '同包同版本存在不同字节', {'id': key[0], 'version': key[1], 'sources': [str(found[key]['manifest_path']), str(record['manifest_path'])]})
        found.setdefault(key, record)
    return sorted(found.values(), key=lambda item: (item['manifest']['id'], _version(item['manifest']['version'])))


def discover_packs(roots: list[Path]) -> list[dict]:
    """只读发现显式本地目录；包根遇manifest即停止递归，ZIP须由独立导入流程处理。"""
    manifests = set()
    for supplied in sorted(map(Path, roots), key=str):
        if any(path.is_symlink() for path in (supplied, *supplied.parents)):
            raise PackError('InvalidPackPath', '发现根不得为符号链接', {'source': str(supplied)})
        if supplied.is_file() and supplied.name == 'pack.json':
            manifests.add(supplied.resolve())
            continue
        if not supplied.is_dir():
            raise PackError('InvalidPackRoot', '发现根须为目录或pack.json', {'source': str(supplied)})
        for folder, directories, files in os.walk(supplied, followlinks=False, onerror=_walk_error):
            directories.sort()
            for name in directories + files:
                if (Path(folder) / name).is_symlink():
                    raise PackError('InvalidPackPath', '发现目录不得含链接', {'source': str(Path(folder) / name)})
            if 'pack.json' in files:
                manifests.add((Path(folder) / 'pack.json').resolve())
                directories[:] = []
    return _deduplicate([_snapshot(path) for path in sorted(manifests)])


def load_pack_runtime() -> dict:
    """从两个受信工程声明组合规则API和能力，不采用作者包自报支持集。"""
    rules_api = _read_json(ROOT / 'game/data/pack_runtime.json')[0]
    return {'rules_api': rules_api, 'capabilities': sorted(entry['capability_id'] for entry in load_contract()['capabilities'])}


def _runtime(runtime: dict) -> tuple[dict, set[str]]:
    """验证调用方提供的受信运行声明形状，不将坏声明降级为空能力。"""
    if not isinstance(runtime, dict) or set(runtime) != {'rules_api', 'capabilities'}:
        raise PackError('InvalidPackRuntime', '运行声明须含rules_api和capabilities')
    rules, capabilities = runtime['rules_api'], runtime['capabilities']
    if (not isinstance(rules, dict) or set(rules) != {'id', 'version'} or
            not StrictValidator(PACK_SCHEMA['$defs']['id']).is_valid(rules['id']) or
            not isinstance(capabilities, list) or any(not isinstance(value, str) for value in capabilities) or
            len(set(capabilities)) != len(capabilities)):
        raise PackError('InvalidPackRuntime', '运行声明形状或能力列表无效')
    _version(rules['version'])
    return rules, set(capabilities)


def _matches(requirement: dict, version: str) -> bool:
    """采用半开整数版本区间，不按字符串顺序比较版本。"""
    return _version(requirement['min_version']) <= _version(version) < _version(requirement['max_version_exclusive'])


def _legacy_imports(manifests: dict[str, dict], namespaces: dict[str, str]) -> None:
    """旧ID只可经直接依赖和显式同名映射导入，拒绝模糊别名或重复目标。"""
    for manifest in manifests.values():
        dependencies = {entry['id'] for entry in manifest['dependencies']}
        targets = set()
        for qualified, local in manifest.get('legacy_imports', {}).items():
            namespace, name = qualified.split('::')
            target_id = namespaces.get(namespace)
            target = manifests.get(target_id, {})
            if target_id not in dependencies or 'legacy_source' not in target or name != local or local in targets:
                raise PackError('InvalidLegacyImport', '旧ID映射必须指向已选直接依赖旧包且不得重命名或重复', {'id': manifest['id'], 'reference': qualified, 'target': local})
            targets.add(local)


def resolve_pack_set(packs: list[dict], selected: list[dict], runtime: dict) -> dict:
    """显式选择唯一版本并按拓扑层排序；不自动补依赖、选最新或修改输入。"""
    _schema({'format': 'HM-PACK-SELECTION', 'format_version': 1, 'packs': selected},
            SELECTION_VALIDATOR, 'InvalidPackSelection', 'selection')
    rules_api, available = _runtime(runtime)
    for record in packs:
        if not isinstance(record, dict) or not all(field in record for field in ('manifest', 'manifest_path', 'manifest_sha256', 'content_sha256', 'resources_sha256')):
            raise PackError('InvalidPackRecord', 'resolve只接受discover产生的包快照')
        _validate_manifest(record['manifest'], str(record['manifest_path']))
    discovered = _deduplicate(packs)
    index = {(record['manifest']['id'], record['manifest']['version']): record for record in discovered}
    chosen = {}
    for selection in selected:
        identifier, version = selection['id'], selection['version']
        if identifier in chosen:
            raise PackError('PackVersionConflict', '每个包只能显式选择一个版本', {'id': identifier})
        if (identifier, version) not in index:
            versions = sorted([key[1] for key in index if key[0] == identifier], key=_version)
            raise PackError('PackVersionMismatch' if versions else 'MissingPack', '所选精确包版本不可用', {'id': identifier, 'version': version, 'available_versions': versions})
        chosen[identifier] = index[identifier, version]
    manifests = {identifier: record['manifest'] for identifier, record in chosen.items()}
    namespaces, required = {}, set()
    pending, dependents = {}, defaultdict(set)
    legacy = []
    for identifier in sorted(manifests):
        manifest = manifests[identifier]
        namespace = manifest['namespace']
        if namespace in namespaces:
            raise PackError('PackNamespaceConflict', '所选包namespace不能重名', {'namespace': namespace, 'packs': [namespaces[namespace], identifier]})
        namespaces[namespace] = identifier
        if 'legacy_source' in manifest:
            legacy.append(identifier)
        requirement = manifest['rules_api']
        if requirement['id'] != rules_api['id'] or not _matches(requirement, rules_api['version']):
            raise PackError('IncompatibleRulesApi', '包要求的规则API不受当前工程支持', {'id': identifier, 'required': requirement, 'actual': rules_api})
        missing = sorted(set(manifest['required_capabilities']) - available)
        if missing:
            raise PackError('UnsupportedPackCapability', '包要求未声明支持的规则能力', {'id': identifier, 'missing_capabilities': missing})
        required.update(manifest['required_capabilities'])
        pending[identifier] = set()
        for dependency in manifest['dependencies']:
            dependency_id = dependency['id']
            if dependency_id not in manifests:
                raise PackError('MissingPackDependency', '直接依赖必须显式选择', {'id': identifier, 'dependency': dependency})
            if not _matches(dependency, manifests[dependency_id]['version']):
                raise PackError('PackDependencyVersionMismatch', '所选依赖版本不满足范围', {'id': identifier, 'dependency': dependency, 'selected_version': manifests[dependency_id]['version']})
            pending[identifier].add(dependency_id)
            dependents[dependency_id].add(identifier)
        for conflict in manifest['conflicts']:
            if conflict['id'] in manifests and _matches(conflict, manifests[conflict['id']]['version']):
                raise PackError('PackConflict', '所选集合包含声明冲突', {'id': identifier, 'conflict': conflict})
    if len(legacy) > 1:
        raise PackError('LegacyPackConflict', '旧五包是独立内容集，不能同集启用', {'packs': legacy})
    _legacy_imports(manifests, namespaces)
    ordered = []
    ready = sorted(identifier for identifier, dependencies in pending.items() if not dependencies)
    while ready:
        next_ready = set()
        for identifier in ready:
            ordered.append(chosen[identifier])
            for dependent in dependents[identifier]:
                pending[dependent].remove(identifier)
                if not pending[dependent]:
                    next_ready.add(dependent)
        ready = sorted(next_ready, key=lambda identifier: (identifier, _version(manifests[identifier]['version'])))
    if len(ordered) != len(chosen):
        unresolved = sorted(set(chosen) - {record['manifest']['id'] for record in ordered})
        raise PackError('PackDependencyCycle', '包依赖包含环', {'packs': unresolved})
    return {'packs': ordered, 'rules_api': dict(rules_api),
            'required_capabilities': sorted(required), 'runtime_verified': False}
