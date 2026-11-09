"""按 Schema 枚举作者引用及局部节点；半成品草稿也保留稳定字段定位。"""
import re
from .validation import SCHEMA, TABLES, pointer


def _possible(value, schema, discriminators=True):
    """按类型和判别字段选择分支，不要求当前编辑草稿已经语义合法。"""
    if '$ref' in schema:
        return _possible(value, SCHEMA['$defs'][schema['$ref'].split('/')[-1]], discriminators)
    if 'const' in schema:
        return type(value) is type(schema['const']) and (not discriminators or value == schema['const'])
    kind = schema.get('type')
    checks = {'object': isinstance(value, dict), 'array': isinstance(value, list),
              'string': isinstance(value, str), 'null': value is None,
              'integer': type(value) is int, 'boolean': type(value) is bool}
    if kind in checks and not checks[kind]:
        return False
    if discriminators and isinstance(value, dict):
        for key, sub in schema.get('properties', {}).items():
            if key in value and 'const' in sub and value[key] != sub['const']:
                return False
    return True


def _iter_annotated(value, schema=None, path=(), annotation="x-hanmo-ref"):
    """联合分支和同级公共字段均枚举；相同路径引用只输出一次，避免重复限定命名空间。"""
    schema = SCHEMA if schema is None else schema
    if not isinstance(schema, dict):
        return
    if '$ref' in schema:
        yield from _iter_annotated(value, SCHEMA['$defs'][schema['$ref'].split('/')[-1]], path, annotation)
        return
    found = []
    for union in ('oneOf', 'anyOf'):
        if union in schema:
            options = [s for s in schema[union] if _possible(value, s)]
            if not options:
                options = [s for s in schema[union] if _possible(value, s, discriminators=False)]
            literals = [s for s in options if 'const' in s and _possible(value, s)]
            for option in literals or options:
                found.extend(_iter_annotated(value, option, path, annotation))
    if annotation in schema and isinstance(value, str):
        found.append((path, schema[annotation], value))
    if isinstance(value, dict):
        for key, child in value.items():
            sub = schema.get('properties', {}).get(key, schema.get('additionalProperties', {}))
            if isinstance(sub, dict):
                found.extend(_iter_annotated(child, sub, path + (key,), annotation))
    elif isinstance(value, list):
        for n, child in enumerate(value):
            found.extend(_iter_annotated(child, schema.get('items', {}), path + (n,), annotation))
    yield from dict.fromkeys(found)


def iter_references(value, schema=None, path=()):
    """公开跨表引用接口保持原路径和返回形状，不能把相同字符串误当作引用。"""
    yield from _iter_annotated(value, schema, path)


def iter_key_references(value, schema=None, path=()):
    """身份键同时沿联合分支与公共字段遍历，相同路径去重，普通对象键不隐式限定。"""
    schema = SCHEMA if schema is None else schema
    if not isinstance(schema, dict):
        return
    if '$ref' in schema:
        yield from iter_key_references(value, SCHEMA['$defs'][schema['$ref'].split('/')[-1]], path)
        return
    found = []
    for union in ('oneOf', 'anyOf'):
        if union in schema:
            for option in schema[union]:
                if _possible(value, option):
                    found.extend(iter_key_references(value, option, path))
    if isinstance(value, dict):
        target = schema.get('x-hanmo-key-ref')
        for key, child in value.items():
            if target:
                found.append((path, target, key))
            sub = schema.get('properties', {}).get(key, schema.get('additionalProperties', {}))
            found.extend(iter_key_references(child, sub, path + (key,)))
    elif isinstance(value, list):
        for number, child in enumerate(value):
            found.extend(iter_key_references(child, schema.get('items', {}), path + (number,)))
    yield from dict.fromkeys(found)


def local_targets(pack, record, scope):
    """解析当前记录集合或跨表集合，不合并重复 ID，保留歧义给诊断与选择器。"""
    if not isinstance(scope, str): return None
    if re.fullmatch(r'[a-z][a-z0-9_]*', scope):
        parents, field = [record], scope
    else:
        match = re.fullmatch(r'([a-z][a-z0-9_]*)\[\*\]\.([a-z][a-z0-9_]*)', scope)
        if not match: return None
        parents, field = pack.get(match[1], []), match[2]
    return [child for parent in parents if isinstance(parent, dict)
            for child in (parent.get(field, []) if isinstance(parent.get(field, []), list) else [])
            if isinstance(child, dict) and isinstance(child.get('id'), str)]


def local_reference_issues(pack, source):
    """局部引用失效或重名时生成原错误列表需要的字段指针，允许明确声明的空引用分支。"""
    issues = []
    for path, scope, target in _iter_annotated(pack, annotation='x-hanmo-local-ref'):
        if len(path) < 2 or path[0] not in TABLES or not isinstance(path[1], int): continue
        record = pack[path[0]][path[1]]
        choices = local_targets(pack, record, scope)
        count = sum(row['id'] == target for row in choices) if choices is not None else 0
        if choices is None:
            code, message = 'LOCAL_REFERENCE_SCOPE', '局部引用集合声明无法解析：' + str(scope)
        elif count == 1:
            continue
        elif count == 0:
            code, message = 'LOCAL_REFERENCE_MISSING', '局部引用不存在：' + target + '（' + scope + '）'
        else:
            code, message = 'LOCAL_REFERENCE_AMBIGUOUS', '局部引用 ID 重复，无法确定目标：' + target
        issues.append({'code': code, 'path': source + '#' + pointer(path),
                       'message': message, 'record_id': record.get('id', '')})
    return issues


def find_references(pack, table, target):
    """返回跨记录删除影响所需的稳定身份和字段指针，不改写作者引用。"""
    result = {}
    for path, ref_table, ref_id in iter_references(pack):
        if ref_table != table or ref_id != target:
            continue
        source_table, source_id = '', ''
        if len(path) >= 2 and path[0] in TABLES and isinstance(path[1], int):
            source_table = path[0]
            source_id = pack[source_table][path[1]].get('id', '')
        location = pointer(path)
        result[location] = {'path': location, 'source_table': source_table,
                            'source_id': source_id, 'target_table': table, 'target_id': target}
    for path, ref_table, ref_id in iter_key_references(pack):
        if ref_table != table or ref_id != target:
            continue
        key_path = path + (ref_id,)
        source_table = key_path[0] if key_path and key_path[0] in TABLES else ''
        source_id = pack[source_table][key_path[1]].get('id', '') if source_table and len(key_path) > 1 and isinstance(key_path[1], int) else ''
        location = pointer(key_path)
        result[location] = {'path': location, 'source_table': source_table, 'source_id': source_id,
                            'target_table': table, 'target_id': target, 'key_reference': True}
    return [result[key] for key in sorted(result)]
