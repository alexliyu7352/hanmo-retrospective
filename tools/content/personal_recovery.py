"""个人休养作者只选择原真实服务设施；伤势、付款、半日履约与康复来源均由运行时产生。"""


def check_personal_recovery(pack, index):
    """精确版本和设施资格，拒绝把回血数值、当前角色或自造疗程当作临床资格。"""
    issues = []
    raw = index.get('roles', {})
    roles = dict(raw) if isinstance(raw, dict) else {r['id']: r for r in raw if isinstance(r, dict) and 'id' in r}
    roles.update({r['id']: r for r in pack.get('roles', []) if isinstance(r, dict) and 'id' in r})
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        if not isinstance(dynamic, dict) or 'personal_injury_recovery' not in dynamic:
            continue
        path = ('scenarios', number, 'dynamic_world', 'personal_injury_recovery')
        def fail(code, detail):
            """保留实际字段路径，诊断不替作者补默认资质。"""
            issues.append((code, path, detail))
        required = {'personal_injury_recovery_v1', 'regional_life_v1', 'family_life_v1', 'family', 'native_dynamic_world'}
        if not required <= set(pack.get('required_capabilities', [])) or not isinstance(scenario.get('family_life'), dict):
            fail('RECOVERY_DEPENDENCIES', '需要原生命史、家庭与真实地区服务能力')
        config = dynamic['personal_injury_recovery']
        if not isinstance(config, dict) or set(config) != {'version', 'services'} or type(config.get('version')) is not int or config['version'] != 1:
            fail('RECOVERY_FIELDS', '只允许版本1的设施选择，不预写伤势或恢复点'); continue
        services = config['services']
        if not isinstance(services, list) or not 1 <= len(services) <= 4096:
            fail('RECOVERY_SERVICES', '设施必须是非空有限列表'); continue
        seen = set()
        for row in services:
            if not isinstance(row, dict) or set(row) != {'role_id', 'method'} or not isinstance(row.get('role_id'), str) or not row['role_id'] or row.get('method') not in {'safe_lodging', 'qualified_clinic'}:
                fail('RECOVERY_SERVICE_FIELDS', '只接受真实原岗位和明确的住宿或医馆策略'); continue
            role_id = row['role_id']
            if role_id in seen:
                fail('RECOVERY_DUPLICATE_ROLE', role_id)
            seen.add(role_id)
            if role_id not in scenario.get('enabled', {}).get('roles', []):
                fail('RECOVERY_ROLE_DISABLED', role_id)
            if index or role_id in roles:
                role = roles.get(role_id)
                services_at_role = role.get('services', []) if isinstance(role, dict) else []
                rest = role.get('activities', {}).get('rest') if isinstance(role, dict) and 'rest' in services_at_role else None
                if not isinstance(rest, dict) or type(rest.get('duration')) is not int or not 1 <= rest['duration'] <= 720:
                    fail('RECOVERY_REST_REQUIRED', role_id)
    return issues
