"""正常书面赴约1.2后继只显式选原托领撤实际结果段，不预制消息、控制或提交历史。"""
import argparse,json
from pathlib import Path
from build_personal_appointment_author import emit

def build(repo):
    source=repo/'content/packs/authored/visit_appointments_1_1'
    target=repo/'content/packs/authored/visit_appointments_1_2'
    pack=json.loads((source/'pack.json').read_text());data=json.loads((source/'content.json').read_text())
    for document in (pack,data):
        document['version']='1.2.0'
        document['required_capabilities'].append('visit_message_receipts_v1')
    scene=data['scenarios'][0];scene['id']='scenario.qingchuan_visit_receipts';scene['enabled']['scenarios']=[scene['id']];scene['name_key']='name.scenario.visit_receipts'
    scene['visit_message_receipt_profile']='permanent_visit_message_results_v1'
    data['texts'][scene['name_key']]='青川拜帖：真实托领撤与书面赴约'
    emit(target/'pack.json',pack);emit(target/'content.json',data)
    emit(repo/'content/packs/selections/visit-appointments-1.2.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.visit_appointments','version':'1.2.0'}]})
    (target/'README.md').write_text('''# 青川拜帖原始回执 1.2

选择 visit-appointments-1.2.0.json，场景 visit_appointments::scenario.qingchuan_visit_receipts。沿原1.1真实托帖、领读、固定日期邀约与本人赴约动线，原版本不覆盖。

本版另选 visit_message_receipts/1，把玩家每次托/领/撤帖绑定真正本次结果；NPC沿原明确接待意向、原会话认领和实际Advance办理。未有完整原控制提交组证明时，NPC在真实控制切换的同半日暂缓托领，下一半日再按原预算复评，不回填旧信或偷换提交人。玩家三种本人办理仍沿原真实say和原命令。

旧已发生消息而没有新永久凭据的银行不升级。没有预置消息、绑定、控制、会面或结果；必须与本版Native完整组配对，当前仅源码，未验自然World/GUI。
''')
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);build(parser.parse_args().repo)
