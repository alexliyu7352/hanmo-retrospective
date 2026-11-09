"""旁支只定义分开的认可和本人许可节点，打开私人会面不泄露未知族谱。"""
from .family_legacy import _id


def check_family_collateral_legacy(pack, index):
    """同场景Known模板只提供真实说话人；血缘运行时另按原两支来源核验。"""
    issues = []
    talks = dict(index.get('dialogues', {}))
    for row in pack.get('dialogues', []):
        if isinstance(row, dict) and _id(row.get('id')):
            talks[row['id']] = row
    required = {'family', 'family_life_v1', 'family_legacy_v3',
                'family_collateral_legacy_v1', 'known_personal_dialogues_v1', 'native_dynamic_world'}
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'family_collateral_legacy' not in scenario:
            continue
        path = ('scenarios', number, 'family_collateral_legacy')
        def issue(code, message):
            """保留原作者定位，不通过默认字段修补未声明权源。"""
            issues.append((code, path, message))
        config = scenario['family_collateral_legacy']
        if not isinstance(config, dict) or set(config) != {'version', 'profile', 'requests'}:
            issue('COLLATERAL_CONTROL_FIELDS', '须明确版本、策略和有限请求，不接受初始认可或控制许可')
            continue
        if type(config['version']) is not int or config['version'] != 1 or config['profile'] != 'recognized_collateral_control_v1':
            issue('COLLATERAL_CONTROL_PROFILE', '须显式选择旁支本人许可版本1')
        legacy = scenario.get('family_legacy', {})
        dynamic = scenario.get('dynamic_world', {})
        known = dynamic.get('known_personal_dialogues', {}) if isinstance(dynamic, dict) else {}
        if not isinstance(known, dict):
            issue('COLLATERAL_CONTROL_DEPENDENCIES', '私人模板必须是显式目录对象')
            known = {}
        if not isinstance(legacy, dict) or legacy.get('profile') != 'posthumous_control_sources_v3':
            issue('COLLATERAL_CONTROL_LEGACY_VERSION', '须配对原唯一控制账版本3')
            legacy = {}
        if not required <= set(pack.get('required_capabilities', [])) or any(not isinstance(scenario.get(k), dict) for k in ('family_life', 'family_lineage', 'dynamic_world')) or known.get('version') != 1:
            issue('COLLATERAL_CONTROL_DEPENDENCIES', '真实生命、谱系、私人会面和版本3须完整配对')
        rows = config['requests']
        if not isinstance(rows, list) or not 1 <= len(rows) <= 4096:
            issue('COLLATERAL_CONTROL_REQUESTS', '须选择有限且非空的请求')
            continue
        templates = known.get('templates', [])
        bio = scenario.get('family_biological_legacy', {})
        if not isinstance(bio, dict):
            issue('COLLATERAL_CONTROL_DEPENDENCIES', '亲生并启来源必须是有效目录对象')
            bio = {}
        reserved = set(legacy.get('request_ids', [])) | {r.get('id') for r in bio.get('requests', []) if isinstance(r, dict)}
        ids, used = set(), set()
        fields = {'id', 'template_id', 'recognize_node_id', 'accept_node_id', 'revoke_node_id', 'terms_version', 'recognition_scope', 'control_scope'}
        for row in rows:
            if not isinstance(row, dict) or set(row) != fields or any(not _id(row[k]) for k in fields - {'terms_version'}):
                issue('COLLATERAL_CONTROL_REQUEST_FIELDS', '请求须保存三个不同本人节点，不混入继产或公职条款')
                continue
            if row['id'] in ids or row['id'] in reserved:
                issue('COLLATERAL_CONTROL_REQUEST_COLLISION', '不能覆盖旧收养、弟子或亲生许可请求')
            ids.add(row['id'])
            if type(row['terms_version']) is not int or row['terms_version'] != 1 or row['recognition_scope'] != 'mutual_collateral_recognition_only' or row['control_scope'] != 'own_life_control_only':
                issue('COLLATERAL_CONTROL_SCOPE', '旁支认可与本人控制许可必须分开，均不包含财产或职位')
            if len({row[k] for k in ('recognize_node_id', 'accept_node_id', 'revoke_node_id')}) != 3:
                issue('COLLATERAL_CONTROL_DISTINCT_DECLARATIONS', '认可、接受和撤回须是三句不同声明')
            selected = [t for t in templates if isinstance(t, dict) and t.get('id') == row['template_id']]
            if len(selected) != 1:
                issue('COLLATERAL_CONTROL_TEMPLATE', '须引用本场景唯一真实私人模板')
                continue
            talk = talks.get(selected[0].get('dialogue_id'), {})
            if talk.get('context_role') or selected[0].get('dialogue_id') not in scenario.get('enabled', {}).get('dialogues', []):
                issue('COLLATERAL_CONTROL_PRIVATE_DIALOGUE', '只能使用实际启用的无岗位私人对白')
            for key in ('recognize_node_id', 'accept_node_id', 'revoke_node_id'):
                nodes = [n for n in talk.get('nodes', []) if isinstance(n, dict) and n.get('id') == row[key]]
                if len(nodes) != 1 or nodes[0].get('kind') != 'say' or nodes[0].get('speaker') != '$provider':
                    issue('COLLATERAL_CONTROL_PROVIDER_SAY', '真实提供者必须本人发言，选项或玩家台词不能代替')
                binding = (row['template_id'], row[key])
                if binding in used:
                    issue('COLLATERAL_CONTROL_DUPLICATE_NODE', '同一真实本人节点不能重复承担不同条款')
                used.add(binding)
    return issues
