"""从实际 Kernel 分派发现旧命令并比对 Godot 返回的清单，不能用同一枚举自证覆盖。"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_legacy_descriptors_exist_without_kernel_registration():
    """首片仅提供清单；缺文件应失败，生产分派不得提前依赖未迁移合同。"""
    assert (ROOT / 'game/core/runtime/legacy_commands.gd').is_file(), '缺少显式兼容命令清单'
    for source in ['world_kernel.gd', 'runtime/builtin_modules.gd']:
        assert 'legacy_commands.gd' not in (ROOT / 'game/core' / source).read_text()

from copy import deepcopy
import json
import os
import shutil
import subprocess

import pytest

from tools.legacy_command_coverage import coverage_errors, discover_routes, constant_kinds, imports_of, registered_shadow_errors


@pytest.fixture(scope='module')
def coverage_bundle():
    """用实际固定引擎取得GDScript返回值；导入失败或运行错误不能被JSON输出掩盖。"""
    engine = os.environ.get('GODOT_BIN') or shutil.which('godot')
    if not engine:
        pytest.skip('固定Godot引擎未配置，不能执行跨语言覆盖门')
    imported = subprocess.run([engine, '--headless', '--path', str(ROOT / 'game'), '--editor', '--quit'], capture_output=True, text=True, timeout=300)
    assert imported.returncode == 0 and 'SCRIPT ERROR' not in imported.stdout + imported.stderr, imported.stdout + imported.stderr
    result = subprocess.run([engine, '--headless', '--path', str(ROOT / 'game'), '--script', 'res://tests/test_module_coverage.gd'], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0 and 'SCRIPT ERROR' not in result.stdout + result.stderr, result.stdout + result.stderr
    rows = [line.split('=', 1)[1] for line in result.stdout.splitlines() if line.startswith('LEGACY_COMMAND_DESCRIPTORS=')]
    assert len(rows) == 1
    return json.loads(rows[0])


@pytest.fixture(scope='module')
def descriptors(coverage_bundle):
    """只取尚未接入的实际描述，不把已生产注册命令重复放进兼容清单。"""
    return coverage_bundle['legacy']


@pytest.fixture(scope='module')
def registered(coverage_bundle):
    """已接入集合来自真正Builtin/Registry，不由测试复制命令名白名单。"""
    return coverage_bundle['registered']


@pytest.fixture(scope='module')
def owners():
    """采用实际源码归属合同，不另外维护测试领域owner表。"""
    return json.loads((ROOT / 'game/data/module_contracts.json').read_text())['files']


def test_actual_kernel_routes_match_staging_descriptors(descriptors, owners, registered):
    """真实Kernel命令增删、模式或源owner变化必须改变覆盖结论。"""
    assert coverage_errors(ROOT, descriptors, owners, registered=registered) == []


@pytest.mark.parametrize('mutation', ['missing', 'extra', 'duplicate', 'owner', 'mode', 'source'])
def test_descriptor_mutations_fail(descriptors, owners, registered, mutation):
    """证明缺登记、catch-all、多owner和错模式不会被同一源枚举自比较放行。"""
    entries = deepcopy(descriptors)
    if mutation == 'missing': entries.pop()
    elif mutation == 'extra': entries.append({**entries[0], 'kind': '*'})
    elif mutation == 'duplicate': entries.append(deepcopy(entries[0]))
    elif mutation == 'owner': entries[0]['owner'] = 'military'
    elif mutation == 'mode': entries[0]['mode'] = 'action'
    else: entries[0]['source']['path'] = 'game/core/world_kernel.gd'
    assert coverage_errors(ROOT, entries, owners, registered=registered)


@pytest.mark.parametrize('method', ['_preview', '_apply'])
def test_new_real_inline_route_is_discovered(descriptors, owners, registered, method):
    """只在真正Kernel分派添加新命令就变红，测试期望不依赖内联组合表。"""
    text = (ROOT / 'game/core/world_kernel.gd').read_text()
    offset = text.index('\n', text.index('func ' + method + '('))
    text = text[:offset] + '\n\tif kind=="unregistered_probe": return {"ok":true}' + text[offset:]
    errors = coverage_errors(ROOT, descriptors, owners, text, registered=registered)
    assert errors and any('unregistered_probe' in error for error in errors)


@pytest.mark.parametrize('domain', ['personal_combat', 'journey_commands', 'long_rest'])
def test_new_source_kind_is_discovered(tmp_path, descriptors, owners, registered, domain):
    """变动原领域KINDS而保留旧清单会失败，排除两份手写枚举同步自证。"""
    text = (ROOT / 'game/core/world_kernel.gd').read_text()
    source_paths = set(imports_of(text).values()) | {entry['source']['path'] for entry in descriptors} | {'game/core/runtime/legacy_commands.gd'}
    for path in source_paths:
        target = tmp_path / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text((ROOT / path).read_text())
    path = 'game/core/' + domain + '.gd'
    source = tmp_path / path
    original = source.read_text()
    offset = original.index('[', original.index('const KINDS')) + 1
    source.write_text(original[:offset] + '"source_only_probe",' + original[offset:])
    found = discover_routes(tmp_path, text, '_preview')
    assert found['source_only_probe']['path'] == path
    assert any('source_only_probe' in error for error in coverage_errors(tmp_path, descriptors, owners, text, registered=registered))


def test_comments_and_strings_do_not_become_dispatches():
    """显示文本与注释中的代码不能冒充真实命令。"""
    text = 'func _preview():\n\t# if kind=="fake_comment": pass\n\tvar text="if kind==fake_string: pass"\n\tmatch kind:\n\t\t"real": return {}\n'
    assert set(discover_routes(ROOT, text, '_preview')) == {'real'}


@pytest.mark.parametrize('branch', ['\t\t_: return {}', '\t\t"real" when enabled: return {}'])
def test_unbounded_or_unknown_match_syntax_fails(branch):
    """发现器不认识的扩展或默认分派须明确失败，不能静默认为覆盖完整。"""
    with pytest.raises(ValueError):
        discover_routes(ROOT, 'func _preview():\n\tmatch kind:\n' + branch, '_preview')


def test_dynamic_source_constant_fails(tmp_path):
    """领域常量变成动态计算时停止证明覆盖，不退回旧手写清单。"""
    (tmp_path / 'domain.gd').write_text('const KINDS=["known"]+extra()\n')
    with pytest.raises(ValueError, match='dynamic'):
        constant_kinds(tmp_path, 'domain.gd', 'KINDS')


@pytest.mark.parametrize('method', ['_preview', '_apply'])
@pytest.mark.parametrize('branch', [
    '\tif (kind=="unregistered_probe"): return {"ok":true}',
    '\tif params.has("probe") and kind=="unregistered_probe": return {"ok":true}',
    '\tif params.has("probe"):\n\t\tif kind=="unregistered_probe": return {"ok":true}',
])
def test_parenthesized_compound_and_nested_routes_fail_coverage(descriptors, owners, registered, method, branch):
    """独立短审复现的三类真实新增分派均必须打破覆盖，不能只扫描首项kind。"""
    text = (ROOT / 'game/core/world_kernel.gd').read_text()
    offset = text.index('\n', text.index('func ' + method + '('))
    text = text[:offset] + '\n' + branch + text[offset:]
    errors = coverage_errors(ROOT, descriptors, owners, text, registered=registered)
    assert errors and any('unregistered_probe' in error for error in errors)


@pytest.mark.parametrize('method', ['_preview', '_apply'])
@pytest.mark.parametrize('kind', ['accept_offer', 'join_party', 'battle_plan', 'travel_wait', 'long_rest', 'rest_cancel', 'encounter_talk', 'encounter_leave', 'advance', 'wait', 'service'])
def test_registered_shadow_route_is_not_hidden(descriptors, owners, registered, method, kind):
    """已经注册的命令被新内联分派截走仍须变红，各域合法早门不能掩盖第二条路径。"""
    text = (ROOT / 'game/core/world_kernel.gd').read_text()
    offset = text.index('\n', text.index('func ' + method + '('))
    text = text[:offset] + '\n\tif kind=="' + kind + '": return {"ok":true}' + text[offset:]
    errors = coverage_errors(ROOT, descriptors, owners, text, registered=registered)
    assert any('registered command shadow route: ' + method + ':' + kind in error for error in errors)


@pytest.mark.parametrize('mutation', ['owner', 'mode', 'gate_id', 'source'])
def test_migrated_descriptor_mutations_are_not_hidden(descriptors, owners, registered, mutation):
    """已迁移历史项仍验证owner、模式、门位和源身份，不能随registered过滤而消失。"""
    entries = deepcopy(descriptors)
    entry = next(row for row in entries if row['kind'] == 'join_party')
    if mutation == 'source': entry['source']['path'] = 'game/core/world_kernel.gd'
    else: entry[mutation] = {'owner': 'military', 'mode': 'action', 'gate_id': 'legacy_kernel'}[mutation]
    assert any('registered descriptor' in error for error in coverage_errors(ROOT, entries, owners, registered=registered))


def test_party_gate_must_preserve_exact_guard_and_callback(descriptors, owners, registered):
    """早门owner或真实Callable任一被改写，源覆盖不可继续认可特许路由。"""
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    for before, after in [('party_command.get("owner")=="travel"', 'party_command.get("owner")=="family"'),
                          ('return party_command.preview.call(state,data,actor_id,kind,params)', 'return {"ok":true}')]:
        text = source.replace(before, after, 1)
        assert text != source
        assert any('registered command shadow route' in error for error in coverage_errors(ROOT, descriptors, owners, text, registered=registered))


def test_registered_lookup_does_not_hide_compound_kind_route():
    """真实军事gate读取commands[kind]仅为查表，后续同条件的新kind分派仍必须被发现。"""
    guard = '_registry.commands.has(kind) and _registry.commands[kind].owner=="military"'
    text = 'func _preview():\n\tif ' + guard + ': return {}\n'
    assert discover_routes(ROOT, text, '_preview') == {}
    text = 'func _preview():\n\tif ' + guard + ' and kind=="probe": return {}\n'
    assert set(discover_routes(ROOT, text, '_preview')) == {'probe'}


# 只描述需固定的源码形状，命令集合仍从真实Registry和原领域文件取得。
EARLY_GATE_CASES = [
    ('encounter_preview', '_preview', 'encounter_talk',
     '\tif encounter_command.get("owner")=="interaction" and encounter_command.get("gate_id")=="encounter_before_pending" and kind in Encounters.KINDS:',
     '"interaction"', '"encounter_before_pending"', 'kind in Encounters.KINDS',
     'return encounter_command.preview.call(state,data,actor_id,kind,params)'),
    ('personal_preview', '_preview', 'battle_plan',
     '\tif personal_command.get("owner")=="personal" and personal_command.get("gate_id")=="personal_before_world" and kind in Personal.KINDS:',
     '"personal"', '"personal_before_world"', 'kind in Personal.KINDS',
     'return personal_command.preview.call(state,data,actor_id,kind,params)'),
    ('personal_before', '_apply', 'battle_plan',
     '\t\tif registered.owner=="personal" and registered.gate_id=="personal_before_world" and kind in Personal.KINDS:',
     '"personal"', '"personal_before_world"', 'kind in Personal.KINDS',
     'var ready: Dictionary=PersonalCoordination.before_command(state,_catalog.data,actor_id,kind,params,offer,_advance)'),
    ('journey_preview', '_preview', 'travel_wait',
     '\tif journey_command.get("owner")=="travel" and journey_command.get("gate_id")=="journey_before_actor" and kind in Journey.KINDS:',
     '"travel"', '"journey_before_actor"', 'kind in Journey.KINDS',
     'return journey_command.preview.call(state,data,actor_id,kind,params)'),
    ('journey_after', '_apply', 'travel_wait',
     '\t\tif registered.owner=="travel" and registered.gate_id=="journey_before_actor" and kind in Journey.KINDS:',
     '"travel"', '"journey_before_actor"', 'kind in Journey.KINDS',
     'return JourneyModule.after_command(state,actor_id,kind,applied,_advance)'),
    ('rest_preview', '_preview', 'long_rest',
     '\tif kind in Rest.KINDS and _registry.commands.has(kind) and _registry.commands[kind].owner=="services" and _registry.commands[kind].gate_id=="rest_before_actor":',
     '"services"', '"rest_before_actor"', 'kind in Rest.KINDS',
     'return _registry.commands[kind].preview.call(state,data,actor_id,kind,params)'),
    ('rest_cancel', '_apply', 'rest_cancel',
     '\t\tif registered.owner=="services" and kind=="rest_cancel" and registered.gate_id=="rest_before_actor": Step.release(state,state.actions[offer.action_id])',
     '"services"', '"rest_before_actor"', 'kind=="rest_cancel"',
     'Step.release(state,state.actions[offer.action_id])'),
]


@pytest.mark.parametrize('case', EARLY_GATE_CASES, ids=[case[0] for case in EARLY_GATE_CASES])
@pytest.mark.parametrize('mutation', ['owner', 'gate_id', 'callback', 'kind'])
def test_early_gate_requires_complete_exact_shape(descriptors, owners, registered, case, mutation):
    """owner、门位、回调和集合任一改写均撤销该块豁免，新增kind不能随原门被整块吞掉。"""
    _, method, kind, guard, owner, gate_id, kind_expression, callback = case
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    assert registered_shadow_errors(ROOT, source, registered) == []
    assert source.count(guard) == 1
    if mutation == 'callback':
        # Rest预览共用其他门的回调文本，只从本guard起替换一次。
        start = source.index(guard)
        text = source[:start] + source[start:].replace(callback, 'return {"ok":true}', 1)
    else:
        before = {'owner': owner, 'gate_id': gate_id, 'kind': kind_expression}[mutation]
        after = '((' + kind_expression + ') or kind=="early_gate_probe")' if mutation == 'kind' else '"wrong_gate_contract"'
        text = source.replace(guard, guard.replace(before, after, 1), 1)
    assert text != source
    errors = coverage_errors(ROOT, descriptors, owners, text, registered=registered)
    assert 'registered command shadow route: ' + method + ':' + kind in errors
    if mutation == 'kind':
        assert any('early_gate_probe' in error for error in errors)


@pytest.mark.parametrize('mutation', ['battle_guard', 'nested_kind', 'callback_suffix'])
def test_personal_nested_guard_is_part_of_exact_exemption(descriptors, owners, registered, mutation):
    """个人早门包含内嵌BattlePending，修改守卫、插入命令或在回调尾追加代码均不能被前缀匹配掩盖。"""
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    before = '\t\tif Combat.occupied(state,actor_id) and Combat.for_actor(state,actor_id).get("kind")=="military": return {"ok":false,"code":"BattlePending"}'
    if mutation == 'battle_guard': after = before.replace('"BattlePending"', '"WrongPending"')
    elif mutation == 'nested_kind': after = before + '\n\t\tif kind=="nested_gate_probe": return {"ok":true}'
    else:
        before = '\t\treturn personal_command.preview.call(state,data,actor_id,kind,params)'
        after = before + '; pass'
    assert source.count(before) == 1
    text = source.replace(before, after, 1)
    errors = coverage_errors(ROOT, descriptors, owners, text, registered=registered)
    assert 'registered command shadow route: _preview:battle_plan' in errors
    if mutation == 'nested_kind':
        assert any('nested_gate_probe' in error for error in errors)


@pytest.mark.parametrize('case', EARLY_GATE_CASES, ids=[case[0] for case in EARLY_GATE_CASES])
def test_second_shadow_after_exact_gate_remains_visible(descriptors, owners, registered, case):
    """完整合法门后紧接的第二条注册kind分支仍须留给发现器，不得按整个领域忽略。"""
    _, method, kind, guard, _, _, _, callback = case
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    start = source.index(guard)
    final_line = 'if not ready.ok: return ready' if case[0] == 'personal_before' else callback
    end = source.index('\n', source.index(final_line, start))
    indent = '\t' if method == '_preview' else '\t\t'
    text = source[:end] + '\n' + indent + 'if kind=="' + kind + '": return {"ok":true}' + source[end:]
    assert 'registered command shadow route: ' + method + ':' + kind in coverage_errors(ROOT, descriptors, owners, text, registered=registered)


@pytest.mark.parametrize('kind', ['join_party', 'battle_plan', 'travel_wait', 'long_rest', 'rest_cancel', 'encounter_talk', 'encounter_leave'])
@pytest.mark.parametrize('mutation', ['owner', 'mode', 'gate_id'])
def test_early_gate_exemption_requires_actual_registration_contract(registered, kind, mutation):
    """豁免同时核对真实注册合同，long_rest必须是action而rest_cancel必须是instant。"""
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    assert registered_shadow_errors(ROOT, source, registered) == []
    changed = deepcopy(registered)
    changed[kind][mutation] = ('instant' if registered[kind]['mode'] == 'action' else 'action') if mutation == 'mode' else 'wrong_contract'
    assert 'registered command shadow route: _preview:' + kind in registered_shadow_errors(ROOT, source, changed)


@pytest.mark.parametrize('mutation', ['kind', 'guard', 'callback', 'duplicate'])
def test_journey_encounter_guard_has_only_exact_exemption(descriptors, owners, registered, mutation):
    """途中等待沿用完整EncounterPending守卫，新增kind、守卫改写、错误回调或第二条同形分支都须拒绝。"""
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    before = '\tif kind in ["advance","travel_wait"] and not Encounters.pending_for(state,state.player_actor_id).is_empty(): return {"ok":false,"code":"EncounterPending"}'
    after = {
        'kind': before.replace('["advance","travel_wait"]', '["advance","travel_wait","encounter_gate_probe"]'),
        'guard': before.replace('state.player_actor_id', 'actor_id'),
        'callback': before.replace('"EncounterPending"', '"WrongPending"'),
        'duplicate': before + '\n' + before,
    }[mutation]
    assert source.count(before) == 1
    text = source.replace(before, after, 1)
    errors = coverage_errors(ROOT, descriptors, owners, text, registered=registered)
    assert 'registered command shadow route: _preview:travel_wait' in errors
    if mutation == 'kind':
        assert any('encounter_gate_probe' in error for error in errors)


@pytest.mark.parametrize('alias,path,kind', [
    ('Encounters', 'game/core/encounter_rules.gd', 'encounter_talk'),
    ('Parties', 'game/core/party_rules.gd', 'join_party'),
    ('Personal', 'game/core/personal_combat.gd', 'battle_plan'),
    ('Journey', 'game/core/journey_commands.gd', 'travel_wait'),
    ('Rest', 'game/core/long_rest.gd', 'long_rest'),
])
def test_early_gate_requires_original_domain_source(tmp_path, registered, alias, path, kind):
    """即使适配器复制同样KINDS也不能冒充原领域来源，固定门不得只相信别名。"""
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    for original in imports_of(source).values():
        target = tmp_path / original
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text((ROOT / original).read_text())
    adapter_path = 'game/core/copied_adapter.gd'
    (tmp_path / adapter_path).write_text('const KINDS=' + json.dumps(constant_kinds(ROOT, path, 'KINDS')))
    text = source.replace('res://' + path.removeprefix('game/'), 'res://' + adapter_path.removeprefix('game/'), 1)
    assert imports_of(text)[alias] == adapter_path
    assert 'registered command shadow route: _preview:' + kind in registered_shadow_errors(tmp_path, text, registered)


@pytest.mark.parametrize('case', EARLY_GATE_CASES, ids=[case[0] for case in EARLY_GATE_CASES])
@pytest.mark.parametrize('mutation', ['remove_kind', 'remove_block', 'duplicate_block'])
def test_registered_gate_cannot_disappear_from_source_scanner(registered, case, mutation):
    """删除kind谓词或整个门会避开一般路由发现，必须由完整必需块合同独立拒绝。"""
    _, method, _, guard, _, _, kind_expression, _ = case
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    if mutation == 'remove_kind':
        if guard.startswith('\tif ' + kind_expression + ' and '):
            changed = guard.replace(kind_expression + ' and ', '', 1)
        else:
            changed = guard.replace(' and ' + kind_expression, '', 1)
        assert changed != guard
        text = source.replace(guard, changed, 1)
    else:
        lines = source.splitlines()
        start = lines.index(guard)
        indent = len(guard) - len(guard.lstrip('\t'))
        end = start + 1
        while end < len(lines) and lines[end].startswith('\t' * (indent + 1)):
            end += 1
        if case[0] in ['personal_preview', 'journey_preview', 'encounter_preview']:
            start -= 1
            assert lines[start].startswith('\tvar ')
        block = lines[start:end]
        lines[start:end] = [] if mutation == 'remove_block' else block + block
        text = '\n'.join(lines)
    assert any('registered command gate mismatch:' in error for error in registered_shadow_errors(ROOT, text, registered))


def test_encounter_gate_must_preserve_exact_guard_and_callback(descriptors, owners, registered):
    """路遇门仅允许固定owner、KINDS、gate及真实preview，任意扩大条件必须失去豁免。"""
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    for before, after in [('encounter_command.get("owner")=="interaction"', 'encounter_command.get("owner")=="travel"'),
                          ('encounter_command.get("gate_id")=="encounter_before_pending"', 'encounter_command.get("gate_id")=="legacy_kernel"'),
                          (' and kind in Encounters.KINDS', ''),
                          ('return encounter_command.preview.call(state,data,actor_id,kind,params)', 'return {"ok":true}')]:
        text = source.replace(before, after, 1)
        assert text != source
        assert coverage_errors(ROOT, descriptors, owners, text, registered=registered)


@pytest.mark.parametrize('before,after', [
    ('advance_command.get("owner")=="runtime"', 'advance_command.get("owner")=="foreign"'),
    ('advance_command.get("gate_id")=="advance_before_actor"', 'advance_command.get("gate_id")=="wrong"'),
    ('kind=="advance" and advance_command', 'advance_command'),
    ('return advance_command.preview.call(state,data,actor_id,kind,params)', 'return {"ok":true}'),
    ('registered.owner!="runtime" or kind!="advance"', 'kind!="advance"'),
    ('or kind!="advance" or registered.gate_id', 'or registered.gate_id'),
    ('or registered.gate_id!="advance_before_actor" or typeof(applied.ok)', 'or typeof(applied.ok)'),
    ('return _advance(state)', 'return {"ok":true}'),
    ('or applied!={"ok":true,"control":{"advance_world":true}}', ''),
])
def test_core_control_locks_and_consumption_are_exact(descriptors, owners, registered, before, after):
    """核心早门、owner/kind/gate三锁、精确载荷和真实消费任一变更都撤销豁免。"""
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    assert registered_shadow_errors(ROOT, source, registered) == []
    assert source.count(before) == 1
    text = source.replace(before, after, 1)
    assert any('registered command gate mismatch: Runtime:' in error for error in coverage_errors(ROOT, descriptors, owners, text, registered=registered))


@pytest.mark.parametrize('method', ['_preview', '_apply'])
@pytest.mark.parametrize('mutation', ['remove', 'duplicate'])
def test_core_complete_control_blocks_cannot_disappear(registered, method, mutation):
    """完整协调块被删除或复制仍必须失败，不能因为扫描器失去kind谓词而静默成功。"""
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    if method == '_preview':
        start = source.index('\tvar advance_command:')
        end = source.index('\n', source.index('return advance_command.preview.call(state,data,actor_id,kind,params)', start))
    else:
        start = source.index('\t\tif applied.has("control"):')
        end = source.index('\n', source.index('\t\tif registered.owner=="runtime" and kind=="advance":', start))
    block = source[start:end]
    text = source[:start] + ('' if mutation == 'remove' else block + '\n' + block) + source[end:]
    assert 'registered command gate mismatch: Runtime:' + method in registered_shadow_errors(ROOT, text, registered)


def test_rescue_gate_exact_guard_and_callback(descriptors, owners, registered):
    """救援早门只豁免完整固定owner、kind、gate与真实Callable，影子分支仍失败。"""
    source = (ROOT / 'game/core/world_kernel.gd').read_text()
    for before, after in [
        ('rescue_command.get("owner")=="travel"', 'rescue_command.get("owner")=="military"'),
        ('rescue_command.get("gate_id")=="rescue_before_actor"', 'rescue_command.get("gate_id")=="legacy_kernel"'),
        ('and kind=="rescue":', 'and kind in ["rescue","move_local"]:'),
        ('return rescue_command.preview.call(state,data,actor_id,kind,params)', 'return {"ok":true}'),
    ]:
        text = source.replace(before, after, 1)
        assert text != source
        assert any('registered command shadow route' in error for error in coverage_errors(ROOT, descriptors, owners, text, registered=registered))
    offset = source.index('\n', source.index('func _preview('))
    shadow = source[:offset] + '\n\tif kind=="rescue": return {"ok":true}' + source[offset:]
    assert any('registered command shadow route: _preview:rescue' in error for error in coverage_errors(ROOT, descriptors, owners, shadow, registered=registered))


@pytest.mark.parametrize('method', ['_preview', '_apply'])
@pytest.mark.parametrize('kind', ['talk', 'open_event', 'dialogue_next', 'dialogue_choose', 'dialogue_confirm', 'dialogue_cancel', 'close_trade', 'end_interaction', 'open_trade'])
def test_dialogue_inline_shadow_is_not_hidden(descriptors, owners, registered, method, kind):
    """九项已迁移对白若重新接回内联，精确守卫豁免不得掩盖旧分支残留。"""
    text = (ROOT / 'game/core/world_kernel.gd').read_text()
    offset = text.index('\n', text.index('func ' + method + '('))
    text = text[:offset] + '\n\tif kind=="' + kind + '": return {"ok":true}' + text[offset:]
    assert any('registered command shadow route: ' + method + ':' + kind in error
               for error in coverage_errors(ROOT, descriptors, owners, text, registered=registered))


@pytest.mark.parametrize('needle,replacement', [
    ('registered.get("owner")=="interaction" and kind in DialogueCommands.KINDS', 'registered.get("owner")=="tasks" and kind in DialogueCommands.KINDS'),
    ('params,_preview)', 'params)'),
    ('offer,_drive_dialogue,_apply)', 'offer,_drive_dialogue)'),
])
def test_dialogue_callback_gate_requires_exact_trusted_shape(descriptors, owners, registered, needle, replacement):
    """丢失owner限制或原递归回调时停止覆盖证明，不按对白名整批忽略分派。"""
    text = (ROOT / 'game/core/world_kernel.gd').read_text().replace(needle, replacement)
    assert any('registered command shadow route:' in error
               for error in coverage_errors(ROOT, descriptors, owners, text, registered=registered))


@pytest.mark.parametrize('source', [
    'game/core/runtime/runtime_commands.gd',
    'game/core/domains/travel/movement_commands.gd',
    'game/core/domains/interaction/dialogue_commands.gd',
    'game/core/domains/social/news_commands.gd',
    'game/core/domains/interaction/discovery_commands.gd',
])
def test_registered_inventory_cannot_drop_whole_source(descriptors, owners, registered, source):
    """全注册后删掉整域清单仍须由真实preload来源KINDS发现，不能靠剩余行自证完整。"""
    removed = {row['kind'] for row in descriptors if row['source']['path'] == source}
    assert removed and removed <= registered.keys()
    changed = [row for row in descriptors if row['source']['path'] != source]
    errors = coverage_errors(ROOT, changed, owners, registered=registered)
    assert any('missing source command descriptors:' in error and all(kind in error for kind in removed) for error in errors)
