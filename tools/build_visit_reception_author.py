"""独立1.11作者让真实NPC在当地定期问讯；不种拜帖或Director会话/计划。"""
import argparse,copy,json
from pathlib import Path
from build_visit_message_author import write


def build(repo):
    """旧1.10保留；只让习剑人在学舍按本人意向领读学徒可能托留的拜帖。"""
    root=repo/'content/packs/authored';old=root/'task_contacts_1_10';target=root/'task_contacts_1_11';pack=json.loads((old/'pack.json').read_text());content=json.loads((old/'content.json').read_text())
    for value in (pack,content):value['version']='1.11.0';value['required_capabilities'].append('visit_reception_v1')
    scene=copy.deepcopy(content['scenarios'][-1]);scene['id']='scenario.qingchuan_reception';scene['name_key']='name.scenario.reception';scene['enabled']['scenarios']=[scene['id']];content['texts']['name.scenario.reception']='青川拜帖：本人来取'
    receiver='society::actor.study_swordswoman';source=scene['visit_messages']['policies'][0]
    scene['visit_reception_profile']='authored_local_reception_v1';scene['visit_reception_intents']={'version':1,'intents':[{'id':'reception.receiver','actor_id':receiver,'message_policy_id':source['id'],'purpose':'receive','cooldown_ticks':4,'route':[{'node_id':'greeting'},{'node_id':'menu','option_id':'receive'}]}]}
    for actor in scene['initial']['actors']:
        if actor['actor_id']==receiver:actor['place_id']='society::place.school'
    scene['dynamic_world']['director']['subjects'][receiver]={'templates':['maintain_commitment','visit_reception','await_adjustment'],'request_arrear_after_ticks':1,'decline_work_after_ticks':2,'leave_after_ticks':3,'report_source_keys':[]}
    content['scenarios'].append(scene);write(target/'pack.json',pack);write(target/'content.json',content)
    write(repo/'content/packs/selections/visit-reception-1.11.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.task_contacts','version':'1.11.0'}]})
    (target/'README.md').write_text('''# 青川本人领读 1.11

选择 `visit-reception-1.11.0.json`，新场景 `contacts::scenario.qingchuan_reception`。学徒为原玩家，习剑人按本场景真实初始地点在学舍。她会按明确本人意向，在原公平预算内向先生的拜帖岗位交谈并问讯；没有待收帖也会正常问讯、离开并隔四个半日再问。

玩家实际到学舍交谈、托留自己的原拜帖并结束会话后，习剑人只有亲自推进至先生的领读声明才可得知并领取。原Task责任/Action优先，接待人被占用时等待。原寄件人仍不能隔空知道已经领取。没有预造信件、会话、事实、同意或会面预约；目录/源码可用不代表正式World与GUI已通过。

旧1.10与所有旧场景保持原样。新来访目录只有作者意向，进度使用原Director计划，认领使用原Talk实际建立的session。当前版本只做本地问讯，不因远方未知信件自动寻路。
''')


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);build(p.parse_args().repo)
