"""拜帖只使用三名真实人物与原接待人明确声明，作者不能预填领取或任意秘密正文。"""


def check_visit_messages(pack, index):
    """正规作者守显式版本、完整启用引用、不可变接待本人和三种不同原say节点。"""
    errors = []
    fields = {'id', 'sender_actor_id', 'recipient_actor_id', 'custodian_actor_id', 'role_id', 'dialogue_id',
              'deposit_node_id', 'receive_node_id', 'withdraw_node_id'}
    for number, scene in enumerate(pack.get('scenarios', [])):
        profile, block = scene.get('visit_message_profile'), scene.get('visit_messages')
        root = ('scenarios', number, 'visit_messages')
        if profile is None and block is None:
            continue
        if (profile != 'witnessed_reception_v1' or not isinstance(block, dict) or set(block) != {'version', 'policies'}
                or type(block.get('version')) is not int or block['version'] != 1
                or not isinstance(block.get('policies'), list) or not 1 <= len(block['policies']) <= 4096):
            errors.append(('VISIT_MESSAGE_PROFILE', root, '须显式版本1和有限接待声明'))
            continue
        if (scene.get('dynamic_world', {}).get('profile') != 'local_society_v1'
                or not {'visit_messages_v1', 'native_dynamic_world', 'multi_actor', 'dialogue'} <= set(pack.get('required_capabilities', []))):
            errors.append(('VISIT_MESSAGE_CAPABILITY', root, '须启用原生动态世界与具名拜帖能力'))
        enabled = scene.get('enabled', {})
        ids, pairs = set(), set()
        for offset, row in enumerate(block['policies']):
            path = root + ('policies', offset)
            if not isinstance(row, dict) or set(row) != fields or any(not isinstance(row[key], str) or not row[key] for key in fields):
                errors.append(('VISIT_MESSAGE_POLICY', path, '仅含具名人物、岗位和原节点'))
                continue
            actors = row['sender_actor_id'], row['recipient_actor_id'], row['custodian_actor_id']
            if row['id'] in ids or actors in pairs or len(set(actors)) != 3:
                errors.append(('VISIT_MESSAGE_PARTIES', path, '三人须各自真实且不同，同一寄件关系只准一个声明'))
            ids.add(row['id']); pairs.add(actors)
            role = index.get('roles', {}).get(row['role_id'], {})
            dialogue = index.get('dialogues', {}).get(row['dialogue_id'], {})
            if (any(actor not in enabled.get('actors', []) or actor not in index.get('actors', {}) for actor in actors)
                    or row['role_id'] not in enabled.get('roles', []) or row['dialogue_id'] not in enabled.get('dialogues', [])
                    or role.get('initial_actor') != row['custodian_actor_id'] or role.get('dialogue_id') != row['dialogue_id']
                    or role.get('place_id') not in enabled.get('places', []) or role.get('place_id') not in index.get('places', {})
                    or dialogue.get('context_role') != row['role_id']):
                errors.append(('VISIT_MESSAGE_SOURCE', path, '须启用三名本人、原接待岗位/地点及其对白'))
            nodes = [row[key] for key in ('deposit_node_id', 'receive_node_id', 'withdraw_node_id')]
            if len(set(nodes)) != 3:
                errors.append(('VISIT_MESSAGE_NODE', path, '托留、领读、撤回各用具名独立声明'))
            for node_id in nodes:
                source = [node for node in dialogue.get('nodes', []) if node.get('id') == node_id]
                if len(source) != 1 or source[0].get('kind') != 'say' or source[0].get('speaker') not in ('$provider', row['custodian_actor_id']):
                    errors.append(('VISIT_MESSAGE_SPEAKER', path, '声明必须是接待本人原say，不借第三人或选项代答'))
    return errors
