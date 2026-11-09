"""青川原会面后继作者，只声明可问话题；不注入见闻、答案、行动或新的钱货。"""
import argparse
import json
from pathlib import Path
from build_personal_appointment_author import emit

def build(repo):
    """原导师私人say由本人表态；问询按钮独立准备/确认，不在对白选项自动开启。"""
    root=repo/'content/packs/authored';source=root/'personal_appointments_1_1';target=root/'personal_appointments_1_2'
    manifest=json.loads((source/'pack.json').read_text());content=json.loads((source/'content.json').read_text())
    manifest['version']=content['version']='1.2.0'
    for document in [manifest,content]:document['required_capabilities'].append('contact_inquiries_v1')
    scene=content['scenarios'][0];scene.update(id='scenario.qingchuan_contact_inquiries',name_key='name.scenario.contact_inquiries');scene['enabled']['scenarios']=[scene['id']]
    scene['contact_inquiry_profile']='witnessed_personal_inquiries_v1';scene['contact_inquiries']={'version':1,'policies':[]}
    content['texts'].update({'name.scenario.contact_inquiries':'青川会面：问旧识去向','option.inquiry.menu':'有熟人的去向，想向先生打听。','line.inquiry.menu':'先说清想问谁。先生只谈自己的亲见，远处现况未必知道。','option.inquiry.share':'问卫简先生最近一次见于何处。','line.inquiry.share':'卫简的事，我们可以坐下来细谈半日。我只说自己亲自见过的；不知的便说不知，也不能保他如今仍在那里。要不要花这半日，由你确认。','option.inquiry.decline':'问剑院执事的私人行止。','line.inquiry.decline':'关于执事的私事，可以把你的来意当面说清；是否回答，我自有分寸。若要详谈仍需半日，请你先想好。','option.inquiry.back':'先回到其他事务。'})
    dialogue=next(d for d in content['dialogues'] if d['id']=='dialog.sender');menu=next(n for n in dialogue['nodes'] if n['id']=='menu')
    menu['options'].insert(-1,{'id':'inquiry_menu','text_key':'option.inquiry.menu','conditions':[],'next':'inquiry_menu'})
    inquiry_menu={'id':'inquiry_menu','kind':'choice','text_key':'line.inquiry.menu','options':[]};dialogue['nodes'].append(inquiry_menu)
    for purpose,subject,disclosure in [('share','society::actor.study_caravanner','share_last_seen'),('decline','society::actor.sect_steward','decline')]:
        node='inquiry_'+purpose
        inquiry_menu['options'].append({'id':node,'text_key':'option.inquiry.'+purpose,'conditions':[],'next':node})
        dialogue['nodes'].append({'id':node,'kind':'say','speaker':'$provider','text_key':'line.inquiry.'+purpose,'next':'inquiry_menu'})
        scene['contact_inquiries']['policies'].append({'id':'inquiry.'+purpose,'provider_actor_id':'society::actor.tutor','subject_actor_id':subject,'role_id':'role.sender','dialogue_id':'dialog.sender','node_id':node,'disclosure':disclosure,'duration_ticks':1})
    inquiry_menu['options'].append({'id':'back','text_key':'option.inquiry.back','conditions':[],'next':'menu'})
    emit(target/'pack.json',manifest);emit(target/'content.json',content)
    emit(repo/'content/packs/selections/personal-appointments-1.2.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.personal_appointments','version':'1.2.0'}]})
    (target/'README.md').write_text('''# 青川当面问去向 1.2

选择 personal-appointments-1.2.0.json，场景 appointments::scenario.qingchuan_contact_inquiries。

沿原学舍私事岗位，与导师真实交谈，选择问去向，再停在导师本人原 say。有限页只显示对象和一个半日占用，经原生重新报价和本人确认后开始；本人 Continue 完成后，才会出现真实答复。普通对白选择不启动行动，也不提前给出答案。

分享题目只读取导师在开工前已真实发生的预约/信件实交/返报亲见；没有来源就明确不知道。第二题只在实际会面完成后记录未透露，不授予任何位置知识。答复保原亲见时刻和本次听到时刻，不能当远方当前地点。失败不生成新知识。

本作者没有预置任何接触、任务受领、答案或过去成功。默认新局不保证导师已经与题目所问NPC产生具名亲见，所以不能把本包称为已命中 known 答复分支。已知答复须在同一真实世界先形成这些原来源；后续合法接续的人物组合可消费旧亲见。当前只作正规显式作者输入，未compose、Installed、GUI或自然World运行。

原1.0/1.1内容和依赖没有覆盖；本后继保留原约会、信件、同行和费用，不新增资产或社交奖励。
''')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);build(p.parse_args().repo)
