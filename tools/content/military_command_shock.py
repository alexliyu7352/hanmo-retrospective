"""军级一次震动只在完整真实溃退战场内启用，不以同侧推定统帅。"""
def check_military_command_shock(pack: dict, index: dict) -> list:
    """作者明确固定减10与原10前置，普通旧场景完全不新增隐含策略。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        policy = scenario.get('military_command_shock')
        if policy is None:
            continue
        path = ('scenarios', number, 'military_command_shock')
        if (not isinstance(policy, dict) or set(policy) != {'profile', 'morale_loss'}
                or policy.get('profile') != 'army_commander_once_v1'
                or type(policy.get('morale_loss')) is not int or policy.get('morale_loss') != 10):
            issues.append(('MILITARY_COMMAND_SHOCK_POLICY', path, '须明确每个原军团一次的10点士气震动'))
        if (scenario.get('military_rout_retreat', {}).get('profile') != 'two_blocked_rounds_v1'
                or not {'military_command_shock_v1', 'military_rout_retreat_v1'} <= set(pack.get('required_capabilities', []))):
            issues.append(('MILITARY_COMMAND_SHOCK_PREREQUISITE', path, '须完整军事10及显式军级震动能力'))
    return issues
