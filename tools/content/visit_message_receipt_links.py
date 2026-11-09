"""实际托领撤提交来源只能显式选择，不能由历史消息或已收知识补造旧结果。"""
PROFILE='permanent_visit_message_results_v1'
CAP='visit_message_receipts_v1'

def check_visit_message_receipts(pack,index):
    errors=[]
    for number,scene in enumerate(pack.get('scenarios',[])):
        selected=scene.get('visit_message_receipt_profile')
        if selected is None:continue
        path=('scenarios',number,'visit_message_receipt_profile')
        if selected!=PROFILE:
            errors.append(('VISIT_MESSAGE_RECEIPT_PROFILE',path,'须显式选择原托领撤实际结果1'));continue
        if not {CAP,'visit_messages_v1','dialogue','multi_actor','native_dynamic_world'}<=set(pack.get('required_capabilities',[])):
            errors.append(('VISIT_MESSAGE_RECEIPT_CAPABILITY',path,'真实提交源须与原实交拜帖能力同选'))
        messages=scene.get('visit_messages')
        if (scene.get('visit_message_profile')!='witnessed_reception_v1'
                or not isinstance(messages,dict) or type(messages.get('version')) is not int or messages['version']!=1
                or not isinstance(messages.get('policies'),list) or not messages['policies']
                or scene.get('dynamic_world',{}).get('profile')!='local_society_v1'):
            errors.append(('VISIT_MESSAGE_RECEIPT_ORIGINAL_REQUIRED',path,'须真实原拜帖目录与原生世界；不预种持久回执'))
    return errors
