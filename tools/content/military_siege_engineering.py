"""围城工程只显式选择既有城防；材料、器械与实际军令仍由原作者及军事规则校验。"""

def check_military_siege_engineering(pack: dict, index: dict) -> list:
    """不补默认拓扑或兵员，只核完整16前置、真实已启用城防和固定原05公式。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        policy = scenario.get('military_siege_engineering')
        if policy is None:
            continue
        path = ('scenarios', number, 'military_siege_engineering')
        if (not isinstance(policy, dict) or set(policy) != {'version', 'profile', 'topology_ids'}
                or type(policy.get('version')) is not int or policy['version'] != 1
                or policy.get('profile') != 'commander_fortification_05_v1'
                or not isinstance(policy.get('topology_ids'), list)
                or not 1 <= len(policy['topology_ids']) <= 64):
            issues.append(('MILITARY_SIEGE_ENGINEERING_POLICY', path, '须明确原05工程策略与有限城防列表'))
            continue
        learning = scenario.get('military_tactic_learning', {})
        lessons = scenario.get('dynamic_world', {}).get('growth_tactic_lessons', {})
        if (learning.get('version') != 1 or lessons.get('version') != 1
                or not {'military_siege_engineering_v1', 'military_tactic_learning_v1', 'growth_tactic_lessons_v1'} <= set(pack.get('required_capabilities', []))):
            issues.append(('MILITARY_SIEGE_ENGINEERING_PREREQUISITE', path, '须完整原军事16与本人教学配对'))
        seen = set()
        enabled = set(scenario.get('enabled', {}).get('siege_topologies', []))
        strongholds = set(scenario.get('enabled', {}).get('strongholds', []))
        for i, identifier in enumerate(policy['topology_ids']):
            at = path + ('topology_ids', i)
            if (not isinstance(identifier, str) or identifier in seen or identifier not in enabled
                    or identifier not in index.get('siege_topologies', {})):
                issues.append(('MILITARY_SIEGE_ENGINEERING_TOPOLOGY', at, '城防须唯一且实际启用'))
                continue
            seen.add(identifier)
            if not any(row.get('topology_id') == identifier and key in strongholds for key, row in index.get('strongholds', {}).items()):
                issues.append(('MILITARY_SIEGE_ENGINEERING_STRONGHOLD', at, '城防须属于本场景真实启用据点'))
    return issues
