"""独立读取Kernel实际分派与原领域KINDS，校验A1.7清单；只支持明确的现有源码形状。"""
from __future__ import annotations

import ast
from pathlib import Path
import re

from tools.check_architecture import _tokens

KERNEL = 'game/core/world_kernel.gd'


def function_lines(text: str, name: str) -> list[str]:
    """读取一个具名函数的真实缩进行，不把其他函数、注释或字符串里的分派算入。"""
    lines = text.splitlines()
    starts = [i for i, line in enumerate(lines) if re.match(r'^func\s+' + re.escape(name) + r'\(', line)]
    if len(starts) != 1:
        raise ValueError('missing or ambiguous function: ' + name)
    result = []
    for line in lines[starts[0] + 1:]:
        if line and not line[0].isspace() and not line.startswith('#'):
            break
        result.append(line)
    return result


def imports_of(text: str) -> dict[str, str]:
    """仅接受真实固定preload赋值，不执行脚本或采用清单自己声明的领域路径。"""
    tokens = _tokens(text)
    result = {}
    for i in range(len(tokens) - 6):
        if tokens[i] == 'const' and tokens[i + 2:i + 5] == ['=', 'preload', '('] and tokens[i + 6] == ')':
            resource = ast.literal_eval(tokens[i + 5])
            if isinstance(resource, str) and resource.startswith('res://'):
                result[tokens[i + 1]] = 'game/' + resource[6:]
    return result


def literal_array(tokens: list[str]) -> list[str]:
    """数组必须是无重复非空字面命令；拒绝动态拼接和通配，不能把未知语法当空集合。"""
    try:
        value = ast.literal_eval(' '.join(tokens))
    except (ValueError, SyntaxError) as error:
        raise ValueError('unsupported command array') from error
    if (not isinstance(value, list) or not value or any(not isinstance(x, str) or not x or '*' in x for x in value)
            or len(set(value)) != len(value)):
        raise ValueError('invalid command array')
    return value


def constant_kinds(root: Path, path: str, name: str) -> list[str]:
    """直接读取领域公开常量，常量变成动态表达式时失败，不回退到另一份手写枚举。"""
    tokens = _tokens((root / path).read_text(encoding='utf-8'))
    starts = [i + 3 for i in range(len(tokens) - 3) if tokens[i:i + 3] == ['const', name, '=']]
    if len(starts) != 1 or tokens[starts[0]] != '[':
        raise ValueError('unsupported command constant: ' + path + ':' + name)
    start = starts[0]
    end = tokens.index(']', start) + 1
    if tokens[end:end + 1] == ['+']:
        raise ValueError('dynamic command constant: ' + path + ':' + name)
    return literal_array(tokens[start:end])


def expression_kinds(root: Path, tokens: list[str], imports: dict[str, str]) -> tuple[list[str], dict]:
    """只解析字面数组或固定源常量；返回独立来源供owner与来源路径校验。"""
    if len(tokens) == 3 and tokens[1] == '.' and tokens[0] in imports:
        path = imports[tokens[0]]
        return constant_kinds(root, path, tokens[2]), {'path': path, 'constant': tokens[2]}
    return literal_array(tokens), {'path': KERNEL, 'constant': ''}


def condition_kinds(root: Path, condition: list[str], imports: dict[str, str]) -> list[tuple[list[str], dict]]:
    """识别条件任意位置的独立kind比较；括号、复合和嵌套语句不能隐藏新命令。"""
    result = []
    for i, token in enumerate(condition):
        if token != 'kind' or i and condition[i - 1] == '.':
            continue
        tail = condition[i + 1:]
        if tail[:2] == ['=', '='] and len(tail) >= 3:
            result.append((literal_array(['[', tail[2], ']']), {'path': KERNEL, 'constant': ''}))
        elif tail[:1] == ['in']:
            if tail[1:2] == ['[']:
                end = tail.index(']') + 1
                expression = tail[1:end]
            else:
                expression = tail[1:4]
            result.append(expression_kinds(root, expression, imports))
        elif tail[:2] in (['not', 'in'], ['!', '=']):
            continue
        elif tail[:1] == [')'] and condition[max(0, i - 4):i] == ['commands', '.', 'has', '(']:
            continue
        elif tail[:1] == [']'] and condition[max(0, i - 4):i] == ['_registry', '.', 'commands', '[']:
            continue
        else:
            raise ValueError('unsupported kind condition: ' + ' '.join(condition))
    return result


def discover_routes(root: Path, text: str, method: str) -> dict[str, dict]:
    """遍历所有缩进的kind条件和明确match；新语法不能静默跳过，通配match直接拒绝。"""
    result = {}; imports = imports_of(text); match_indent = None
    for line in function_lines(text, method):
        indent = len(line) - len(line.lstrip('\t'))
        tokens = _tokens(line)
        if not tokens:
            continue
        if match_indent is not None and indent <= match_indent:
            match_indent = None
        routes = []
        if tokens[:1] == ['match'] and 'kind' in tokens:
            if tokens != ['match', 'kind', ':']:
                raise ValueError('unsupported kind match')
            match_indent = indent
        elif match_indent is not None and indent == match_indent + 1:
            if ':' not in tokens:
                raise ValueError('unsupported kind match')
            pattern = tokens[:tokens.index(':')]
            routes.append((literal_array(['['] + pattern + [']']), {'path': KERNEL, 'constant': ''}))
        elif tokens[0] in ('if', 'elif'):
            if ':' not in tokens:
                raise ValueError('unsupported multiline condition')
            routes = condition_kinds(root, tokens[1:tokens.index(':')], imports)
        for kinds, source in routes:
            for kind in kinds:
                # 复合前置守卫中的字面例外不得覆盖后面实际领域常量的身份。
                if kind not in result or source['path'] != KERNEL:
                    result[kind] = source.copy()
    return result


def discover_action_starters(root: Path, text: str) -> set[str]:
    """旧开工入口白名单是action-mode独立真值，rescue原地换kind不能冒充新开工。"""
    matches = []
    for line in function_lines(text, '_begin_action'):
        tokens = _tokens(line)
        if tokens[:5] == ['if', 'not', 'kind', 'in', '[']:
            matches.append(tokens[4:tokens.index(':')])
    if len(matches) != 1:
        raise ValueError('unsupported legacy action admission')
    terms = []; start = 0; expression = matches[0]
    for i, token in enumerate(expression):
        if token == '+':
            terms.append(expression[start:i]); start = i + 1
    terms.append(expression[start:])
    result = set(); imports = imports_of(text)
    for term in terms:
        kinds, _ = expression_kinds(root, term, imports)
        if result.intersection(kinds):
            raise ValueError('duplicate legacy action starter')
        result.update(kinds)
    return result



def remove_exact_route_block(text: str, method: str, block: list[str]) -> str:
    """仅删除指定函数内唯一的完整固定行块；同形第二路径、行尾追加或块内插入均保留。"""
    body = function_lines(text, method)
    matches = [i for i in range(len(body) - len(block) + 1) if body[i:i + len(block)] == block]
    if len(matches) != 1:
        return text
    lines = text.splitlines()
    start = next(i + 1 for i, line in enumerate(lines) if re.match(r'^func\s+' + re.escape(method) + r'\(', line))
    offset = start + matches[0]
    del lines[offset:offset + len(block)]
    return '\n'.join(lines)


def registered_shadow_errors(root: Path, text: str, registered: dict) -> list[str]:
    """只豁免原领域、真实注册合同和完整固定早门/协调块；其余任意缩进shadow分派仍须拒绝。"""
    gates = [
        ('DialogueCommands', 'game/core/domains/interaction/dialogue_commands.gd', 'interaction', 'legacy_kernel', set(), [('_preview', ['\t\tif registered.get("owner")=="interaction" and kind in DialogueCommands.KINDS:', '\t\t\treturn registered.preview.call(state,data,actor_id,kind,params,_preview)']), ('_apply', ['\t\telif registered.get("owner")=="interaction" and kind in DialogueCommands.KINDS:', '\t\t\tapplied=registered.apply.call(state,_catalog.data,actor_id,kind,params,offer,_drive_dialogue,_apply)']), ('_preview', ['\tif not meeting.is_empty() and not kind in ["dialogue_next","dialogue_choose","end_interaction"]: return {"ok":false,"code":"EncounterPending"}']), ('_preview', ['\tif not actor.action_id.is_empty() and not (not meeting.is_empty() and kind in ["dialogue_next","dialogue_choose","end_interaction"] and state.interactions.has(actor_id) and state.interactions[actor_id].context.get("encounter_id")==meeting.id): return {"ok":false,"code":"ActorBusy"}'])]),
        ('Encounters', 'game/core/encounter_rules.gd', 'interaction', 'encounter_before_pending', set(), [
            ('_preview', [
                '\tvar encounter_command: Dictionary=_registry.commands.get(kind,{})',
                '\tif encounter_command.get("owner")=="interaction" and encounter_command.get("gate_id")=="encounter_before_pending" and kind in Encounters.KINDS:',
                '\t\treturn encounter_command.preview.call(state,data,actor_id,kind,params)',
            ]),
        ]),
        ('Parties', 'game/core/party_rules.gd', 'travel', 'party_before_actor', set(), [
            ('_preview', [
                '\tvar party_command: Dictionary=_registry.commands.get(kind,{})',
                '\tif party_command.get("owner")=="travel" and party_command.get("gate_id")=="party_before_actor" and kind in Parties.KINDS:',
                '\t\treturn party_command.preview.call(state,data,actor_id,kind,params)',
            ]),
        ]),
        ('Personal', 'game/core/personal_combat.gd', 'personal', 'personal_before_world', set(), [
            ('_preview', [
                '\tvar personal_command: Dictionary=_registry.commands.get(kind,{})',
                '\tif personal_command.get("owner")=="personal" and personal_command.get("gate_id")=="personal_before_world" and kind in Personal.KINDS:',
                '\t\tif Combat.occupied(state,actor_id) and Combat.for_actor(state,actor_id).get("kind")=="military": return {"ok":false,"code":"BattlePending"}',
                '\t\treturn personal_command.preview.call(state,data,actor_id,kind,params)',
            ]),
            ('_apply', [
                '\t\tif registered.owner=="personal" and registered.gate_id=="personal_before_world" and kind in Personal.KINDS:',
                '\t\t\tvar ready: Dictionary=PersonalCoordination.before_command(state,_catalog.data,actor_id,kind,params,offer,_advance)',
                '\t\t\tif not ready.ok: return ready',
            ]),
        ]),
        ('Journey', 'game/core/journey_commands.gd', 'travel', 'journey_before_actor', set(), [
            ('_preview', [
                '\tvar journey_command: Dictionary=_registry.commands.get(kind,{})',
                '\tif journey_command.get("owner")=="travel" and journey_command.get("gate_id")=="journey_before_actor" and kind in Journey.KINDS:',
                '\t\treturn journey_command.preview.call(state,data,actor_id,kind,params)',
            ]),
            ('_preview', [
                '\tif kind in ["advance","travel_wait"] and not Encounters.pending_for(state,state.player_actor_id).is_empty(): return {"ok":false,"code":"EncounterPending"}',
            ]),
            ('_apply', [
                '\t\tif registered.owner=="travel" and registered.gate_id=="journey_before_actor" and kind in Journey.KINDS:',
                '\t\t\treturn JourneyModule.after_command(state,actor_id,kind,applied,_advance)',
            ]),
        ]),
        ('Rest', 'game/core/long_rest.gd', 'services', 'rest_before_actor', {'long_rest'}, [
            ('_preview', [
                '\tif kind in Rest.KINDS and _registry.commands.has(kind) and _registry.commands[kind].owner=="services" and _registry.commands[kind].gate_id=="rest_before_actor":',
                '\t\treturn _registry.commands[kind].preview.call(state,data,actor_id,kind,params)',
            ]),
            ('_apply', [
                '\t\tif registered.owner=="services" and kind=="rest_cancel" and registered.gate_id=="rest_before_actor": Step.release(state,state.actions[offer.action_id])',
            ]),
        ]),
    ]
    imports = imports_of(text)
    errors = []
    for alias, path, owner, gate_id, action_kinds, blocks in gates:
        kinds = constant_kinds(root, path, 'KINDS')
        if not all(registered.get(kind) == {'owner': owner, 'mode': 'action' if kind in action_kinds else 'instant', 'gate_id': gate_id} for kind in kinds):
            continue
        if imports.get(alias) != path:
            errors.append('registered command gate source mismatch: ' + alias)
            continue
        for method, block in blocks:
            stripped = remove_exact_route_block(text, method, block)
            if stripped == text:
                # 删除kind谓词会让通用扫描看不到分派，因此必需完整门本身也须失败关闭。
                errors.append('registered command gate mismatch: ' + alias + ':' + method)
            text = stripped
    if registered.get('rescue') == {'owner': 'travel', 'mode': 'instant', 'gate_id': 'rescue_before_actor'}:
        block = ['\tvar rescue_command: Dictionary=_registry.commands.get(kind,{})', '\tif rescue_command.get("owner")=="travel" and rescue_command.get("gate_id")=="rescue_before_actor" and kind=="rescue":', '\t\treturn rescue_command.preview.call(state,data,actor_id,kind,params)']
        stripped = remove_exact_route_block(text, '_preview', block)
        if stripped == text:
            errors.append('registered command gate mismatch: Rescue:_preview')
        text = stripped
    if registered.get('advance') == {'owner': 'runtime', 'mode': 'instant', 'gate_id': 'advance_before_actor'}:
        # 只移除函数内唯一完整协调块；缺锁、删块、重复块和消费替换均失败关闭。
        core_blocks = [('_preview', ['\tvar advance_command: Dictionary=_registry.commands.get(kind,{})', '\tif kind=="advance" and advance_command.get("owner")=="runtime" and advance_command.get("gate_id")=="advance_before_actor":', '\t\treturn advance_command.preview.call(state,data,actor_id,kind,params)']), ('_apply', ['\t\tif applied.has("control"):', '\t\t\tif registered.owner!="runtime" or kind!="advance" or registered.gate_id!="advance_before_actor" or typeof(applied.ok)!=TYPE_BOOL or not applied.control is Dictionary or typeof(applied.control.get("advance_world"))!=TYPE_BOOL or applied!={"ok":true,"control":{"advance_world":true}}: return {"ok":false,"code":"InvalidCommandControl"}', '\t\t\treturn _advance(state)', '\t\tif registered.owner=="runtime" and kind=="advance": return {"ok":false,"code":"InvalidCommandControl"}'])]
        for method, block in core_blocks:
            stripped = remove_exact_route_block(text, method, block)
            if stripped == text:
                errors.append('registered command gate mismatch: Runtime:' + method)
            text = stripped
    for method in ('_preview', '_apply'):
        for kind in discover_routes(root, text, method).keys() & registered.keys():
            errors.append('registered command shadow route: ' + method + ':' + kind)
    return errors

def coverage_errors(root: Path, entries: list[dict], files: dict[str, str], text: str | None = None, registered: dict | None = None) -> list[str]:
    """源路由、双向集合、模式、领域owner、来源均独立比对；清单本身只作为被测输入。"""
    text = (root / KERNEL).read_text(encoding='utf-8') if text is None else text
    preview = discover_routes(root, text, '_preview')
    apply = discover_routes(root, text, '_apply')
    registered = registered or {}
    expected = {kind: source for kind, source in preview.items() if files.get(source['path']) != 'military' and kind not in registered}
    applied = {kind for kind, source in apply.items() if files.get(source['path']) != 'military' and kind not in registered}
    starters = discover_action_starters(root, text)
    errors = registered_shadow_errors(root, text, registered)
    if set(expected) != applied:
        errors.append('preview/apply source mismatch: ' + str(sorted(set(expected) ^ applied)))
    actual = {}
    for entry in entries:
        kind = entry['kind']
        if kind in actual:
            errors.append('duplicate descriptor: ' + kind)
        actual[kind] = entry
    # 注册后Kernel不再暴露领域内联路由；仍从固定来源原KINDS核对必需描述，不能随registered过滤丢失整项或整域。
    inventory = (root / 'game/core/runtime/legacy_commands.gd').read_text(encoding='utf-8')
    required = set()
    for path in imports_of(inventory).values():
        required.update(constant_kinds(root, path, 'KINDS'))
    missing = required - actual.keys()
    if missing:
        errors.append('missing source command descriptors: ' + str(sorted(missing)))
    for kind in actual.keys() & registered.keys():
        entry = actual[kind]
        if any(entry.get(key) != registered[kind].get(key) for key in ('owner', 'mode', 'gate_id')):
            errors.append('registered descriptor contract mismatch: ' + kind)
        source = entry.get('source', {})
        if source.get('path') == KERNEL or files.get(source.get('path')) != entry.get('owner'):
            errors.append('registered descriptor source mismatch: ' + kind)
        elif source.get('constant') != 'KINDS' or kind not in constant_kinds(root, source['path'], 'KINDS'):
            errors.append('registered descriptor source constant mismatch: ' + kind)
        if entry['mode'] != ('action' if kind in starters else 'instant'):
            errors.append('registered descriptor mode mismatch: ' + kind)
    actual = {kind: entry for kind, entry in actual.items() if kind not in registered}
    if set(actual) != set(expected):
        errors.append('descriptor coverage mismatch: ' + str(sorted(set(actual) ^ set(expected))))
    for kind in sorted(actual.keys() & expected.keys()):
        entry = actual[kind]; source = expected[kind]
        if entry['source'] != source:
            errors.append('descriptor source mismatch: ' + kind)
        if source['path'] != KERNEL and entry['owner'] != files[source['path']]:
            errors.append('descriptor owner mismatch: ' + kind)
        if entry['mode'] != ('action' if kind in starters else 'instant'):
            errors.append('descriptor mode mismatch: ' + kind)
    return errors
