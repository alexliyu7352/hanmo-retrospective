"""驻地休整只选择已有安全供给营地，不为旧情景隐式开启恢复。"""
def check_military_garrison_rest(pack: dict, index: dict) -> list:
    """规则固定半日减10，要求完整军事11与已配置的恢复驻地。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        policy = scenario.get('military_garrison_rest')
        if policy is None:
            continue
        path = ('scenarios', number, 'military_garrison_rest')
        if (not isinstance(policy, dict) or set(policy) != {'profile', 'garrison_ids', 'fatigue_reduction'}
                or policy.get('profile') != 'supplied_garrison_rest_v1'
                or type(policy.get('fatigue_reduction')) is not int or policy.get('fatigue_reduction') != 10):
            issues.append(('MILITARY_GARRISON_REST_POLICY', path, '须明确原供给驻营半日减10疲劳'))
            continue
        ids = policy['garrison_ids']
        prior = scenario.get('military_wounded_recovery', {}).get('garrison_ids', [])
        if (not isinstance(ids, list) or not 1 <= len(ids) <= 1024
                or any(not isinstance(value, str) or not value for value in ids)
                or len(set(ids)) != len(ids) or not set(ids) <= set(prior)):
            issues.append(('MILITARY_GARRISON_REST_GARRISON', path, '休整只可选择原显式安全恢复驻地'))
        if (scenario.get('military_command_shock', {}).get('profile') != 'army_commander_once_v1'
                or not {'military_command_shock_v1', 'military_garrison_rest_v1'} <= set(pack.get('required_capabilities', []))):
            issues.append(('MILITARY_GARRISON_REST_PREREQUISITE', path, '须完整军事11，休整仅适用于原有部队条件字段'))
    return issues
