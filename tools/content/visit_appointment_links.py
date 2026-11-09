"""同原拜帖和当面预约配对的新固定日期承诺；不把普通回复解释为同意。"""
PROFILE = 'witnessed_visit_appointments_v1'
CAP = 'visit_appointments_v1'
INTENT_CAP = 'visit_appointment_intents_v1'


def check_visit_appointments(pack, index):
    errors = []
    for number, scene in enumerate(pack.get('scenarios', [])):
        profile, block, intents = (scene.get(k) for k in ('visit_appointment_profile', 'visit_appointment_invitations', 'visit_appointment_intents'))
        if profile is None and block is None and intents is None:
            continue
        root = ('scenarios', number, 'visit_appointment_invitations')
        def error(code, message, path=root): errors.append((code, path, message))
        if (profile != PROFILE or not isinstance(block, dict) or set(block) != {'version', 'policies'}
                or type(block.get('version')) is not int or block['version'] != 1
                or not isinstance(block.get('policies'), list) or not 1 <= len(block['policies']) <= 4096):
            error('VISIT_APPOINTMENT_PROFILE', '须明确日期邀约1与有限非空目录'); continue
        if not {CAP, 'personal_appointments_v1', 'visit_messages_v1', 'native_dynamic_world'} <= set(pack.get('required_capabilities', [])):
            error('VISIT_APPOINTMENT_CAPABILITY', '原拜帖、原当面预约与新约期能力须齐全')
        if (scene.get('visit_message_profile') != 'witnessed_reception_v1' or scene.get('personal_appointment_profile') != 'witnessed_personal_meetings_v1'
                or scene.get('dynamic_world', {}).get('profile') != 'local_society_v1'):
            error('VISIT_APPOINTMENT_ORIGINAL_PROFILE', '须原生世界、原实交拜帖与原私人预约')
        find = lambda rows, key: next((r for r in rows if isinstance(r, dict) and r.get('id') == key), None)
        original = scene.get('visit_messages', {}).get('policies', [])
        personal = scene.get('personal_appointments', {}).get('policies', [])
        ordinary = scene.get('visit_message_replies', {}).get('policies', [])
        ids, linked = set(), set()
        strings = {'id', 'message_policy_id', 'appointment_policy_id', 'offer_node_id', 'collect_node_id'}
        integers = {'accept_before_offset', 'earliest_offset', 'latest_offset'}
        for offset, policy in enumerate(block['policies']):
            path = root + ('policies', offset)
            if (not isinstance(policy, dict) or set(policy) != strings | integers | {'inviter_commits_fixed_window'}
                    or any(not isinstance(policy.get(k), str) or not 1 <= len(policy[k]) <= 256 for k in strings)
                    or any(type(policy.get(k)) is not int for k in integers)
                    or policy.get('inviter_commits_fixed_window') is not True
                    or not 1 <= policy['accept_before_offset'] <= policy['earliest_offset'] <= policy['latest_offset'] <= 720):
                error('VISIT_APPOINTMENT_POLICY', '发邀者须明确承诺有界固定日期，领取不会重算约期', path); continue
            m, a = find(original, policy['message_policy_id']), find(personal, policy['appointment_policy_id'])
            if (policy['id'] in ids or policy['message_policy_id'] in linked or not m or not a
                    or a.get('provider_actor_id') != m.get('recipient_actor_id') or a.get('provider_actor_id') == m.get('sender_actor_id')):
                error('VISIT_APPOINTMENT_SOURCE', '只能原收件本人发邀、原发送人领回，唯一原政策对应', path); continue
            ids.add(policy['id']); linked.add(policy['message_policy_id'])
            if any(r.get('message_policy_id') == m['id'] for r in ordinary):
                error('VISIT_APPOINTMENT_ORDINARY_CONFLICT', '同原拜帖不可同时普通答复和新日期承诺', path)
            role = index.get('roles', {}).get(m.get('role_id'), {})
            dialogue = index.get('dialogues', {}).get(m.get('dialogue_id'), {})
            if role.get('initial_actor') != m.get('custodian_actor_id') or role.get('dialogue_id') != m.get('dialogue_id') or dialogue.get('context_role') != m.get('role_id'):
                error('VISIT_APPOINTMENT_CUSTODIAN', '原保管者、私人岗位和实际对白须同源', path)
            nodes = [m.get(k) for k in ('deposit_node_id', 'receive_node_id', 'withdraw_node_id')] + [policy[k] for k in ('offer_node_id', 'collect_node_id')]
            if len(set(nodes)) != 5: error('VISIT_APPOINTMENT_DISTINCT_NODES', '托留、领读、撤回、发邀与领回须各有原声明', path)
            for key in ('offer_node_id', 'collect_node_id'):
                matches = [n for n in dialogue.get('nodes', []) if isinstance(n, dict) and n.get('id') == policy[key]]
                if len(matches) != 1 or matches[0].get('kind') != 'say' or matches[0].get('speaker') not in ('$provider', m.get('custodian_actor_id')) or 'command' in matches[0]:
                    error('VISIT_APPOINTMENT_DECLARATION', '只准真实原接待者说话，不准自动替本人承诺', path + (key,))
        if intents is None: continue
        if (not isinstance(intents, dict) or set(intents) != {'version', 'intents'} or type(intents.get('version')) is not int or intents['version'] != 1
                or not isinstance(intents.get('intents'), list) or not 1 <= len(intents['intents']) <= 4096):
            error('VISIT_APPOINTMENT_INTENT_PROFILE', 'NPC须独立显式意向1与有限路径'); continue
        reception = scene.get('visit_reception_intents', {})
        if INTENT_CAP not in pack.get('required_capabilities', []) or 'visit_reception_v1' not in pack.get('required_capabilities', []) or scene.get('visit_reception_profile') != 'authored_local_reception_v1' or reception.get('version') != 1:
            error('VISIT_APPOINTMENT_INTENT_PAIR', '须原模板13实际认领来源与显式NPC能力')
        seen, sources = set(), set()
        for offset, intent in enumerate(intents['intents']):
            path = ('scenarios', number, 'visit_appointment_intents', 'intents', offset)
            if (not isinstance(intent, dict) or set(intent) != {'id', 'reception_intent_id', 'invitation_policy_id', 'route'}
                    or any(not isinstance(intent.get(k), str) or not 1 <= len(intent[k]) <= 256 for k in ('id', 'reception_intent_id', 'invitation_policy_id'))
                    or not isinstance(intent.get('route'), list) or not 1 <= len(intent['route']) <= 16):
                error('VISIT_APPOINTMENT_INTENT', '只准具名原收件意向和有界实际对白路径', path); continue
            r = find(reception.get('intents', []), intent['reception_intent_id']); p = find(block['policies'], intent['invitation_policy_id']); m = find(original, p.get('message_policy_id')) if p else None
            if (intent['id'] in seen or intent['reception_intent_id'] in sources or not r or not m or r.get('purpose') != 'receive'
                    or r.get('actor_id') != m.get('recipient_actor_id') or r.get('message_policy_id') != m.get('id')):
                error('VISIT_APPOINTMENT_INTENT_SOURCE', '只准原收件者真实领读后自己的固定承诺', path); continue
            seen.add(intent['id']); sources.add(intent['reception_intent_id'])
            dialogue = index.get('dialogues', {}).get(m.get('dialogue_id'), {}); nodes = {n.get('id'): n for n in dialogue.get('nodes', []) if isinstance(n, dict)}
            if any(step.get('node_id') not in nodes or 'command' in nodes[step['node_id']] for step in r.get('route', [])):
                error('VISIT_APPOINTMENT_INTENT_ORIGINAL_ROUTE', '原接待路径不可藏自动业务', path)
            current, target, visited, valid = m.get('receive_node_id'), p.get('offer_node_id'), set(), True
            for step in intent['route']:
                if not isinstance(step, dict) or not {'node_id'} <= set(step) <= {'node_id', 'option_id'} or step.get('node_id') != current or current == target or current in visited:
                    valid = False; break
                visited.add(current); node = nodes.get(current, {})
                if 'command' in node: valid = False; break
                if node.get('kind') == 'say' and not step.get('option_id'): current = node.get('next')
                elif node.get('kind') == 'choice' and isinstance(step.get('option_id'), str):
                    options = [o for o in node.get('options', []) if o.get('id') == step['option_id']]
                    if len(options) != 1: valid = False; break
                    current = options[0].get('next')
                else: valid = False; break
            if not valid or current != target or current in visited:
                error('VISIT_APPOINTMENT_INTENT_ROUTE', '须真实从原领读say到原发邀say的无环路径', path)
    return errors
