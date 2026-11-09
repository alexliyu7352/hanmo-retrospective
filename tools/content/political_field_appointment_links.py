"""野外接替显式配对及双专项Grant检查；不预置死亡、军令或实际交接。"""
def check_political_field_appointments(pack, index):
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        config = dynamic.get('political_field_appointments')
        military = scenario.get('military_field_commands')
        director = dynamic.get('director', {})
        policies = director.get('policies', director.get('subjects', {}))
        root = ('scenarios', number, 'dynamic_world', 'political_field_appointments')
        def error(code, path, message):
            issues.append((code, root + tuple(path), message))
        requested = any('field_appointment' in p.get('templates', []) for p in policies.values())
        if config is None:
            if military or requested:
                error('POLITICAL_FIELD_PAIR', (), '野外军令与政治专项签受须明确同启')
            continue
        if (not isinstance(config, dict) or config.get('enabled') is not True
                or type(config.get('version')) is not int or config.get('version') != 1
                or military != {'profile': 'casualty_appointment_v1'}
                or scenario.get('military_officer_assignments') != {'profile': 'political_appointment_v1'}
                or not dynamic.get('political_military_appointments', {}).get('enabled')
                or not dynamic.get('politics', {}).get('enabled')
                or not {'political_field_appointments_v1', 'military_field_commands_v1'} <= set(pack.get('required_capabilities', []))):
            error('POLITICAL_FIELD_PAIR', (), '仅允许有驻营前置的Military7与野外专项签受1配对')
            continue
        org = dynamic.get('organization', {})
        offices, grants = org.get('offices', {}), org.get('grants', {})
        enabled = set(scenario.get('enabled', {}).get('garrisons', []))
        organizations = set()
        for key, policy in config.get('policies', {}).items():
            garrison = index.get('garrisons', {}).get(policy.get('origin_garrison_id'), {})
            organizations.add(policy.get('organization_id'))
            if (key != policy.get('id') or policy.get('origin_garrison_id') not in enabled
                    or garrison.get('organization_id') != policy.get('organization_id')
                    or garrison.get('place_id') != policy.get('origin_site_id')
                    or not (policy.get('allow_unit') or policy.get('allow_army'))):
                error('POLITICAL_FIELD_SCOPE', (key,), '专项军令须绑定原编军营地、组织及至少一种接替')
            for enabled_kind, command in [('allow_unit', 'political_assign_field_unit'), ('allow_army', 'political_assign_field_army')]:
                if not policy.get(enabled_kind):
                    continue
                authorized = False
                for office in offices.values():
                    if office.get('organization_id') != policy.get('organization_id'):
                        continue
                    for gid in office.get('grant_ids', []):
                        grant = grants.get(gid, {})
                        if (grant.get('office_id') == office.get('id') and command in grant.get('allowed_command_kinds', [])
                                and not grant.get('wallet_ids') and not grant.get('inventory_ids')
                                and grant.get('amount_limit', 0) >= 1
                                and {'kind': 'site', 'id': policy.get('origin_site_id')} in grant.get('scopes', [])):
                            authorized = True
                if not authorized:
                    error('POLITICAL_FIELD_GRANT', (key, enabled_kind), '接部与接军各须具名原公职专项Grant')
        for key, policy in policies.items():
            if 'field_appointment' in policy.get('templates', []) and (director.get('registry_version') != 1 or policy.get('organization_id') not in organizations):
                error('POLITICAL_FIELD_DIRECTOR', (key,), '野外NPC须沿原公平登记并持对应组织专项政策')
    return issues
