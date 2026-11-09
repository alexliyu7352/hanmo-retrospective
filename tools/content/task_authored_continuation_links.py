"""已受领NPC信件只使用明确作者地址，不读取远方现任岗位或自动承诺新任务。"""


def check_task_authored_continuation(pack, index):
    """仅证明静态地址与原收件人匹配，运行期同场、占用与钱货资格由原规则核验。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        director = scenario.get('dynamic_world', {}).get('director', {})
        if 'authored_task_continuation' not in director:
            continue
        block = director['authored_task_continuation']
        root = ('scenarios', number, 'dynamic_world', 'director', 'authored_task_continuation')
        if (not isinstance(block, dict) or set(block) != {'version', 'message_addresses'}
                or type(block.get('version')) is not int or block['version'] not in (1, 2)
                or not isinstance(block.get('message_addresses'), list)
                or len(block['message_addresses']) > 4096):
            issues.append(('TASK_CONTINUATION_PROFILE', root, '既有任务续行须显式版本1或2与有限地址列表'))
            continue
        if (scenario.get('dynamic_world', {}).get('profile') != 'local_society_v1'
                or not {'native_dynamic_world', 'task_authored_continuation_v'+str(block['version'])} <= set(pack.get('required_capabilities', []))):
            issues.append(('TASK_CONTINUATION_DEPENDENCIES', root, '续行须启用原生动态世界和具名能力'))
        enabled = scenario.get('enabled', {})
        seen = set()
        for offset, row in enumerate(block['message_addresses']):
            path = root + ('message_addresses', offset)
            if not isinstance(row, dict) or set(row) != {'offer_id', 'recipient_role_id'}:
                issues.append(('TASK_CONTINUATION_ADDRESS', path, '地址只能指定原委托与收件人岗位'))
                continue
            offer_id, role_id = row['offer_id'], row['recipient_role_id']
            if not isinstance(offer_id, str) or not isinstance(role_id, str):
                issues.append(('TASK_CONTINUATION_ADDRESS', path, '委托与岗位须为真实具名引用'))
                continue
            offer = index.get('offers', {}).get(offer_id, {})
            role = index.get('roles', {}).get(role_id, {})
            if (offer_id in seen or offer_id not in enabled.get('offers', [])
                    or role_id not in enabled.get('roles', []) or offer.get('template') != 'message'
                    or role.get('initial_actor') != offer.get('goal', {}).get('receiver_actor')
                    or 'deliver_message' not in role.get('services', [])):
                issues.append(('TASK_CONTINUATION_ADDRESS', path, '须为已启用信件原收件人的唯一已知办理地址'))
            seen.add(offer_id)
    return issues
