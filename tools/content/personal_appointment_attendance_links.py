"""NPC只赴已经当面承诺的原地点；作者模板不代双方接受或创建预约。"""
CAPABILITY = 'personal_appointment_attendance_v1'
TEMPLATE = 'personal_appointment_attendance'


def check_personal_appointment_attendance(pack, index):
    """显式目录、原预约policy、真实provider与原Director准入共同限定新行为。"""
    errors = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        director = scenario.get('dynamic_world', {}).get('director', {})
        configured = director.get('personal_appointment_attendance')
        policies = director.get('policies', director.get('subjects', {}))
        selected = [key for key, p in policies.items() if TEMPLATE in p.get('templates', [])]
        if configured is None and not selected:
            continue
        path = ('scenarios', number, 'dynamic_world', 'director', 'personal_appointment_attendance')
        if (not isinstance(configured, dict) or set(configured) != {'version', 'policies'}
                or type(configured.get('version')) is not int or configured['version'] != 1
                or not isinstance(configured.get('policies'), list) or not 1 <= len(configured['policies']) <= 4096):
            errors.append(('APPOINTMENT_ATTENDANCE_PROFILE', path, '须明确版本1的原本人赴约政策'))
            continue
        if (scenario.get('personal_appointment_profile') != 'witnessed_personal_meetings_v1'
                or not {CAPABILITY, 'personal_appointments_v1', 'npc_society', 'native_dynamic_world'} <= set(pack.get('required_capabilities', []))
                or scenario.get('dynamic_world', {}).get('profile') != 'local_society_v1'):
            errors.append(('APPOINTMENT_ATTENDANCE_CAPABILITY', path, '须显式启用原预约和NPC赴约能力'))
        original = {p['id']: p for p in scenario.get('personal_appointments', {}).get('policies', [])}
        seen = set()
        covered = set()
        for offset, p in enumerate(configured['policies']):
            here = path + ('policies', offset)
            if (not isinstance(p, dict) or set(p) != {'appointment_policy_id', 'depart_before_ticks'}
                    or not isinstance(p.get('appointment_policy_id'), str) or not p['appointment_policy_id']
                    or type(p.get('depart_before_ticks')) is not int or not 0 <= p['depart_before_ticks'] <= 720):
                errors.append(('APPOINTMENT_ATTENDANCE_POLICY', here, '只准原预约policy及有限提前复评量'))
                continue
            key = p['appointment_policy_id']
            source = original.get(key)
            if key in seen or not source:
                errors.append(('APPOINTMENT_ATTENDANCE_SOURCE', here, '重复或不存在的预约不能创造承诺'))
                continue
            seen.add(key)
            actor = source['provider_actor_id']
            if actor not in scenario.get('enabled', {}).get('actors', []):
                errors.append(('APPOINTMENT_ATTENDANCE_ACTOR', here, '只能原情景启用的具名provider赴约'))
            if 'registry_version' in director:
                admitted = any(a.get('policy_id') in selected and actor not in a.get('excluded_actor_ids', []) for a in director.get('admissions', {}).values())
            else:
                admitted = actor in selected
            if not admitted:
                errors.append(('APPOINTMENT_ATTENDANCE_DIRECTOR', here, '须由原主体政策明确启用赴约模板，不能额外造NPC登记'))
            else:
                covered.add(actor)
        if not selected or not covered:
            errors.append(('APPOINTMENT_ATTENDANCE_DIRECTOR', path, '须有真实provider的原预算内赴约模板'))
    return errors
