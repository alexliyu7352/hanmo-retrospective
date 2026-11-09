"""显式野外溃退只选择原战场；不创造军籍、战果或新退路。"""


def check_military_rout_retreat(pack: dict, index: dict) -> list:
    """旧场景无需此能力，新场景须完整9与两方真实有限出口。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        policy = scenario.get('military_rout_retreat')
        if policy is None:
            continue
        path = ('scenarios', number, 'military_rout_retreat')
        ids = policy.get('battlefield_ids', []) if isinstance(policy, dict) else []
        if (not isinstance(policy, dict)
                or set(policy) != {'profile', 'blocked_rounds_before_capture', 'battlefield_ids'}
                or policy.get('profile') != 'two_blocked_rounds_v1'
                or type(policy.get('blocked_rounds_before_capture')) is not int
                or policy.get('blocked_rounds_before_capture') != 2
                or not isinstance(ids, list) or not 1 <= len(ids) <= 32
                or any(not isinstance(value, str) or not value for value in ids)
                or (all(isinstance(value, str) for value in ids) and len(set(ids)) != len(ids))):
            issues.append(('MILITARY_ROUT_POLICY', path, '须明确两轮受阻与有限唯一原战场列表'))
            continue
        if (scenario.get('military_supply_attrition', {}).get('profile') != 'anonymous_shortage_v1'
                or not {'military_rout_retreat_v1', 'military_supply_attrition_v1'} <= set(pack.get('required_capabilities', []))):
            issues.append(('MILITARY_ROUT_PREREQUISITE', path, '须完整军事9与显式溃退能力'))
        enabled = scenario.get('enabled', {}).get('battlefields', [])
        for field_id in ids:
            board = index.get('battlefields', {}).get(field_id)
            if not board or field_id not in enabled:
                issues.append(('MILITARY_ROUT_BATTLEFIELD', path, '须选择情景已经启用的原战场'))
                continue
            exits = board.get('exits', [])
            if not {0, 1} <= {row.get('side') for row in exits}:
                issues.append(('MILITARY_ROUT_EXITS', path, '两方均须真实作者出口；无路的本轮不会另造出口'))
    return issues
