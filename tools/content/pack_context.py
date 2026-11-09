"""冻结作者明确提供的依赖包；仅暴露只读定义，不扫描邻域或建立可写子文档。"""
from copy import deepcopy
import json
from pathlib import Path

from . import packs, pack_compose
from .document import DocumentError
from .io import canonical
from .references import _iter_annotated
from .validation import SCHEMA, TABLES, pointer


def binding(record):
    """返回实际身份和三个原件摘要，保持与正式锁同一逐包字段。"""
    return {**{key: record['manifest'][key] for key in ('id', 'namespace', 'version')},
            **{key: record[key] for key in ('manifest_sha256', 'content_sha256', 'resources_sha256')}}


def primary_record(path, manifest, inputs, documents=None):
    """由主包已冻结输入构建解析记录；可选内存草稿只供作者只读上下文。"""
    files = {file.relative_to(path.parent).as_posix(): raw for file, raw in inputs.items()
             if file != path and raw is not None}
    if documents is not None:
        files.update({name: canonical(value) for name, value in documents.items()})
    raw = inputs[path] if json.loads(inputs[path]) == manifest else canonical(manifest)
    content_paths = sorted(manifest['content_entries'] + manifest['presentation_entries'])
    resource_paths = sorted(row['path'] for row in manifest['resources'] if row['path'] in files)
    paths = content_paths + [row['path'] for row in manifest['resources']]
    return {'manifest': deepcopy(manifest), 'root': path.parent, 'manifest_path': path,
            'manifest_bytes': raw, 'manifest_sha256': packs._sha(raw), 'files': files,
            'file_paths': {name: path.parent / name for name in paths}, 'content_paths': content_paths,
            'resource_paths': resource_paths, 'missing_resources': [name for name in paths if name not in files],
            'content_sha256': packs._digest_files(files, content_paths),
            'resources_sha256': packs._digest_files(files, resource_paths)}


def resolve_context(primary, dependencies):
    """仅解析固定版本并拒绝无关根，所有直接与传递依赖都须由作者显式提供。"""
    records = [primary, *dependencies]
    selected = [{'id': record['manifest']['id'], 'version': record['manifest']['version']} for record in records]
    result = packs.resolve_pack_set(records, selected, packs.load_pack_runtime())
    index = {record['manifest']['id']: record for record in records}
    reached, pending = set(), [primary['manifest']['id']]
    while pending:
        identifier = pending.pop()
        if identifier in reached:
            continue
        reached.add(identifier)
        pending.extend(row['id'] for row in index[identifier]['manifest']['dependencies'])
    if reached != set(index):
        raise DocumentError('UNDECLARED_DEPENDENCY_ROOT', '只可提供当前主包声明的直接或传递依赖根',
                            packs=sorted(set(index) - reached))
    return result['packs']


def freeze_dependencies(primary, roots, selection):
    """每个根只接受本目录pack.json，显式版本须一一对应；失败不影响原会话。"""
    if not isinstance(roots, list) or not isinstance(selection, list):
        raise DocumentError('INVALID_DEPENDENCY_SELECTION', 'roots与selection须为显式列表')
    manifests = []
    for root in roots:
        if not isinstance(root, (str, Path)):
            raise DocumentError('INVALID_DEPENDENCY_ROOT', '依赖根须为目录或pack.json路径')
        path = Path(root)
        if '..' in path.parts or any(part.is_symlink() for part in (path, *path.parents)):
            raise DocumentError('INVALID_DEPENDENCY_ROOT', '依赖根不得含链接或上级跳转')
        path = path.absolute()
        if path.is_dir():
            path /= 'pack.json'
        if path.name != 'pack.json' or not path.is_file() or path.is_symlink():
            raise DocumentError('INVALID_DEPENDENCY_ROOT', '显式根本身须含正规pack.json；不读取存档或目录产物', path=str(path))
        if (path.parent.is_relative_to(primary['root']) or primary['root'].is_relative_to(path.parent)
                or any(path.parent.is_relative_to(other.parent) or other.parent.is_relative_to(path.parent) for other in manifests)):
            raise DocumentError('OVERLAPPING_DEPENDENCY_ROOT', '主包与每个只读依赖根须互不嵌套', path=str(path))
        manifests.append(path)
    records = packs.discover_packs(manifests)
    if len(records) != len(manifests):
        raise DocumentError('AMBIGUOUS_DEPENDENCY_ROOT', '同一依赖身份只能提供一个实际根')
    # 先使用原选择Schema与版本错误，不能按目录名猜选包或悄悄选最新版。
    chosen = packs.resolve_pack_set([primary, *records],
        [{'id': primary['manifest']['id'], 'version': primary['manifest']['version']}, *selection], packs.load_pack_runtime())['packs']
    dependencies = [record for record in chosen if record['manifest']['id'] != primary['manifest']['id']]
    if len(dependencies) != len(records):
        raise DocumentError('UNSELECTED_DEPENDENCY_ROOT', '每个提供的依赖根都须显式选择实际id/version')
    resolve_context(primary, dependencies)
    snapshots = pack_compose._input_snapshots(dependencies, {})
    if set(snapshots) & set(pack_compose._input_snapshots([primary], {})):
        raise DocumentError('OVERLAPPING_DEPENDENCY_SOURCE', '只读依赖不得与主包可写输入共用文件')
    pack_compose._unchanged(snapshots)
    return {'records': dependencies, 'snapshots': snapshots}


def author_documents(primary, dependencies, documents):
    """沿正式限定函数读取依赖定义，主包草稿保留可编辑本地ID及真实来源。"""
    records = resolve_context(primary, dependencies)
    selected = {record['manifest']['namespace']: record for record in records}
    result, origins = deepcopy(documents), {}
    for name, value in result.items():
        for path, _target, reference in _iter_annotated(value):
            pack_compose._qualified(reference, primary, selected, name)
    for record in dependencies:
        manifest = record['manifest']
        entries = ([name for name in record['content_paths'] if name.endswith('/' + manifest['legacy_source'] + '.json')]
                   if 'legacy_source' in manifest else manifest['content_entries'])
        for name in entries:
            key = '@dependency/' + manifest['id'] + '/' + manifest['version'] + '/' + name
            value, _chains = pack_compose._qualify_content(json.loads(record['files'][name]), record, name, selected)
            result[key] = value
            origins[key] = {'pack_id': manifest['id'], 'namespace': manifest['namespace'],
                            'version': manifest['version'], 'entry': name, 'read_only': True}
    identifiers = set()
    dictionaries = set()
    for entry, document in result.items():
        prefix = '' if entry in origins else primary['manifest']['namespace'] + '::'
        for table in TABLES:
            for row in document.get(table, []):
                identifier = prefix + row['id']
                if identifier in identifiers:
                    raise DocumentError('AMBIGUOUS_REFERENCE', '上下文实体ID全表必须唯一', id=identifier, entry=entry)
                identifiers.add(identifier)
        for table in ('texts', 'flags'):
            for identifier in document.get(table, {}):
                key = table, prefix + identifier
                if key in dictionaries:
                    raise DocumentError('AMBIGUOUS_REFERENCE', '上下文字典键不得重复', id=key[1], entry=entry)
                dictionaries.add(key)
    return result, origins


def proposal_issues(primary, dependencies, proposal, entry, scenario_id):
    """新初态引用仍须满足主包直接依赖与legacy导入，不借只读上下文越过编译边界。"""
    selected = {record['manifest']['namespace']: record for record in [primary, *dependencies]}
    issues = []
    rule = SCHEMA['properties']['scenarios']['items']
    for path, _target, reference in _iter_annotated(proposal, rule):
        try:
            pack_compose._qualified(reference, primary, selected, entry + '#' + pointer(path))
        except packs.PackError as exc:
            issues.append({'code': exc.code, 'entry': entry, 'path': pointer(('scenarios', scenario_id) + path),
                           'record_id': scenario_id, 'message': str(exc)})
    for path, _target, identifier in _iter_annotated(proposal, rule, annotation='x-hanmo-global-definition'):
        if '::' in identifier and pack_compose._initial_unit_override_path(('scenarios', 0) + path):
            try:
                pack_compose._validate_initial_unit_override({'scenarios': [proposal]}, primary, selected, ('scenarios', 0) + path, identifier, entry)
            except packs.PackError as exc:
                issues.append({'code': exc.code, 'entry': entry, 'path': pointer(('scenarios', scenario_id) + path), 'record_id': scenario_id, 'message': str(exc)})
        elif '::' in identifier:
            issues.append({'code': 'QualifiedPackDefinition', 'entry': entry,
                           'path': pointer(('scenarios', scenario_id) + path), 'record_id': scenario_id,
                           'message': '现编译合同要求嵌套全局定义使用主包本地ID，不能复制依赖部队身份为新定义'})
    return issues


def reference_context(primary, dependencies, documents, table):
    """列出带包归属的候选和完整记录副本，非直接依赖不能作为可选引用。"""
    tables = (*TABLES, 'texts', 'flags')
    if table != '*' and table not in tables:
        raise DocumentError('UNKNOWN_TABLE', '引用表不在当前内容Schema中')
    values, origins = author_documents(primary, dependencies, documents)
    selected = {record['manifest']['namespace']: record for record in [primary, *dependencies]}
    result = {}
    for name in tables if table == '*' else (table,):
        rows = []
        for entry, value in values.items():
            source = origins.get(entry, {'pack_id': primary['manifest']['id'], 'namespace': primary['manifest']['namespace'],
                                         'version': primary['manifest']['version'], 'entry': entry, 'read_only': False})
            definitions = value.get(name, {}) if name in ('texts', 'flags') else {row['id']: row for row in value.get(name, [])}
            for identifier, record in definitions.items():
                qualified = identifier if source['read_only'] else source['namespace'] + '::' + identifier
                selectable = True
                try:
                    pack_compose._qualified(qualified, primary, selected, entry)
                except packs.PackError:
                    selectable = False
                rows.append({**source, 'id': identifier if not source['read_only'] else qualified,
                             'qualified_id': qualified, 'selectable': selectable, 'record': deepcopy(record)})
        result[name] = sorted(rows, key=lambda row: row['qualified_id'])
    return {'table': table, **({'tables': result} if table == '*' else {'records': result[table]}),
            'dependencies': [binding(record) for record in dependencies], 'runtime_verified': False}
