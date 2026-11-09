"""生前独立条件控制许可作者入口；不授遗产、代理或死亡知情。"""
PROFILE = 'authored_posthumous_control_consent_v1'
IDS = ('id', 'predecessor_actor_id', 'successor_actor_id', 'dialogue_id', 'accept_node_id', 'revoke_node_id')
FIELDS = set(IDS) | {'terms_version', 'consent_kind', 'scope'}


def _id(value):
    """永久来源必须是有限非空引用，不把对白内容解释为授权键。"""
    return isinstance(value, str) and 0 < len(value.encode('utf-8')) <= 256


def check_family_legacy(pack, index):
    """所选请求必须实际启用两个人物、岗位和各自明确的接受/撤回节点。"""
    issues = []
    tables = {key: dict(index.get(key, {})) for key in
              ('actors', 'roles', 'dialogues', 'legacy_control_consent_requests')}
    for key, table in tables.items():
        for row in pack.get(key, []):
            if isinstance(row, dict) and _id(row.get('id')):
                table[row['id']] = row

    def issue(code, path, message):
        """保留真实作者路径，通用编辑器可定位而不吞错。"""
        issues.append((code, path, message))

    def definition(value, path):
        """接受和撤回必须有不同节点，范围仅为本人原有人生控制。"""
        if not isinstance(value, dict) or set(value) != FIELDS or not all(_id(value[key]) for key in IDS):
            issue('LEGACY_FIELDS', path, '须完整声明双方与独立接受、撤回节点')
            return None
        if type(value['terms_version']) is not int or value['terms_version'] != 1 or value['consent_kind'] != 'successor_accepts_control_after_death_v1' or value['scope'] != 'own_life_control_only':
            issue('LEGACY_SCOPE', path, '只允许可撤回的条件控制许可，不包含遗产、代理或知情')
        if value['predecessor_actor_id'] == value['successor_actor_id'] or any(value[key] not in tables['actors'] for key in ('predecessor_actor_id', 'successor_actor_id')):
            issue('LEGACY_PARTIES', path, '双方须为不同真实人物')
        dialogue = tables['dialogues'].get(value['dialogue_id'])
        role = dialogue.get('context_role') if isinstance(dialogue, dict) else None
        if not _id(role) or role not in tables['roles']:
            issue('LEGACY_DIALOGUE', path, '许可须来自实际当面岗位对白')
            return None
        if value['accept_node_id'] == value['revoke_node_id']:
            issue('LEGACY_WITHDRAWAL', path, '接受和撤回不能共用同一个来源节点')
        for key in ('accept_node_id', 'revoke_node_id'):
            nodes = [node for node in dialogue.get('nodes', []) if isinstance(node, dict) and node.get('id') == value[key]]
            if len(nodes) != 1 or nodes[0].get('kind') != 'say' or nodes[0].get('speaker') not in ('$provider', value['successor_actor_id']):
                issue('LEGACY_CANDIDATE_SAY', path + (key,), '须由候选本人陈述；玩家选择或他人台词不能代签')
        return role

    declared = set()
    for number, value in enumerate(pack.get('legacy_control_consent_requests', [])):
        path = ('legacy_control_consent_requests', number)
        definition(value, path)
        if isinstance(value, dict) and _id(value.get('id')):
            if value['id'] in declared:
                issue('LEGACY_IDS', path, '请求不能重复')
            declared.add(value['id'])
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'family_legacy' not in scenario:
            continue
        path = ('scenarios', number, 'family_legacy')
        config = scenario['family_legacy']
        if not isinstance(config, dict) or set(config) != {'profile', 'request_ids'} or config.get('profile') not in (PROFILE, 'posthumous_control_sources_v2', 'posthumous_control_sources_v3'):
            issue('LEGACY_PROFILE', path, '须显式选择独立条件许可版本')
            continue
        biological = config['profile'] == 'posthumous_control_sources_v2'
        collateral = config['profile'] == 'posthumous_control_sources_v3'
        if collateral and not isinstance(scenario.get('family_collateral_legacy'), dict):
            issue('LEGACY_COLLATERAL_REQUIRED', path, '版本3须与旁支本人策略完整配对')
        if biological and not isinstance(scenario.get('family_biological_legacy'), dict):
            issue('LEGACY_BIOLOGICAL_REQUIRED', path, '版本2须与亲生本人策略完整配对')
        if not isinstance(scenario.get('dynamic_world'), dict) or not isinstance(scenario.get('family_life'), dict) or scenario['family_life'].get('profile') != 'verified_military_deaths_v1':
            issue('LEGACY_LIFE_REQUIRED', path, '须有同源动态世界与真实生命终点策略')
        if pack.get('required_capabilities') is not None and ('family_legacy_v3' if collateral else 'family_legacy_v2' if biological else 'family_legacy_v1') not in pack['required_capabilities']:
            issue('LEGACY_CAPABILITY', path, '须明确声明 family_legacy_v1，旧核心不能忽略')
        selected = config['request_ids']
        if not isinstance(selected, list) or not (0 if biological or collateral else 1) <= len(selected) <= 4096 or not all(_id(x) for x in selected) or len(selected) != len(set(selected)):
            issue('LEGACY_IDS', path, '须选择1至4096个唯一请求')
            continue
        enabled = scenario.get('enabled', {})
        present = {row.get('actor_id') for row in scenario.get('initial', {}).get('actors', []) if isinstance(row, dict)} & set(enabled.get('actors', []))
        bindings = set()
        for request_id in selected:
            value = tables['legacy_control_consent_requests'].get(request_id)
            at = path + ('request_ids', request_id)
            role = definition(value, at)
            if not isinstance(value, dict) or set(value) != FIELDS or not all(_id(value[key]) for key in IDS):
                continue
            if value['id'] != request_id or not {value['predecessor_actor_id'], value['successor_actor_id']} <= present:
                issue('LEGACY_ACTORS', at, '双方须实际启用且具有原初态，请求索引须一致')
            if role and role not in enabled.get('roles', []):
                issue('LEGACY_ROLE', at, '岗位须实际启用')
            if value['dialogue_id'] not in enabled.get('dialogues', []):
                issue('LEGACY_DIALOGUE_DISABLED', at, '本人对白须实际启用')
            for key in ('accept_node_id', 'revoke_node_id'):
                binding = tuple(value[k] for k in ('predecessor_actor_id', 'successor_actor_id', 'dialogue_id')) + (value[key],)
                if binding in bindings:
                    issue('LEGACY_BINDING', at, '相同双方与节点不能重复授权')
                bindings.add(binding)
    return issues
