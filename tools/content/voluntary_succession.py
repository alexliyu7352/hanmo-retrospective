"""作者明确声明接续人接受的分项方案；收养、同姓与一般对白不隐含接续许可。"""

PROFILE = 'authored_voluntary_succession_v1'
IDS = ('id', 'predecessor_actor_id', 'successor_actor_id', 'dialogue_id', 'node_id')
FIELDS = set(IDS) | {'terms_version', 'consent_kind', 'scope', 'personal'}


def _id(value):
    """持久引用限制为非空有限UTF-8，不接受任意对象当作来源。"""
    return isinstance(value, str) and 0 < len(value.encode('utf-8')) <= 256


def check_voluntary_succession(pack, index):
    """逐项检查情景实际启用的人物/节点及明确私产条款，不在作者准入时执行交割。"""
    issues = []
    tables = {key: dict(index.get(key, {})) for key in
              ('actors', 'items', 'roles', 'dialogues', 'succession_consent_requests')}
    for key, table in tables.items():
        for row in pack.get(key, []):
            if isinstance(row, dict) and _id(row.get('id')):
                table[row['id']] = row

    def issue(code, path, text):
        """诊断保实际作者路径，供通用编辑器定位，不吞掉后续坏候选。"""
        issues.append((code, path, text))

    def definition(value, path):
        """同意语义固定到具体接续人、原节点及所选钱货。"""
        if not isinstance(value, dict) or set(value) != FIELDS or not all(_id(value[key]) for key in IDS):
            issue('SUCCESSION_FIELDS', path, '须完整声明双方、节点与分项接续方案')
            return None
        if type(value['terms_version']) is not int or value['terms_version'] != 1 or value['consent_kind'] != 'successor_accepts_voluntary_succession_v1' or value['scope'] != 'player_control_and_selected_personal_assets':
            issue('SUCCESSION_CONSENT_SCOPE', path, '只接受明确接续控制与列出的私产；不包含公职或全部家产')
        if value['predecessor_actor_id'] == value['successor_actor_id'] or any(value[key] not in tables['actors'] for key in ('predecessor_actor_id', 'successor_actor_id')):
            issue('SUCCESSION_PARTIES', path, '双方须是不同的真实作者人物')
        personal = value['personal']
        if not isinstance(personal, dict) or set(personal) != {'money', 'goods'} or type(personal.get('money')) is not int or not 0 <= personal['money'] <= 9223372036854775807 or not isinstance(personal.get('goods'), list) or len(personal['goods']) > 4096:
            issue('SUCCESSION_PERSONAL', path + ('personal',), '只声明有限非负私钱与明确品类数量')
        else:
            seen = set()
            for number, good in enumerate(personal['goods']):
                if not isinstance(good, dict) or set(good) != {'item_id', 'quantity'} or not _id(good.get('item_id')) or good['item_id'] not in tables['items'] or type(good.get('quantity')) is not int or not 1 <= good['quantity'] <= 9223372036854775807 or good['item_id'] in seen:
                    issue('SUCCESSION_GOODS', path + ('personal', 'goods', number), '品类须存在且只列一次，数量为正整数')
                elif good['item_id']:
                    seen.add(good['item_id'])
        dialogue = tables['dialogues'].get(value['dialogue_id'])
        role = dialogue.get('context_role') if isinstance(dialogue, dict) else None
        if not _id(role) or role not in tables['roles']:
            issue('SUCCESSION_DIALOGUE', path, '本人同意须来自实际当面岗位对白')
            return None
        nodes = [node for node in dialogue.get('nodes', []) if isinstance(node, dict) and node.get('id') == value['node_id']]
        if len(nodes) != 1 or nodes[0].get('kind') != 'say' or nodes[0].get('speaker') not in ('$provider', value['successor_actor_id']):
            issue('SUCCESSION_CANDIDATE_SAY', path, '只接受候选本人陈述；玩家选择或他人台词不能代签分项方案')
        return role

    declared = set()
    for number, value in enumerate(pack.get('succession_consent_requests', [])):
        path = ('succession_consent_requests', number)
        definition(value, path)
        if isinstance(value, dict) and _id(value.get('id')):
            if value['id'] in declared:
                issue('SUCCESSION_IDS', path, '请求ID不能重复')
            declared.add(value['id'])
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'voluntary_succession' not in scenario:
            continue
        path = ('scenarios', number, 'voluntary_succession')
        config = scenario['voluntary_succession']
        if not isinstance(config, dict) or set(config) != {'profile', 'request_ids'} or config.get('profile') not in (PROFILE, 'living_accepted_family_paths_v2'):
            issue('SUCCESSION_PROFILE', path, '须显式选择自愿接续方案版本')
            continue
        if (config.get('profile') == 'living_accepted_family_paths_v2') != ('family_voluntary_paths' in scenario):
            issue('SUCCESSION_LIVING_PAIR', path, '新版接续须与独立生前来源配对')
        if not isinstance(scenario.get('dynamic_world'), dict):
            issue('SUCCESSION_NATIVE', path, '本方案需要真实原生世界')
        selected = config['request_ids']
        if not isinstance(selected, list) or not (0 if config.get('profile') == 'living_accepted_family_paths_v2' else 1) <= len(selected) <= 4096 or not all(_id(x) for x in selected) or len(selected) != len(set(selected)):
            issue('SUCCESSION_IDS', path, '须选择1至4096个唯一请求')
            continue
        enabled = scenario.get('enabled', {})
        present = {row.get('actor_id') for row in scenario.get('initial', {}).get('actors', []) if isinstance(row, dict)} & set(enabled.get('actors', []))
        bindings = set()
        for request_id in selected:
            value = tables['succession_consent_requests'].get(request_id)
            at = path + ('request_ids', request_id)
            role = definition(value, at)
            if not isinstance(value, dict) or set(value) != FIELDS or not all(_id(value[key]) for key in IDS):
                continue
            if value['id'] != request_id:
                issue('SUCCESSION_IDS', at, '索引ID须与本身一致')
            if not {value['predecessor_actor_id'], value['successor_actor_id']} <= present:
                issue('SUCCESSION_ACTORS', at, '双方须实际启用且具有原初态')
            if role and role not in enabled.get('roles', []):
                issue('SUCCESSION_ROLE', at, '接受方案岗位须实际启用')
            if value['dialogue_id'] not in enabled.get('dialogues', []):
                issue('SUCCESSION_DIALOGUE_DISABLED', at, '接受方案的对白须实际启用')
            binding = tuple(value[key] for key in ('predecessor_actor_id', 'successor_actor_id', 'dialogue_id', 'node_id'))
            if binding in bindings:
                issue('SUCCESSION_BINDING', at, '相同双方与节点不能重复提供接受来源')
            bindings.add(binding)
    return issues
