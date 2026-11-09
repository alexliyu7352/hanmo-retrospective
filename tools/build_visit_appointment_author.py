"""原学舍拜帖、原人物和新增本人会面岗位；不预置信件、承诺或NPC认领。"""
import argparse, copy, json
from pathlib import Path
from build_personal_appointment_author import emit

def build(repo):
    root=repo/'content/packs/authored';source=root/'task_contacts_1_11';target=root/'visit_appointments_1_0'
    pack=json.loads((source/'pack.json').read_text());data=json.loads((source/'content.json').read_text())
    pack.update(id='hanmo.visit_appointments',version='1.0.0',namespace='visit_appointments');data.update(id='visit_appointments',version='1.0.0')
    for doc in (pack,data):doc['required_capabilities']+=['personal_appointments_v1','visit_appointments_v1','visit_appointment_intents_v1']
    s=copy.deepcopy(next(s for s in data['scenarios'] if s['id']=='scenario.qingchuan_reception'));s.update(id='scenario.qingchuan_visit_appointments',name_key='name.scenario.visit_appointments');s['enabled']['scenarios']=[s['id']];data['scenarios']=[s]
    s['personal_appointment_profile']='witnessed_personal_meetings_v1';s['personal_appointments']={'version':1,'policies':[{'id':'appointment.swordswoman','provider_actor_id':'society::actor.study_swordswoman','role_id':'role.appointment_swordswoman','dialogue_id':'dialog.appointment_swordswoman','accept_node_id':'accept','cancel_node_id':'cancel','meet_node_id':'meet','earliest_offset':8,'latest_offset':16,'duration_ticks':1}]}
    s['visit_appointment_profile']='witnessed_visit_appointments_v1';s['visit_appointment_invitations']={'version':1,'policies':[{'id':'invitation.swordswoman','message_policy_id':'visit.0','appointment_policy_id':'appointment.swordswoman','offer_node_id':'offer_invitation','collect_node_id':'collect_invitation','inviter_commits_fixed_window':True,'accept_before_offset':6,'earliest_offset':8,'latest_offset':16}]}
    s['visit_appointment_intents']={'version':1,'intents':[{'id':'invitation.receiver','reception_intent_id':'reception.receiver','invitation_policy_id':'invitation.swordswoman','route':[{'node_id':'receive'},{'node_id':'again','option_id':'again'},{'node_id':'menu','option_id':'offer_invitation'}]}]}
    data['roles'].append({'id':'role.appointment_swordswoman','name_key':'name.role.appointment_swordswoman','organization_id':None,'initial_actor':'society::actor.study_swordswoman','place_id':'society::place.school','dialogue_id':'dialog.appointment_swordswoman','services':['information']})
    s['initial']['roles'].append({'role_id':'role.appointment_swordswoman','actor_id':'society::actor.study_swordswoman','enabled':True});s['enabled']['roles'].append('role.appointment_swordswoman');s['enabled']['dialogues'].append('dialog.appointment_swordswoman')
    texts={'name.scenario.visit_appointments':'青川拜帖：领回之后再答允','name.role.appointment_swordswoman':'习剑人私人会面','line.invitation.menu':'可以谈原拜帖的固定约期，也可以另谈一次当面预约。','line.invitation.accept':'若你当面另约，从现在起第八至第十六个半日，可以来这处学舍叙谈半日；请你明确核对。','line.invitation.cancel':'若你要取消那份尚未开始的原约，我们现在当面说清，再核对原件。','line.invitation.meet':'若现在仍在双方原先答允的约期里，就在学舍叙谈半日，由你确认开始。','option.invitation.accept':'另约一次当面会面。','option.invitation.cancel':'当面谈取消原约。','option.invitation.meet':'按本人已接受的原帖赴约。','option.invitation.back':'返回约期事务。','option.invitation.leave':'结束交谈。','option.invitation.offer':'本人托回一份固定日期邀约。','option.invitation.collect':'亲自领回给我的日期邀约。','line.invitation.offer':'这次托回邀约，就是收帖本人承诺自己的日期：托回时起第八至第十六个半日，在学舍相谈一个半日。原发送人必须在第六个半日前亲自领回并另行接受，领取本身不等于答允。请本人核对后确认。','line.invitation.collect':'请核对给自己的原邀约回帖。只有亲自领回才可看日期和地点，领回后仍要自己决定是否接受；日期不会重新计算。'}
    data['texts'].update(texts)
    nodes=[{'id':'menu','kind':'choice','text_key':'line.invitation.menu','options':[]}]
    for purpose in ('accept','cancel','meet'):
        nodes[0]['options'].append({'id':purpose,'text_key':'option.invitation.'+purpose,'conditions':[],'next':purpose});nodes.append({'id':purpose,'kind':'say','speaker':'$provider','text_key':'line.invitation.'+purpose,'next':'menu'})
    nodes[0]['options'].append({'id':'leave','text_key':'option.invitation.leave','conditions':[],'next':'end'});nodes.append({'id':'end','kind':'end'})
    data['dialogues'].append({'id':'dialog.appointment_swordswoman','context_role':'role.appointment_swordswoman','entry':'menu','nodes':nodes})
    dialogue=next(d for d in data['dialogues'] if d['id']=='dialog.visit_reception');menu=next(n for n in dialogue['nodes'] if n['id']=='menu')
    for node,verb in [('offer_invitation','offer'),('collect_invitation','collect')]:
        menu['options'].insert(-1,{'id':node,'text_key':'option.invitation.'+verb,'conditions':[],'next':node});dialogue['nodes'].append({'id':node,'kind':'say','speaker':'$provider','text_key':'line.invitation.'+verb,'next':'again'})
    emit(target/'pack.json',pack);emit(target/'content.json',data);emit(repo/'content/packs/selections/visit-appointments-1.0.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.visit_appointments','version':'1.0.0'}]})
    (target/'README.md').write_text('''# 青川拜帖约期 1.0

选择 visit-appointments-1.0.0.json，场景 visit_appointments::scenario.qingchuan_visit_appointments。原学徒向学舍接待者托留给习剑人的拜帖；原本就在学舍的习剑人仅沿显式原 template13 意向询问、真实领读并在其原认领会话中托回固定约期。此承诺由实际发邀时刻计算，不按玩家看到页面的时刻计算。

学徒后来须亲自回原接待者处领回，然后自己明确接受。接受限发邀后第六个半日之前，实际会面限第八至第十六个半日；会面一个半日。领回不是接受，远方页面不提前显示条款，发邀者不能远方知道对方领回/接受。约期到时在学舍与习剑人原私人岗位交谈，到实际赴约say再开始；取消同样须双方当面原声明。未到、忙碌或离场由原规则拒绝或实际中断，不授教学、关系或资源。

旧当面预约页仍可另约，但与已承诺日期冲突会拒；新邀约与普通contact_in_person回复不共用同一原帖政策。没有预置拜帖、领读、认领、答允、未来Action或控制接续。此包为正规作者源码，未compose/Installed/World/GUI，NPC自然往返与实际时长仍待同代验证。没有复制素材。
''')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);build(p.parse_args().repo)
