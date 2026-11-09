"""显式新局自建政策；模板不激活，初财只来自作者，玩家仅提交点数及外貌选择。"""
import re

POLICY = 'created_character_v1'
PROFILE = 'point_buy_character_v1'
FEATURES = {'face_shape', 'eyes', 'brows', 'nose', 'mouth', 'hair', 'beard', 'skin_palette', 'build'}
FIELDS = {'profile', 'prototype_actor_id', 'starting_place_id', 'allowed_modes', 'initial_money', 'initial_goods', 'appearance_choices'}
TOKEN = re.compile(r'[a-z][a-z0-9_.-]{0,63}\Z')


def check_character_creation(pack, index):
    tables = {}
    for table in ('actors', 'places', 'items'):
        tables[table] = dict(index.get(table, {}))
        tables[table].update({row['id']: row for row in pack.get(table, []) if isinstance(row, dict) and isinstance(row.get('id'), str)})
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if not isinstance(scenario, dict):
            continue
        dynamic = scenario.get('dynamic_world', {})
        enabled = isinstance(dynamic, dict) and dynamic.get('founder_policy') == POLICY
        if not enabled and 'character_creation' not in scenario:
            continue
        path = ('scenarios', number, 'character_creation')

        def issue(code, suffix, message):
            issues.append((code, path + suffix, message))

        config = scenario.get('character_creation')
        if not enabled or not isinstance(config, dict) or set(config) != FIELDS or config.get('profile') != PROFILE:
            issue('CHARACTER_CREATION_PROFILE', (), '须同时显式声明自建人物开局政策与标准点买配置')
            continue
        prototype = config['prototype_actor_id']
        definition = tables['actors'].get(prototype) if isinstance(prototype, str) else None
        active = scenario.get('enabled', {})
        initial = scenario.get('initial', {})
        if (not definition or definition.get('origin') != 'original'
                or scenario.get('player_actor') != prototype
                or prototype in active.get('actors', [])
                or any(row.get('actor_id') == prototype for row in initial.get('actors', []) if isinstance(row, dict))):
            issue('CHARACTER_CREATION_PROTOTYPE', ('prototype_actor_id',), '来源须为未激活的原创模板，不能覆写已有世界人物')
        for field in ('playable_actor_ids', 'playable_actor_appearance_policies', 'player_start_descriptions', 'player_appearance_choices', 'actor_card_unlocks'):
            if field in scenario and scenario[field] != []:
                issue('CHARACTER_CREATION_EXISTING_ACTORS', (), '自建入口不能同时选择或改写既有人物名单')
        place = config['starting_place_id']
        if not isinstance(place, str) or place not in tables['places'] or place not in active.get('places', []):
            issue('CHARACTER_CREATION_PLACE', ('starting_place_id',), '启程地点须为本情景已启用的真实场所')
        modes = config['allowed_modes']
        if (not isinstance(modes, list) or not 1 <= len(modes) <= 2
                or any(mode not in ('standard', 'free') for mode in modes)
                or modes.count('standard') != 1 or modes.count('free') > 1):
            issue('CHARACTER_CREATION_MODES', ('allowed_modes',), '标准模式必备；自由模式须由作者明确启用')
        money = config['initial_money']
        if type(money) is not int or not 0 <= money <= 2147483647:
            issue('CHARACTER_CREATION_MONEY', ('initial_money',), '初始盘缠须为作者固定的有界整数')
        goods = config['initial_goods']
        if not isinstance(goods, list) or len(goods) > 20:
            issue('CHARACTER_CREATION_GOODS', ('initial_goods',), '初货须为最多20项的固定清单')
        else:
            seen, weight = set(), 0
            for position, row in enumerate(goods):
                item_id = row.get('item_id') if isinstance(row, dict) else None
                item = tables['items'].get(item_id) if isinstance(item_id, str) else None
                if (not isinstance(row, dict) or set(row) != {'item_id', 'quantity'}
                        or not item or item_id in seen or type(row.get('quantity')) is not int
                        or not 1 <= row['quantity'] <= 100 or type(item.get('weight')) is not int
                        or not 1 <= item['weight'] <= 10000):
                    issue('CHARACTER_CREATION_GOODS', ('initial_goods', position), '初货须引用真实物品且数量、负重合法，不能重复授予')
                    continue
                seen.add(item_id)
                weight += item['weight'] * row['quantity']
            if weight > 20:
                issue('CHARACTER_CREATION_CAPACITY', ('initial_goods',), '初货不能超过原人物行囊容量20')
        choices = config['appearance_choices']
        if not isinstance(choices, list) or not 1 <= len(choices) <= 4096:
            issue('CHARACTER_CREATION_APPEARANCE', ('appearance_choices',), '须明确列出1至4096项允许的完整外貌组合')
            continue
        ids, tuples = set(), set()
        for position, row in enumerate(choices):
            if (not isinstance(row, dict) or set(row) != {'id', 'base_features', 'attire_style'}
                    or not isinstance(row['id'], str) or not TOKEN.fullmatch(row['id'])
                    or not isinstance(row['attire_style'], str) or not TOKEN.fullmatch(row['attire_style'])
                    or not isinstance(row['base_features'], dict) or set(row['base_features']) != FEATURES
                    or any(not isinstance(value, str) or not TOKEN.fullmatch(value) for value in row['base_features'].values())):
                issue('CHARACTER_CREATION_APPEARANCE', ('appearance_choices', position), '须给出完整 appearance-v1 语义组合')
                continue
            shape = (tuple(sorted(row['base_features'].items())), row['attire_style'])
            if row['id'] in ids or shape in tuples:
                issue('CHARACTER_CREATION_APPEARANCE_DUPLICATE', ('appearance_choices', position), '组合ID和实际外貌不能重复')
            ids.add(row['id']); tuples.add(shape)
    return issues
