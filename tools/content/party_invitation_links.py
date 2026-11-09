"""随行邀请只取当前情景的明确双方和节点；共享Schema/namespace负责稳定引用限定。"""
FIELDS = {'id', 'leader_actor_id', 'invitee_actor_id', 'dialogue_id', 'node_id', 'role_id'}


def check_party_invitations(pack, index):
    """原场景不继承其它包或情景的邀请；错误保原作者位置，不创建运行同意。"""
    issues = []
    actors, roles, dialogues = (index.get(table, {}) for table in ('actors', 'roles', 'dialogues'))
    for number, scenario in enumerate(pack.get('scenarios', [])):
        profile = scenario.get('party_invitation_profile')
        block = scenario.get('party_invitations')
        path = ('scenarios', number, 'party_invitations')
        if profile is None and block is None:
            continue
        if profile != 'authored_node_v1' or not isinstance(block, dict) or set(block) != {'version', 'invitations'} or type(block.get('version')) is not int or block.get('version') != 1 or not isinstance(block.get('invitations'), list):
            issues.append(('PARTY_INVITATION_PROFILE', path, '情景须同时声明本版邀请profile和自己的version1邀请块'))
            continue
        if scenario.get('dynamic_world', {}).get('profile') != 'local_society_v1' or not {'native_dynamic_world', 'multi_actor', 'dialogue', 'companions'} <= set(pack.get('required_capabilities', [])):
            issues.append(('PARTY_INVITATION_CAPABILITY', path, '邀请须由原生情景与既有原生世界、人物、对白、同行能力共同启用'))
        enabled = scenario.get('enabled', {})
        present = {row.get('actor_id') for row in scenario.get('initial', {}).get('actors', [])}
        assignments = {row.get('role_id'): row for row in scenario.get('initial', {}).get('roles', [])}
        ids, bindings = set(), set()
        for offset, row in enumerate(block['invitations']):
            source = path + ('invitations', offset)
            if not isinstance(row, dict) or set(row) != FIELDS or not all(isinstance(row[key], str) and row[key] for key in FIELDS):
                issues.append(('PARTY_INVITATION_FIELDS', source, '邀请只允许具名双方、岗位、对白及实际节点'))
                continue
            identity = row['id'];binding = tuple(row[key] for key in ('leader_actor_id', 'invitee_actor_id', 'dialogue_id', 'node_id'))
            if identity in ids or binding in bindings:
                issues.append(('PARTY_INVITATION_DUPLICATE', source, '同一节点与双方不能同时代表两份邀请'))
            ids.add(identity);bindings.add(binding)
            participants = {row['leader_actor_id'], row['invitee_actor_id']}
            if len(participants) != 2 or not participants <= set(actors) & set(enabled.get('actors', [])) & present:
                issues.append(('PARTY_INVITATION_ACTORS', source, '邀请双方须是本情景实际启用的不同人物'))
            role = roles.get(row['role_id'], {})
            assigned = assignments.get(row['role_id'], {})
            if row['role_id'] not in enabled.get('roles', []) or assigned.get('actor_id') != row['leader_actor_id'] or assigned.get('enabled') is not True or 'join_party' not in role.get('services', []) or role.get('companion_cost_per_tick', -1) < 0:
                issues.append(('PARTY_INVITATION_ROLE', source, '邀请岗位须由该领队实际担任，并保原同行费用服务'))
            dialogue = dialogues.get(row['dialogue_id'], {})
            nodes = [node for node in dialogue.get('nodes', []) if node.get('id') == row['node_id']]
            if row['dialogue_id'] not in enabled.get('dialogues', []) or dialogue.get('context_role') != row['role_id'] or len(nodes) != 1 or nodes[0].get('kind') not in ('say', 'choice', 'action'):
                issues.append(('PARTY_INVITATION_NODE', source, '邀请须指向本岗位可实际显示的明确对白节点'))
    return issues
