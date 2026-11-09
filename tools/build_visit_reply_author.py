"""青川拜帖后继只声明本人回复/实领入口；不预置回信、事实或角色接续。"""
import argparse
import copy
import json
from pathlib import Path
from build_personal_appointment_author import emit

def build(repo):
    """原接待者本人say受理两个动作，实际答复仍由受控的原收帖人明确选择。"""
    root=repo/'content/packs/authored';source=root/'task_contacts_1_11';target=root/'visit_replies_1_0'
    pack=json.loads((source/'pack.json').read_text());content=json.loads((source/'content.json').read_text())
    pack.update(id='hanmo.visit_replies',version='1.0.0',namespace='visit_reply');content.update(id='visit_replies',version='1.0.0')
    for document in (pack,content):document['required_capabilities'].append('visit_message_replies_v1')
    scene=copy.deepcopy(next(s for s in content['scenarios'] if s['id']=='scenario.qingchuan_reception'))
    scene.update(id='scenario.qingchuan_visit_replies',name_key='name.scenario.visit_replies');scene['enabled']['scenarios']=[scene['id']]
    scene['visit_reply_profile']='witnessed_visit_replies_v1';scene['visit_message_replies']={'version':1,'policies':[]}
    for i,original in enumerate(scene['visit_messages']['policies']):
        scene['visit_message_replies']['policies'].append({'id':'reply.'+str(i),'message_policy_id':original['id'],'reply_node_id':'reply','collect_node_id':'collect_reply'})
    content['scenarios']=[scene]
    content['texts'].update({'name.scenario.visit_replies':'青川拜帖：托回与亲领','option.visit.reply':'把已经亲领的拜帖答复托回。','line.visit.reply':'只有你亲自领读过的原帖，才可在这里留下答复。你可以婉拒，或说愿意另行当面联络；这都不是约期、婚约、引荐或交情承诺。请你本人在答复页核对原帖并选择。','option.visit.collect_reply':'亲自领取别人给我的答复。','line.visit.collect_reply':'请在答复页查看有没有给你的托回文书；只有本人实际领取以后，才会展示原答复。这里不会隔空通知寄件人，也不保证对方现在在何处。'})
    dialogue=next(d for d in content['dialogues'] if d['id']=='dialog.visit_reception');menu=next(n for n in dialogue['nodes'] if n['id']=='menu')
    for node,key in [('reply','reply'),('collect_reply','collect_reply')]:
        menu['options'].insert(-1,{'id':node,'text_key':'option.visit.'+key,'conditions':[],'next':node})
        dialogue['nodes'].append({'id':node,'kind':'say','speaker':'$provider','text_key':'line.visit.'+key,'next':'again'})
    emit(target/'pack.json',pack);emit(target/'content.json',content)
    emit(repo/'content/packs/selections/visit-replies-1.0.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.visit_replies','version':'1.0.0'}]})
    (target/'README.md').write_text('''# 青川拜帖回复 1.0

选择 visit-replies-1.0.0.json，场景 visit_reply::scenario.qingchuan_visit_replies。原学徒、习剑人和学舍接待者不变，保留原托留、本人领读、撤回和NPC当地问讯。

原收帖人实际领读以后，本人到原接待者处选择“托回答复”，停在接待者原say，有限页核对原帖并选择婉拒或另行当面联系。原发信人后来亲自来领取，确认成功后才可读答复；未领取前不显示内容。托回者也不能远程查询对方是否已经领取。回应不授预约、关系、位置或经济利益。

本版两个回复命令只允许实际玩家提交，旧NPC问讯只负责原帖领读，没有NPC自动回复。默认学徒开局不能因此被称为已可自然完成整条NPC往返信路；选择习剑人开局也不预造另一人的拜帖。两边本人入口是正规作者声明，完整自然往返仍需后继明确NPC本人回复意向，或既有合法控制者接续的实际过程，不能从测试切换metadata代替。没有预置信件、答复、收据、人物同意或新钱货。

本包只完成源码和作者引用门，未compose/Installed/GUI/World运行。旧1.11包和原资产均保留，不覆盖原作者。
''')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);build(p.parse_args().repo)
