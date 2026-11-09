#!/usr/bin/env python3
"""只在原生前交接作者加入明确后代提名条款，不预置新的任职或死亡。"""
import argparse,copy,json,subprocess
from pathlib import Path

def main():
    """原首任先真实交接，后任的权源、期限与本人答允均留待原生命令执行。"""
    p=argparse.ArgumentParser();p.add_argument('--base',required=True);p.add_argument('--repo',type=Path,default=Path('.'));a=p.parse_args()
    def read(path):
        """严格只读指定Git输入，作者后继另存新目录。"""
        return json.loads(subprocess.check_output(['git','show',a.base+':'+path],cwd=a.repo))
    root='content/packs/authored/political_head_handovers_1_0/'
    c,m=read(root+'content.json'),read(root+'pack.json');c['id']='political_successor_rounds';scenario=c['scenarios'][0];scenario['id']='scenario.qingchuan_successor_rounds';c['texts'][scenario['name_key']]='青川后任首长提名';c['required_capabilities'].append('political_successor_rounds_v1')
    d=scenario['dynamic_world'];org=d['organization'];g=copy.deepcopy(org['grants']['grant.first_successor']);g.update(id='grant.successor_round',allowed_command_kinds=['political_nominate_next_successor','political_withdraw_next_successor']);org['grants'][g['id']]=g;org['offices']['office.city_steward']['grant_ids'].append(g['id'])
    d['political_successor_rounds']={'version':1,'policies':{'policy.successor_round':{'id':'policy.successor_round','succession_policy_id':'policy.first_successor','nomination_grant_id':g['id'],'response_ticks':80,'accession_ticks':160}}}
    for kind in g['allowed_command_kinds']:
        key='successor_round.command.'+kind;c['texts'][key]=kind;d['politics']['command_name_keys'][kind]=key
    # 第三名成年候选先正常收养和零资产换控，再本人接受政治新轮；不预设亲属、控制或公职。
    society=read('content/packs/authored/local_society_1_11/content.json')
    clerk=copy.deepcopy(next(x for x in society['actors'] if x['id']=='actor.apprentice'))
    clerk.update(id='actor.next_clerk',name_key='next.clerk.name',initial_age_years=20,initial_place='society::place.courtyard',initial_goods=[],source_note='本独立情景的二十岁原创文吏。仅预置原普通成员身份，无收养关系、继位意愿或公职。')
    c['actors'].append(clerk);c['texts']['next.clerk.name']='沈简'
    c['wallets'].append({'id':'wallet.next_clerk','owner':{'kind':'actor','id':clerk['id']},'initial_balance':0})
    d['local_economy']['named_actors_excluded'].append(clerk['id'])
    d['director']['admissions']['a.qingchuan_steward']['excluded_actor_ids'].append(clerk['id'])
    scenario['enabled']['actors'].append(clerk['id']);scenario['enabled']['wallets'].append('wallet.next_clerk')
    initial_clerk=copy.deepcopy(next(x for x in scenario['initial']['actors'] if x['actor_id']=='society::actor.apprentice'));initial_clerk.update(actor_id=clerk['id'],age_years=20,initial_goods=[]);scenario['initial']['actors'].append(initial_clerk)
    scenario['initial']['wallets'].append(copy.deepcopy(c['wallets'][-1]))
    org['members']['member.next_clerk']={'id':'member.next_clerk','actor_id':clerk['id'],'organization_id':'society::org.city','duty_ids':[],'policy_id':'policy.city','response':'serving','response_since_tick':0,'next_review_tick':1,'last_fact_id':''}
    for suffix,node,text in [('adoption','request','我已成年，愿在真实办理后与您形成收养关系；此事不授予任何公职。'),('control','consent','若我们已有真实收养关系，我愿承接本人控制而不接任何私资。原公职仍须另行明确办理。')]:
        role='role.next_clerk_'+suffix;dialog='dialog.next_clerk_'+suffix;key='next.clerk.'+suffix
        scenario['enabled']['roles'].append(role);scenario['enabled']['dialogues'].append(dialog);scenario['initial']['roles'].append({'role_id':role,'actor_id':clerk['id'],'enabled':True})
        c['texts'][key]=text;c['roles'].append({'id':role,'name_key':'next.clerk.name','organization_id':None,'initial_actor':clerk['id'],'place_id':'society::place.courtyard','services':['information','deliver_message'],'dialogue_id':dialog})
        c['dialogues'].append({'id':dialog,'context_role':role,'entry':node,'nodes':[{'id':node,'kind':'say','speaker':'$provider','text_key':key,'next':'end'},{'id':'end','kind':'end'}]})
    c['adult_adoption_requests'].append({'id':'request.next_clerk_adoption','adopter_actor_id':'society::actor.apprentice','adoptee_actor_id':clerk['id'],'dialogue_id':'dialog.next_clerk_adoption','node_id':'request','terms_version':1,'consent_kind':'adoptee_requests_adoption_v1','scope':'adult_adoption_only'})
    scenario['adult_adoption']['request_ids'].append('request.next_clerk_adoption')
    c['succession_consent_requests'].append({'id':'request.next_clerk_control','predecessor_actor_id':'society::actor.apprentice','successor_actor_id':clerk['id'],'dialogue_id':'dialog.next_clerk_control','node_id':'consent','terms_version':1,'consent_kind':'successor_accepts_voluntary_succession_v1','scope':'player_control_and_selected_personal_assets','personal':{'money':0,'goods':[]}})
    scenario['voluntary_succession']['request_ids'].append('request.next_clerk_control')
    # 第二任真实死亡的同一原文书机制，仍由实际见闻者出具并当面交给新候选。
    notice=copy.deepcopy(c['offers'][0]);notice['id']='offer.next_leader_notice';notice['name_key']='next.notice.offer';notice['goal']['message_id']='item.next_leader_notice';notice['goal']['receiver_actor']=clerk['id'];c['offers'].append(notice)
    c['items'].append({'id':'item.next_leader_notice','name_key':'next.notice.item','kind':'message','base_price':0,'weight':1})
    scenario['enabled']['offers'].append(notice['id']);scenario['initial']['offer_ids'].append(notice['id']);scenario['enabled']['items'].append('item.next_leader_notice')
    c['texts']['next.notice.offer']='转交本任首长死亡文书';c['texts']['next.notice.item']='本任首长死亡文书'
    scenario['task_message_notices']['bindings'].append({'offer_id':notice['id'],'decedent_actor_id':'society::actor.apprentice'})
    m.update(id='hanmo.political_successor_rounds',namespace='successorround',version='1.0.0',required_capabilities=c['required_capabilities'])
    out=a.repo/'content/packs/authored/political_successor_rounds_1_0';out.mkdir(parents=True,exist_ok=True)
    for name,value in [('content.json',c),('pack.json',m)]: (out/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
    selection=read('content/packs/selections/political-head-handovers-1.0.0.json');selection['packs'][-1]={'id':m['id'],'version':'1.0.0'};(a.repo/'content/packs/selections/political-successor-rounds-1.0.0.json').write_text(json.dumps(selection,ensure_ascii=False,indent=2)+'\n')

if __name__=='__main__':main()
