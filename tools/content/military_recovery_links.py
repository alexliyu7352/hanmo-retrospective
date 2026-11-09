"""军中伤员恢复只准入实际营地和完整人物康复前置；不造初始伤亡或补军粮。"""


def check_military_wounded_recovery(pack: dict, index: dict) -> list:
    """显式政策保持旧情景禁用，并把可恢复营地限定在本情景真实启用驻军。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        policy = scenario.get('military_wounded_recovery')
        if policy is None:
            continue
        path = ('scenarios', number, 'military_wounded_recovery')
        dynamic = scenario.get('dynamic_world', {})
        if (dynamic.get('profile') != 'local_society_v1'
                or scenario.get('military_field_commands') != {'profile': 'casualty_appointment_v1'}
                or scenario.get('military_officer_assignments') != {'profile': 'political_appointment_v1'}
                or not isinstance(dynamic.get('personal_injury_recovery'), dict)
                or not dynamic['personal_injury_recovery']
                or not {'military', 'military_wounded_recovery_v1'} <= set(pack.get('required_capabilities', []))):
            issues.append(('MILITARY_RECOVERY_PREREQUISITE', path,
                           '伤员恢复须动态军旅、原野外/驻营配对和完整个人伤势康复政策'))
        if (not isinstance(policy, dict) or set(policy) != {'profile', 'garrison_ids'}
                or policy.get('profile') != 'supplied_wound_batches_v1'
                or not isinstance(policy.get('garrison_ids'), list)
                or not 1 <= len(policy['garrison_ids']) <= 1024
                or any(not isinstance(value, str) or not value for value in policy['garrison_ids'])
                or len(set(policy['garrison_ids'])) != len(policy['garrison_ids'])):
            issues.append(('MILITARY_RECOVERY_POLICY', path, '须明确原伤批恢复规则与唯一有限驻营清单'))
            continue
        enabled = set(scenario.get('enabled', {}).get('garrisons', []))
        for position, garrison_id in enumerate(policy['garrison_ids']):
            camp = index.get('garrisons', {}).get(garrison_id)
            if not camp or garrison_id not in enabled:
                issues.append(('MILITARY_RECOVERY_GARRISON', path + ('garrison_ids', position),
                               '恢复营地必须实际存在并在本情景启用'))
            elif type(camp.get('ration_per_person')) is not int or camp['ration_per_person'] <= 0:
                issues.append(('MILITARY_RECOVERY_RATION', path + ('garrison_ids', position),
                               '恢复营地须实际正额原军粮来源，不能以零口粮政策视为足额供给'))
    return issues
