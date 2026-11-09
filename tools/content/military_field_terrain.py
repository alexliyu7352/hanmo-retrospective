"""野外地形只选独立原接敌战场；不把围城道路或旧情景隐式升级。"""

def check_military_field_terrain(pack: dict, index: dict) -> list:
    """按真实映射、原目标和显式单位初态验证固定成本，拒绝递归或攻城共用板。"""
    issues = []
    boards = index.get('battlefields', {})
    troops = index.get('troop_types', {})
    siege_boards = {n['battlefield_id'] for t in index.get('siege_topologies', {}).values() for n in t['nodes']}
    for i, scenario in enumerate(pack.get('scenarios', [])):
        policy = scenario.get('military_field_terrain')
        if policy is None:
            continue
        path = ('scenarios', i, 'military_field_terrain')
        if (set(policy) != {'profile', 'battlefields'} or policy.get('profile') != 'terrain_and_zones_05_v1'
                or not isinstance(policy.get('battlefields'), list) or not 1 <= len(policy['battlefields']) <= 32):
            issues.append(('MILITARY_FIELD_TERRAIN_POLICY', path, '须明确固定地形规则及有限原接敌映射'))
            continue
        if (scenario.get('military_sequential_rounds') != {'profile': 'entry_initiative_05_v1'}
                or not {'military_field_terrain_v1', 'military_sequential_rounds_v1'} <= set(pack.get('required_capabilities', []))):
            issues.append(('MILITARY_FIELD_TERRAIN_PREREQUISITE', path, '须完整真实入战顺序轮及两显式能力'))
        sources, targets = set(), set()
        enabled = set(scenario.get('enabled', {}).get('battlefields', []))
        rout = set(scenario.get('military_rout_retreat', {}).get('battlefield_ids', []))
        for j, binding in enumerate(policy['battlefields']):
            at = path + ('battlefields', j)
            if set(binding) != {'approach_battlefield_id', 'field_battlefield_id'}:
                issues.append(('MILITARY_FIELD_TERRAIN_MAPPING', at, '映射只能指定原接敌与独立野战场'))
                continue
            old, new = binding['approach_battlefield_id'], binding['field_battlefield_id']
            if old == new or old in sources or new in targets:
                issues.append(('MILITARY_FIELD_TERRAIN_MAPPING', at, '映射两端必须唯一且不同'))
            sources.add(old); targets.add(new)
            if old not in boards or new not in boards or old not in enabled or new not in enabled or new not in rout:
                issues.append(('MILITARY_FIELD_TERRAIN_BOARD', at, '两场须实际启用，新场沿原溃退历史'))
            elif boards[old]['objective_id'] != boards[new]['objective_id']:
                issues.append(('MILITARY_FIELD_TERRAIN_OBJECTIVE', at, '独立地形不能改变原军令目标'))
            if new in siege_boards:
                issues.append(('MILITARY_FIELD_TERRAIN_SIEGE', at, '新野战场不能兼作围城节点'))
        if sources & targets:
            issues.append(('MILITARY_FIELD_TERRAIN_RECURSION', path, '禁止递归接敌映射'))
        for camp in scenario.get('initial', {}).get('garrisons', []):
            for unit in camp['units']:
                troop = troops.get(unit['troop_type_id'], {})
                category = troop.get('category')
                limits = {'infantry': (3, 1, 1, 1, 2), 'archer': (2, 2, 4, 2, 2), 'cavalry': (5, 1, 1, 3, 4)}.get(category)
                if limits is None:
                    issues.append(('MILITARY_FIELD_TERRAIN_TROOP', path, '地形仅支持原步弓骑'))
                    continue
                move, low, high, forest, mud = limits
                expected = {'plain': 1, 'road': 1, 'forest': forest, 'hill': 2, 'mud': mud, 'water': 0, 'wall': 0}
                if ((troop.get('movement'), troop.get('minimum_range'), troop.get('maximum_range')) != (move, low, high)
                        or any(troop.get('movement_costs', {}).get(k) != v for k, v in expected.items())):
                    issues.append(('MILITARY_FIELD_TERRAIN_TROOP', path, '启用部队须明确原05步弓骑移动、射程与七地形成本'))
    return issues
