"""把作者明确选择的定义投影为剧本初态候选；不激活依赖、不建立世界。"""
from copy import deepcopy

from .document import DocumentError
from .references import iter_references, iter_key_references
from .scenarios import ACTOR_FIELDS, INITIAL_FIELDS, SNAPSHOT_FIELDS, register_schema, resolve
from .validation import SCHEMA, SCHEMA_BYTES, StrictValidator, TABLES, _bounded, pointer


def _validator(rule):
    """沿受信作者Schema校验局部候选，保留严格整数和原引用定义。"""
    return StrictValidator({**rule, '$defs': SCHEMA['$defs']})


def _differences(before, after, path=''):
    """完整保留新增、删除及替换值；数组作为整体展示，避免隐藏旧初态成员。"""
    if type(before) is type(after) and before == after:
        return []
    if isinstance(before, dict) and isinstance(after, dict):
        result = []
        for key in sorted(before.keys() | after.keys()):
            target = path + pointer((key,))
            if key not in before or key not in after:
                row = {'path': target, 'before_present': key in before, 'after_present': key in after}
                if key in before:
                    row['before'] = deepcopy(before[key])
                if key in after:
                    row['after'] = deepcopy(after[key])
                result.append(row)
            else:
                result.extend(_differences(before[key], after[key], target))
        return result
    return [{'path': path, 'before_present': True, 'after_present': True,
             'before': deepcopy(before), 'after': deepcopy(after)}]


def preview_snapshot(documents, entry, scenario_id, selection, namespace=''):
    """复制显式选中定义并返回完整差异和定位诊断；调用者负责会话及原件token。"""
    fields = {'enabled', 'playable_actor_ids', 'start_date', 'actor_options', 'role_options'}
    if not _bounded(selection) or not isinstance(selection, dict) or set(selection) != fields:
        raise DocumentError('INVALID_SNAPSHOT_SELECTION', '选择须明确包含活动实体、可控人物、日期与人物/岗位选项')
    enabled = selection['enabled']
    allowed = SCHEMA['$defs']['scenario_enabled']['properties']
    if not isinstance(enabled, dict) or set(enabled) - set(allowed):
        raise DocumentError('INVALID_SNAPSHOT_SELECTION', '活动实体只接受当前Schema声明的数据表')
    for table, identifiers in enabled.items():
        if (not isinstance(identifiers, list) or not all(isinstance(value, str) for value in identifiers)
                or len(set(identifiers)) != len(identifiers)):
            raise DocumentError('INVALID_SNAPSHOT_SELECTION', '活动实体须逐项明确且不得重复', table=table)
    for key, table, option_fields in [('actor_options', 'actors', {'alive', 'available', 'copy_initial_goods'}),
                                      ('role_options', 'roles', {'enabled'})]:
        options = selection[key]
        if not isinstance(options, dict) or set(options) != set(enabled.get(table, [])):
            raise DocumentError('INVALID_SNAPSHOT_SELECTION', '每个选中人物或岗位都须明确确认选项', field=key)
        if any(not isinstance(row, dict) or set(row) != option_fields or
               any(type(value) is not bool for value in row.values()) for row in options.values()):
            raise DocumentError('INVALID_SNAPSHOT_SELECTION', '人物存活、可用、初货与岗位开关须明确为布尔值', field=key)
    if entry not in documents:
        raise DocumentError('UNKNOWN_ENTRY', '剧本入口不存在', entry=entry)
    scenario = next((row for row in documents[entry].get('scenarios', []) if row['id'] == scenario_id), None)
    if scenario is None:
        raise DocumentError('UNKNOWN_RECORD', '当前入口没有该剧本', record_id=scenario_id)
    if scenario.get('dynamic_world', {}).get('founder_policy') in ('fixed_actor', 'fixed_actor_appearance_v1', 'fixed_actor_appearance_composition_v1') and selection['playable_actor_ids'] != [scenario['player_actor']]:
        raise DocumentError('DYNAMIC_FIXED_ACTOR_REQUIRED', '此宏观情景使用固定作者人物，活动选择不能扩大创始人范围', record_id=scenario_id)
    index = {table: {} for table in (*TABLES, 'texts', 'flags')}
    origins, issues, notes = {}, [], []

    def issue(code, table, ident, suffix, message):
        """将定义错误映回真实入口及字段，供面板使用已有定位机制。"""
        issues.append({'code': code, 'entry': origins.get((table, ident), entry),
                       'path': pointer((table, ident)) + suffix, 'record_id': ident, 'message': message})

    for name, document in documents.items():
        for table in index:
            rows = document.get(table, {}) if table in ('texts', 'flags') else {row['id']: row for row in document.get(table, [])}
            for ident, row in rows.items():
                if ident in index[table]:
                    issue('DUPLICATE_ID', table, ident, '', '跨入口定义重复，不能猜测来源：' + ident)
                else:
                    index[table][ident] = deepcopy(row)
                    origins[table, ident] = name
    proposal = {'playable_actor_ids': deepcopy(selection['playable_actor_ids']),
                'start_date': deepcopy(selection['start_date']), 'enabled': deepcopy(enabled),
                'initial': {'actors': [], 'offer_ids': deepcopy(enabled.get('offers', []))}}
    for table in INITIAL_FIELDS:
        proposal['initial'][table] = []
    for table in ('actors', *INITIAL_FIELDS):
        for ident in enabled.get(table, []):
            source = index[table].get(ident)
            if source is None:
                issue('UNKNOWN_RECORD', table, ident, '', '选中定义不存在：' + ident)
                continue
            if table == 'actors':
                row = {'actor_id': ident, 'alive': selection['actor_options'][ident]['alive'],
                       'available': selection['actor_options'][ident]['available']}
                aliases = {'place_id': 'initial_place', 'age_years': 'initial_age_years'}
                required = [field for field in ACTOR_FIELDS if field not in row]
            elif table == 'roles':
                row = {'role_id': ident, 'enabled': selection['role_options'][ident]['enabled']}
                aliases, required = {'actor_id': 'initial_actor'}, ['actor_id']
            else:
                row, aliases, required = {}, {}, INITIAL_FIELDS[table]
            for field in required:
                source_field = aliases.get(field, field)
                if table == 'actors' and field == 'combat' and source_field not in source:
                    row[field] = {}
                    notes.append({'entry': origins[table, ident], 'path': pointer((table, ident, 'combat')),
                                  'message': '源未声明战斗配置；沿现有无装备合同生成 combat={}，请确认'})
                elif source_field not in source:
                    issue('MISSING_SNAPSHOT_SOURCE', table, ident, pointer((source_field,)), '请先补齐定义字段：' + source_field)
                else:
                    row[field] = deepcopy(source[source_field])
            if table == 'actors':
                copy_goods = selection['actor_options'][ident]['copy_initial_goods']
                if copy_goods and 'initial_goods' not in source:
                    issue('MISSING_SNAPSHOT_SOURCE', table, ident, '/initial_goods', '定义未配置个人初货；补齐后复制，或明确选择空初货')
                row['initial_goods'] = deepcopy(source.get('initial_goods', [])) if copy_goods else []
            proposal['initial'][table].append(row)
    candidate = deepcopy(scenario)
    candidate.update(deepcopy(proposal))
    index['scenarios'][scenario_id] = candidate
    if namespace:
        from .pack_compose import _assign
        prefix = namespace + '::'
        for table in TABLES:
            for record in index[table].values():
                for path, _target, reference in iter_references(record, SCHEMA['properties'][table]['items']):
                    if reference.startswith(prefix):
                        _assign(record, path, reference[len(prefix):])
                for path, _target, reference in sorted(iter_key_references(record, SCHEMA['properties'][table]['items']), key=lambda row: len(row[0]), reverse=True):
                    if not reference.startswith(prefix):
                        continue
                    parent = record
                    for part in path:
                        parent = parent[part]
                    local = reference[len(prefix):]
                    if local in parent:
                        raise DocumentError('DUPLICATE_SNAPSHOT_REFERENCE', '身份键去除本包前缀后发生冲突', record_id=record['id'])
                    replacement = {local if key == reference else key: value for key, value in parent.items()}
                    parent.clear()
                    parent.update(replacement)
    for error in _validator(SCHEMA['properties']['scenarios']['items']).iter_errors(candidate):
        issue('SCHEMA', 'scenarios', scenario_id, pointer(error.absolute_path), error.message)
    # 只检查显式实体及其声明引用的定义，不把依赖写回活动选择。
    queue = [(table, ident) for table, identifiers in enabled.items() for ident in identifiers]
    queue.append(('scenarios', scenario_id))
    visited = set()
    while queue:
        table, ident = queue.pop()
        if (table, ident) in visited:
            continue
        visited.add((table, ident))
        if ident not in index.get(table, {}):
            issue('UNKNOWN_RECORD', table, ident, '', '引用定义不存在：' + ident)
            continue
        if table in ('texts', 'flags'):
            continue
        record = index[table][ident]
        rule = SCHEMA['properties'][table]['items']
        for error in _validator(rule).iter_errors(record):
            issue('SCHEMA', table, ident, pointer(error.absolute_path), error.message)
        for path, target_table, target in iter_references(record, rule):
            if target not in index.get(target_table, {}):
                issue('REFERENCE', table, ident, pointer(path), '引用定义不存在：' + target_table + '/' + target)
            else:
                queue.append((target_table, target))
    if not issues:
        registration = register_schema(SCHEMA_BYTES)
        result = resolve({'ok': True, 'data': index, 'schema_sha256': registration['schema_sha256']}, scenario_id)
        if not result['ok']:
            parts = result['path'].split('/')
            table, ident = (parts[1], parts[2]) if len(parts) > 2 else ('scenarios', scenario_id)
            issue(result['code'], table, ident, '/' + '/'.join(parts[3:]) if len(parts) > 3 else '',
                  '初态检查未通过：' + result['code'] + '；缺失依赖须自行勾选或修正定义')
    before = {field: deepcopy(scenario[field]) for field in SNAPSHOT_FIELDS if field in scenario}
    return {'entry': entry, 'scenario_id': scenario_id, 'selection': deepcopy(selection),
            'before': before, 'proposal': proposal, 'differences': _differences(before, proposal),
            'issues': issues, 'notes': notes, 'can_apply': not issues, 'runtime_verified': False}
