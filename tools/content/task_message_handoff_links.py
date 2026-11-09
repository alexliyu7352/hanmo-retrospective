"""文书交接是原送信的直接观察，不允许作者预先宣称获知远方死亡或其他未送达事实。"""


def check_task_message_handoffs(pack, _index):
    """缺省保旧情景，显式profile须具备原生、真人会话和真实信件任务依赖。"""
    issues = []
    for index, scenario in enumerate(pack.get('scenarios', [])):
        if 'task_message_handoff_profile' not in scenario:
            continue
        path = ('scenarios', index, 'task_message_handoff_profile')
        if scenario['task_message_handoff_profile'] not in ('observed_letter_delivery_v1', 'observed_letter_delivery_and_report_v2', 'known_personal_delivery_and_report_v3','known_personal_delivery_and_sender_report_v4'):
            issues.append(('TASK_MESSAGE_HANDOFF_PROFILE', path, '文书交接须明确已登记的实交1、返报2或原Known本人接信3'))
        for capability in ('task_message_handoffs_v1', 'tasks.message', 'native_dynamic_world', 'dialogue', 'multi_actor'):
            if capability not in pack.get('required_capabilities', []):
                issues.append(('TASK_MESSAGE_HANDOFF_CAPABILITY', path, '真实文书交接需要显式能力：' + capability))
        if scenario['task_message_handoff_profile'] in ('observed_letter_delivery_and_report_v2','known_personal_delivery_and_report_v3','known_personal_delivery_and_sender_report_v4') and 'task_message_reports_v2' not in pack.get('required_capabilities', []):
            issues.append(('TASK_MESSAGE_REPORT_CAPABILITY', path, '当面返报需要显式task_message_reports_v2能力'))
        if scenario['task_message_handoff_profile'] in ('known_personal_delivery_and_report_v3','known_personal_delivery_and_sender_report_v4'):
            block=scenario.get('dynamic_world',{}).get('task_personal_message_deliveries')
            if 'task_personal_message_deliveries_v1' not in pack.get('required_capabilities',[]) or not isinstance(block,dict) or block.get('version')!=1 or block.get('profile')!='known_private_recipient_v1':issues.append(('TASK_PERSONAL_MESSAGE_PROFILE',path,'handoff3须真实Known接信目录及独立能力'))
        if scenario['task_message_handoff_profile']=='known_personal_delivery_and_sender_report_v4':
            block=scenario.get('dynamic_world',{}).get('task_personal_message_reports')
            if 'task_personal_message_reports_v1' not in pack.get('required_capabilities',[]) or not isinstance(block,dict) or block.get('version')!=1 or block.get('profile')!='known_private_sender_report_v1':issues.append(('TASK_PERSONAL_REPORT_PROFILE',path,'handoff4须原发信人返报目录及独立能力'))
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or dynamic.get('profile') != 'local_society_v1':
            issues.append(('TASK_MESSAGE_HANDOFF_NATIVE_PROFILE', path, '文书交接沿原生世界真人与同候选任务运行'))
    return issues
