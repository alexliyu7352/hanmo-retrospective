"""读取受信规则能力合同；只描述已接入边界与证据入口，不执行作者脚本或认证运行。"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONTRACT_PATH = ROOT / 'game/data/rule_capabilities.json'


def _unique_pairs(pairs):
    """拒绝重复JSON键，避免受信描述的版本或范围被隐式覆盖。"""
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate contract key: ' + key)
        result[key] = value
    return result


def load_contract(path: Path = CONTRACT_PATH) -> dict:
    """加载项目受信描述并核对模块、Schema能力及唯一测试注册表；坏描述直接阻断。"""
    try:
        contract = json.loads(Path(path).read_text(encoding='utf8'), object_pairs_hook=_unique_pairs)
        validate_contract(contract)
        return contract
    except (OSError, TypeError, KeyError, ValueError) as exc:
        raise ValueError('Invalid rule capability contract: ' + str(exc)) from exc


def _schema_at(schema: dict, path: str) -> dict:
    """沿本地Schema解析受限选择器，未知字段或非数组通配符必须失败。"""
    node = schema
    for part in path.split('.'):
        while '$ref' in node:
            node = schema['$defs'][node['$ref'].split('/')[-1]]
        node = node.get('items', {}) if part == '*' and node.get('type') == 'array' else node.get('properties', {}).get(part, {})
        if not node:
            raise ValueError('Unknown schema selector: ' + path)
    while '$ref' in node:
        node = schema['$defs'][node['$ref'].split('/')[-1]]
    return node


def validate_contract(contract: dict) -> None:
    """检查描述身份与关联完整性；作者内容不能注册模块或注入可执行能力。"""
    try:
        from tools.verify import TESTS, EXCLUDED_TESTS
    except ModuleNotFoundError:
        from verify import TESTS, EXCLUDED_TESTS
    schema = json.loads((ROOT / 'schemas/content.schema.json').read_text(encoding='utf8'))
    allowed = set(schema['properties']['required_capabilities']['items']['enum'])
    if not isinstance(contract, dict) or contract.get('format') != 'HM-RULE-CAPABILITIES/1' or type(contract.get('content_abi')) is not int or contract['content_abi'] != 1:
        raise ValueError('Unsupported contract format or ABI')
    modules = contract.get('modules')
    if not isinstance(modules, dict) or not modules:
        raise ValueError('Missing trusted modules')
    for module_id, path in modules.items():
        if not isinstance(module_id, str) or not isinstance(path, str) or not path.startswith('res://core/') or '..' in path or not (ROOT / 'game' / path.removeprefix('res://')).is_file():
            raise ValueError('Unknown trusted module: ' + str(module_id))
    entries = contract.get('capabilities')
    if not isinstance(entries, list) or not entries:
        raise ValueError('Missing capabilities')
    seen = set()
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError('Invalid capability entry')
        identifier = entry.get('capability_id')
        if not isinstance(identifier, str) or identifier not in allowed or identifier in seen:
            raise ValueError('Unknown or duplicate capability: ' + str(identifier))
        seen.add(identifier)
        if entry.get('module_id') not in modules:
            raise ValueError('Unknown module: ' + str(entry.get('module_id')))
        if not isinstance(entry.get('rule_version'), str) or not entry['rule_version']:
            raise ValueError('Missing rule version')
        if entry.get('support_status') not in ('implemented/unverified', 'verified'):
            raise ValueError('Invalid support status')
        tests = entry.get('test_ids')
        if not isinstance(tests, list) or not tests or any(not isinstance(test, str) or test not in set(TESTS) | set(EXCLUDED_TESTS) for test in tests) or len(tests) != len(set(tests)):
            raise ValueError('Missing or unknown runtime test association')
        selectors = entry.get('schema_selectors')
        if not isinstance(selectors, list) or not selectors:
            raise ValueError('Missing schema selectors')
        for selector in selectors:
            if not isinstance(selector, dict) or not isinstance(selector.get('path'), str) or not selector['path'] or set(selector) - {'path', 'equals', 'contains'} or ('equals' in selector and 'contains' in selector):
                raise ValueError('Invalid schema selector')
            node = _schema_at(schema, selector['path'])
            if 'equals' in selector and selector['equals'] not in node.get('enum', []):
                raise ValueError('Unknown selector enum')
            if 'contains' in selector and selector['contains'] not in _schema_at(schema, selector['path'] + '.*').get('enum', []):
                raise ValueError('Unknown selector membership')
        semantics = entry.get('supported_semantics')
        if not isinstance(semantics, list):
            raise ValueError('InvalidCapabilitySemantics: supported_semantics must be an explicit array')
        for boundary in semantics:
            if not isinstance(boundary, dict) or not all(isinstance(boundary.get(key), str) and boundary[key] for key in ('table', 'field', 'code')) or not isinstance(boundary.get('values'), list) or not boundary['values']:
                raise ValueError('Invalid supported semantics')
            if not isinstance(boundary.get('where', {}), dict):
                raise ValueError('Invalid semantic selector')
            node = _schema_at(schema, boundary['table'] + '.*.' + boundary['field'])
            if any(value not in node.get('enum', []) for value in boundary['values']):
                raise ValueError('Unknown semantic enum')
            for field, value in boundary.get('where', {}).items():
                if value not in _schema_at(schema, boundary['table'] + '.*.' + field).get('enum', []):
                    raise ValueError('Unknown semantic condition')
        evidence = entry.get('evidence', [])
        if not isinstance(evidence, list) or (entry['support_status'] == 'verified' and not evidence):
            raise ValueError('Verified status requires independent evidence')
        if entry['support_status'] == 'verified' and any(not isinstance(proof, dict) or proof.get('scope') != 'runtime_verified' or proof.get('status') != 'PASS' for proof in evidence):
            raise ValueError('Historical evidence cannot certify current runtime')
    if seen != allowed:
        raise ValueError('Schema capability set differs from trusted contract')


def _values(node, parts: list[str]) -> list:
    """沿声明路径读取数组或索引目录；通配符只遍历数据，不执行动态表达式。"""
    if not parts:
        return [node]
    if parts[0] == '*':
        children = node.values() if isinstance(node, dict) else node if isinstance(node, list) else []
        return [value for child in children for value in _values(child, parts[1:])]
    if isinstance(node, dict) and parts[0] in node:
        return _values(node[parts[0]], parts[1:])
    return []


def required_capabilities(pack: dict, contract: dict | None = None) -> set[str]:
    """按唯一描述中的选择器推导声明要求，缺少可选字段不自动写回作者包。"""
    contract = load_contract() if contract is None else contract
    required = set()
    for entry in contract['capabilities']:
        for selector in entry['schema_selectors']:
            values = _values(pack, selector['path'].split('.'))
            if any(value == selector['equals'] if 'equals' in selector else
                   isinstance(value, (list, dict)) and selector['contains'] in value if 'contains' in selector else
                   bool(value) for value in values):
                required.add(entry['capability_id'])
                break
    return required


def check_supported_semantics(pack: dict, contract: dict) -> list[tuple[str, tuple, str]]:
    """对通过Schema的作者包逐记录检查支持边界，返回原始表序号和字段。"""
    issues = []
    for entry in contract['capabilities']:
        for boundary in entry.get('supported_semantics', []):
            for number, record in enumerate(pack.get(boundary['table'], [])):
                if all(record.get(key) == value for key, value in boundary.get('where', {}).items()) and record.get(boundary['field']) not in boundary['values']:
                    issues.append((boundary['code'], (boundary['table'], number, boundary['field']),
                                   '当前规则合同不支持此语义：' + str(record.get(boundary['field']))))
    return issues


def contract_digest(path: Path = CONTRACT_PATH) -> str:
    """绑定项目受信描述原字节；编译与Godot启动须使用同一描述。"""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def runtime_declaration(contract: dict | None = None, *, path: Path = CONTRACT_PATH) -> dict:
    """生成兼容旧工具的静态能力清单，历史测试入口不能提升为本次运行认证。"""
    contract = load_contract() if contract is None else contract
    return {'format': 'HM-RUNTIME-CAPABILITIES', 'content_abi': contract['content_abi'],
            'runtime_verified': False,
            'capabilities': [entry['capability_id'] for entry in contract['capabilities']],
            'rule_capabilities_sha256': contract_digest(path),
            'verification_scope': 'Static declaration only; implemented/unverified is not platform or runtime certification.',
            'evidence': contract.get('evidence', [])}
