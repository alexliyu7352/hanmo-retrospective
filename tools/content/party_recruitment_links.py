"""招伴只绑定明确双方与本人原说话节点；作者表达不会自动创建Party成员。"""


def check_party_recruitment_consents(pack, index):
    """正规作者同时守profile、双方启用、岗位服务和NPC自己的明确接受/拒绝。"""
    errors = []
    for number, scene in enumerate(pack.get('scenarios', [])):
        profile = scene.get('party_recruitment_profile')
        block = scene.get('party_recruitment_consents')
        root = ('scenarios', number, 'party_recruitment_consents')
        if profile is None and block is None:
            continue
        if (profile != 'provider_declaration_v1' or not isinstance(block, dict)
                or set(block) != {'version', 'declarations'} or type(block.get('version')) is not int
                or block['version'] != 1 or not isinstance(block.get('declarations'), list)
                or len(block['declarations']) > 4096):
            errors.append(('PARTY_RECRUITMENT_PROFILE', root, '须显式版本1和有限本人声明'))
            continue
        if (scene.get('dynamic_world', {}).get('profile') != 'local_society_v1'
                or not {'party_provider_consent_v1', 'native_dynamic_world', 'multi_actor', 'dialogue', 'companions'} <= set(pack.get('required_capabilities', []))):
            errors.append(('PARTY_RECRUITMENT_CAPABILITY', root, '须启用原生动态世界、同行及本人声明能力'))
        enabled = scene.get('enabled', {})
        ids, sources = set(), set()
        invitation_roles = {row.get('role_id') for row in scene.get('party_invitations', {}).get('invitations', [])}
        for offset, row in enumerate(block['declarations']):
            path = root + ('declarations', offset)
            fields = {'id', 'leader_actor_id', 'companion_actor_id', 'role_id', 'dialogue_id', 'node_id', 'decision'}
            if not isinstance(row, dict) or set(row) != fields or any(not isinstance(row[key], str) or not row[key] for key in fields):
                errors.append(('PARTY_RECRUITMENT_DECLARATION', path, '声明仅含具名双方、原节点与接受/拒绝'))
                continue
            key = row['leader_actor_id'], row['companion_actor_id'], row['dialogue_id'], row['node_id']
            if row['id'] in ids or key in sources or row['leader_actor_id'] == row['companion_actor_id'] or row['decision'] not in ('accept', 'decline'):
                errors.append(('PARTY_RECRUITMENT_DECLARATION', path, '双方须不同，声明与节点唯一，不能由参数决定对方意愿'))
            ids.add(row['id']); sources.add(key)
            role = index.get('roles', {}).get(row['role_id'], {})
            dialogue = index.get('dialogues', {}).get(row['dialogue_id'], {})
            nodes = [node for node in dialogue.get('nodes', []) if node.get('id') == row['node_id']]
            if (row['leader_actor_id'] not in enabled.get('actors', []) or row['companion_actor_id'] not in enabled.get('actors', [])
                    or row['role_id'] not in enabled.get('roles', []) or row['dialogue_id'] not in enabled.get('dialogues', [])
                    or role.get('initial_actor') != row['companion_actor_id'] or 'join_party' not in role.get('services', [])
                    or dialogue.get('context_role') != row['role_id'] or row['role_id'] in invitation_roles):
                errors.append(('PARTY_RECRUITMENT_PARTICIPANTS', path, '须为启用的原本人招募岗位，不能翻转相邀方向'))
            if len(nodes) != 1 or nodes[0].get('kind') != 'say' or nodes[0].get('speaker') not in ('$provider', row['companion_actor_id']):
                errors.append(('PARTY_RECRUITMENT_SPEAKER', path, '须由原provider本人在say节点明确声明'))
    return errors
