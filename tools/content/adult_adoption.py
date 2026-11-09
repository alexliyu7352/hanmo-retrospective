"""成年收养的具名自愿来源；目录本身不成立收养、继任或资产转移。"""

PROFILE = 'authored_adult_adoption_requests_v1'
IDS = ('id', 'adopter_actor_id', 'adoptee_actor_id', 'dialogue_id', 'node_id')
FIELDS = set(IDS) | {'terms_version', 'consent_kind', 'scope'}


def _id(value):
    return isinstance(value, str) and 0 < len(value.encode('utf-8')) <= 256


def check_adult_adoption(pack, index):
    issues = []
    tables = {key: dict(index.get(key, {})) for key in
              ('actors', 'roles', 'dialogues', 'adult_adoption_requests')}
    for key, table in tables.items():
        for value in pack.get(key, []):
            if isinstance(value, dict) and _id(value.get('id')):
                table[value['id']] = value

    def issue(code, path, text):
        issues.append((code, path, text))

    def definition(value, path):
        if not isinstance(value, dict) or set(value) != FIELDS or not all(_id(value[key]) for key in IDS):
            issue('ADULT_ADOPTION_FIELDS', path, '请求须完整绑定双方、对白、节点与明确同意范围')
            return None
        if type(value['terms_version']) is not int or value['terms_version'] != 1 or value['consent_kind'] != 'adoptee_requests_adoption_v1' or value['scope'] != 'adult_adoption_only':
            issue('ADULT_ADOPTION_CONSENT_SCOPE', path, '本版只承认被收养人明确请求收养，不包含继任或财物处分')
        if value['adopter_actor_id'] == value['adoptee_actor_id'] or any(value[key] not in tables['actors'] for key in ('adopter_actor_id', 'adoptee_actor_id')):
            issue('ADULT_ADOPTION_PARTIES', path, '双方须是两个已经存在的真实作者人物')
        dialogue = tables['dialogues'].get(value['dialogue_id'])
        role = dialogue.get('context_role') if isinstance(dialogue, dict) else None
        if not _id(role) or role not in tables['roles']:
            issue('ADULT_ADOPTION_DIALOGUE', path, '请求须出现在真实普通岗位的当面对话中')
            return None
        nodes = [node for node in dialogue.get('nodes', []) if isinstance(node, dict) and node.get('id') == value['node_id']]
        if len(nodes) != 1 or nodes[0].get('kind') not in ('say', 'choice'):
            issue('ADULT_ADOPTION_NODE', path, '请求只绑定已到达的陈述或选择节点')
        return role

    declared = set()
    for number, value in enumerate(pack.get('adult_adoption_requests', [])):
        path = ('adult_adoption_requests', number)
        definition(value, path)
        if isinstance(value, dict) and _id(value.get('id')):
            if value['id'] in declared:
                issue('ADULT_ADOPTION_IDS', path, '请求ID不能重复')
            declared.add(value['id'])

    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'adult_adoption' not in scenario:
            continue
        path = ('scenarios', number, 'adult_adoption')
        config = scenario['adult_adoption']
        if not isinstance(config, dict) or set(config) != {'profile', 'request_ids'} or config.get('profile') != PROFILE:
            issue('ADULT_ADOPTION_PROFILE', path, '情景须明确启用成年收养请求版本')
            continue
        if not isinstance(scenario.get('dynamic_world'), dict):
            issue('ADULT_ADOPTION_NATIVE', path, '成年收养请求需要真实原生世界')
        selected = config['request_ids']
        if not isinstance(selected, list) or not 1 <= len(selected) <= 4096 or not all(_id(value) for value in selected) or len(selected) != len(set(selected)):
            issue('ADULT_ADOPTION_IDS', path, '须明确选择1至4096个不同请求')
            continue
        enabled = scenario.get('enabled', {})
        present = {value.get('actor_id') for value in scenario.get('initial', {}).get('actors', []) if isinstance(value, dict)}
        actual = present & set(enabled.get('actors', []))
        bindings = set()
        for request_id in selected:
            at = path + ('request_ids', request_id)
            value = tables['adult_adoption_requests'].get(request_id)
            if value is None:
                issue('ADULT_ADOPTION_IDS', at, '所选请求缺少定义')
                continue
            role = definition(value, at)
            if not isinstance(value, dict) or set(value) != FIELDS or not all(_id(value[key]) for key in IDS):
                continue
            if value['id'] != request_id:
                issue('ADULT_ADOPTION_IDS', at, '索引ID必须与请求本身一致')
            if not {value['adopter_actor_id'], value['adoptee_actor_id']} <= actual:
                issue('ADULT_ADOPTION_ACTORS', at, '双方须在本情景实际启用且拥有原初态')
            if role and role not in enabled.get('roles', []):
                issue('ADULT_ADOPTION_ROLE', at, '请求岗位须实际启用')
            if value['dialogue_id'] not in enabled.get('dialogues', []):
                issue('ADULT_ADOPTION_DIALOGUE_DISABLED', at, '请求对白须实际启用')
            binding = tuple(value[key] for key in ('adopter_actor_id', 'adoptee_actor_id', 'dialogue_id', 'node_id'))
            if binding in bindings:
                issue('ADULT_ADOPTION_BINDING', at, '同一双方和节点不能重复声明请求')
            bindings.add(binding)
    return issues
