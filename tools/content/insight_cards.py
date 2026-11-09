"""本人见闻作者定义；卡片只引用既有完整经历，不执行效果脚本或发训练奖励。"""

FIELDS = {'id', 'category', 'name_key', 'description_key', 'hint_key', 'icon_id',
          'prerequisites', 'acquire_condition', 'effect_kind', 'effect_params', 'repeat_policy'}
PROFILE = 'completed_camp_training_insights_v1'


def _id(value):
    return isinstance(value, str) and 0 < len(value.encode('utf-8')) <= 256


def check_insight_cards(pack, index):
    """仅场景显式选择的定义进入运行目录；已声明定义仍按固定作者合同诊断。"""
    issues = []
    definitions = dict(index.get('insight_cards', {}))
    texts = dict(index.get('texts', {}))
    for text in pack.get('texts', []):
        if isinstance(text, dict) and _id(text.get('id')):
            texts[text['id']] = text

    def issue(code, path, message):
        issues.append((code, path, message))

    def definition(card, path):
        if not isinstance(card, dict) or set(card) != FIELDS or not _id(card.get('id')):
            issue('INSIGHT_CARD_FIELDS', path, '见闻卡须完整声明稳定ID、真实来源、效果与展示字段')
            return False
        if card['category'] != 'insight' or card['effect_kind'] != 'record_insight' or card['effect_params'] != {'kind': 'camp_training'}:
            issue('INSIGHT_CARD_EFFECT', path, '本版见闻只记录真实军营参训经历，不授技能或任职')
        if card['prerequisites'] != {'kind': 'current_player_participation'}:
            issue('INSIGHT_CARD_PREREQUISITE', path + ('prerequisites',), '须由当前本人实际参与，不能借教官身份')
        if card['acquire_condition'] != {'kind': 'completed_camp_training_v1'}:
            issue('INSIGHT_CARD_CONDITION', path + ('acquire_condition',), '本版只接已合法完成军营训练的永久事实')
        if card['repeat_policy'] != 'once_per_actor_world':
            issue('INSIGHT_CARD_REPEAT', path + ('repeat_policy',), '同世界同人物保留最早真实经历')
        for key in ('name_key', 'description_key', 'hint_key'):
            if not _id(card[key]) or card[key] not in texts:
                issue('INSIGHT_CARD_TEXT', path + (key,), '见闻展示文本须有实际定义')
        if not isinstance(card['icon_id'], str) or len(card['icon_id'].encode('utf-8')) > 256:
            issue('INSIGHT_CARD_ICON', path + ('icon_id',), '图标须为有限字符串，未制作时可明确留空')
        return True

    declared = set()
    for number, card in enumerate(pack.get('insight_cards', [])):
        path = ('insight_cards', number)
        definition(card, path)
        if isinstance(card, dict) and _id(card.get('id')):
            if card['id'] in declared:
                issue('INSIGHT_CARD_IDS', path, '见闻稳定ID不能重复')
            declared.add(card['id'])
            definitions[card['id']] = card

    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'insight_cards' not in scenario:
            continue
        config = scenario['insight_cards']
        path = ('scenarios', number, 'insight_cards')
        if not isinstance(config, dict) or set(config) != {'profile', 'card_ids'} or config.get('profile') != PROFILE:
            issue('INSIGHT_CARDS_PROFILE', path, '须显式选择本版见闻profile与卡片名单')
            continue
        if not isinstance(scenario.get('dynamic_world'), dict):
            issue('INSIGHT_CARDS_NATIVE', path, '见闻经历须来自正式原生世界，不向旧GD场景隐式启用')
            continue
        selected = config['card_ids']
        if not isinstance(selected, list) or not 1 <= len(selected) <= 4096 or not all(_id(value) for value in selected) or len(selected) != len(set(selected)):
            issue('INSIGHT_CARD_IDS', path + ('card_ids',), '见闻名单须有1至4096个不同稳定ID')
            continue
        conditions = set()
        for card_id in selected:
            card = definitions.get(card_id)
            if card is None:
                issue('INSIGHT_CARD_IDS', path + ('card_ids', card_id), '缺少所选见闻定义')
                continue
            if not definition(card, path + ('card_ids', card_id)):
                continue
            if card['id'] != card_id:
                issue('INSIGHT_CARD_IDS', path + ('card_ids', card_id), '定义字典ID与稳定ID必须相同')
            condition = card['acquire_condition']
            if condition == {'kind': 'completed_camp_training_v1'}:
                if 'completed_camp_training_v1' in conditions:
                    issue('INSIGHT_CARD_DUPLICATE_CONDITION', path, '同一经历不能复制为多张同效见闻')
                conditions.add('completed_camp_training_v1')
    return issues
