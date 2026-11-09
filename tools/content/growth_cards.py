"""卡片作者语义只引用既有本领和真实情景初态，不授予能力或改写人物。"""

DEFINITION_FIELDS = {'id', 'category', 'name_key', 'description_key',
                     'prerequisites', 'acquire_condition', 'effect_kind',
                     'effect_params', 'hint_key', 'icon_id', 'repeat_policy'}


def check_growth_cards(pack, index):
    """独立叶供中央作者入口注册；其他五类效果仍明确未支持。"""
    issues = []
    definitions = dict(index.get('cards', {}))
    abilities = index.get('personal_skills', {})
    texts = index.get('texts', {})

    def issue(code, path, message):
        """沿原作者诊断合同返回稳定代码、准确位置和白话原因。"""
        issues.append((code, path, message))

    for n, card in enumerate(pack.get('cards', [])):
        path = ('cards', n)
        if not isinstance(card, dict) or set(card) != DEFINITION_FIELDS:
            issue('CARD_FIELDS', path, '卡片必须完整声明来源、资格、效果与展示字段')
            continue
        card_id = card.get('id')
        if not isinstance(card_id, str) or not card_id:
            issue('CARD_ID', path + ('id',), '卡片须有稳定ID')
            continue
        definitions[card_id] = card
        if card['category'] not in ('technique', 'heart'):
            issue('CARD_EFFECT_UNSUPPORTED', path + ('category',), '当前已接入武功与心法，其他卡片效果尚未实现')
        if card['prerequisites'] != {'kind': 'personal_ability_eligibility'}:
            issue('CARD_PREREQUISITE', path + ('prerequisites',), '卡片资格必须引用原本领资格，不接受自由条件脚本')
        if card['acquire_condition'] != {'kind': 'initial_or_completed_training'}:
            issue('CARD_ACQUISITION', path + ('acquire_condition',), '取得来源须为明确初态或成功完成的学习与传授')
        params = card.get('effect_params')
        if card['effect_kind'] != 'personal_ability' or not isinstance(params, dict) or set(params) != {'personal_skill_id'}:
            issue('CARD_EFFECT', path + ('effect_params',), '效果只引用已有个人本领，不能自由执行脚本或发资源')
            continue
        ability = abilities.get(params['personal_skill_id']) if isinstance(params['personal_skill_id'], str) else None
        if not ability or (ability.get('kind') == 'heart') != (card['category'] == 'heart'):
            issue('CARD_ABILITY', path + ('effect_params',), '卡片必须引用同一目录中类别相符的真实本领')
        if card['repeat_policy'] != 'once_per_actor':
            issue('CARD_REPEAT', path + ('repeat_policy',), '每人只记录一次首次取得，继续修炼沿原掌握度规则')
        for field in ('name_key', 'description_key', 'hint_key'):
            if not isinstance(card[field], str) or card[field] not in texts:
                issue('CARD_TEXT', path + (field,), '卡片展示文本须有实际定义')
        if not isinstance(card['icon_id'], str):
            issue('CARD_ICON', path + ('icon_id',), '图标引用须为字符串；未制作可明确留空')

    for n, scenario in enumerate(pack.get('scenarios', [])):
        config = scenario.get('growth_cards')
        if config is None:
            continue
        path = ('scenarios', n, 'growth_cards')
        if not isinstance(config, dict) or set(config) != {'profile', 'initial_grants'} or config.get('profile') != 'learned_ability_cards_v1':
            issue('CARDS_PROFILE', path, '卡片必须显式选择已实现profile与初态来源')
            continue
        grants = config.get('initial_grants')
        if not isinstance(grants, list) or len(grants) > 16384 or not 1 <= len(definitions) <= 4096:
            issue('CARD_CATALOG_SIZE', path, '卡片目录与初态来源超出有限范围')
            continue
        initial = {row['actor_id']: row for row in scenario.get('initial', {}).get('actors', []) if isinstance(row, dict) and isinstance(row.get('actor_id'), str)}
        enabled = set(scenario.get('enabled', {}).get('actors', []))
        seen = set()
        for number, grant in enumerate(grants):
            loc = path + ('initial_grants', number)
            if not isinstance(grant, dict) or set(grant) != {'actor_id', 'card_id'} or not all(isinstance(grant[k], str) for k in grant):
                issue('CARD_INITIAL_GRANT', loc, '初态来源须精确指明人物与卡片')
                continue
            key = (grant['actor_id'], grant['card_id'])
            card = definitions.get(key[1])
            actor = initial.get(key[0])
            if key in seen or not card or not actor or key[0] not in enabled:
                issue('CARD_INITIAL_GRANT', loc, '初态来源必须唯一且对应实际启用人物与卡片')
                continue
            seen.add(key)
            skill = card.get('effect_params', {}).get('personal_skill_id')
            learned = actor.get('combat', {}).get('learned', [])
            if not any(row.get('skill_id') == skill and type(row.get('mastery')) is int and 0 <= row['mastery'] <= 100 for row in learned if isinstance(row, dict)):
                issue('CARD_INITIAL_ABILITY', loc, '人物须在真实初态已学会该本领；卡片不能替其授予能力')
        for actor_id, actor in initial.items():
            if actor_id not in enabled:
                continue
            learned = {row.get('skill_id') for row in actor.get('combat', {}).get('learned', []) if isinstance(row, dict)}
            for card_id, card in definitions.items():
                if card.get('effect_params', {}).get('personal_skill_id') in learned and (actor_id, card_id) not in seen:
                    issue('CARD_INITIAL_SOURCE_REQUIRED', path, '已学卡片本领必须有明确初态来源：' + actor_id + ' / ' + card_id)
    return issues
