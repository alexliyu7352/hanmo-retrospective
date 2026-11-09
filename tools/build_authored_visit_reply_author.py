"""中断后新重建的1.1正常作者；保旧1.0原人物/资产，不生成任何消息或控制史。"""
import argparse,json
from pathlib import Path
from build_personal_appointment_author import emit

def build(repo):
    """唯一NPC在原真实领读后沿原接待对白托回固定答复，原发信人仍须亲领。"""
    root=repo/'content/packs/authored';source=root/'visit_replies_1_0';target=root/'visit_replies_1_1'
    pack=json.loads((source/'pack.json').read_text());content=json.loads((source/'content.json').read_text())
    for obj in [pack,content]:obj['version']='1.1.0';obj['required_capabilities'].append('visit_message_replies_v2')
    scene=content['scenarios'][0];scene['visit_reply_profile']='witnessed_authored_visit_replies_v2';scene['visit_message_replies']['version']=2
    scene['visit_reply_intents']={'version':1,'intents':[{'id':'reply.receiver','reception_intent_id':'reception.receiver','reply_policy_id':'reply.0','response':'contact_in_person','route':[{'node_id':'receive'},{'node_id':'again','option_id':'again'},{'node_id':'menu','option_id':'reply'}]}]}
    emit(target/'pack.json',pack);emit(target/'content.json',content)
    emit(repo/'content/packs/selections/visit-replies-1.1.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.visit_replies','version':'1.1.0'}]})
    (target/'README.md').write_text('''# 青川拜帖回复1.1

选择 visit-replies-1.1.0.json，场景 visit_reply::scenario.qingchuan_visit_replies。沿1.0全部原人物、财物、岗位和对白，仅显式选择回复2和一份原收帖人的本人意向。

学徒须先真实托留原帖并结束会面；习剑人只在原当地来访、原真实领读且同一认领会话后，沿原节点回复“另行当面联络”。学徒仍须后来向原接待者亲领才能知道答复，没有即时远程通知，也没有预约或关系授权。

回复时是否由NPC办理必须由当时原Family控制史证明；原认领之后若已合法成为玩家，只能沿本人实际命令答复，不能借旧claim省掉玩家永久结果。后来合法控制接续保留早先真实NPC回复。

此包为中断后的新重建源码，不冒原遗失作者字节。未运行compose、Installed、自然World或GUI；原1.0保持原字节，不预置消息、同意、关系或业务事实。
''')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);build(p.parse_args().repo)
