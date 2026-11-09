"""个人婚議作者边界：旧处境未知不补未婚，本人意愿不携带财产或照护处分。"""


def check_family_marriage(pack, index):
    """严格核明确策略、双方原处境与同场节点引用；来源到达及本人接受由原生办理。"""
    issues = []
    tables = {}
    for name in ('actors', 'roles', 'dialogues'):
        supplied = index.get(name, {})
        tables[name] = dict(supplied) if isinstance(supplied, dict) else {row['id']: row for row in supplied if isinstance(row, dict) and isinstance(row.get('id'), str)}
        for row in pack.get(name, []):
            if isinstance(row, dict) and isinstance(row.get('id'), str):
                tables[name][row['id']] = row
    required = {'family', 'family_marriage_v1', 'family_life_v1', 'native_dynamic_world', 'dialogue'}
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'family_marriage' not in scenario:
            continue
        path = ('scenarios', number, 'family_marriage')
        def issue(code, detail):
            """定位本情景策略，不把错误折成允许默认值。"""
            issues.append((code, path, detail))
        config = scenario['family_marriage']
        if not isinstance(config, dict) or set(config) != {'version', 'profile', 'origins', 'statements'}:
            issue('MARRIAGE_FIELDS', '婚议须有完整原处境和意愿表，不接受种子婚姻或额外条款')
            continue
        if type(config['version']) is not int or config['version'] != 1 or config['profile'] != 'personal_marriage_separate_assets_v1':
            issue('MARRIAGE_PROFILE', '只支持明确个人关系、原财产分离的版本1策略')
        if not required <= set(pack.get('required_capabilities', [])):
            issue('MARRIAGE_CAPABILITY', '须明确启用婚议、原生命及原生动态世界能力')
        if scenario.get('family_life') != {'profile': 'verified_military_deaths_v1', 'player_death': 'await_explicit_continuation'}:
            issue('MARRIAGE_LIFE', '婚姻死亡边界须使用原真实生命来源')
        origins, statements = config['origins'], config['statements']
        if not isinstance(origins, dict) or not 1 <= len(origins) <= 4096 or not isinstance(statements, dict) or not 1 <= len(statements) <= 4096:
            issue('MARRIAGE_TABLES', '原处境和意愿须为非空有限表')
            continue
        for actor, origin in origins.items():
            if not isinstance(origin, dict) or not {'actor_id', 'status'} <= set(origin) <= {'actor_id', 'status', 'spouse_actor_id'} or origin.get('actor_id') != actor or origin.get('status') not in {'unknown', 'unmarried', 'married'}:
                issue('MARRIAGE_ORIGIN', '原人物处境不得省略、猜测或附加未知语义')
                continue
            if (origin['status'] == 'married') != ('spouse_actor_id' in origin):
                issue('MARRIAGE_ORIGIN_SPOUSE', '已婚与原配偶来源须同时提供')
            if origin['status'] == 'married':
                spouse = origin.get('spouse_actor_id')
                other = origins.get(spouse, {}) if isinstance(spouse, str) else {}
                if spouse == actor or not isinstance(other, dict) or other.get('status') != 'married' or other.get('spouse_actor_id') != actor:
                    issue('MARRIAGE_ORIGIN_RECIPROCAL', '原配偶必须双方互指，不能单方创设关系')
        bindings = set()
        for key, statement in statements.items():
            fields = {'id', 'speaker_actor_id', 'listener_actor_id', 'dialogue_id', 'node_id', 'intent', 'terms_version', 'scope'}
            if not isinstance(statement, dict) or set(statement) != fields:
                issue('MARRIAGE_STATEMENT_FIELDS', '意愿只表达具名个人婚议，不接受财物、迁居或生育附权')
                continue
            if any(not isinstance(statement[k], str) or not 1 <= len(statement[k]) <= 256 for k in fields - {'terms_version'}):
                issue('MARRIAGE_STATEMENT_REFERENCE', '所有人物和节点引用必须明确且有限')
                continue
            if statement['id'] != key or type(statement['terms_version']) is not int or statement['terms_version'] != 1 or statement['scope'] != 'personal_marriage_separate_assets' or statement['intent'] not in {'offer_marriage', 'accept_dissolution'}:
                issue('MARRIAGE_STATEMENT_SCOPE', '未知版本或范围不能默认成本人同意')
            speaker, listener = statement['speaker_actor_id'], statement['listener_actor_id']
            if speaker == listener or speaker not in origins or listener not in origins:
                issue('MARRIAGE_PARTIES', '须有不同当事人及各自原处境')
            binding = (speaker, listener, statement['dialogue_id'], statement['node_id'])
            if binding in bindings:
                issue('MARRIAGE_DUPLICATE_NODE', '同一当事人节点不能同时表达两份不同意愿')
            bindings.add(binding)
            if index:
                dialogues = tables['dialogues']
                dialogue = dialogues.get(statement['dialogue_id'])
                node = next((n for n in dialogue.get('nodes', []) if n.get('id') == statement['node_id']), None) if dialogue else None
                if not dialogue or not node or node.get('kind') not in {'say', 'choice'}:
                    issue('MARRIAGE_DIALOGUE', '意愿须来自真实普通会话的指定节点')
    return issues
