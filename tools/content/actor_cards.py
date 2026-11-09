"""人物卡作者叶；显式场景政策只增加新局收藏资格。"""
FIELDS = {'id', 'category', 'name_key', 'description_key', 'hint_key', 'icon_id',
          'prerequisites', 'acquire_condition', 'effect_kind', 'effect_params', 'repeat_policy'}

def _id(value):
    return isinstance(value, str) and 0 < len(value.encode('utf-8')) <= 256

def _ids(value):
    return isinstance(value, list) and 1 <= len(value) <= 4096 and all(_id(item) for item in value) and len(value) == len(set(value))

def check_actor_cards(pack, index):
    """沿现作者诊断格式返回闭合字段和真实人物引用错误；不授予资格。"""
    issues = []
    definitions = dict(index.get('actor_cards', {}))
    actors, texts = index.get('actors', {}), index.get('texts', {})
    def issue(code, path, message):
        issues.append((code, path, message))
    def definition(card, path, version=None):
        if not isinstance(card, dict) or set(card) != FIELDS or not _id(card.get('id')):
            issue('ACTOR_CARD_FIELDS', path, '人物卡必须完整声明稳定ID、来源、资格、效果与展示字段')
            return None
        if card['category'] != 'actor' or card['effect_kind'] != 'unlock_actor':
            issue('ACTOR_CARD_EFFECT', path, '人物卡只授新局收藏资格，不能在本局直接控制人物')
        if card['prerequisites'] != {'kind': 'current_player_completed_tasks'}:
            issue('ACTOR_CARD_PREREQUISITE', path + ('prerequisites',), '人物卡必须引用当前人物的真实已完成委托')
        conditions = [{'kind': 'completed_procurement_twice_v1'}]
        if version in (None, 2): conditions.append({'kind': 'completed_envoy_round_trip_v1'})
        if card['acquire_condition'] not in conditions:
            issue('ACTOR_CARD_CONDITION', path + ('acquire_condition',), '条件必须由显式人物卡版本支持：两次采购或一次真实交涉往返')
        if card['repeat_policy'] != 'once_per_actor_profile':
            issue('ACTOR_CARD_REPEAT', path + ('repeat_policy',), '跨局账保留人物首次取得来源')
        effect = card['effect_params']
        target = effect.get('target_actor_id') if isinstance(effect, dict) else None
        if not isinstance(effect, dict) or set(effect) != {'target_actor_id'} or not _id(target) or target not in actors:
            issue('ACTOR_CARD_TARGET', path + ('effect_params',), '人物卡须引用真实作者人物')
            target = None
        for field in ('name_key', 'description_key', 'hint_key'):
            if not _id(card[field]) or card[field] not in texts:
                issue('ACTOR_CARD_TEXT', path + (field,), '人物卡展示文本须有实际定义')
        if not isinstance(card['icon_id'], str):
            issue('ACTOR_CARD_ICON', path + ('icon_id',), '图标须为字符串，未制作时明确留空')
        return target

    declared = set()
    for number, card in enumerate(pack.get('actor_cards', [])):
        path = ('actor_cards', number)
        definition(card, path)
        if isinstance(card, dict) and _id(card.get('id')):
            if card['id'] in declared:
                issue('ACTOR_CARD_IDS', path, '人物卡定义ID不能重复')
            declared.add(card['id'])
            definitions[card['id']] = card
    for number, scenario in enumerate(pack.get('scenarios', [])):
        config = scenario.get('actor_card_unlocks')
        if config is None:
            continue
        path = ('scenarios', number, 'actor_card_unlocks')
        if not isinstance(config, dict) or set(config) != {'profile', 'card_ids', 'initially_available_actor_ids'} or config.get('profile') not in ('completed_procurement_actor_cards_v1', 'completed_task_actor_cards_v2'):
            issue('ACTOR_CARDS_PROFILE', path, '人物卡须显式选择已实现的版本与初始可选人物')
            continue
        version = 2 if config['profile'] == 'completed_task_actor_cards_v2' else 1
        selected, initial, roster = config['card_ids'], config['initially_available_actor_ids'], scenario.get('playable_actor_ids')
        if scenario.get('dynamic_world', {}).get('founder_policy') not in ('selected_existing_actor_v1', 'selected_existing_actor_roster_v1'):
            issue('ACTOR_CARDS_ROSTER_POLICY', path, '人物卡只能叠加显式现存人物选择政策')
            continue
        if not _ids(selected):
            issue('ACTOR_CARD_IDS', path + ('card_ids',), '所选人物卡须为1至4096个唯一ID')
            continue
        if not _ids(roster) or not _ids(initial) or not set(initial) <= set(roster) or scenario.get('player_actor') not in initial:
            issue('ACTOR_CARD_INITIAL', path + ('initially_available_actor_ids',), '初始可选人物须来自原名单且包含原默认人物')
            continue
        targets = set()
        enabled = set(scenario.get('enabled', {}).get('actors', []))
        actual = {row.get('actor_id') for row in scenario.get('initial', {}).get('actors', []) if isinstance(row, dict)}
        for card_id in selected:
            card = definitions.get(card_id)
            if not card:
                issue('ACTOR_CARD_IDS', path + ('card_ids',), '所选人物卡缺少实际定义：' + card_id)
                continue
            target = definition(card, path + ('card_ids', card_id), version)
            if card.get('acquire_condition') == {'kind': 'completed_envoy_round_trip_v1'}:
                diplomacy = scenario.get('dynamic_world', {}).get('diplomacy', {})
                if diplomacy.get('enabled') is not True or diplomacy.get('version') != 2:
                    issue('ACTOR_CARD_ENVOY_PROFILE', path + ('card_ids', card_id), '交涉往返来源必须启用同世界外交2与实际返报')
            if target not in roster or target not in enabled or target not in actual:
                issue('ACTOR_CARD_TARGET', path + ('card_ids', card_id), '目标须属于本情景实际启用与初态名单')
            if target in targets:
                issue('ACTOR_CARD_TARGET', path + ('card_ids', card_id), '每个人物只能有一张已选解锁卡')
            if target:
                targets.add(target)
        if set(roster) - set(initial) - targets:
            issue('ACTOR_CARD_COVERAGE', path, '每个非初始可选人物都须有明确人物卡来源')
    return issues
