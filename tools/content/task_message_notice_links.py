"""亲历死亡通知只绑定已启用的本人送信委托；作者不可写事实ID或预造收件知情。"""


def check_task_message_notices(pack, index):
    """跨引用仍用正式作者索引，真实亲历资格只在原受领时由S4判断。"""
    issues=[]
    for number,scenario in enumerate(pack.get('scenarios',[])):
        if 'task_message_notices' not in scenario:
            continue
        block=scenario['task_message_notices'];path=('scenarios',number,'task_message_notices')
        if not isinstance(block,dict) or set(block)!={'version','profile','bindings'} or type(block.get('version')) is not int or block['version']!=1 or block.get('profile')!='witnessed_death_letters_v1' or not isinstance(block.get('bindings'),list) or not 1<=len(block['bindings'])<=4096:
            issues.append(('TASK_MESSAGE_NOTICE_PROFILE',path,'死亡文书须显式版本1、具名主题和已有送信委托'));continue
        required={'task_death_notices_v1','task_message_handoffs_v1','family_life_v1','tasks.message','native_dynamic_world','dialogue','multi_actor'}
        if not required<=set(pack.get('required_capabilities',[])) or scenario.get('family_life',{}).get('profile')!='verified_military_deaths_v1' or scenario.get('task_message_handoff_profile') not in ('observed_letter_delivery_v1','observed_letter_delivery_and_report_v2') or scenario.get('dynamic_world',{}).get('profile')!='local_society_v1':
            issues.append(('TASK_MESSAGE_NOTICE_DEPENDENCIES',path,'通知需要真实死亡、原信件实交和同一原生世界'))
        seen=set()
        for offset,row in enumerate(block['bindings']):
            where=path+('bindings',offset)
            if not isinstance(row,dict) or set(row)!={'offer_id','decedent_actor_id'}:
                issues.append(('TASK_MESSAGE_NOTICE_BINDING',where,'作者仅可指定原委托与死者，不能填入任意事实来源'));continue
            offer=row['offer_id'];decedent=row['decedent_actor_id'];definition=index.get('offers',{}).get(offer,{})
            if offer in seen:issues.append(('TASK_MESSAGE_NOTICE_DUPLICATE',where,'同一原委托只能绑定一个死亡主题'))
            seen.add(offer)
            issuer=definition.get('issuer',{});recipient=definition.get('goal',{}).get('receiver_actor')
            if offer not in scenario.get('enabled',{}).get('offers',[]) or definition.get('template')!='message' or issuer.get('kind')!='actor' or not recipient or issuer.get('id')==recipient or decedent in (issuer.get('id'),recipient) or decedent not in index.get('actors',{}):
                issues.append(('TASK_MESSAGE_NOTICE_SOURCE',where,'仅本人私信可取亲历来源，死者和收件人须是不同的具名人物'))
    return issues
