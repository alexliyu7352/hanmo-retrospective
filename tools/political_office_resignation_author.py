#!/usr/bin/env python3
"""正常原首府新增下属辞任场景，保原公职/亲属命令，绝不预置申请、批准或换控事实。"""
import argparse,copy,json,subprocess
from pathlib import Path

def make(read):
    """固定旧作者只作为定义输入，新申请人的初职与专用空公库明确列出。"""
    root='content/packs/authored/political_successions_1_0/'
    c,m=read(root+'content.json'),read(root+'pack.json');s=c['scenarios'][0];d=s['dynamic_world'];org=d['organization']
    c['id']='political_office_resignations';s['id']='scenario.qingchuan_office_resignations';s['seed_text']='青川下属辞任/1.0.0';c['texts'][s['name_key']]='青川属吏当面辞任';c['required_capabilities'].append('political_office_resignations_v1')
    template=read('content/packs/authored/local_society_1_11/content.json');actor=copy.deepcopy(next(a for a in template['actors'] if a['id']=='actor.apprentice'))
    actor.update(id='actor.retiring_clerk',name_key='resignation.clerk.name',initial_age_years=66,initial_place='society::place.courtyard',initial_goods=[],source_note='本独立情景的六十六岁原创粮务属吏。原目录明确在任；无辞任申请、审批结果、收养或换控事实。')
    c['actors'].append(actor);c['texts']['resignation.clerk.name']='许谦';s['enabled']['actors'].append(actor['id']);s['playable_actor_ids'].append(actor['id']);s['playable_actor_appearance_policies'].append({'actor_id':actor['id'],'policy':'preserve_existing_v1'});s['player_actor']=actor['id'];s['player_start_descriptions'].append({'actor_id':actor['id'],'text_key':'resignation.start'});c['texts']['resignation.start']='原粮务属吏许谦到首府当面申请辞任。未获原首长批准前仍在原任，旧约与债权照旧。'
    a=copy.deepcopy(next(a for a in s['initial']['actors'] if a['actor_id']=='society::actor.apprentice'));a.update(actor_id=actor['id'],age_years=66,initial_goods=[]);s['initial']['actors'].append(a)
    for wallet in [{'id':'wallet.retiring_clerk','owner':{'kind':'actor','id':actor['id']},'initial_balance':20},{'id':'wallet.grain_office','owner':{'kind':'organization','id':'society::org.city'},'initial_balance':0}]:
        c['wallets'].append(wallet);s['enabled']['wallets'].append(wallet['id']);s['initial']['wallets'].append(copy.deepcopy(wallet))
    stock={'id':'inventory.grain_office','owner':{'kind':'organization','id':'society::org.city'},'capacity':100,'goods':[]};c['inventories'].append(stock);s['enabled']['inventories'].append(stock['id']);s['initial']['inventories'].append(copy.deepcopy(stock))
    org['offices']['office.grain_clerk']={'id':'office.grain_clerk','organization_id':'society::org.city','holder_actor_id':actor['id'],'deputy_actor_id':None,'epoch':1,'status':'active','grant_ids':[]};org['organizations']['society::org.city']['office_ids'].append('office.grain_clerk')
    # 新辖区沿原政治财政要求明确本职税务权限，公库空载且与首府不共用；辞任不消费该权限。
    org['grants']['grant.grain_clerk_fiscal']={'id':'grant.grain_clerk_fiscal','office_id':'office.grain_clerk','allowed_command_kinds':['political_set_tax'],'scopes':[{'kind':'site','id':'society::place.grain'}],'wallet_ids':['wallet.grain_office'],'inventory_ids':['inventory.grain_office'],'amount_limit':0,'may_delegate':False,'valid_from':0,'valid_until':None,'continuity_policy_id':'stop_when_vacant'}
    org['offices']['office.grain_clerk']['grant_ids'].append('grant.grain_clerk_fiscal')
    org['members']['member.retiring_clerk']={'id':'member.retiring_clerk','actor_id':actor['id'],'organization_id':'society::org.city','duty_ids':[],'policy_id':'policy.city','response':'serving','response_since_tick':0,'next_review_tick':1,'last_fact_id':''}
    d['local_economy']['named_actors_excluded'].append(actor['id']);d['director']['admissions']['a.qingchuan_steward']['excluded_actor_ids'].append(actor['id'])
    d['politics']['jurisdictions']['jurisdiction.grain_office']={'id':'jurisdiction.grain_office','name_key':'resignation.district','office_name_key':'resignation.office','office_id':'office.grain_clerk','political_organization_id':'society::org.city','controlling_organization_id':'society::org.city','revision':0,'seat_place_id':'society::place.grain','public_wallet_id':'wallet.grain_office','public_inventory_ids':['inventory.grain_office']};c['texts'].update({'resignation.district':'青川粮务辖区','resignation.office':'粮务属吏'})
    grant=copy.deepcopy(org['grants']['grant.first_successor']);grant.update(id='grant.resignation_approval',allowed_command_kinds=['political_answer_office_resignation']);org['grants'][grant['id']]=grant;org['offices']['office.city_steward']['grant_ids'].append(grant['id'])
    d['political_office_resignations']={'version':1,'policies':{'policy.grain_clerk_resignation':{'id':'policy.grain_clerk_resignation','jurisdiction_id':'jurisdiction.grain_office','approval_succession_policy_id':'policy.first_successor','approval_grant_id':grant['id'],'response_ticks':80}}}
    for kind in ['political_request_office_resignation','political_answer_office_resignation','political_withdraw_office_resignation']:
        key='resignation.command.'+kind;c['texts'][key]=kind;d['politics']['command_name_keys'][kind]=key
    # 原Family零资产自愿接续仅令操作者成为首长；批准权限始终来自该首长自己原Office/Grant。
    head='society::actor.city_steward'
    for suffix,node,text in [('adoption','request','我愿与你当面办理成年收养。此亲属关系不授予你我的公职，也不批准你的辞任。'),('control','consent','若成年收养已经依法完成，我愿接续本人控制，私资为零。辞任仍由我凭自己的原首职和专项权另行决定。')]:
        role='role.resignation_'+suffix;dialog='dialog.resignation_'+suffix;key='resignation.'+suffix;c['texts'][key]=text
        c['roles'].append({'id':role,'name_key':'resignation.role','organization_id':None,'initial_actor':head,'place_id':'society::place.courtyard','services':['information','deliver_message'],'dialogue_id':dialog});s['enabled']['roles'].append(role);s['initial']['roles'].append({'role_id':role,'actor_id':head,'enabled':True});s['enabled']['dialogues'].append(dialog)
        c['dialogues'].append({'id':dialog,'context_role':role,'entry':node,'nodes':[{'id':node,'kind':'say','speaker':'$provider','text_key':key,'next':'end'},{'id':'end','kind':'end'}]})
    c['texts']['resignation.role']='首府本人商谈'
    c['adult_adoption_requests'].append({'id':'request.resignation_adoption','adopter_actor_id':actor['id'],'adoptee_actor_id':head,'dialogue_id':'dialog.resignation_adoption','node_id':'request','terms_version':1,'consent_kind':'adoptee_requests_adoption_v1','scope':'adult_adoption_only'});s['adult_adoption']['request_ids'].append('request.resignation_adoption')
    c['succession_consent_requests'].append({'id':'request.resignation_control','predecessor_actor_id':actor['id'],'successor_actor_id':head,'dialogue_id':'dialog.resignation_control','node_id':'consent','terms_version':1,'consent_kind':'successor_accepts_voluntary_succession_v1','scope':'player_control_and_selected_personal_assets','personal':{'money':0,'goods':[]}});s['voluntary_succession']['request_ids'].append('request.resignation_control')
    m.update(id='hanmo.political_office_resignations',namespace='officeresign',version='1.0.0',required_capabilities=c['required_capabilities'])
    selection=read('content/packs/selections/political-successions-1.0.0.json');selection['packs'][-1]={'id':m['id'],'version':'1.0.0'}
    return c,m,selection

def main():
    """只读调用方固定Git原件，另存新版本作者和正常选择清单。"""
    p=argparse.ArgumentParser();p.add_argument('--base',required=True);p.add_argument('--repo',type=Path,default=Path('.'));a=p.parse_args()
    def read(path):return json.loads(subprocess.check_output(['git','show',a.base+':'+path],cwd=a.repo))
    c,m,selection=make(read);out=a.repo/'content/packs/authored/political_office_resignations_1_0';out.mkdir(parents=True,exist_ok=True)
    for name,value in [('content.json',c),('pack.json',m)]: (out/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
    (a.repo/'content/packs/selections/political-office-resignations-1.0.0.json').write_text(json.dumps(selection,ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
