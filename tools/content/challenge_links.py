"""礼议定义的有限顺序条件与课程引用校验；不计算游戏奖励或执行作者表达式。"""


def check_challenges(pack: dict, index: dict) -> list:
    """Schema之后核唯一局部ID、只向前引用和课程技能边界，返回真实作者路径。"""
    issues = []
    for number, definition in enumerate(pack.get('etiquette_challenges', [])):
        base = ('etiquette_challenges', number)
        earlier = {}
        for offset, node in enumerate(definition['nodes']):
            at = base + ('nodes', offset)
            if node['id'] in earlier:
                issues.append(('ETIQUETTE_NODE_DUPLICATE', at + ('id',), '节点ID必须在当前题面唯一'))
            options = set()
            for position, option in enumerate(node['options']):
                path = at + ('options', position)
                if option['id'] in options:
                    issues.append(('ETIQUETTE_OPTION_DUPLICATE', path + ('id',), '选项ID必须在当前节点唯一'))
                options.add(option['id'])
                seen = set()
                for entry, condition in enumerate(option['requires_choices']):
                    if condition['node_id'] in seen or condition['option_id'] not in earlier.get(condition['node_id'], set()):
                        issues.append(('ETIQUETTE_CONDITION', path + ('requires_choices', entry), '条件只可引用前面节点的一条真实选项'))
                    seen.add(condition['node_id'])
            for key in [node['topic_key'], node['stance_key'], *(row['text_key'] for row in node['options'])]:
                text = pack['texts'].get(key)
                if not isinstance(text, str) or not text or len(text) > 4096:
                    issues.append(('ETIQUETTE_TEXT', at, '礼议公开文本必须非空且不超过4096字符'))
            earlier[node['id']] = options
    for number, definition in enumerate(pack.get('bargain_challenges', [])):
        issues.extend(_bargain(definition, pack['texts'], ('bargain_challenges', number)))
    for number, role in enumerate(pack.get('roles', [])):
        for offset, course in enumerate(role.get('skill_courses', [])):
            if 'challenge' in course:
                challenge = course['challenge']; kind = challenge['kind']
                allowed = ('rites', 'debate') if kind == 'etiquette' else ('commerce', 'negotiation')
                if course['skill_id'] not in allowed or challenge['id'] not in index.get(kind + '_challenges', {}):
                    issues.append(('STUDY_CHALLENGE_REFERENCE', ('roles', number, 'skill_courses', offset, 'challenge'), '课程必须引用对应技能的真实练习题面'))
            if 'etiquette_challenge_id' not in course:
                continue
            path = ('roles', number, 'skill_courses', offset, 'etiquette_challenge_id')
            if course['skill_id'] not in ('rites', 'debate') or course['etiquette_challenge_id'] not in index.get('etiquette_challenges', {}):
                issues.append(('STUDY_CHALLENGE_REFERENCE', path, '礼议题面仅供礼学或辩说的真实课程引用'))
    for number, shop in enumerate(pack.get('shops', [])):
        if 'bargaining' not in shop:
            continue
        policy = shop['bargaining']
        definition = index.get('bargain_challenges', {}).get(policy['challenge_id'], {})
        conditions = {row['id']: row['kind'] for row in definition.get('conditions', [])}
        seen = set()
        for source in policy['condition_sources']:
            identifier = source['condition_id']
            if identifier in seen or identifier not in conditions or (source['source'] == 'stock_covers_quantity' and conditions.get(identifier) != 'supply') or (source['source'] == 'cash_payment' and conditions.get(identifier) != 'terms'):
                issues.append(('BARGAIN_CONDITION_SOURCE', ('shops', number, 'bargaining'), '每条公开条件必须唯一对应正确类别的当前可见来源'))
            seen.add(identifier)
        if seen != set(conditions):
            issues.append(('BARGAIN_CONDITION_SOURCE', ('shops', number, 'bargaining'), '条件来源须覆盖该题面全部条件'))
    return issues


def _bargain(definition: dict, texts: dict, base: tuple) -> list:
    """只核公开条件的局部类型和唯一引用，不把作者练习布尔当真实交易供需。"""
    issues, conditions, rounds = [], {}, set()
    methods = {'quantity': 'small_quantity', 'batch': 'combined_purchase', 'payment': 'cash'}
    text_keys = []
    for number, condition in enumerate(definition['conditions']):
        if condition['id'] in conditions:
            issues.append(('BARGAIN_CONDITION_DUPLICATE', base + ('conditions', number), '同题面条件ID不能重复'))
        conditions[condition['id']] = condition
        text_keys.append(condition['text_key'])
    for number, row in enumerate(definition['rounds']):
        path = base + ('rounds', number)
        if row['id'] in rounds:
            issues.append(('BARGAIN_ROUND_DUPLICATE', path, '同题面轮次ID不能重复'))
        rounds.add(row['id']); text_keys.append(row['prompt_key'])
        ids, kinds = set(), set()
        for offset, option in enumerate(row['options']):
            at = path + ('options', offset)
            if option['id'] in ids or option['kind'] in kinds or option['method'] != methods[option['kind']]:
                issues.append(('BARGAIN_OPTION', at, '每轮三种明确提议各一项，方法必须匹配'))
            ids.add(option['id']); kinds.add(option['kind']); text_keys.append(option['text_key'])
            for kind in ('supply', 'terms'):
                reference = option[kind + '_condition_id']
                if reference is not None and conditions.get(reference, {}).get('kind') != kind:
                    issues.append(('BARGAIN_CONDITION_REFERENCE', at + (kind + '_condition_id',), '只可引用同题面对应类别条件'))
    if any(not isinstance(texts.get(key), str) or not texts[key] or len(texts[key]) > 4096 for key in text_keys):
        issues.append(('BARGAIN_TEXT', base, '公开题面文字必须非空且不超过4096字符'))
    return issues
