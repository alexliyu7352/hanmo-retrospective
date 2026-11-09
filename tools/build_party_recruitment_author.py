"""在新青川版本添加NPC本人当面招伴声明；旧场景与Party初态不改。"""
import argparse
import copy
import json
from pathlib import Path


def emit(path,value):
    """新版本拒覆盖，普通作者流水另行验证。"""
    path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists():raise ValueError('refuse existing author: '+str(path))
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')


def build(repo):
    """原学师场所/演员/费用复用；问不同旅程只能触发NPC原声明，不能参数指定同意。"""
    root=repo/'content/packs/authored';old=root/'task_contacts_1_8';target=root/'task_contacts_1_9'
    pack=json.loads((old/'pack.json').read_text());content=json.loads((old/'content.json').read_text())
    for obj in (pack,content):obj['version']='1.9.0';obj['required_capabilities'].append('party_provider_consent_v1')
    role=copy.deepcopy(next(r for r in content['roles'] if r['id']=='role.companion'));role['id']='role.consent_companion';role['dialogue_id']='dialog.consent_companion';content['roles'].append(role)
    content['texts'].update({'name.scenario.recruitment':'青川招伴：本人说定','line.recruitment.greeting':'想邀我同行么？先说说去哪儿。我手头还有事，不是什么行程都去。','line.recruitment.menu':'卫简听你说明打算。','option.recruitment.near':'问他是否愿意一道走动。','option.recruitment.long':'问他是否愿意立刻远行数月。','line.recruitment.accept':'这一程我愿意同行。行路时每半日六钱吃用，由你照应；加入时不收钱，停驻也不收这笔路费。眼前说定，再一道走。','line.recruitment.decline':'数月远行，我现在不能答应。家中还有事，这次就不同行了。','option.recruitment.reconsider':'重新问清眼前的同行安排。'})
    content['dialogues'].append({'id':'dialog.consent_companion','context_role':role['id'],'entry':'greeting','nodes':[
        {'id':'greeting','kind':'say','speaker':'$provider','text_key':'line.recruitment.greeting','next':'request'},
        {'id':'request','kind':'choice','text_key':'line.recruitment.menu','options':[{'id':'near','text_key':'option.recruitment.near','conditions':[],'next':'willing'},{'id':'long','text_key':'option.recruitment.long','conditions':[],'next':'unwilling'},{'id':'leave','text_key':'option.leave','conditions':[],'next':'end'}]},
        {'id':'willing','kind':'say','speaker':'$provider','text_key':'line.recruitment.accept','next':'leave_menu'},
        {'id':'unwilling','kind':'say','speaker':'$provider','text_key':'line.recruitment.decline','next':'leave_menu'},
        {'id':'leave_menu','kind':'choice','text_key':'line.recruitment.menu','options':[{'id':'ask','text_key':'option.recruitment.reconsider','conditions':[],'next':'request'},{'id':'leave','text_key':'option.leave','conditions':[],'next':'end'}]},
        {'id':'end','kind':'end'}]})
    scene=copy.deepcopy(content['scenarios'][0]);scene['id']='scenario.qingchuan_recruitment';scene['name_key']='name.scenario.recruitment';scene['enabled']['scenarios']=[scene['id']]
    scene['enabled']['roles']=[role['id'] if x=='role.companion' else x for x in scene['enabled']['roles']]
    scene['enabled']['dialogues']=[role['dialogue_id'] if x=='dialog.companion' else x for x in scene['enabled']['dialogues']]
    for binding in scene['initial']['roles']:
        if binding['role_id']=='role.companion':binding['role_id']=role['id']
    scene['party_recruitment_profile']='provider_declaration_v1';scene['party_recruitment_consents']={'version':1,'declarations':[]}
    for leader,label in [('society::actor.apprentice','apprentice'),('society::actor.study_swordswoman','swordswoman')]:
        for node,decision in [('willing','accept'),('unwilling','decline')]:scene['party_recruitment_consents']['declarations'].append({'id':'recruitment.'+label+'.'+node,'leader_actor_id':leader,'companion_actor_id':role['initial_actor'],'role_id':role['id'],'dialogue_id':role['dialogue_id'],'node_id':node,'decision':decision})
    content['scenarios'].append(scene);emit(target/'pack.json',pack);emit(target/'content.json',content)
    emit(repo/'content/packs/selections/party-recruitment-1.9.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.task_contacts','version':'1.9.0'}]})
    (target/'README.md').write_text('''# 青川本人招伴 1.9

正规选择 `content/packs/selections/party-recruitment-1.9.0.json`，场景 `contacts::scenario.qingchuan_recruitment`。基于 1.8 新增独立招伴角色/对白和显式 `party_recruitment_consents/1` 规则；原场景、人物、Party 初态和费用不改。

原学徒或习剑人正常到青川学舍，与卫简的招伴岗位当面交谈。问眼前同行安排后，只有卫简实际说出愿意的当前节点出现原 `join_party` 确认入口；问数月远行则本人明确拒绝。声明不自动加入，玩家必须确认。点继续离开当前声明后需重新询问，旧页不能代新的本人许可。

原行旅每半日六钱由领队照原 Travel/Party 规则支付；加入与停驻不扣这笔钱。原信任、同地、占用与成员上限仍适用。作者没有新成员、知情事实或付款初态。源码/组件门不代正式 Installed、原引擎按钮连续试玩或 World 失败回滚。
''')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);build(parser.parse_args().repo)
