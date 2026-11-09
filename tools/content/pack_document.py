"""用私有作者文档编辑单个声明包；保存草稿不等于组合或运行验收。

首片固定入口与资源拓扑，不提供路径导入、受信旧包编辑或自动依赖搜索。
所有编辑经过包版本门，子文档只以独立快照返回。
"""
from __future__ import annotations

from copy import deepcopy
import hashlib
import inspect
import json
from pathlib import Path
import tempfile

from .document import AuthorDocument, DocumentError, _parts, _put
from .document_io import _read_bytes, _write_author_bytes
from .io import MAX_BYTES, _check_tree, canonical
from .pack_errors import PackError
from . import pack_compose, packs
from .validation import TABLES

EDIT_OPERATIONS = frozenset({'set_value', 'add_record', 'delete_record', 'clone_record',
                             'set_text', 'set_flag', 'set_metadata', 'add_field', 'undo', 'redo'})
INPUT_FIELDS = ('content_entries', 'presentation_entries', 'resources')


def _manifest(value: dict, path: Path) -> None:
    """使用完整包Schema及既有JSON预算，拒绝受信旧包适配入口。"""
    packs._validate_manifest(value, str(path))
    try:
        _check_tree(value)
        raw = canonical(value)
    except (ValueError, TypeError, UnicodeError, RecursionError) as exc:
        raise DocumentError('INVALID_JSON', 'Manifest contains invalid JSON values') from exc
    if len(raw) > MAX_BYTES:
        raise DocumentError('DOCUMENT_LIMIT', 'Manifest exceeds 2 MiB')
    if 'legacy_source' in value:
        raise DocumentError('LEGACY_PACK_READ_ONLY', 'Trusted legacy adapters are not editable author packs')
    paths = ['pack.json']
    for field in ('content_entries', 'presentation_entries'):
        for relative in value[field]:
            if not relative.endswith('.json') or relative.endswith('.hmcatalog.json'):
                raise DocumentError('INVALID_DESTINATION', 'Entries must be authored JSON files', entry=relative)
            paths.append(relative)
    paths.extend(resource['path'] for resource in value['resources'])
    if len(paths) != len(set(paths)):
        raise DocumentError('DUPLICATE_ENTRY', 'Manifest, entries and resources must have distinct paths')
    for left in paths:
        if any(right.startswith(left + '/') for right in paths):
            raise DocumentError('INVALID_DESTINATION', 'A declared file cannot also be a parent directory')


def _regular_path(path: Path) -> Path:
    """在读取前拒绝整条路径中的链接和非普通manifest，不扫描其他包。"""
    path = Path(path).absolute()
    if (path.name != 'pack.json' or not path.is_file() or
            any(part.is_symlink() for part in (path, *path.parents))):
        raise DocumentError('INVALID_DESTINATION', 'Open a regular pack.json without symlink parents')
    return path


class PackDocument:
    """聚合私有AuthorDocument与包级单调版本，不对外泄漏可写子文档。"""
    def __init__(self, record: dict, documents: dict[str, AuthorDocument]):
        """保存已核字节基线；构造入口只由open_pack及create_pack使用。"""
        self._path = record['manifest_path']
        self._manifest = deepcopy(record['manifest'])
        self._saved_manifest = canonical(self._manifest)
        self._documents = documents
        self._inputs = pack_compose._input_snapshots([record], {})
        self._revision = 0
        self._project_query = None
        self._import_pending = False
        self._last_compose = None
        self._scenario_query = None
        self._dependency_context = {'records': [], 'snapshots': {}}

    @classmethod
    def open_pack(cls, manifest_path: Path) -> PackDocument:
        """仅打开显式普通包及声明输入，允许内容语义尚未完成的草稿。"""
        path = _regular_path(manifest_path)
        value, raw = packs._read_json(path)
        _manifest(value, path)
        record = packs.discover_packs([path])[0]
        if raw != record['manifest_bytes']:
            raise DocumentError('SOURCE_CHANGED', 'Manifest changed while opening the pack')
        documents = {}
        for entry in value['content_entries']:
            document = AuthorDocument.open(record['file_paths'][entry])
            if document.source_sha256 != hashlib.sha256(record['files'][entry]).hexdigest():
                raise DocumentError('SOURCE_CHANGED', 'Entry changed while opening the pack', entry=entry)
            documents[entry] = document
        result = cls(record, documents)
        result._check_inputs()
        return result

    @classmethod
    def create_pack(cls, directory: Path, manifest: dict) -> PackDocument:
        """只在新目录创建空内容草稿；有资源原件的创建须另走显式导入流程。"""
        directory = Path(directory).absolute()
        if '..' in directory.parts:
            raise DocumentError('INVALID_DESTINATION', 'Creation path must not contain ..')
        value = deepcopy(manifest)
        _manifest(value, directory / 'pack.json')
        if not value['content_entries'] or value['presentation_entries'] or value['resources']:
            raise DocumentError('CREATE_INPUTS_REQUIRED', 'Create requires content entries and no unprovided presentation or resources')
        if any(path.is_symlink() for path in (directory, *directory.parents)):
            raise DocumentError('INVALID_DESTINATION', 'Creation path must not contain symlinks')
        drafts = {}
        for entry in value['content_entries']:
            draft = {'format': 'HM-CONTENT', 'schema_version': 1, 'id': value['id'],
                     'version': value['version'], 'required_capabilities': deepcopy(value['required_capabilities']),
                     **{table: [] for table in TABLES}, 'texts': {}, 'flags': {}}
            raw = canonical(draft)
            AuthorDocument(draft, directory / entry, raw)
            drafts[entry] = raw
        try:
            directory.mkdir()
        except FileExistsError as exc:
            raise DocumentError('DESTINATION_EXISTS', 'Create requires a new directory', path=str(directory)) from exc
        except OSError as exc:
            raise DocumentError('WRITE_FAILED', 'Cannot create pack directory', path=str(directory)) from exc
        try:
            for name, raw in {**drafts, 'pack.json': canonical(value)}.items():
                _write_author_bytes(directory / name, raw)
        except OSError as exc:
            raise DocumentError('WRITE_FAILED', 'Creation incomplete; existing files were not replaced', path=str(directory)) from exc
        return cls.open_pack(directory / 'pack.json')

    def snapshot(self) -> dict:
        """返回manifest及各内容入口的副本，不能借返回值绕过包版本。"""
        return {'manifest': deepcopy(self._manifest),
                'documents': {entry: document.snapshot() for entry, document in self._documents.items()}}

    def status(self) -> dict:
        """报告包与子文档状态；语义校验及运行验收不由dirty标记推断。"""
        entries = {entry: document.status() for entry, document in self._documents.items()}
        manifest_dirty = canonical(self._manifest) != self._saved_manifest
        from .pack_context import binding
        return {'path': str(self._path), 'revision': self._revision,
                'dirty': self._import_pending or manifest_dirty or any(status['dirty'] for status in entries.values()),
                'import_pending': self._import_pending,
                'manifest_dirty': manifest_dirty,
                'manifest_sha256': hashlib.sha256(self._inputs[self._path]).hexdigest(),
                'entries': entries, 'dependencies': [binding(record) for record in self._dependency_context['records']],
                'runtime_verified': False}

    def _primary_record(self, drafts=False) -> dict:
        """主包原摘要从已核输入生成；只有只读作者上下文可读取尚未保存的内存草稿。"""
        from .pack_context import primary_record
        documents = {entry: document.snapshot() for entry, document in self._documents.items()} if drafts else None
        return primary_record(self._path, self._manifest, self._inputs, documents)

    def select_dependencies(self, roots: list, selection: list, expected_revision: int) -> dict:
        """显式冻结依赖根与精确版本；失败保持旧上下文，依赖从不加入可写文档集合。"""
        from .pack_context import freeze_dependencies, binding
        self._guard(expected_revision)
        self._check_inputs()
        candidate = freeze_dependencies(self._primary_record(True), roots, selection)
        self._check_inputs()
        changed = candidate != self._dependency_context
        if changed:
            self._dependency_context = candidate
            self._revision += 1
            self._scenario_query = None
            self._last_compose = None
        return {'changed': changed, 'revision': self._revision,
                'dependencies': [binding(record) for record in candidate['records']], 'runtime_verified': False}

    def reference_context(self, table: str) -> dict:
        """返回带namespace与来源的只读引用候选，所选依赖变化时拒绝旧上下文。"""
        from .pack_context import reference_context
        self._check_inputs()
        result = reference_context(self._primary_record(True), self._dependency_context['records'],
            {entry: document.snapshot() for entry, document in self._documents.items()}, table)
        self._check_inputs()
        return {**result, 'revision': self._revision}

    def project_resources(self, selection: list[dict]) -> dict:
        """只查询当前受信项目表现，选择与全部原件摘要均由同一资源入口冻结。"""
        self._check_inputs()
        from .pack_resources import project_snapshot
        self._project_query = project_snapshot(selection)
        return deepcopy(self._project_query['result'])

    def scenario_snapshot_preview(self, entry: str, scenario_id: str, selection: dict) -> dict:
        """冻结源、当前草稿、选择与Schema，返回可审阅差异；不改变作者版本或文件。"""
        from .scenario_authoring import preview_snapshot
        from .validation import SCHEMA_PATH, SCHEMA_BYTES
        self._check_inputs()
        if SCHEMA_PATH.read_bytes() != SCHEMA_BYTES:
            raise DocumentError('SOURCE_CHANGED', '受信作者Schema已变化，请重启工作进程')
        documents = {name: document.snapshot() for name, document in self._documents.items()}
        if entry not in documents:
            raise DocumentError('UNKNOWN_ENTRY', '初态只可写入当前主包内容入口', entry=entry)
        from .pack_context import author_documents, binding, proposal_issues
        documents, dependency_origins = author_documents(self._primary_record(True), self._dependency_context['records'], documents)
        result = preview_snapshot(documents, entry, scenario_id, selection, self._manifest['namespace'])
        result['issues'].extend(proposal_issues(self._primary_record(True), self._dependency_context['records'],
                                               result['proposal'], entry, scenario_id))
        result['can_apply'] = not result['issues']
        for issue in result['issues']:
            if issue['entry'] in dependency_origins:
                issue['dependency'] = deepcopy(dependency_origins[issue['entry']])
                issue['read_only'] = True
        token = hashlib.sha256(canonical({'manifest': self._manifest, 'documents': documents,
            'revision': self._revision, 'source_sha256': self.status()['manifest_sha256'],
            'dependencies': [binding(record) for record in self._dependency_context['records']],
            'inputs': {path.relative_to(self._path.parent).as_posix(): None if raw is None else hashlib.sha256(raw).hexdigest()
                       for path, raw in self._inputs.items()},
            'schema_sha256': hashlib.sha256(SCHEMA_BYTES).hexdigest(), 'result': result})).hexdigest()
        self._check_inputs()
        self._scenario_query = {'token': token, 'revision': self._revision, 'result': deepcopy(result)}
        return {**result, 'token': token, 'revision': self._revision}

    def apply_scenario_snapshot(self, token: str, expected_revision: int) -> dict:
        """只应用当前会话已预览的候选，源变化或过期确认不得替换旧初态。"""
        from .validation import SCHEMA_PATH, SCHEMA_BYTES
        self._guard(expected_revision)
        self._check_inputs()
        query = self._scenario_query
        if query is None or query['token'] != token or query['revision'] != self._revision:
            raise DocumentError('STALE_SNAPSHOT_PREVIEW', '请重新预览当前剧本选择和完整差异')
        if SCHEMA_PATH.read_bytes() != SCHEMA_BYTES:
            raise DocumentError('SOURCE_CHANGED', '受信作者Schema已变化，请重启工作进程')
        result = query['result']
        if not result['can_apply']:
            raise DocumentError('INVALID_SCENARIO_SNAPSHOT', '请先修复预览诊断', issues=result['issues'])
        document = self._documents[result['entry']]
        changed = document.set_scenario_snapshot(result['scenario_id'], result['proposal'], document.revision)
        if changed['changed']:
            self._revision += 1
        self._scenario_query = None
        return {'changed': changed['changed'], 'revision': self._revision, 'result': changed, 'runtime_verified': False}

    def import_project_resources(self, selection: list[dict], expected_project_sha256: str,
                                 expected_revision: int) -> dict:
        """显式导入冻结的项目资源到新受控路径；部分写入有回执且必须完成后再保存。"""
        self._guard(expected_revision)
        self._check_inputs()
        from .pack_resources import PRESENTATION_ENTRY, project_snapshot
        if (self._project_query is None or
                self._project_query['result']['project_sha256'] != expected_project_sha256):
            raise DocumentError('SOURCE_CHANGED', 'Query the exact project selection again before importing')
        try:
            pack_compose._unchanged(self._project_query['snapshots'])
        except PackError as exc:
            raise DocumentError('SOURCE_CHANGED', 'Project inputs changed since resource selection') from exc
        record = project_snapshot(selection)
        if record['result']['project_sha256'] != expected_project_sha256:
            raise DocumentError('SOURCE_CHANGED', 'The import selection differs from the queried snapshot')
        candidate = deepcopy(self._manifest)
        if candidate['presentation_entries'] not in ([], [PRESENTATION_ENTRY]):
            raise DocumentError('PRESENTATION_IMPORT_CONFLICT', 'Existing presentation entries cannot be implicitly replaced')
        candidate['presentation_entries'] = [PRESENTATION_ENTRY]
        resources = {entry['path']: entry for entry in candidate['resources']}
        for entry in record['resources']:
            if entry['path'] in resources and resources[entry['path']] != entry:
                raise DocumentError('IMPORT_DESTINATION_CONFLICT', 'A different resource declaration already owns the import path', path=entry['path'])
            resources[entry['path']] = entry
        candidate['resources'] = [resources[path] for path in sorted(resources)]
        _manifest(candidate, self._path)
        targets, missing = {}, []
        for name, raw in record['artifacts'].items():
            target = packs._contained(self._path.parent, name)
            if target.exists():
                if _read_bytes(target, _max_bytes=packs.MAX_RESOURCE_BYTES) != raw:
                    raise DocumentError('IMPORT_DESTINATION_CONFLICT', 'Import will not overwrite an existing different file', path=name)
            else:
                missing.append(name)
            targets[name] = target
        members = set(self._path.parent.rglob('*'))
        total_bytes = sum(path.stat().st_size for path in members if path.is_file())
        total_bytes += sum(len(record['artifacts'][name]) for name in missing)
        for name in missing:
            target = targets[name]
            members.add(target)
            members.update(parent for parent in target.parents if parent.is_relative_to(self._path.parent) and parent != self._path.parent)
        if total_bytes > packs.MAX_TOTAL_BYTES or len(members) > packs.MAX_MEMBERS:
            raise DocumentError('RESOURCE_IMPORT_LIMIT', 'Import would exceed the existing package member or byte budget')
        if not missing and canonical(candidate) == canonical(self._manifest) and not self._import_pending:
            return {'changed': False, 'revision': self._revision, 'imported': [], 'runtime_verified': False}
        self._import_pending = True
        self._revision += 1
        imported = []
        try:
            for name in sorted(record['artifacts'], key=lambda path: (path == PRESENTATION_ENTRY, path)):
                raw = record['artifacts'][name]
                self._check_inputs()
                pack_compose._unchanged(record['snapshots'])
                if name in missing:
                    _write_author_bytes(targets[name], raw, _max_bytes=packs.MAX_RESOURCE_BYTES)
                self._inputs[targets[name]] = raw
                imported.append(name)
            self._check_inputs()
            pack_compose._unchanged(record['snapshots'])
        except (DocumentError, PackError) as exc:
            code = 'SOURCE_CHANGED' if isinstance(exc, PackError) and exc.code == 'SourceChanged' else exc.code
            raise DocumentError(code, str(exc), imported=imported, pack_status=self.status()) from exc
        self._manifest = candidate
        self._import_pending = False
        self._project_query = None
        return {'changed': True, 'revision': self._revision, 'imported': imported,
                'project_sha256': expected_project_sha256, 'requires_save': True,
                'external_distribution_verified': False, 'runtime_verified': False}

    def _guard(self, expected_revision: int) -> None:
        """每次写入都要求包版本精确相等，拒绝布尔值或省略版本。"""
        if type(expected_revision) is not int or expected_revision != self._revision:
            raise DocumentError('STALE_REVISION', 'The pack changed; refresh the view', actual_revision=self._revision)

    def _check_inputs(self) -> None:
        """校验打开或最后成功保存的全部原字节、可选缺件和受限目录。"""
        try:
            pack_compose._unchanged({**self._inputs, **self._dependency_context['snapshots']})
            packs._check_members(self._path.parent)
            for record in self._dependency_context['records']:
                packs._check_members(record['root'])
        except PackError as exc:
            raise DocumentError('SOURCE_CHANGED', 'Pack inputs changed outside this document', cause=exc.to_dict()) from exc
        for entry, document in self._documents.items():
            raw = self._inputs[document.path]
            if hashlib.sha256(raw).hexdigest() != document.source_sha256:
                raise DocumentError('SOURCE_CHANGED', 'Child source no longer matches the pack snapshot', entry=entry)

    def edit(self, entry: str, operation: str, arguments: dict, expected_revision: int) -> dict:
        """固定白名单转发既有编辑引擎，同时检查包版本和子文档当前版本。"""
        self._guard(expected_revision)
        self._check_inputs()
        if operation not in EDIT_OPERATIONS:
            raise DocumentError('UNKNOWN_OPERATION', 'Unsupported author edit operation')
        if entry not in self._documents:
            raise DocumentError('UNKNOWN_ENTRY', 'Entry is not an editable content document', entry=entry)
        if not isinstance(arguments, dict) or 'expected_revision' in arguments:
            raise DocumentError('INVALID_ARGUMENTS', 'Supply edit fields without a child revision')
        document = self._documents[entry]
        method = getattr(document, operation)
        kwargs = deepcopy(arguments)
        kwargs['expected_revision'] = document.revision
        try:
            inspect.signature(method).bind(**kwargs)
        except TypeError as exc:
            raise DocumentError('INVALID_ARGUMENTS', str(exc)) from exc
        before = document.revision
        result = method(**kwargs)
        changed = document.revision != before
        if changed:
            self._revision += 1
        return {'changed': changed, 'revision': self._revision, 'result': deepcopy(result), 'runtime_verified': False}

    def set_manifest(self, pointer: str, value, expected_revision: int) -> dict:
        """以完整Schema检查manifest字段候选；首片不隐式更换声明输入拓扑。"""
        self._guard(expected_revision)
        self._check_inputs()
        candidate = deepcopy(self._manifest)
        _put(candidate, _parts(pointer), value)
        _manifest(candidate, self._path)
        if any(candidate[field] != self._manifest[field] for field in INPUT_FIELDS):
            raise DocumentError('INPUT_LAYOUT_CHANGED', 'Reopen or explicitly import inputs before changing entry/resource layout')
        changed = canonical(candidate) != canonical(self._manifest)
        if changed:
            self._manifest = candidate
            self._revision += 1
        return {'changed': changed, 'revision': self._revision, 'runtime_verified': False}

    def _save_manifest(self) -> bool:
        """沿既有作者暂存与原件备份机制保存声明，发布前再核全包输入。"""
        data = canonical(self._manifest)
        if data == self._saved_manifest:
            return False
        _write_author_bytes(self._path, data,
                            expected_sha256=hashlib.sha256(self._inputs[self._path]).hexdigest(),
                            before_publish=self._check_inputs)
        self._inputs[self._path] = data
        self._saved_manifest = data
        self._revision += 1
        return True

    def save(self, expected_revision: int) -> dict:
        """逐文件复用作者保护保存；失败明确已保存项，不伪称跨文件原子事务。"""
        self._guard(expected_revision)
        self._check_inputs()
        if self._import_pending:
            raise DocumentError('INCOMPLETE_IMPORT', 'Finish or reopen the interrupted resource import before saving')
        saved = {}
        try:
            for entry, document in self._documents.items():
                self._check_inputs()
                result = document.save(expected_revision=document.revision)
                saved[entry] = result
                if result['changed']:
                    self._inputs[document.path] = canonical(document.snapshot())
                    self._revision += 1
            self._check_inputs()
            manifest_changed = self._save_manifest()
        except DocumentError as exc:
            raise DocumentError(exc.code, str(exc), **{**exc.details, 'saved_entries': saved,
                                'pack_status': self.status()}) from exc
        return {'status': 'PACK_SAVED', 'revision': self._revision, 'saved_entries': saved,
                'manifest_changed': manifest_changed, 'runtime_verified': False}

    def compose(self, output: Path, expected_revision: int) -> dict:
        """正式组合先产私有代，再核原快照发布；来源变化不会替换调用者旧代。"""
        self._guard(expected_revision)
        self._check_inputs()
        if self.status()['dirty']:
            raise DocumentError('UNSAVED_CHANGES', 'Save all pack edits before composing')
        from .pack_context import resolve_context, binding
        records = resolve_context(self._primary_record(), self._dependency_context['records'])
        roots = [record['manifest_path'] for record in records]
        output = pack_compose._protected_output(Path(output), [record['root'] for record in records])
        baseline = deepcopy({**self._inputs, **self._dependency_context['snapshots']})
        trusted = pack_compose._trusted_inputs()
        selection = {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                     'packs': [{'id': record['manifest']['id'], 'version': record['manifest']['version']} for record in records]}
        output.parent.mkdir(parents=True, exist_ok=True)
        try:
            with tempfile.TemporaryDirectory(prefix='.pack-document-', dir=output.parent) as temporary:
                candidate = Path(temporary) / 'generation'
                result = pack_compose.compose_pack_set(roots, selection, candidate)
                self._check_inputs()
                lock = json.loads((candidate / 'pack-set.lock.json').read_bytes())
                if lock['packs'] != [binding(record) for record in records]:
                    raise DocumentError('SOURCE_CHANGED', 'Composer inputs differ from the saved author snapshot')
                expected_trusted = {
                    'schema_sha256': packs._sha(trusted[pack_compose.ROOT / 'schemas/content.schema.json']),
                    'rule_capabilities_sha256': packs._sha(trusted[pack_compose.ROOT / 'game/data/rule_capabilities.json']),
                    'rules_api': json.loads(trusted[pack_compose.ROOT / 'game/data/pack_runtime.json']),
                }
                if any(lock[key] != value for key, value in expected_trusted.items()):
                    raise DocumentError('SOURCE_CHANGED', 'Composer rules or schema differ from the frozen trusted snapshot')
                marker = pack_compose._verify_generation(candidate)
                artifacts = {name: (candidate / name).read_bytes() for name in marker['files']}
                warnings = pack_compose._publish(output, artifacts, {**trusted, **baseline},
                                                lambda _stage: self._check_inputs())
                self._last_compose = {'output': output, 'pack_set_digest': result['pack_set_digest'],
                                      'revision': self._revision}
                return {**result, 'warnings': result['warnings'] + warnings,
                        'revision': self._revision, 'runtime_verified': False}
        except PackError as exc:
            if exc.code == 'SourceChanged':
                raise DocumentError('SOURCE_CHANGED', str(exc), cause=exc.to_dict()) from exc
            raise

    def preview_prepare(self, destination: Path, expected_pack_set_digest: str,
                        expected_revision: int, *, native_stage: Path | None = None) -> dict:
        """在原版本与源门内准备当前组合；可选原生来源不改变作者身份或存档隔离。"""
        self._guard(expected_revision)
        self._check_inputs()
        if self.status()['dirty']:
            raise DocumentError('UNSAVED_CHANGES', 'Save the entire pack before preparing an isolated preview')
        if (self._last_compose is None or self._last_compose['revision'] != self._revision or
                self._last_compose['pack_set_digest'] != expected_pack_set_digest):
            raise DocumentError('COMPOSE_REQUIRED', 'Compose this saved pack revision before preparing its preview')
        from .pack_preview import prepare_preview
        result = prepare_preview(self._last_compose['output'], Path(destination),
                                 expected_pack_set_digest=expected_pack_set_digest, native_stage=native_stage)
        try:
            self._check_inputs()
        except DocumentError as exc:
            raise DocumentError('SOURCE_CHANGED', 'Author inputs changed during preview preparation; do not launch it',
                                prepared_directory=str(destination)) from exc
        return {**result, 'revision': self._revision, 'runtime_verified': False}
