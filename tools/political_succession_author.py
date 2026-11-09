#!/usr/bin/env python3
"""从已冻结成年收养/志愿换控作者派生正常生前继位意愿入口；不注入运行事实。"""
import argparse, copy, json, subprocess
from pathlib import Path

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--base',required=True);parser.add_argument('--repo',type=Path,default=Path('.'));args=parser.parse_args()
    def read(path):return json.loads(subprocess.check_output(['git','show',args.base+':'+path],cwd=args.repo))
    p=read('content/packs/authored/voluntary_succession_1_0/content.json');manifest=read('content/packs/authored/voluntary_succession_1_0/pack.json')
    p['id']='political_successions';p['version']='1.0.0';s=p['scenarios'][0];s['id']='scenario.qingchuan_first_successor';s['name_key']='succession.scene.name'
    p['texts']['succession.scene.name']='青川首职继位意愿';p['texts']['succession.consent']='若我们已有合法成年收养关系，我愿接受此次志愿接续。只交接今后行旅的本人控制，私资为零；你的原首职继续由你本人持有。实际交接后我会另行答复原政治提名。'
    p['succession_consent_requests'][0]['personal']={'money':0,'goods':[]}
    p['roles'][0]['services'].append('deliver_message')
    caps=['political_successions_v1','family_life_v1','task_death_notices_v1','task_message_handoffs_v1','tasks.message']
    for cap in caps:
        if cap not in p['required_capabilities']:p['required_capabilities'].append(cap)
    s['family_life']={'profile':'verified_military_deaths_v1','player_death':'await_explicit_continuation'};s['task_message_handoff_profile']='observed_letter_delivery_v1'
    d=s['dynamic_world'];o=d['organization'];org=o['organizations']['society::org.city'];org['succession_policy_id']='policy.first_successor'
    # 原作者的行政首任一直是city_steward；本新剧本明确以同一首职作为该辖区法定Office。
    d['politics']['jurisdictions']['jurisdiction.qingchuan']['office_id']='office.city_steward'
    grant=copy.deepcopy(o['grants']['grant.qingchuan_appointment']);grant.update(id='grant.first_successor',allowed_command_kinds=['political_nominate_successor','political_withdraw_successor'],wallet_ids=[],inventory_ids=[],amount_limit=0,may_delegate=False,continuity_policy_id='stop_when_vacant',scopes=[{'kind':'site','id':'society::place.courtyard'}]);o['grants'][grant['id']]=grant;o['offices']['office.city_steward']['grant_ids'].append(grant['id'])
    for entry in o['offices']['office.city_steward']['grant_ids']:
        for kind in o['grants'][entry]['allowed_command_kinds']:
            key='succession.command.'+kind;p['texts'][key]=kind;d['politics']['command_name_keys'][kind]=key
    # 候选仅是作者明确原成员；不是预设继任人、已接受人或新Office持有人。
    o['members']['member.successor_candidate']={'id':'member.successor_candidate','actor_id':'society::actor.apprentice','organization_id':'society::org.city','duty_ids':[],'policy_id':'policy.city','response':'serving','response_since_tick':0,'next_review_tick':1,'last_fact_id':''}
    d['political_successions']={'version':1,'policies':{'policy.first_successor':{'id':'policy.first_successor','organization_id':'society::org.city','leader_office_id':'office.city_steward','seat_place_id':'society::place.courtyard','nomination_grant_id':'grant.first_successor','jurisdiction_id':'jurisdiction.qingchuan','response_ticks':80,'accession_ticks':160}}}
    p['texts'].update({'notice.item':'待亲历后可交的原首任文书','notice.offer':'原首任亲历通知','notice.role':'私人文书接待','notice.say':'此信只有原真实死亡已由我本人亲历时才可托交；未发生或未亲见，不作死亡陈述。','notice.leave':'结束会面。'})
    p['items'].append({'id':'item.first_leader_notice','name_key':'notice.item','kind':'message','base_price':0,'weight':1})
    p['offers'].append({'id':'offer.first_leader_notice','name_key':'notice.offer','template':'message','issuer':{'kind':'actor','id':'society::actor.tutor'},'discovery_sources':[{'kind':'actor','id':'society::actor.tutor'}],'accept_role':'role.notice_sender','report_role':'role.notice_sender','funding_wallet':'society::wallet.tutor','budget':0,'reward':0,'deadline_ticks':80,'accept_ticks':1,'goal':{'kind':'deliver_message','message_id':'item.first_leader_notice','receiver_actor':'society::actor.apprentice'},'on_unavailable':'wait_or_leave'})
    p['roles'].append({'id':'role.notice_sender','name_key':'notice.role','organization_id':None,'initial_actor':'society::actor.tutor','place_id':'society::place.school','services':['information','accept_offer','report_offer'],'dialogue_id':'dialog.notice_sender'})
    p['dialogues'].append({'id':'dialog.notice_sender','context_role':'role.notice_sender','entry':'notice','nodes':[{'id':'notice','kind':'say','speaker':'$provider','text_key':'notice.say','next':'menu'},{'id':'menu','kind':'choice','text_key':'notice.say','options':[{'id':'leave','text_key':'notice.leave','conditions':[],'next':'end'}]},{'id':'end','kind':'end'}]})
    for key,item in [('roles','role.notice_sender'),('dialogues','dialog.notice_sender'),('items','item.first_leader_notice'),('offers','offer.first_leader_notice')]:s['enabled'].setdefault(key,[]).append(item)
    s['initial']['roles'].append({'role_id':'role.notice_sender','actor_id':'society::actor.tutor','enabled':True});s['initial']['offer_ids'].append('offer.first_leader_notice')
    s['task_message_notices']={'version':1,'profile':'witnessed_death_letters_v1','bindings':[{'offer_id':'offer.first_leader_notice','decedent_actor_id':'society::actor.city_steward'}]}
    manifest.update(id='hanmo.political_successions',namespace='polsucc',version='1.0.0',required_capabilities=p['required_capabilities'])
    out=args.repo/'content/packs/authored/political_successions_1_0';out.mkdir(parents=True,exist_ok=True)
    for name,value in [('content.json',p),('pack.json',manifest)]: (out/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
    selection={'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.political_economy','version':'1.2.0'},{'id':'hanmo.adult_adoption','version':'1.0.0'},{'id':'hanmo.political_successions','version':'1.0.0'}]}
    (args.repo/'content/packs/selections/political-successions-1.0.0.json').write_text(json.dumps(selection,ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
