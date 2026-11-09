"""检查显式本人委托响应profile与能力声明；不伪造过去的拒绝或替作者生成任务。"""


def check_task_offer_responses(pack, _index):
    """只检查本叶实际支持的profile，旧情景没有字段则原样通过。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'task_offer_response_profile' not in scenario:
            continue
        path = ('scenarios', number, 'task_offer_response_profile')
        if scenario['task_offer_response_profile'] not in ('known_offers_v1', 'known_offers_v2'):
            issues.append(('TASK_OFFER_RESPONSE_PROFILE', path,
                           '本人响应只支持明确的known_offers_v1/v2；不能把未知规则当作暂不承接'))
        if 'task_offer_responses_v1' not in pack.get('required_capabilities', []):
            issues.append(('TASK_OFFER_RESPONSE_CAPABILITY', path,
                           '显式本人响应profile必须声明task_offer_responses_v1能力'))
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or dynamic.get('profile') != 'local_society_v1':
            issues.append(('TASK_OFFER_RESPONSE_NATIVE_PROFILE', path,
                           '本人响应只在原生local_society_v1情景启用；能力并集不能替旧情景补响应域'))
        if scenario['task_offer_response_profile'] == 'known_offers_v2':
            for capability in ('native_dynamic_world', 'dialogue', 'multi_actor'):
                if capability not in pack.get('required_capabilities', []):
                    issues.append(('RUNTIME_OFFER_RESPONSE_CAPABILITY', path,
                                   '动态机会回应须声明既有能力：' + capability))
    return issues


def check_task_offer_response_command(command, role, index):
    """显式婉拒沿委托原受理岗位，不给任意说话者新增处置合同权限。"""
    if command.get('kind') == 'decline_runtime_offer':
        return '动态机会ID与Need版本须来自实际本人会面页，不能在静态作者对白中预造'
    if command.get('kind') != 'decline_offer':
        return ''
    offer = index.get('offers', {}).get(command.get('offer_id'))
    if offer is None:
        return '这次暂不承接必须引用已定义的作者委托'
    if role.get('id') != offer.get('accept_role') or 'accept_offer' not in role.get('services', []):
        return '这次暂不承接须由原委托的合法受理岗位当面接洽'
    return ''
