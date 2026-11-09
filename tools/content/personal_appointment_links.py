"""私人预约作者必须是原岗位本人明确说话；预约只表达会面，不包办教学、任职或收费。"""
PROFILE = 'witnessed_personal_meetings_v1'
CAPABILITY = 'personal_appointments_v1'


def check_personal_appointments(pack, index):
    """原生相同的私人岗位、三个声明节点和有限窗口守卫，不预造真实答允。"""
    errors = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        profile, catalog = scenario.get('personal_appointment_profile'), scenario.get('personal_appointments')
        if profile is None and catalog is None:
            continue
        root = ('scenarios', number, 'personal_appointments')
        if (profile != PROFILE or not isinstance(catalog, dict) or set(catalog) != {'version', 'policies'}
                or type(catalog.get('version')) is not int or catalog['version'] != 1
                or not isinstance(catalog.get('policies'), list) or not 1 <= len(catalog['policies']) <= 4096):
            errors.append(('PERSONAL_APPOINTMENT_PROFILE', root, '须明确私人会面版本1和有限非空政策'))
            continue
        if not {CAPABILITY, 'dialogue', 'multi_actor', 'native_dynamic_world'} <= set(pack.get('required_capabilities', [])):
            errors.append(('PERSONAL_APPOINTMENT_CAPABILITY', root, '须明确真人会面、原生和私人预约能力'))
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or dynamic.get('profile') != 'local_society_v1':
            errors.append(('PERSONAL_APPOINTMENT_NATIVE', root, '预约须沿原生同候选世界'))
        enabled = scenario.get('enabled', {})
        seen = set()
        for offset, policy in enumerate(catalog['policies']):
            path = root + ('policies', offset)
            strings = {'id', 'provider_actor_id', 'role_id', 'dialogue_id', 'accept_node_id', 'cancel_node_id', 'meet_node_id'}
            integers = {'earliest_offset', 'latest_offset', 'duration_ticks'}
            if (not isinstance(policy, dict) or set(policy) != strings | integers
                    or any(not isinstance(policy[k], str) or not 1 <= len(policy[k]) <= 256 for k in strings)
                    or any(type(policy[k]) is not int for k in integers)
                    or not 1 <= policy['earliest_offset'] <= policy['latest_offset'] <= 720
                    or policy['duration_ticks'] != 1):
                errors.append(('PERSONAL_APPOINTMENT_POLICY', path, '只准固定有限开始窗口和一个半日实际会面'))
                continue
            if policy['id'] in seen:
                errors.append(('PERSONAL_APPOINTMENT_DUPLICATE', path, '政策身份必须唯一'))
            seen.add(policy['id'])
            role = index.get('roles', {}).get(policy['role_id'], {})
            dialogue = index.get('dialogues', {}).get(policy['dialogue_id'], {})
            if (policy['provider_actor_id'] not in enabled.get('actors', [])
                    or policy['role_id'] not in enabled.get('roles', [])
                    or policy['dialogue_id'] not in enabled.get('dialogues', [])
                    or role.get('organization_id') is not None or role.get('initial_actor') != policy['provider_actor_id']
                    or role.get('dialogue_id') != policy['dialogue_id']
                    or role.get('place_id') not in enabled.get('places', [])
                    or dialogue.get('context_role') != policy['role_id']):
                errors.append(('PERSONAL_APPOINTMENT_ORIGINAL_PERSON', path, '须本情景原私人岗位本人、同一会面地点及对白'))
            node_ids = [policy[k] for k in ('accept_node_id', 'cancel_node_id', 'meet_node_id')]
            if len(set(node_ids)) != 3:
                errors.append(('PERSONAL_APPOINTMENT_SEPARATE_DECLARATIONS', path, '答允、取消和开始须各有明确节点'))
            for node_id in node_ids:
                nodes = [n for n in dialogue.get('nodes', []) if isinstance(n, dict) and n.get('id') == node_id]
                if (len(nodes) != 1 or nodes[0].get('kind') != 'say'
                        or nodes[0].get('speaker') not in ('$provider', policy['provider_actor_id'])
                        or 'command' in nodes[0]):
                    errors.append(('PERSONAL_APPOINTMENT_DECLARATION', path, '只原本人say声明，不能用选项、旁人或自动命令代答'))
    return errors
