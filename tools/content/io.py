"""严格读取本地作者源并确定性准备目录；失败保留旧文件，不执行游戏行动。"""
import hashlib
import json
import math
import os
from pathlib import Path
import tempfile
from .validation import SCHEMA_BYTES, TABLES, ROOT, validate_pack
from .capability_contract import contract_digest

MAX_BYTES = 2 * 1024 * 1024


def _unique_pairs(pairs):
    """在Python覆盖同名键前拒绝重复JSON属性，避免隐藏作者输入。"""
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON key: ' + key)
        result[key] = value
    return result


def _reject_constant(value):
    """拒绝NaN、Infinity等非JSON常量，保持可跨端读取的内容格式。"""
    raise ValueError('Non-finite JSON constant: ' + value)


def _check_tree(value):
    """限制嵌套深度并拒绝非有限指数值与孤立代理字符。"""
    todo = [(value, 0)]
    count = 0
    while todo:
        node, depth = todo.pop(); count += 1
        if depth > 32 or count > 200000:
            raise ValueError('Content nesting or node limit exceeded')
        if isinstance(node, dict):
            for key, child in node.items():
                key.encode('utf8')
                todo.append((child, depth + 1))
        elif isinstance(node, list):
            todo.extend((v, depth + 1) for v in node)
        elif isinstance(node, str):
            node.encode('utf8')
        elif isinstance(node, float) and not math.isfinite(node):
            raise ValueError('Non-finite JSON number')


def _read_snapshot(path: Path) -> tuple[dict, bytes]:
    """读取并校验同一份不可变字节快照，同时返回原字节和解析内容。"""
    path = Path(path)
    try:
        with path.open('rb') as stream:
            raw = stream.read(MAX_BYTES + 1)
        if len(raw) > MAX_BYTES:
            raise ValueError('Content file exceeds 2 MiB')
        result = json.loads(raw.decode('utf8'), object_pairs_hook=_unique_pairs,
                            parse_constant=_reject_constant)
        _check_tree(result)
        if not isinstance(result, dict):
            raise ValueError('Content root must be an object')
        return result, raw
    except (OSError, UnicodeError, ValueError, RecursionError) as exc:
        raise ValueError('{}: {}'.format(path, exc)) from exc



def read_pack(path: Path) -> dict:
    """读取大小受限的UTF-8对象，不向调用者暴露内部字节快照。"""
    return _read_snapshot(path)[0]


def canonical(value) -> bytes:
    """生成确定性JSON字节，不掺入路径、时钟或随机数据。"""
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':'),
                       allow_nan=False) + '\n').encode('utf8')


def _protected_output(source: Path, output: Path) -> None:
    """拒绝把输出写到作者源或现有工程受保护位置，防止参数错误覆盖真值。"""
    resolved = output.resolve()
    if resolved == source.resolve():
        raise ValueError('Output must not overwrite the input source')
    if not output.name.endswith('.hmcatalog.json'):
        raise ValueError('Prepared output must end with .hmcatalog.json')
    for folder in [ROOT/'content', ROOT/'schemas', ROOT/'game', ROOT/'tools', ROOT/'tests']:
        if resolved == folder.resolve() or folder.resolve() in resolved.parents:
            raise ValueError('Output must be outside authored/source directories')


def prepare_pack(source: Path, output: Path, *, expected_source_sha256: str | None = None) -> dict:
    """原子准备稳定ID索引；可选缺失表不写入摘要，不将静态成功视为可玩。"""
    source, output = Path(source), Path(output)
    _protected_output(source, output)
    # 校验和摘要复用同一不可变输入缓冲，不把随后发生的编辑混入本次准备。
    pack, source_bytes = _read_snapshot(source)
    if expected_source_sha256 is not None and hashlib.sha256(source_bytes).hexdigest() != expected_source_sha256:
        raise ValueError('Source changed since the editor loaded/saved it')
    report = validate_pack(pack, str(source))
    if not report.ok:
        raise ValueError('Invalid content: ' + json.dumps(report.to_dict(), ensure_ascii=False))
    # 正式准备不接受缺件或未知表现引用，校验发生在任何输出创建之前。
    try:
        from tools.presentation import validate_project_pack
    except ModuleNotFoundError:
        from presentation import validate_project_pack
    presentation_issues = validate_project_pack(pack, ROOT)
    if presentation_issues:
        raise ValueError('Invalid presentation: ' + json.dumps(presentation_issues, ensure_ascii=False))
    catalog = {table:{record['id']:record for record in pack[table]} for table in TABLES if table in pack}
    catalog.update(texts=pack['texts'], flags=pack['flags'],
                   required_capabilities=pack['required_capabilities'])
    result = {'format':'HM-CONTENT-INDEX', 'format_version':1, 'status':'AUTHORING_ONLY',
              'runtime_verified':False, 'content_id':pack['id'], 'content_version':pack['version'],
              'content_abi':pack['schema_version'],
              'source_sha256':hashlib.sha256(source_bytes).hexdigest(),
              'schema_sha256':hashlib.sha256(SCHEMA_BYTES).hexdigest(),
              'rule_capabilities_sha256':contract_digest(),
              'catalog_sha256':hashlib.sha256(canonical(catalog)).hexdigest(), 'catalog':catalog}
    data = canonical(result)
    temp = None
    try:
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=output.parent, prefix='.content-', suffix='.tmp', delete=False) as stream:
            temp = Path(stream.name)
            stream.write(data); stream.flush(); os.fsync(stream.fileno())
        os.replace(temp, output)
    except OSError as exc:
        raise ValueError('Cannot prepare content output: ' + str(exc)) from exc
    finally:
        if temp is not None and temp.exists():
            temp.unlink()
    return {k:v for k,v in result.items() if k != 'catalog'}


def check_target(pack: dict, runtime: dict) -> dict:
    """只比较目标能力声明；匹配结果不代表平台实际运行通过。"""
    required = set(pack['required_capabilities'])
    listed = runtime.get('capabilities', [])
    valid_shape = (runtime.get('format') == 'HM-RUNTIME-CAPABILITIES'
                   and isinstance(listed, list) and all(isinstance(c, str) for c in listed)
                   and type(runtime.get('content_abi')) is int
                   and isinstance(runtime.get('runtime_verified'), bool))
    available = set(listed) if valid_shape else set()
    missing = sorted(required - available)
    ready = valid_shape and runtime['content_abi'] == pack['schema_version'] and not missing
    return {'status':'DECLARED_CAPABILITIES_MATCH' if ready else 'TARGET_NOT_READY',
            'runtime_verified':False, 'declaration_match':ready, 'missing_capabilities':missing,
            'content_abi_matches':runtime.get('content_abi') == pack['schema_version'],
            'note':'Static declaration comparison only. Godot execution, editor authoring and platform tests are separate.'}
