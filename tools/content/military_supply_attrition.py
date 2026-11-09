"""Military9匿名兵卒缺粮策略准入；额度来自真实fit token，不自动处置具名军官。"""


def check_military_supply_attrition(pack: dict, index: dict) -> list:
    """完整Military8为必要前置；无新政策的旧情景保持原供给语义。"""
    issues = []
    expected = {'profile': 'anonymous_shortage_v1', 'shortage_threshold_half_days': 2,
                'desertion_basis_points': 200, 'morale_loss': 10}
    for number, scenario in enumerate(pack.get('scenarios', [])):
        policy = scenario.get('military_supply_attrition')
        if policy is None:
            continue
        path = ('scenarios', number, 'military_supply_attrition')
        if (not isinstance(policy, dict) or policy != expected
                or any(type(policy.get(key)) is not int for key in expected if key != 'profile')):
            issues.append(('MILITARY_SUPPLY_ATTRITION_POLICY', path,
                           '须显式匿名士卒连续两缺粮半日、2%额度和10点士气规则'))
        recovery = scenario.get('military_wounded_recovery', {})
        if (recovery.get('profile') != 'supplied_wound_batches_v1'
                or not scenario.get('dynamic_world', {}).get('personal_injury_recovery')
                or not {'military_supply_attrition_v1', 'military_wounded_recovery_v1'} <=
                set(pack.get('required_capabilities', []))):
            issues.append(('MILITARY_SUPPLY_ATTRITION_PREREQUISITE', path,
                           '缺粮逃散须完整军中/个人恢复前置及两项显式能力'))
    return issues
