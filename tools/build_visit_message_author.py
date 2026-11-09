"""新增独立青川拜帖作者版本；不改旧包、不预置留言/领读事实，也不复制资产。"""
import argparse, copy, json
from pathlib import Path


def write(path,value):
    """作者新文件拒覆盖，只有正式compose另行准入。"""
    path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists(): raise ValueError('existing author '+str(path))
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')


def build(repo):
    """复用原青川演员和学舍接待岗位，让来访意向可真实托留与撤回。"""
    root=repo/'content/packs/authored';old=root/'task_contacts_1_9';target=root/'task_contacts_1_10'
    pack=json.loads((old/'pack.json').read_text());content=json.loads((old/'content.json').read_text())
    for value in (pack,content):value['version']='1.10.0';value['required_capabilities'].append('visit_messages_v1')
    role=copy.deepcopy(next(row for row in content['roles'] if row['id']=='role.sender'));role.update(id='role.visit_reception',name_key='name.role.visit_reception',dialogue_id='dialog.visit_reception',services=['information'])
    content['roles'].append(role)
    content['texts'].update({'name.role.visit_reception':'拜帖接待','name.scenario.visits':'青川拜帖：当面托留','line.visit.greeting':'有事寻人么？若只是留个来访口信，我可在这里替你收着。须本人来取，不替谁答应会面。','line.visit.menu':'先向接待人说清要办理的事。','option.visit.deposit':'想给熟人留一封拜帖。','option.visit.receive':'问是否有留给本人的拜帖。','option.visit.withdraw':'想撤回本人托留的拜帖。','line.visit.deposit':'你若确认，我只记你来过、希望另约会面。不记其他消息，也不收费。对方何时来取，我不会隔空告诉你。','line.visit.receive':'我只将留给你的拜帖交给你本人。先核对，再亲自领读；领到也不等于答应赴约。','line.visit.withdraw':'还在我这里的原帖，你本人可以撤回。若已亲手交出去，就不能再收回。','option.visit.again':'换一件拜帖事务再问。'})
    content['dialogues'].append({'id':role['dialogue_id'],'context_role':role['id'],'entry':'greeting','nodes':[
        {'id':'greeting','kind':'say','speaker':'$provider','text_key':'line.visit.greeting','next':'menu'},
        {'id':'menu','kind':'choice','text_key':'line.visit.menu','options':[{'id':key,'text_key':'option.visit.'+key,'conditions':[],'next':key} for key in ('deposit','receive','withdraw')]+[{'id':'leave','text_key':'option.leave','conditions':[],'next':'end'}]},
        *[{'id':key,'kind':'say','speaker':'$provider','text_key':'line.visit.'+key,'next':'again'} for key in ('deposit','receive','withdraw')],
        {'id':'again','kind':'choice','text_key':'line.visit.menu','options':[{'id':'again','text_key':'option.visit.again','conditions':[],'next':'menu'},{'id':'leave','text_key':'option.leave','conditions':[],'next':'end'}]},
        {'id':'end','kind':'end'}]})
    scene=copy.deepcopy(content['scenarios'][-1]);scene['id']='scenario.qingchuan_visits';scene['name_key']='name.scenario.visits';scene['enabled']['scenarios']=[scene['id']]
    scene['enabled']['roles'].append(role['id']);scene['enabled']['dialogues'].append(role['dialogue_id']);scene['initial']['roles'].append({'role_id':role['id'],'actor_id':role['initial_actor'],'enabled':True})
    scene['visit_message_profile']='witnessed_reception_v1';scene['visit_messages']={'version':1,'policies':[]}
    actors=['society::actor.apprentice','society::actor.study_swordswoman']
    for n,sender in enumerate(actors):scene['visit_messages']['policies'].append({'id':'visit.'+str(n),'sender_actor_id':sender,'recipient_actor_id':actors[1-n],'custodian_actor_id':role['initial_actor'],'role_id':role['id'],'dialogue_id':role['dialogue_id'],'deposit_node_id':'deposit','receive_node_id':'receive','withdraw_node_id':'withdraw'})
    content['scenarios'].append(scene);write(target/'pack.json',pack);write(target/'content.json',content)
    write(repo/'content/packs/selections/visit-messages-1.10.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.task_contacts','version':'1.10.0'}]})
    (target/'README.md').write_text('''# 青川当面拜帖 1.10

选择 `content/packs/selections/visit-messages-1.10.0.json`，新场景 `contacts::scenario.qingchuan_visits`。旧 1.9 与原场景保持原样，新场景显式选择 `visit_messages/1`。

学徒与习剑人均可按原开局选择。到青川学舍，找先生的拜帖接待岗位，真实交谈到托留/领读/撤回的本人声明，再核对确认。每人只能把固定来访意向留给原另一人；不含任意事实、费用或预约承诺。确认后继续对话或离开，本人侧栏保有限历史；已领读结果不会遥传原寄件人。

初态没有拜帖或接收记录。此版本作者可直接办理托留与撤回，完整领读须另一人实际成为命令主体并亲自到场；当前没有新增 NPC 自动来访/领读策略，不能把两套开局拼成同一封信的自然完成证据。该可达性后继与正式 World、GUI 验收均尚待完成。没有为了演示预置消息或倒转会话。
''')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);build(parser.parse_args().repo)
