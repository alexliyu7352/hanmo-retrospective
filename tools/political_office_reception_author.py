#!/usr/bin/env python3
"""沿固定原生前交接作者新增纯公共柜台，不预置岗位移交、控制变化或任职结果。"""
import argparse,copy,json,subprocess
from pathlib import Path

def main():
    """只添加独立Role/对白、原Office专项Grant与显式选择，真实原任职流程沿现有作者。"""
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,default=Path('.'));parser.add_argument('--base',required=True);a=parser.parse_args()
    def read(path):return json.loads(subprocess.check_output(['git','show',a.base+':'+path],cwd=a.repo))
    root='content/packs/authored/political_term_handovers_1_0/';content,pack=read(root+'content.json'),read(root+'pack.json');content['id']='political_office_receptions';scenario=content['scenarios'][0];scenario['id']='scenario.qingchuan_office_reception';content['texts'][scenario['name_key']]='青川公职接待岗位移交';content['required_capabilities'].append('political_office_receptions_v1');dynamic=scenario['dynamic_world'];org=dynamic['organization']
    grant=copy.deepcopy(org['grants']['grant.term_handover']);grant.update(id='grant.office_reception',allowed_command_kinds=['political_assume_office_reception']);org['grants'][grant['id']]=grant;org['offices']['office.city_steward']['grant_ids'].append(grant['id'])
    role={'id':'role.public_reception','name_key':'reception.role.name','organization_id':'society::org.city','initial_actor':'society::actor.city_steward','place_id':'society::place.courtyard','services':['information'],'dialogue_id':'dialog.public_reception'}
    content['roles'].append(role);content['dialogues'].append({'id':'dialog.public_reception','context_role':role['id'],'entry':'greeting','nodes':[{'id':'greeting','kind':'say','speaker':'$provider','text_key':'reception.greeting','next':'leave'},{'id':'leave','kind':'choice','text_key':'reception.greeting','options':[{'id':'leave','text_key':'reception.leave','conditions':[],'next':'end'}]},{'id':'end','kind':'end'}]});scenario['enabled']['roles'].append(role['id']);scenario['initial']['roles'].append({'role_id':role['id'],'actor_id':role['initial_actor'],'enabled':True});content['texts'].update({'reception.role.name':'公署接待席','reception.greeting':'此处按公署现行职权接待。具体公事仍须逐项核准。','reception.leave':'告辞','reception.command':'本人接任公共接待席'});dynamic['politics']['command_name_keys']['political_assume_office_reception']='reception.command'
    dynamic['political_office_receptions']={'version':1,'policies':{'policy.office_reception':{'id':'policy.office_reception','succession_policy_id':'policy.first_successor','role_id':role['id'],'reception_grant_id':grant['id']}}};pack.update(id='hanmo.political_office_receptions',namespace='officereception',version='1.0.0',required_capabilities=content['required_capabilities']);out=a.repo/'content/packs/authored/political_office_receptions_1_0';out.mkdir(parents=True,exist_ok=True)
    for name,value in [('content.json',content),('pack.json',pack)]:(out/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
    selection=read('content/packs/selections/political-term-handovers-1.0.0.json');selection['packs'][-1]={'id':pack['id'],'version':'1.0.0'};(a.repo/'content/packs/selections/political-office-receptions-1.0.0.json').write_text(json.dumps(selection,ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
