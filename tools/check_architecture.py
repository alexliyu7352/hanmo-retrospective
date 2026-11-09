#!/usr/bin/env python3
"""校验核心字面依赖、唯一源码归属与受信模块元数据；声明不等于写入屏障。"""
from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import re

TOKEN = re.compile(r'''\#[^\n]*|"""[\s\S]*?"""|\'\'\'[\s\S]*?\'\'\'|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_]\w*|[^\s]''')
IDENTIFIER = re.compile(r'[A-Za-z_][A-Za-z_0-9]*\Z')


def _object(pairs):
    """JSON重复键会隐藏归属或边，必须在读取时拒绝。"""
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate JSON key: ' + key)
        result[key] = value
    return result


def _read(path: Path) -> dict:
    """缺文件和坏JSON均为失败，不能以空字典冒充有效合同。"""
    value = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=_object)
    if not isinstance(value, dict):
        raise ValueError('contract must be an object')
    return value


def _path(value) -> bool:
    """仅接受明确项目相对路径，不提供通配或路径穿越豁免。"""
    return (isinstance(value, str) and bool(value) and not PurePosixPath(value).is_absolute()
            and all(part not in ('', '.', '..') for part in value.split('/'))
            and not any(c in value for c in '*?[]\\\x00'))


def _name(value) -> bool:
    """版本和标识只要求非空字符串，不臆造SemVer兼容规则。"""
    return isinstance(value, str) and bool(value)


def _cycle(graph: dict[str, set[str]]) -> bool:
    """检查每一条真实边，legacy仅放宽分层方向而不能取消文件环检测。"""
    remaining = {node: set(deps) for node, deps in graph.items()}
    while remaining:
        ready = {node for node, deps in remaining.items() if not deps & remaining.keys()}
        if not ready:
            return True
        for node in ready:
            del remaining[node]
    return False


def _tokens(text: str) -> list[str]:
    """保留完整字符串token并去掉注释，使显示文字不能成为类声明或依赖。"""
    return [match.group() for match in TOKEN.finditer(text) if not match.group().startswith('#')]


def _project_classes(root: Path) -> dict[str, str]:
    """从项目所有源码层收集全局class_name；同名歧义失败，不依赖Godot缓存或执行脚本。"""
    symbols = {}
    for path in sorted((root / 'game').rglob('*.gd')):
        if any(part.startswith('.') for part in path.relative_to(root / 'game').parts):
            continue
        tokens = _tokens(path.read_text(encoding='utf-8'))
        declarations = [tokens[index + 1] if index + 1 < len(tokens) else ''
                        for index, token in enumerate(tokens) if token == 'class_name']
        if len(declarations) > 1:
            raise ValueError('ambiguous project class declaration: ' + path.relative_to(root).as_posix())
        for name in declarations:
            if not IDENTIFIER.fullmatch(name) or name in symbols:
                raise ValueError('ambiguous project class declaration: ' + name)
            symbols[name] = path.relative_to(root).as_posix()
    return symbols


def _list_bindings(tokens: list[str], opening: int) -> set[str]:
    """只提取参数/枚举列表的声明名字；嵌套类型和默认值里的类引用仍交依赖检查。"""
    result = set(); depth = 0; expect_name = True
    for token in tokens[opening:]:
        if token in ('(', '[', '{'):
            depth += 1
        elif token in (')', ']', '}'):
            depth -= 1
            if depth == 0:
                break
        elif depth == 1:
            if token == ',':
                expect_name = True
            elif expect_name:
                if IDENTIFIER.fullmatch(token):
                    result.add(token)
                expect_name = False
    return result


def _bindings(tokens: list[str]) -> set[str]:
    """识别同名局部/成员声明；不猜测作用域遮蔽，遇已知全局类同名绑定明确失败。"""
    result = set()
    for index, token in enumerate(tokens):
        if index and tokens[index - 1] == '.':
            continue
        following = tokens[index + 1:index + 2]
        if token in ('var', 'const', 'class', 'func', 'signal', 'enum', 'for'):
            if following and IDENTIFIER.fullmatch(following[0]):
                result.add(following[0])
        if token == 'func':
            opening = index + 1 if following == ['('] else index + 2
            if tokens[opening:opening + 1] == ['(']:
                result.update(_list_bindings(tokens, opening))
        elif token == 'signal':
            if tokens[index + 2:index + 3] == ['(']:
                result.update(_list_bindings(tokens, index + 2))
        elif token == 'enum':
            opening = index + 1 if following == ['{'] else index + 2
            if tokens[opening:opening + 1] == ['{']:
                result.update(_list_bindings(tokens, opening))
    return result


def _dictionary_literal_keys(tokens: list[str]) -> set[int]:
    """只识别真实字典项起始处的identifier=value字面键；值、表达式键和枚举不排除。"""
    result = set(); stack = []
    closing = {')': '(', ']': '[', '}': '{'}
    for index, token in enumerate(tokens):
        if token in ('(', '[', '{'):
            if stack and stack[-1]['dictionary']:
                stack[-1]['key'] = False
            enum = (index > 0 and tokens[index - 1] == 'enum'
                    or index > 1 and tokens[index - 2] == 'enum')
            stack.append({'opening': token, 'dictionary': token == '{' and not enum, 'key': True})
        elif token in closing:
            if stack and stack[-1]['opening'] == closing[token]:
                stack.pop()
        elif stack and stack[-1]['dictionary']:
            if token == ',':
                stack[-1]['key'] = True
            elif stack[-1]['key']:
                if (IDENTIFIER.fullmatch(token) and tokens[index + 1:index + 2] == ['=']
                        and tokens[index + 2:index + 3] != ['=']):
                    result.add(index)
                stack[-1]['key'] = False
    return result


def _class_imports(text: str, source: str, symbols: dict[str, str]) -> tuple[list[str], list[str]]:
    """解析已知项目类的直接静态/类型/继承引用；自己和点后的成员名不构成全局依赖。"""
    tokens = _tokens(text)
    literal_keys = _dictionary_literal_keys(tokens)
    shadowed = _bindings(tokens) & symbols.keys()
    errors = ['ambiguous project class shadow: ' + source + ': ' + name for name in sorted(shadowed)]
    paths = set()
    for index, token in enumerate(tokens):
        if token not in symbols or token in shadowed or symbols[token] == source or index in literal_keys:
            continue
        if index and tokens[index - 1] in ('.', 'class_name'):
            continue
        paths.add('res://' + symbols[token].removeprefix('game/'))
    return sorted(paths), errors


def _imports(text: str) -> tuple[list[str], bool]:
    """先分离注释/字面字符串，再识别加载调用；非字面调用无法证明依赖，直接拒绝。"""
    tokens = _tokens(text)
    paths = []; dynamic = False
    for index, token in enumerate(tokens):
        if token == 'extends' and index + 1 < len(tokens) and tokens[index + 1].startswith(('\"', "'")):
            paths.append(tokens[index + 1][1:-1])
            continue
        if token not in ('load', 'preload') or tokens[index + 1:index + 2] != ['(']:
            continue
        args = tokens[index + 2:index + 4]
        if len(args) != 2 or args[1] != ')' or len(args[0]) < 2 or args[0][0] not in '\"\'':
            dynamic = True
            continue
        raw = args[0]
        if raw.startswith(('"""', "'''")) or '\\' in raw:
            dynamic = True
            continue
        paths.append(raw[1:-1])
    return paths, dynamic


def _reference(root: Path, files: dict, reference: dict) -> None:
    """Schema/validator引用必须对应受控源码中的真实静态方法，不执行该函数。"""
    if (not isinstance(reference, dict) or reference.get('path') not in files
            or not isinstance(reference.get('method'), str) or not IDENTIFIER.fullmatch(reference['method'])):
        raise ValueError('invalid validator reference: ' + str(reference))
    path = root / reference['path']
    if not path.is_file() or not re.search(r'^static\s+func\s+' + re.escape(reference['method']) + r'\s*\(', path.read_text(), re.M):
        raise ValueError('unresolved validator reference: ' + str(reference))


def check_architecture(root: Path, contracts: Path) -> dict:
    """输出可执行门禁结果；缺声明、陈旧例外、坏版本、真实环均拒绝合入。"""
    root = root.resolve(); errors = []; actual = set(); graph = {}; modules = {}; legacy = set()
    try:
        contract = _read(contracts)
        if contract.get('format') != 'HM-MODULE-CONTRACTS/1' or contract.get('version_rule', 'exact') != 'exact':
            raise ValueError('invalid module contract format/version rule')
        for key, shape in [('modules', list), ('files', dict), ('source_edges', list), ('legacy_edges', list), ('capability_modules', dict)]:
            if not isinstance(contract.get(key), shape):
                raise ValueError('invalid contract field: ' + key)
        for module in contract['modules']:
            if (not isinstance(module, dict) or not _name(module.get('id')) or not _name(module.get('version'))
                    or module['id'] in modules):
                raise ValueError('invalid/duplicate module')
            for key, shape in [('requires', dict), ('optional_requires', dict), ('capabilities', list), ('owns', list), ('validators', list)]:
                if not isinstance(module.get(key), shape):
                    raise ValueError('invalid module field: ' + key)
            if (any(not _name(x) for x in module['capabilities'])
                    or len(module['capabilities']) != len(set(module['capabilities']))):
                raise ValueError('invalid module capabilities')
            modules[module['id']] = module
        if not modules:
            raise ValueError('empty module inventory')
        module_graph = {name: set() for name in modules}
        for name, module in modules.items():
            if module['requires'].keys() & module['optional_requires'].keys():
                raise ValueError('duplicate dependency declaration: ' + name)
            for field in ('requires', 'optional_requires'):
                for dep, version in module[field].items():
                    if not _name(dep) or not _name(version):
                        raise ValueError('invalid dependency')
                    if dep not in modules:
                        if field == 'requires':
                            raise ValueError('unknown required module: ' + dep)
                    elif modules[dep]['version'] != version:
                        raise ValueError('incompatible dependency version: ' + dep)
                    else:
                        module_graph[name].add(dep)
        if _cycle(module_graph):
            errors.append('module dependency cycle')
        files = contract['files']
        for name, owner in files.items():
            if not _path(name) or owner not in modules or not (root / name).is_file():
                raise ValueError('missing/invalid controlled file: ' + name)
        core = {p.relative_to(root).as_posix() for p in (root / 'game/core').rglob('*.gd')}
        if not core:
            raise ValueError('core source tree is missing')
        for name in sorted(core - files.keys()):
            errors.append('unowned core file: ' + name)
        graph = {name: set() for name in core}
        symbols = _project_classes(root)
        for name in sorted(core):
            text = (root / name).read_text(encoding='utf-8')
            imports, dynamic = _imports(text)
            class_imports, class_errors = _class_imports(text, name, symbols)
            imports.extend(class_imports)
            errors.extend(class_errors)
            if dynamic:
                errors.append('dynamic core load is not allowed: ' + name)
            for target in imports:
                if not target.startswith('res://') or not _path(target[6:]):
                    errors.append('invalid dependency path: ' + name + ' -> ' + target)
                    continue
                target = 'game/' + target[6:]
                actual.add((name, target))
                if not target.startswith('game/core/'):
                    errors.append('core cannot depend on UI/storage/platform/resources: ' + name + ' -> ' + target)
                if target not in files or not (root / target).is_file():
                    errors.append('missing/unowned dependency: ' + target)
                graph[name].add(target)
        declared = set()
        for edge in contract['source_edges']:
            if not isinstance(edge, dict) or set(edge) != {'source', 'target'} or not all(_path(edge[k]) for k in edge):
                raise ValueError('invalid source edge')
            pair = (edge['source'], edge['target'])
            if pair in declared:
                raise ValueError('duplicate source edge: ' + str(pair))
            declared.add(pair)
        for edge in contract['legacy_edges']:
            if (not isinstance(edge, dict) or set(edge) != {'source', 'target', 'reason', 'task'}
                    or not _name(edge['reason']) or not isinstance(edge['task'], str)
                    or not re.fullmatch(r'A[1-4]\.\d+', edge['task'])):
                raise ValueError('legacy edge needs concrete reason and responsible task')
            pair = (edge['source'], edge['target'])
            if pair in legacy or pair not in actual or pair not in declared:
                raise ValueError('duplicate/stale/undeclared legacy edge: ' + str(pair))
            legacy.add(pair)
        errors += ['undeclared source edge: ' + str(e) for e in sorted(actual - declared)]
        errors += ['stale source edge: ' + str(e) for e in sorted(declared - actual)]
        for source, target in sorted(actual):
            if source not in files or target not in files or (source, target) in legacy:
                continue
            owner, dependency = files[source], files[target]
            if owner != dependency and dependency not in module_graph[owner]:
                errors.append('undeclared module dependency: ' + source + ' -> ' + target)
        if _cycle(graph):
            errors.append('core source dependency cycle')
        seen = set(); scopes = {}; validator_ids = set(); validation_orders = set(); legacy_kinds = set(); action_owners = {}
        for name, module in modules.items():
            for claim in module['owns']:
                if (not isinstance(claim, dict) or not _name(claim.get('container'))
                        or claim.get('scope') not in ('root', 'envelope', 'kind', 'actor_field')
                        or not isinstance(claim.get('key'), str) or claim.get('owner') not in modules
                        or claim['owner'] != name or not isinstance(claim.get('schema_refs'), list) or not claim['schema_refs']):
                    raise ValueError('invalid/unknown state owner: ' + name)
                if ((claim['scope'] in ('root', 'envelope')) != (claim['key'] == '')
                        or claim['scope'] == 'actor_field' and claim['container'] != 'actors'):
                    raise ValueError('invalid fixed state region')
                region = (claim['container'], claim['scope'], claim['key'])
                existing = scopes.setdefault(claim['container'], set())
                if region in seen or 'root' in existing or claim['scope'] == 'root' and existing:
                    raise ValueError('duplicate/overlapping state owner: ' + str(region))
                seen.add(region); existing.add(claim['scope'])
                if claim['container'] == 'actions' and claim['scope'] == 'kind':
                    action_owners[claim['key']] = name
                for reference in claim['schema_refs']:
                    _reference(root, files, reference)
            for validator in module['validators']:
                _reference(root, files, validator)
                if (not _name(validator.get('id')) or validator['id'] in validator_ids
                        or validator.get('signature') != 'state,data'
                        or validator.get('stage') not in ('before_actions', 'after_actions')
                        or type(validator.get('order')) is not int or validator['order'] < 0):
                    raise ValueError('invalid/duplicate state validator')
                position = (validator['stage'], validator['order'])
                if position in validation_orders:
                    raise ValueError('duplicate validation order')
                validator_ids.add(validator['id']); validation_orders.add(position)
            if not isinstance(module.get('legacy_action_validators', []), list):
                raise ValueError('invalid legacy action validators')
            for entry in module.get('legacy_action_validators', []):
                _reference(root, files, entry)
                if (not _name(entry.get('kind')) or entry['kind'] in legacy_kinds
                        or entry.get('signature') != 'state,data,action'):
                    raise ValueError('invalid/duplicate legacy action validator')
                legacy_kinds.add(entry['kind'])
        for name, module in modules.items():
            for entry in module.get('legacy_action_validators', []):
                if action_owners.get(entry['kind']) != name:
                    raise ValueError('missing/mismatched legacy action owner: ' + entry['kind'])
        aliases = contract['capability_modules']
        if aliases:
            capability = _read(root / 'game/data/rule_capabilities.json')
            if set(aliases) != set(capability['modules']) or any(target not in modules for target in aliases.values()):
                raise ValueError('incomplete/stale capability module mapping')
            expected = {name: [] for name in modules}
            for entry in capability['capabilities']:
                target = aliases[entry['module_id']]
                if entry['rule_version'] != modules[target]['version']:
                    raise ValueError('capability module version mismatch')
                expected[target].append(entry['capability_id'])
            if any(sorted(module['capabilities']) != sorted(expected[name]) for name, module in modules.items()):
                raise ValueError('capability associations differ from trusted descriptor')
        elif any(module['capabilities'] for module in modules.values()) or (root / 'game/data/rule_capabilities.json').exists():
            raise ValueError('missing capability module mapping')
    except (OSError, ValueError, KeyError, TypeError) as exc:
        errors.append(str(exc))
    return {'ok': not errors, 'errors': sorted(errors), 'source_edges': len(actual),
            'controlled_files': len(graph), 'modules': len(modules), 'legacy_edges': len(legacy),
            'scope': 'Literal loads, project class references and ownership declarations; not arbitrary data-flow/write analysis or runtime migration'}


def main() -> int:
    """供唯一verify阶段调用，任何坏边或缺合同都返回非零。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--contracts', type=Path)
    args = parser.parse_args()
    result = check_architecture(args.root, args.contracts or args.root / 'game/data/module_contracts.json')
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
