"""统一的作者文档与可撤销编辑；只管理内容定义，不模拟游戏世界。"""
from __future__ import annotations

from copy import deepcopy
from dataclasses import dataclass
import hashlib
import re
from pathlib import Path

from .io import _read_snapshot, _check_tree, canonical, MAX_BYTES
from .references import find_references, iter_references, local_reference_issues, _possible
from .validation import SCHEMA, TABLES, validate_pack, _bounded

ID = re.compile(r'^[a-z][a-z0-9_.-]{0,79}$')
MAX_HISTORY = 64
MAX_HISTORY_BYTES = 16 * 1024 * 1024


class DocumentError(ValueError):
    """携带稳定错误码和引用影响的作者编辑异常。"""
    def __init__(self, code: str, message: str, **details):
        """记录可读错误、错误码及细节，不把编辑失败转换成游戏结果。"""
        super().__init__(message)
        self.code = code
        self.details = details

    def to_dict(self) -> dict:
        """输出独立的结构化诊断，防止界面修改异常的内部数据。"""
        return {'code': self.code, 'message': str(self), 'details': deepcopy(self.details)}


def _identifier(value: str) -> None:
    """在记录进入编辑器索引前校验稳定ID的格式和长度。"""
    if not isinstance(value, str) or not ID.fullmatch(value):
        raise DocumentError('INVALID_ID', 'ID must match [a-z][a-z0-9_.-]{0,79}')


def _check_document(pack: dict) -> None:
    """校验可编辑文档的身份、大小和数据边界；允许尚未完成的语义草稿。"""
    if not _bounded(pack):
        raise DocumentError('INVALID_JSON', 'Non-JSON values or tree limits exceeded')
    try:
        _check_tree(pack)
        encoded = canonical(pack)
    except (ValueError, TypeError, UnicodeError, RecursionError) as exc:
        raise DocumentError('INVALID_JSON', str(exc)) from exc
    if len(encoded) > MAX_BYTES:
        raise DocumentError('DOCUMENT_LIMIT', 'Author document exceeds 2 MiB')
    if not isinstance(pack, dict) or pack.get('format') != 'HM-CONTENT' or type(pack.get('schema_version')) is not int or pack.get('schema_version') != 1:
        raise DocumentError('INVALID_DOCUMENT', 'Expected HM-CONTENT schema_version 1')
    ids = set()
    for table in TABLES:
        if table not in pack and table not in SCHEMA.get('required', []):
            continue
        if not isinstance(pack.get(table), list):
            raise DocumentError('INVALID_DOCUMENT', 'Missing record array: ' + table)
        for record in pack[table]:
            if not isinstance(record, dict):
                raise DocumentError('INVALID_DOCUMENT', 'Record must be an object: ' + table)
            _identifier(record.get('id'))
            if record['id'] in ids:
                raise DocumentError('DUPLICATE_ID', 'Entity ID already exists: ' + record['id'])
            ids.add(record['id'])
    for table in ('texts', 'flags'):
        if not isinstance(pack.get(table), dict):
            raise DocumentError('INVALID_DOCUMENT', table + ' must be an object')


def _parts(pointer: str) -> list[str]:
    """解析非根JSON指针，拒绝不完整转义而不是猜测字段路径。"""
    if not isinstance(pointer, str) or not pointer.startswith('/') or re.search(r'~(?![01])', pointer):
        raise DocumentError('INVALID_POINTER', 'Use a non-root JSON pointer with ~0/~1 escapes')
    return [s.replace('~1', '/').replace('~0', '~') for s in pointer[1:].split('/')]


def _put(root, parts, value) -> None:
    """只在独立候选中替换已存在字段；无效路径不新增结构或破坏原数据。"""
    node = root
    try:
        for part in parts[:-1]:
            if isinstance(node, list):
                if not re.fullmatch(r'0|[1-9][0-9]*', part):
                    raise KeyError(part)
                node = node[int(part)]
            else:
                node = node[part]
        key = parts[-1]
        if isinstance(node, list):
            if not re.fullmatch(r'0|[1-9][0-9]*', key):
                raise KeyError(key)
            node[int(key)] = deepcopy(value)
        elif isinstance(node, dict) and key in node:
            node[key] = deepcopy(value)
        else:
            raise KeyError(key)
    except (KeyError, IndexError, TypeError, ValueError) as exc:
        raise DocumentError('INVALID_POINTER', 'Field does not exist at the requested pointer') from exc


def _fresh_id(prefix: str, taken: set[str]) -> str:
    """按确定顺序分配未使用的合法ID，不修改已有实体身份。"""
    _identifier(prefix)
    for count in range(1, 200001):
        suffix = '.copy' if count == 1 else '.copy' + str(count)
        candidate = prefix[:80 - len(suffix)] + suffix
        if candidate not in taken:
            return candidate
    raise DocumentError('ID_EXHAUSTED', 'Cannot allocate an unused ID')


def _resolved_rule(rule: dict, value) -> dict:
    """沿本地引用和当前类型选择字段规则；不要求其他草稿字段已经有效。"""
    for _ in range(32):
        if '$ref' in rule:
            rule = SCHEMA['$defs'][rule['$ref'].split('/')[-1]]
            continue
        variants = rule.get('oneOf', rule.get('anyOf', []))
        if not variants:
            return rule
        matching = [candidate for candidate in variants if _possible(value, candidate)]
        if not matching:
            raise DocumentError('UNKNOWN_FIELD', '请先修复所属对象的类型，再新增字段')
        rule = matching[0]
    raise DocumentError('UNKNOWN_FIELD', '字段Schema引用超过允许深度')


def _object_at(record: dict, rule: dict, pointer: str) -> tuple[dict, dict]:
    """同时定位候选对象与Schema，拒绝不存在的路径及非法数组索引。"""
    node = record
    try:
        for part in ([] if pointer == '' else _parts(pointer)):
            rule = _resolved_rule(rule, node)
            if isinstance(node, list) and re.fullmatch(r'0|[1-9][0-9]*', part):
                node, rule = node[int(part)], rule.get('items', {})
            elif isinstance(node, dict) and part in node:
                node, rule = node[part], rule.get('properties', {}).get(part, {})
            else:
                raise KeyError(part)
        if not isinstance(node, dict):
            raise KeyError(pointer)
        return node, _resolved_rule(rule, node)
    except (KeyError, IndexError, TypeError, ValueError) as exc:
        if isinstance(exc, DocumentError):
            raise
        raise DocumentError('INVALID_POINTER', '新增字段的所属对象不存在') from exc


@dataclass
class HistoryEntry:
    """保存一次原子编辑的前后内容与大小，用于受限撤销历史。"""
    before: dict
    after: dict
    label: str
    size: int


class AuthorDocument:
    """供各编辑面板共用的作者数据，外部只能获取独立副本。"""
    def __init__(self, pack: dict, path: Path, raw: bytes):
        """从真实文件快照建立独立作者文档、原摘要和空撤销栈。"""
        _check_document(pack)
        self._data = deepcopy(pack)
        self.path = Path(path).absolute()
        self.source_sha256 = hashlib.sha256(raw).hexdigest()
        self._saved_content = canonical(pack)
        self.revision = 0
        self._undo: list[HistoryEntry] = []
        self._redo: list[HistoryEntry] = []

    @classmethod
    def open(cls, path: Path) -> AuthorDocument:
        """读取大小受限的作者文件，结构有效但语义有误的内容仍可作为草稿打开。"""
        try:
            pack, raw = _read_snapshot(Path(path))
            return cls(pack, Path(path), raw)
        except DocumentError:
            raise
        except ValueError as exc:
            raise DocumentError('READ_FAILED', str(exc)) from exc

    def snapshot(self) -> dict:
        """返回完整独立快照，界面不能用引用绕过修改历史。"""
        return deepcopy(self._data)

    def _index(self, table: str, record_id: str, pack=None) -> int:
        """在允许的数据表中按稳定ID定位记录，找不到时明确报错。"""
        if table not in TABLES:
            raise DocumentError('UNKNOWN_TABLE', 'Unknown author table: ' + str(table))
        records = (self._data if pack is None else pack).get(table, [])
        for n, record in enumerate(records):
            if record['id'] == record_id:
                return n
        raise DocumentError('UNKNOWN_RECORD', 'Unknown record: ' + str(record_id))

    def record(self, table: str, record_id: str) -> dict:
        """返回单条记录副本，用于表单或图编辑器显示。"""
        index = self._index(table, record_id)
        return deepcopy(self._data[table][index])

    def status(self) -> dict:
        """描述未保存标记与撤销历史，不据此宣称内容已经可玩。"""
        return {'path': str(self.path), 'revision': self.revision,
                'dirty': canonical(self._data) != self._saved_content,
                'can_undo': bool(self._undo), 'can_redo': bool(self._redo),
                'undo_count': len(self._undo), 'redo_count': len(self._redo),
                'history_bytes': sum(e.size for e in self._undo + self._redo),
                'source_sha256': self.source_sha256, 'runtime_verified': False}

    def _guard(self, revision=None) -> None:
        """拒绝过期界面的编辑请求，不改变内容或撤销历史。"""
        if revision is not None and (type(revision) is not int or revision != self.revision):
            raise DocumentError('STALE_REVISION', 'The document changed; refresh the view',
                                actual_revision=self.revision)

    def _commit(self, candidate: dict, label: str) -> dict:
        """校验候选并提交一次可撤销编辑，限制历史容量并清除失效重做。"""
        _check_document(candidate)
        old_bytes, new_bytes = canonical(self._data), canonical(candidate)
        if old_bytes == new_bytes:
            return {'changed': False, 'revision': self.revision}
        self._redo.clear()
        self._undo.append(HistoryEntry(deepcopy(self._data), deepcopy(candidate), label,
                                       len(old_bytes) + len(new_bytes)))
        while len(self._undo) > MAX_HISTORY or sum(e.size for e in self._undo) > MAX_HISTORY_BYTES:
            self._undo.pop(0)
        self._data = candidate
        self.revision += 1
        return {'changed': True, 'revision': self.revision, 'label': label}

    def set_value(self, table, record_id, pointer, value, expected_revision=None) -> dict:
        """按ID和指针替换现有字段；实体ID不可原地改写。"""
        self._guard(expected_revision)
        parts = _parts(pointer)
        if parts[0] == 'id':
            raise DocumentError('IMMUTABLE_ID', 'Duplicate a record instead of changing its stable ID')
        index = self._index(table, record_id)
        candidate = self.snapshot()
        _put(candidate[table][index], parts, value)
        return self._commit(candidate, 'Edit ' + record_id + pointer)

    def set_scenario_snapshot(self, scenario_id, snapshot, expected_revision=None) -> dict:
        """一次替换明确的四组剧本初态并保留完整撤销；协议只能由已确认候选调用。"""
        from .scenarios import SNAPSHOT_FIELDS
        from .scenario_authoring import _validator
        self._guard(expected_revision)
        if not isinstance(snapshot, dict) or set(snapshot) != set(SNAPSHOT_FIELDS):
            raise DocumentError('INVALID_SCENARIO_SNAPSHOT', '必须完整提供四组初态字段')
        candidate = self.snapshot()
        index = self._index('scenarios', scenario_id)
        candidate['scenarios'][index].update(deepcopy(snapshot))
        errors = list(_validator(SCHEMA['properties']['scenarios']['items']).iter_errors(candidate['scenarios'][index]))
        if errors:
            raise DocumentError('INVALID_SCENARIO_SNAPSHOT', errors[0].message)
        return self._commit(candidate, 'Set scenario snapshot ' + scenario_id)

    def add_record(self, table, record, expected_revision=None) -> dict:
        """加入大小合法的新记录，允许非身份字段保持未完成草稿状态。"""
        self._guard(expected_revision)
        if table not in TABLES:
            raise DocumentError('UNKNOWN_TABLE', 'Unknown author table: ' + str(table))
        candidate = self.snapshot()
        candidate.setdefault(table, []).append(deepcopy(record))
        return self._commit(candidate, 'Add record to ' + table)

    def references_to(self, table, record_id) -> list[dict]:
        """根据Schema查找指向记录的引用，附近草稿错误不应丢失有效引用。"""
        self._index(table, record_id)
        return find_references(self._data, table, record_id)

    def delete_record(self, table, record_id, expected_revision=None) -> dict:
        """删除无外部引用的记录；有依赖时拒绝并保留内容。"""
        self._guard(expected_revision)
        refs = [r for r in self.references_to(table, record_id)
                if (r['source_table'], r['source_id']) != (table, record_id)]
        if refs:
            raise DocumentError('REFERENCED_RECORD', 'Other content still refers to this record', references=refs)
        candidate = self.snapshot()
        del candidate[table][self._index(table, record_id)]
        return self._commit(candidate, 'Delete ' + record_id)

    def clone_record(self, table, record_id, new_id=None, expected_revision=None) -> dict:
        """复制记录与自有文本，重连对白内部节点，保留外部人物和任务身份。"""
        self._guard(expected_revision)
        clone = self.record(table, record_id)
        candidate = self.snapshot()
        taken = {r['id'] for name in TABLES for r in candidate.get(name, [])}
        if new_id is None:
            new_id = _fresh_id(record_id, taken)
        _identifier(new_id)
        if new_id in taken:
            raise DocumentError('DUPLICATE_ID', 'Entity ID already exists: ' + new_id)
        clone['id'] = new_id
        if table == 'dialogues':
            nodes = clone.get('nodes', [])
            if not isinstance(nodes, list) or any(not isinstance(n, dict) or not ID.fullmatch(str(n.get('id',''))) for n in nodes):
                raise DocumentError('INVALID_DIALOGUE', 'Repair node IDs before duplicating this dialogue')
            old_ids = [n['id'] for n in nodes]
            if len(old_ids) != len(set(old_ids)):
                raise DocumentError('INVALID_DIALOGUE', 'Duplicate node IDs must be repaired first')
            remap = {}
            used_nodes = set(old_ids)
            for old in old_ids:
                remap[old] = _fresh_id(old, used_nodes)
                used_nodes.add(remap[old])
            if clone.get('entry') not in remap:
                raise DocumentError('INVALID_DIALOGUE', 'Dialogue entry must reference an existing node')
            clone['entry'] = remap[clone['entry']]
            for node in nodes:
                node['id'] = remap[node['id']]
                for key in ('next','if_true','if_false','on_success','on_failure'):
                    if key in node:
                        if node[key] not in remap:
                            raise DocumentError('INVALID_DIALOGUE', 'Repair dangling node connections before duplicating')
                        node[key] = remap[node[key]]
                options = node.get('options', [])
                if not isinstance(options, list) or any(not isinstance(o, dict) for o in options):
                    raise DocumentError('INVALID_DIALOGUE', 'Repair choice options before duplicating')
                for option in options:
                    if not isinstance(option.get('next'), str) or option.get('next') not in remap:
                        raise DocumentError('INVALID_DIALOGUE', 'Repair option connection before duplicating')
                    option['next'] = remap[option['next']]
        # 使用记录实际Schema上下文，只复制被标为自有文本的引用。
        wrapper = {table: [clone]}
        text_map = {}
        for path, ref_table, old_key in iter_references(wrapper):
            if ref_table != 'texts' or old_key not in candidate['texts']:
                continue
            if old_key not in text_map:
                key = _fresh_id(new_id[:65] + '.text', set(candidate['texts']))
                text_map[old_key] = key
                candidate['texts'][key] = candidate['texts'][old_key]
            _put(wrapper, [str(x) for x in path], text_map[old_key])
        candidate[table].append(clone)
        self._commit(candidate, 'Duplicate ' + record_id)
        return deepcopy(clone)

    def set_text(self, key: str, value: str, expected_revision=None) -> dict:
        """新增或修改作者文本，不改变任何实体的稳定ID。"""
        self._guard(expected_revision); _identifier(key)
        if not isinstance(value, str):
            raise DocumentError('INVALID_TEXT', 'Text must be a string')
        candidate = self.snapshot(); candidate['texts'][key] = value
        return self._commit(candidate, 'Edit text ' + key)

    def set_flag(self, key: str, value: bool, expected_revision=None) -> dict:
        """设置初始布尔标志，不修改正在运行的游戏事实。"""
        self._guard(expected_revision); _identifier(key)
        if type(value) is not bool:
            raise DocumentError('INVALID_FLAG', 'Flag must be a boolean')
        candidate = self.snapshot(); candidate['flags'][key] = value
        return self._commit(candidate, 'Edit flag ' + key)

    def undo(self, expected_revision=None) -> dict:
        """撤销一次编辑；即使内容恢复原样，文档版本仍单调增加。"""
        self._guard(expected_revision)
        if not self._undo:
            return {'changed': False, 'revision': self.revision}
        entry = self._undo.pop(); self._redo.append(entry)
        self._data = deepcopy(entry.before); self.revision += 1
        return {'changed': True, 'revision': self.revision, 'label': entry.label}

    def redo(self, expected_revision=None) -> dict:
        """重做一次已撤销编辑，并复用原有历史容量限制。"""
        self._guard(expected_revision)
        if not self._redo:
            return {'changed': False, 'revision': self.revision}
        entry = self._redo.pop(); self._undo.append(entry)
        self._data = deepcopy(entry.after); self.revision += 1
        return {'changed': True, 'revision': self.revision, 'label': entry.label}

    def validate(self) -> dict:
        """复用内容校验并定位局部引用失效；诊断不改文档、撤销历史或真实世界。"""
        report = validate_pack(self._data, str(self.path)).to_dict()
        report['issues'].extend(local_reference_issues(self._data, str(self.path)))
        try:
            from tools.presentation import validate_project_pack
        except ModuleNotFoundError:
            from presentation import validate_project_pack
        report['issues'].extend(validate_project_pack(self._data))
        report['issues'].sort(key=lambda issue: (issue['path'], issue['code'], issue['message']))
        if report['issues']: report['status'] = 'INVALID_CONTENT'
        return report

    def save(self, path=None, expected_revision=None) -> dict:
        """持久化当前作者草稿，检测外部修改后拒绝覆盖。"""
        self._guard(expected_revision)
        from .document_io import save_document
        return save_document(self, path)

    def prepare(self, output: Path, expected_revision=None) -> dict:
        """从已保存内容生成校验过的目录；生成成功不代表游戏运行验收。"""
        self._guard(expected_revision)
        from .document_io import prepare_document
        return prepare_document(self, output)

    def set_metadata(self, field: str, value, expected_revision=None) -> dict:
        """只允许编辑内容版本与能力声明，不改变文档身份或ABI。"""
        self._guard(expected_revision)
        if field not in ('version', 'required_capabilities'):
            raise DocumentError('IMMUTABLE_METADATA', 'Only version and required_capabilities are editable')
        candidate = self.snapshot()
        candidate[field] = deepcopy(value)
        return self._commit(candidate, 'Edit content metadata ' + field)

    def add_field(self, table, record_id, field, value, expected_revision=None, parent_pointer='') -> dict:
        """在指定对象新增Schema可选字段；默认仍为顶层，失败不修改内容或历史。"""
        self._guard(expected_revision)
        index = self._index(table, record_id)
        candidate = self.snapshot()
        node, rule = _object_at(candidate[table][index], SCHEMA['properties'][table]['items'], parent_pointer)
        if not isinstance(field, str) or field not in rule.get('properties', {}):
            raise DocumentError('UNKNOWN_FIELD', '字段未在作者Schema中声明')
        if field == 'id' or field in rule.get('required', []):
            raise DocumentError('NOT_OPTIONAL_FIELD', '此入口只新增可选字段，不能修改身份')
        if field in node:
            raise DocumentError('FIELD_EXISTS', '字段已经存在，请使用普通字段编辑')
        node[field] = deepcopy(value)
        return self._commit(candidate, '新增字段 ' + record_id + parent_pointer + '/' + field)
