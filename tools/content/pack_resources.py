"""冻结当前项目原创表现的选择闭包；只授权项目内复用，不推定对外许可。"""
from __future__ import annotations

from copy import deepcopy
from pathlib import Path
import tempfile

from .document import DocumentError
from .pack_errors import PackError
from .io import canonical
from .pack_compose import _annotated_schema, _unchanged
from .packs import MAX_TOTAL_BYTES, _contained, _read_json, _sha
from .validation import ROOT, StrictValidator
from tools import presentation

GROUPS = ('backgrounds', 'portraits', 'components', 'character_profiles', 'scene_templates')
LOCAL_USE = 'project-local-authorized-use'
PRESENTATION_ENTRY = 'presentation/project.json'
ORIGINAL_PINS = 'content/packs/legacy/archive/identity-bindings.json'


def _json_input(relative: str, snapshots: dict) -> dict:
    """从固定工程根读取受限JSON并保存同一次解析的原字节。"""
    path = _contained(ROOT, relative)
    value, raw = _read_json(path)
    snapshots[path] = raw
    return value


def _has_digest(value, digest: str) -> bool:
    """只查原来源记录是否实际保留精确图像摘要，不按文件名推断原创来源。"""
    todo = [value]
    while todo:
        item = todo.pop()
        if isinstance(item, dict):
            todo.extend(item.values())
        elif isinstance(item, list):
            todo.extend(item)
        elif item == digest:
            return True
    return False


def _selection(registry: dict, selection: list[dict]) -> dict[str, set[str]]:
    """选择只接收注册组和ID，默认背景与肖像必保留，不能传本地源路径。"""
    if not isinstance(selection, list) or len(selection) > 512:
        raise DocumentError('INVALID_RESOURCE_SELECTION', 'Select at most 512 project presentation records')
    chosen = {group: set() for group in GROUPS}
    chosen['backgrounds'].add(registry['default_background'])
    chosen['portraits'].add(registry['default_portrait'])
    for item in selection:
        if (not isinstance(item, dict) or set(item) != {'group', 'id'} or
                not isinstance(item.get('group'), str) or item['group'] not in GROUPS or
                not isinstance(item.get('id'), str) or item['id'] not in registry.get(item['group'], {})):
            raise DocumentError('INVALID_RESOURCE_SELECTION', 'Selection must name an existing project group and ID')
        chosen[item['group']].add(item['id'])
    return chosen


def _closure(registry: dict, schema: dict, chosen: dict[str, set[str]]) -> dict:
    """沿同一Schema引用注解补齐闭包，不猜组件、年龄或场景局部ID。"""
    while True:
        selected = deepcopy(registry)
        for group in GROUPS:
            if group in registry:
                selected[group] = {identifier: deepcopy(registry[group][identifier]) for identifier in sorted(chosen[group])}
        added = False
        for _path, annotation, group, reference in _annotated_schema(selected, schema, schema):
            if annotation == 'x-hanmo-asset-ref':
                group, reference = reference.split('/', 1)
            elif annotation != 'x-hanmo-ref':
                continue
            if group not in GROUPS:
                continue
            if reference not in registry.get(group, {}):
                raise DocumentError('MISSING_PROJECT_RESOURCE', 'Project presentation dependency is missing', group=group, id=reference)
            if reference not in chosen[group]:
                chosen[group].add(reference)
                added = True
        if not added:
            return selected


def project_snapshot(selection: list[dict]) -> dict:
    """冻结清单、Schema、原来源与实际闭包图像；只接受已有原创精确pin。"""
    if any(path.is_symlink() for path in (ROOT, *ROOT.parents)):
        raise DocumentError('INVALID_PROJECT_RESOURCE', 'Trusted project root must not contain symlinks')
    snapshots = {}
    registry = _json_input('game/data/presentation.json', snapshots)
    schema = _json_input('game/data/presentation.schema.json', snapshots)
    presentation._bounded(registry)
    if not StrictValidator(schema).is_valid(registry):
        raise DocumentError('INVALID_PROJECT_PRESENTATION', 'Project presentation does not match its trusted schema')
    pins = _json_input(ORIGINAL_PINS, snapshots)
    if pins.get('format') != 'HM-FOUNDATION-IDENTITY-BINDINGS' or pins.get('format_version') != 1:
        raise DocumentError('UNKNOWN_PROJECT_PROVENANCE', 'Original project asset pins are unavailable')
    originals = {entry['path']: entry['sha256'] for entry in pins.get('resources', [])}
    selected = _closure(registry, schema, _selection(registry, selection))
    resources, artifacts, source_assets = {}, {}, {}
    provenance, provenance_rows = {}, {}
    for group in ('backgrounds', 'portraits'):
        for identifier, record in selected[group].items():
            path = presentation._asset_path(ROOT / 'game', record['path'])
            raw = presentation._asset_bytes(path)
            logical = path.relative_to(ROOT).as_posix()
            digest = _sha(raw)
            if originals.get(logical) != digest:
                raise DocumentError('UNKNOWN_PROJECT_PROVENANCE', 'Resource is not an approved original project asset; external or unclear licenses need explicit review', source=logical)
            provenance_path = path.parent / 'manifest.json'
            provenance_name = provenance_path.relative_to(ROOT).as_posix()
            if provenance_name not in provenance:
                provenance[provenance_name] = _json_input(provenance_name, snapshots)
            if not _has_digest(provenance[provenance_name], digest):
                raise DocumentError('UNKNOWN_PROJECT_PROVENANCE', 'Original generation record does not contain this exact resource digest', source=logical)
            snapshots[path] = raw
            provenance_rows[logical] = {'source': logical, 'sha256': digest,
                                        'generation_manifest': provenance_name,
                                        'generation_manifest_sha256': _sha(snapshots[provenance_path])}
            source_assets[record['path'].removeprefix('res://')] = raw
            target = 'assets/art/project/' + path.relative_to(ROOT / 'game/assets/art').as_posix()
            artifacts[target] = raw
            if sum(len(value) for value in artifacts.values()) > MAX_TOTAL_BYTES:
                raise DocumentError('RESOURCE_IMPORT_LIMIT', 'Selected project resources exceed the existing pack byte budget')
            resources[target] = {'path': target, 'kind': 'image', 'required': True, 'sha256': digest, 'license': LOCAL_USE}
    # 在隔离目录校验已冻结字节，避免校验器再次读取正在变化的原图或Schema。
    with tempfile.TemporaryDirectory(prefix='hanmo-project-selection-') as temporary:
        game = Path(temporary)
        (game / 'data').mkdir()
        (game / 'data/presentation.schema.json').write_bytes(snapshots[ROOT / 'game/data/presentation.schema.json'])
        for logical, raw in source_assets.items():
            path = game / logical
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
        issues = presentation.validate_registry(selected, game)
        if issues:
            raise DocumentError('INVALID_PROJECT_PRESENTATION', 'Selected project resource closure is invalid', issues=issues)
    for group in ('backgrounds', 'portraits'):
        for record in selected[group].values():
            record['path'] = 'res://assets/art/project/' + record['path'].removeprefix('res://assets/art/')
    target = 'assets/art/project/provenance.json'
    raw = canonical({'format': 'HM-PROJECT-ART-USE', 'format_version': 1, 'use': LOCAL_USE,
                     'resources': [provenance_rows[name] for name in sorted(provenance_rows)],
                     'external_distribution_verified': False})
    artifacts[target] = raw
    resources[target] = {'path': target, 'kind': 'data', 'required': True, 'sha256': _sha(raw), 'license': LOCAL_USE}
    artifacts[PRESENTATION_ENTRY] = canonical(selected)
    rows = [{'path': path.relative_to(ROOT).as_posix(), 'sha256': _sha(raw)} for path, raw in sorted(snapshots.items())]
    closure = [{'group': group, 'id': identifier} for group in GROUPS for identifier in sorted(selected.get(group, {}))]
    token = _sha(canonical({'inputs': rows, 'closure': closure, 'use': LOCAL_USE}))
    try:
        _unchanged(snapshots)
    except PackError as exc:
        raise DocumentError('SOURCE_CHANGED', 'Project resources changed while freezing the selection') from exc
    return {'snapshots': snapshots, 'artifacts': artifacts, 'resources': list(resources.values()),
            'result': {'project_sha256': token, 'selection': closure, 'inputs': rows,
                       'available': {group: sorted(registry.get(group, {})) for group in GROUPS},
                       'provenance': [provenance_rows[name] for name in sorted(provenance_rows)],
                       'resources': list(resources.values()), 'use': LOCAL_USE,
                       'external_distribution_verified': False, 'runtime_verified': False}}


def project_resources(selection: list[dict]) -> dict:
    """只向界面返回独立元数据和完整快照token，不暴露字节或可写源路径。"""
    return deepcopy(project_snapshot(selection)['result'])
