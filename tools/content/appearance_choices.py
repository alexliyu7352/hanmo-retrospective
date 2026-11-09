"""限定作者成人代表的完整元组和固定主人公；不读取资源、不创建人物或重抽外貌。"""
from copy import deepcopy

SELECTED_POLICY = 'selected_existing_actor_v1'
ROSTER_POLICY = 'selected_existing_actor_roster_v1'
SELECTED_FIELDS = ('playable_actor_appearance_policies', 'player_start_descriptions')


def _selected_actors(scenario, index, number):
    """各版本名单必须各有实际成人快照；保留者不强套组合代表或重新生成外貌。"""
    issues = []
    root = ('scenarios', number)
    playable = scenario.get('playable_actor_ids', [])
    roster = scenario.get('dynamic_world', {}).get('founder_policy') == ROSTER_POLICY
    if (not isinstance(playable, list)
            or not (1 <= len(playable) <= 4096 if roster else len(playable) == 2)
            or any(not isinstance(actor, str) or not actor for actor in playable)
            or len(set(playable)) != len(playable)
            or scenario.get('player_actor') not in playable):
        message = '名单政策必须明确1至4096名不同的已有可玩人物' if roster else '新政策必须明确恰好两名不同的已有可玩人物'
        return [('SELECTED_ACTORS_REQUIRED', root + ('playable_actor_ids',), message)]
    records = {}
    for field in SELECTED_FIELDS:
        if field == 'player_start_descriptions' and field not in scenario:
            continue
        rows = scenario.get(field)
        if (not isinstance(rows, list) or len(rows) != len(playable)
                or any(not isinstance(row, dict) or row.get('actor_id') not in playable for row in rows)
                or len({row['actor_id'] for row in rows}) != len(playable)):
            issues.append(('SELECTED_ACTOR_COVERAGE', root + (field,), '人物政策与处境说明必须各自恰好覆盖可玩人物名单'))
        else:
            records[field] = {row['actor_id']: row for row in rows}
    if issues:
        return issues
    policies = records['playable_actor_appearance_policies']
    if any(row.get('policy') not in ('fixed_actor_appearance_composition_v1', 'preserve_existing_v1') for row in policies.values()):
        return [('SELECTED_APPEARANCE_POLICIES', root + ('playable_actor_appearance_policies',), '人物外貌政策必须显式选择成人组合或保留既有外貌')]
    if (all(row['policy'] == 'preserve_existing_v1' for row in policies.values())
            and scenario.get('player_appearance_choices') is not None):
        issues.append(('UNEXPECTED_APPEARANCE_CHOICES', root + ('player_appearance_choices',), '全部人物保留原外貌时不能声明无人使用的组合候选'))
    initial = {row['actor_id']: row for row in scenario.get('initial', {}).get('actors', [])}
    if roster:
        enabled = scenario.get('enabled', {}).get('actors', [])
        rows = scenario.get('initial', {}).get('actors', [])
        for actor in playable:
            if enabled.count(actor) != 1 or sum(row['actor_id'] == actor for row in rows) != 1:
                issues.append(('SELECTED_ACTOR_UNAVAILABLE', root + ('playable_actor_ids',), '可玩人物必须恰有一份已启用的真实初态：' + actor))
    for actor in playable:
        actual = initial.get(actor)
        if (actor not in index.get('actors', {}) or actual is None
                or actual.get('alive') is not True or actual.get('available') is not True
                or type(actual.get('age_years')) is not int or not 18 <= actual['age_years'] <= 120):
            issues.append(('SELECTED_ACTOR_UNAVAILABLE', root + ('playable_actor_ids',), '可玩人物必须来自存活、可用且18至120岁的真实成人初态：' + actor))
            continue
        policy = policies[actor]['policy']
        if policy == 'preserve_existing_v1':
            definition = actual.get('appearance_definition', {})
            if definition.get('kind') != 'fixed' or definition.get('generator_version') != 'appearance-v1':
                issues.append(('PRESERVED_APPEARANCE_REQUIRED', root + ('initial', 'actors'), '保留政策必须已有固定appearance-v1外貌，不在创建时重新抽取'))
            continue
        candidate = deepcopy(scenario)
        candidate['player_actor'] = actor
        candidate['playable_actor_ids'] = [actor]
        candidate['dynamic_world']['founder_policy'] = policy
        for field in SELECTED_FIELDS:
            candidate.pop(field, None)
        for code, path, message in check_appearance_choices({'scenarios': [candidate]}, index):
            issues.append((code, root + path[2:], message))
    return issues

def check_appearance_choices(pack, index):
    """各显式政策保持各自完整覆盖；新组合不向旧人物或旧快照追加草案权限。"""
    issues = []
    for n, scenario in enumerate(pack.get('scenarios', [])):
        policy = scenario.get('dynamic_world', {}).get('founder_policy')
        if policy in (SELECTED_POLICY, ROSTER_POLICY):
            issues.extend(_selected_actors(scenario, index, n))
            continue
        for field in SELECTED_FIELDS:
            if field in scenario:
                issues.append(('UNEXPECTED_PLAYER_CREATION_FIELDS', ('scenarios', n, field), '旧人物政策不接受双人物政策或处境字段'))
        choices = scenario.get('player_appearance_choices')
        path = ('scenarios', n, 'player_appearance_choices')
        composition = policy == 'fixed_actor_appearance_composition_v1'
        if policy != 'fixed_actor_appearance_v1' and not composition:
            if choices is not None:
                issues.append(('UNEXPECTED_APPEARANCE_CHOICES', path, '只有显式固定主角成人外观政策允许代表选择'))
            continue
        if choices is None or len(choices) != (192 if composition else 12):
            issues.append(('APPEARANCE_CHOICES_REQUIRED', path, '组合政策须声明192个元组；旧政策须保留十二个代表'))
            continue
        ids, tuples = set(), set()
        for row in choices:
            value = (tuple(sorted(row['base_features'].items())), row['attire_style'])
            if row['id'] in ids or value in tuples:
                issues.append(('DUPLICATE_APPEARANCE_CHOICE', path, '代表身份和完整外观元组均不得重复'))
            ids.add(row['id']); tuples.add(value)
        if composition and not _composition_domain(choices):
            issues.append(('INVALID_APPEARANCE_COMPOSITION', path, '组合必须完整覆盖已制作十二骨相与四项五官，其他槽位固定'))
        player = scenario['player_actor']
        actual = next((x for x in scenario.get('initial', {}).get('actors', []) if x['actor_id'] == player), None)
        if actual is None or index['actors'][player].get('origin') == 'historical' or not 18 <= actual['age_years'] <= 59:
            issues.append(('APPEARANCE_FOUNDER_REQUIRED', path, '本政策只覆盖快照中18至59岁原创固定主人公'))
            continue
        definition = actual['appearance_definition']
        if (definition.get('kind') != 'fixed' or definition.get('generator_version') != 'appearance-v1'
                or (tuple(sorted(definition.get('base_features', {}).items())), definition.get('attire_style')) not in tuples):
            issues.append(('APPEARANCE_DEFAULT_REQUIRED', path, '默认人物须精确使用候选表中的完整appearance-v1代表'))
    return issues


def _composition_domain(choices):
    """只校验作者语义域，不读PNG或建立第二份人物外貌；192不重复项保证笛卡尔积完整。"""
    faces = {'medium_clear_long', 'medium_square_jaw', 'medium_round_chin', 'medium_high_cheek',
             'female_round_broad', 'female_straight_jaw', 'female_short_chin', 'female_high_cheek',
             'long_narrow', 'long_straight_jaw', 'long_full_oval', 'long_high_cheek'}
    options = {'face_shape': faces, 'eyes': {'open', 'narrow'}, 'brows': {'arched', 'straight'},
               'nose': {'straight', 'round'}, 'mouth': {'neutral', 'smile'}, 'hair': {'bun'},
               'beard': {'none'}, 'skin_palette': {'warm'}, 'build': {'normal'}}
    return all(row['attire_style'] == 'plain' and set(row['base_features']) == set(options)
               and all(value in options[key] for key, value in row['base_features'].items())
               for row in choices)
