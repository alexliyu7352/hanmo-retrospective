"""作者工具的本地进程协议；复用同一作者文档，不提供网络服务或第二套游戏。"""
from __future__ import annotations

from copy import deepcopy
import inspect
import json
from pathlib import Path
import uuid

from .document import AuthorDocument, DocumentError
from .pack_document import PackDocument, EDIT_OPERATIONS
from .pack_errors import PackError
from .packs import PACK_SCHEMA
from .io import _unique_pairs, _reject_constant, _check_tree
from .validation import SCHEMA, TABLES

PROTOCOL = 'HM-AUTHOR/1'
MAX_REQUEST_BYTES = 2 * 1024 * 1024
READS = {'snapshot', 'record', 'references_to', 'validate', 'status'}
WRITES = {'set_value', 'add_field', 'add_record', 'clone_record', 'delete_record', 'set_text',
          'set_flag', 'set_metadata', 'undo', 'redo', 'save', 'prepare'}
PACK_WRITES = {'set_manifest', 'edit', 'save', 'compose', 'import_project_resources', 'preview_prepare', 'apply_scenario_snapshot', 'select_dependencies'}
PACK_OPERATIONS = {
    'open_pack': {'required': ['path'], 'optional': ['discard']},
    'create_pack': {'required': ['directory', 'manifest'], 'optional': ['discard']},
    'set_manifest': {'required': ['pointer', 'value'], 'optional': []},
    'edit': {'required': ['entry', 'operation', 'arguments'], 'optional': []},
    'compose': {'required': ['output'], 'optional': []},
    'project_resources': {'required': ['selection'], 'optional': []},
    'import_project_resources': {'required': ['selection', 'expected_project_sha256'], 'optional': []},
    'preview_prepare': {'required': ['destination', 'expected_pack_set_digest'], 'optional': ['native_stage']},
    'scenario_snapshot_preview': {'required': ['entry', 'scenario_id', 'selection'], 'optional': []},
    'apply_scenario_snapshot': {'required': ['token'], 'optional': []},
    'select_dependencies': {'required': ['roots', 'selection'], 'optional': []},
    'reference_context': {'required': ['table'], 'optional': []},
    'save': {'required': [], 'optional': []},
    'snapshot': {'required': [], 'optional': []},
    'status': {'required': [], 'optional': []},
}


class EditorSession:
    """维护一个作者文档或包会话，所有编辑面板使用相同身份与版本。"""
    def __init__(self):
        """初始化空会话，不自动丢弃其他文档未保存修改。"""
        self.document = None
        self.document_id = None

    def _status(self):
        """提供当前文档身份和单调版本号，不暴露可写内部状态。"""
        if self.document is None:
            return None
        status = self.document.status()
        if isinstance(self.document, PackDocument):
            status = {**status, 'kind': 'pack', 'source_sha256': status['manifest_sha256']}
        return {**status, 'document_id': self.document_id}

    def response(self, request_id, result=None, error=None) -> dict:
        """生成结构化进程回复，区分编辑完成与游戏运行验证。"""
        reply = {'protocol': PROTOCOL, 'id': request_id, 'ok': error is None,
                 'document': self._status(), 'runtime_verified': False}
        reply['result' if error is None else 'error'] = result if error is None else error.to_dict()
        return reply

    def _guard_write(self, request) -> None:
        """拒绝指向错误文档或过期版本的写请求。"""
        if request.get('document_id') != self.document_id:
            raise DocumentError('STALE_DOCUMENT', 'Refresh the panel for the currently loaded author document')
        if type(request.get('revision')) is not int:
            raise DocumentError('STALE_REVISION', 'An explicit current document revision is required')
        self.document._guard(request['revision'])
        if isinstance(self.document, PackDocument):
            expected = self.document.status()['manifest_sha256']
            if request.get('source_sha256') != expected:
                raise DocumentError('SOURCE_CHANGED', 'Refresh the exact manifest source token',
                                    expected_source_sha256=expected)

    def _guard_switch(self, request, args):
        """切换任何会话前保护未保存内容，显式放弃也必须持有当前版本。"""
        if self.document is not None and self.document.status()['dirty']:
            if not args.get('discard', False):
                raise DocumentError('UNSAVED_CHANGES', 'Save, Save As or explicitly discard before opening another source')
            self._guard_write(request)

    def _open(self, request, args):
        """打开新内容前检查未保存修改，读取失败保持旧文档。"""
        if set(args) - {'path', 'discard'} or not isinstance(args.get('path'), str):
            raise DocumentError('BAD_ARGUMENTS', 'open requires a source path and optional boolean discard')
        if 'discard' in args and type(args['discard']) is not bool:
            raise DocumentError('BAD_ARGUMENTS', 'discard must be a boolean')
        if 'selection' in args and not isinstance(args['selection'], list):
            raise DocumentError('BAD_ARGUMENTS', 'selection must be an array')
        self._guard_switch(request, args)
        candidate = AuthorDocument.open(Path(args['path']))
        self.document = candidate
        self.document_id = uuid.uuid4().hex
        return self._status()

    def _pack_arguments(self, operation, args):
        """按公开固定字段表检查包请求，不接受任意方法名或子文档版本参数。"""
        contract = PACK_OPERATIONS[operation]
        if (set(contract['required']) - set(args) or
                set(args) - set(contract['required']) - set(contract['optional'])):
            raise DocumentError('BAD_ARGUMENTS', 'Pack operation arguments do not match its declared fields',
                                operation=operation, contract=deepcopy(contract))
        for field in ('path', 'directory', 'pointer', 'entry', 'operation', 'output', 'expected_project_sha256',
                      'destination', 'expected_pack_set_digest', 'native_stage'):
            if field in args and not isinstance(args[field], str):
                raise DocumentError('BAD_ARGUMENTS', field + ' must be a string')
        for field in ('manifest', 'arguments'):
            if field in args and not isinstance(args[field], dict):
                raise DocumentError('BAD_ARGUMENTS', field + ' must be an object')
        if 'discard' in args and type(args['discard']) is not bool:
            raise DocumentError('BAD_ARGUMENTS', 'discard must be a boolean')

    def _open_pack(self, request, args):
        """打开或创建候选包成功后才替换会话；失败保留原文档和未保存状态。"""
        operation = request['op']
        self._pack_arguments(operation, args)
        self._guard_switch(request, args)
        if operation == 'open_pack':
            candidate = PackDocument.open_pack(Path(args['path']))
        else:
            candidate = PackDocument.create_pack(Path(args['directory']), args['manifest'])
        self.document = candidate
        self.document_id = uuid.uuid4().hex
        return self._status()

    def _pack_operation(self, request, args):
        """包会话只分发具名门面，编辑子文档只能经过edit的白名单。"""
        operation = request['op']
        if operation not in PACK_OPERATIONS or operation in ('open_pack', 'create_pack'):
            raise DocumentError('UNKNOWN_OPERATION', 'Operation is not available for pack documents: ' + operation)
        if operation in PACK_WRITES:
            self._guard_write(request)
        self._pack_arguments(operation, args)
        if operation == 'snapshot':
            return self.document.snapshot()
        if operation == 'status':
            return self._status()
        if operation == 'project_resources':
            return self.document.project_resources(args['selection'])
        if operation == 'scenario_snapshot_preview':
            return self.document.scenario_snapshot_preview(args['entry'], args['scenario_id'], args['selection'])
        if operation == 'reference_context':
            return self.document.reference_context(args['table'])
        revision = request['revision']
        if operation == 'save':
            return self.document.save(revision)
        if operation == 'set_manifest':
            return self.document.set_manifest(args['pointer'], args['value'], revision)
        if operation == 'edit':
            return self.document.edit(args['entry'], args['operation'], args['arguments'], revision)
        if operation == 'import_project_resources':
            return self.document.import_project_resources(args['selection'], args['expected_project_sha256'], revision)
        if operation == 'preview_prepare':
            native_stage = Path(args['native_stage']) if args.get('native_stage') else None
            return self.document.preview_prepare(Path(args['destination']), args['expected_pack_set_digest'], revision,
                                                  native_stage=native_stage)
        if operation == 'apply_scenario_snapshot':
            return self.document.apply_scenario_snapshot(args['token'], revision)
        if operation == 'select_dependencies':
            return self.document.select_dependencies(args['roots'], args['selection'], revision)
        return self.document.compose(Path(args['output']), revision)

    def _list(self, args):
        """提供引用选择器标签，保存仍使用稳定ID。"""
        if set(args) != {'table'} or args['table'] not in TABLES + ('texts', 'flags'):
            raise DocumentError('BAD_ARGUMENTS', 'list requires an existing author table')
        table = args['table']; pack = self.document.snapshot()
        if table in ('texts', 'flags'):
            return [{'id': key, 'label': str(value)} for key, value in sorted(pack[table].items())]
        return [{'id': r['id'], 'label': pack['texts'].get(r.get('name_key'), r['id'])}
                for r in pack.get(table, [])]

    def handle(self, request: dict) -> dict:
        """分发允许的单次作者操作，异常返回明确失败结果。"""
        request_id = request.get('id') if isinstance(request, dict) else None
        if type(request_id) not in (str, int, type(None)):
            request_id = None
        try:
            fields = {'id', 'op', 'args', 'document_id', 'revision'}
            if isinstance(self.document, PackDocument):
                fields.add('source_sha256')
            if not isinstance(request, dict) or set(request) - fields:
                raise DocumentError('BAD_REQUEST', 'Expected id, op, args and optional document identity/revision')
            if type(request.get('id')) not in (int, str) or not isinstance(request.get('op'), str):
                raise DocumentError('BAD_REQUEST', 'Request needs a string/integer id and operation name')
            args = request.get('args', {})
            if not isinstance(args, dict):
                raise DocumentError('BAD_ARGUMENTS', 'args must be an object')
            op = request['op']
            if op == 'describe':
                if args:
                    raise DocumentError('BAD_ARGUMENTS', 'describe takes no arguments')
                result = {'protocol': PROTOCOL, 'schema': deepcopy(SCHEMA), 'tables': list(TABLES),
                          'operations': sorted(READS | WRITES | set(PACK_OPERATIONS) | {'describe','open','list'}),
                          'pack_operations': deepcopy(PACK_OPERATIONS),
                          'pack_edit_operations': sorted(EDIT_OPERATIONS), 'pack_schema': deepcopy(PACK_SCHEMA),
                          'pack_write_guards': ['document_id', 'revision', 'source_sha256'],
                          'pack_source_token': 'source_sha256 is the exact saved manifest_sha256; all inputs are also checked on disk',
                          'scope': 'Author-document backend only; no visual editor or Godot execution'}
            elif op == 'open':
                result = self._open(request, args)
            elif op in ('open_pack', 'create_pack'):
                result = self._open_pack(request, args)
            elif op not in READS | WRITES | set(PACK_OPERATIONS) | {'list'}:
                raise DocumentError('UNKNOWN_OPERATION', 'Unsupported author operation: ' + op)
            elif self.document is None:
                raise DocumentError('NO_DOCUMENT', 'Open an author source first')
            elif isinstance(self.document, PackDocument):
                result = self._pack_operation(request, args)
            elif op in set(PACK_OPERATIONS) - READS - WRITES:
                raise DocumentError('UNKNOWN_OPERATION', 'Open a pack before using pack operations')
            elif op == 'list':
                result = self._list(args)
            else:
                if op in WRITES:
                    self._guard_write(request)
                function = getattr(self.document, op)
                if 'expected_revision' in args:
                    raise DocumentError('BAD_ARGUMENTS', 'Use the top-level revision field')
                params = dict(args)
                if op in WRITES:
                    params['expected_revision'] = request['revision']
                try:
                    inspect.signature(function).bind(**params)
                except TypeError as exc:
                    raise DocumentError('BAD_ARGUMENTS', str(exc)) from exc
                result = function(**params)
            return self.response(request_id, result=result)
        except (DocumentError, PackError) as exc:
            return self.response(request_id, error=exc)
        except Exception as exc:
            # 工具异常必须返回失败，不能伪装成保存完成或游戏验证通过。
            return self.response(request_id, error=DocumentError('INTERNAL_ERROR', type(exc).__name__ + ': ' + str(exc)))

    def line(self, text: str) -> dict:
        """解析严格的单行JSON请求，非法帧不能执行其他操作。"""
        try:
            request = json.loads(text, object_pairs_hook=_unique_pairs, parse_constant=_reject_constant)
            _check_tree(request)
        except (ValueError, UnicodeError, RecursionError, TypeError, AttributeError) as exc:
            return self.response(None, error=DocumentError('BAD_JSON', str(exc)))
        return self.handle(request)


def run_stream(instream, outstream) -> int:
    """按行处理本地标准输入，输入结束即退出，不建立网络监听。"""
    session = EditorSession()
    while True:
        raw = instream.readline(MAX_REQUEST_BYTES + 1)
        if not raw:
            return 0
        if len(raw) > MAX_REQUEST_BYTES:
            while raw and not raw.endswith(b'\n'):
                raw = instream.readline(MAX_REQUEST_BYTES + 1)
            response = session.response(None, error=DocumentError('REQUEST_LIMIT', 'Request exceeds 2 MiB'))
        else:
            try:
                response = session.line(raw.decode('utf8'))
            except UnicodeError as exc:
                response = session.response(None, error=DocumentError('BAD_JSON', str(exc)))
        outstream.write(json.dumps(response, ensure_ascii=False, allow_nan=False) + '\n')
        outstream.flush()
