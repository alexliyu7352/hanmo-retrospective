"""亲生接续选择原亲代与本人真实节点，不预设成年、出生、死亡或许可。"""
from .family_legacy import _id


def check_family_biological_legacy(pack, index):
    """作者只定义两种明确来源；真正出生者不放入初态替代运行身份。"""
    issues = []
    tables = {name: dict(index.get(name, {})) for name in ('actors', 'roles', 'dialogues')}
    for name, table in tables.items():
        for row in pack.get(name, []):
            if isinstance(row, dict) and _id(row.get('id')):
                table[row['id']] = row
    required = {'family', 'family_life_v1', 'family_birth_v1', 'family_legacy_v2',
                'family_biological_legacy_v1', 'native_dynamic_world'}
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'family_biological_legacy' not in scenario:
            continue
        path = ('scenarios', number, 'family_biological_legacy')
        def issue(code, message):
            """保留原作者路径；禁止对缺字段自动推断权限。"""
            issues.append((code, path, message))
        config = scenario['family_biological_legacy']
        if not isinstance(config, dict) or set(config) != {'version', 'profile', 'requests'}:
            issue('BIOLOGICAL_CONTROL_FIELDS', '只接受明确版本和本人请求，不接纳种子许可或资产')
            continue
        if type(config['version']) is not int or config['version'] != 1 or config['profile'] != 'biological_posthumous_control_v1':
            issue('BIOLOGICAL_CONTROL_PROFILE', '须显式选择亲生本人接续版本1')
        legacy = scenario.get('family_legacy')
        collateral = isinstance(scenario.get('family_collateral_legacy'), dict)
        selected_required = (required - {'family_legacy_v2'}) | ({'family_legacy_v3'} if collateral else {'family_legacy_v2'})
        if not isinstance(legacy, dict) or legacy.get('profile') != ('posthumous_control_sources_v3' if collateral else 'posthumous_control_sources_v2'):
            issue('BIOLOGICAL_CONTROL_LEGACY_VERSION', '须明确选择原唯一控制账版本2')
            legacy = {}
        if not selected_required <= set(pack.get('required_capabilities', [])) or any(not isinstance(scenario.get(k), dict) for k in ('family_life', 'family_birth', 'family_lineage', 'dynamic_world')):
            issue('BIOLOGICAL_CONTROL_DEPENDENCIES', '完整生命、出生谱系和版本2许可必须配对')
        rows = config['requests']
        if not isinstance(rows, list) or not 1 <= len(rows) <= 4096:
            issue('BIOLOGICAL_CONTROL_REQUESTS', '只选择有限且非空的本人政策')
            continue
        enabled = scenario.get('enabled', {})
        present = set(enabled.get('actors', [])) & {r.get('actor_id') for r in scenario.get('initial', {}).get('actors', []) if isinstance(r, dict)}
        dynamic = scenario.get('dynamic_world', {})
        born = dynamic.get('born_personal_dialogues', {}) if isinstance(dynamic, dict) else {}
        templates = born.get('templates', []) if isinstance(born, dict) else []
        lineage = scenario.get('family_lineage', {})
        lineage = lineage.get('origins', {}) if isinstance(lineage, dict) else {}
        ids, bindings = set(), set()
        for row in rows:
            shared = {'id', 'meeting_kind', 'accept_node_id', 'revoke_node_id', 'terms_version', 'scope'}
            kind = row.get('meeting_kind') if isinstance(row, dict) else None
            fields = shared | ({'predecessor_actor_id', 'successor_actor_id', 'dialogue_id'} if kind == 'installed_candidate' else {'template_id'} if kind == 'born_personal_template' else set())
            if not isinstance(row, dict) or kind not in ('installed_candidate', 'born_personal_template') or set(row) != fields or any(not _id(row[k]) for k in fields - {'terms_version'}):
                issue('BIOLOGICAL_CONTROL_REQUEST_FIELDS', '两种原来源字段不能混用或省略')
                continue
            if row['id'] in ids or row['id'] in legacy.get('request_ids', []):
                issue('BIOLOGICAL_CONTROL_REQUEST_COLLISION', '政策不能覆盖旧收养/弟子请求')
            ids.add(row['id'])
            if type(row['terms_version']) is not int or row['terms_version'] != 1 or row['scope'] != 'own_life_control_only' or row['accept_node_id'] == row['revoke_node_id']:
                issue('BIOLOGICAL_CONTROL_SCOPE', '本人接受和撤回须不同节点，仅授权自己的控制')
            child = row.get('successor_actor_id')
            if kind == 'installed_candidate':
                parent = row['predecessor_actor_id']
                origin = lineage.get(child, {}) if isinstance(lineage, dict) else {}
                if parent == child or not {parent, child} <= present or not isinstance(origin, dict) or parent not in origin.get('parent_actor_ids', []):
                    issue('BIOLOGICAL_CONTROL_INSTALLED_SOURCE', '原作者实际人物须有明确直接亲代记录')
                selected = row['dialogue_id']
                talk = tables['dialogues'].get(selected, {})
                role = tables['roles'].get(talk.get('context_role'), {})
                if not role or talk.get('context_role') not in enabled.get('roles', []):
                    issue('BIOLOGICAL_CONTROL_INSTALLED_MEETING', '须启用原本人会面岗位')
                selector = (kind, parent, child)
            else:
                selected_templates = [t for t in templates if isinstance(t, dict) and t.get('id') == row['template_id']]
                if len(selected_templates) != 1 or born.get('version') != 1 or 'born_personal_dialogues_v1' not in pack.get('required_capabilities', []):
                    issue('BIOLOGICAL_CONTROL_TEMPLATE_SOURCE', '须引用同场景唯一真实出生者私人模板')
                    continue
                selected = selected_templates[0].get('dialogue_id')
                talk = tables['dialogues'].get(selected, {})
                if talk.get('context_role'):
                    issue('BIOLOGICAL_CONTROL_TEMPLATE_ROLE', '出生者模板不替换任何岗位初始人')
                selector = (kind, row['template_id'])
            if selected not in enabled.get('dialogues', []):
                issue('BIOLOGICAL_CONTROL_DIALOGUE', '本人对白须实际启用')
            for field in ('accept_node_id', 'revoke_node_id'):
                nodes = [n for n in talk.get('nodes', []) if isinstance(n, dict) and n.get('id') == row[field]]
                speakers = ('$provider', child) if kind == 'installed_candidate' else ('$provider',)
                if len(nodes) != 1 or nodes[0].get('kind') != 'say' or nodes[0].get('speaker') not in speakers:
                    issue('BIOLOGICAL_CONTROL_PROVIDER_SAY', '须由实际候选本人发言，玩家选项或原型不能代签')
                binding = selector + (selected, row[field])
                if binding in bindings:
                    issue('BIOLOGICAL_CONTROL_DUPLICATE_NODE', '同一来源节点不能重复授权')
                bindings.add(binding)
    return issues
