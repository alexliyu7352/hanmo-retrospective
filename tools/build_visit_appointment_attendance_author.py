"""原1.0完整约期流程的正常1.1后继，显式选原模板15赴本人自己发出的邀请。"""
import argparse,json
from pathlib import Path
from build_personal_appointment_author import emit

def build(repo):
    source=repo/'content/packs/authored/visit_appointments_1_0'
    target=repo/'content/packs/authored/visit_appointments_1_1'
    pack=json.loads((source/'pack.json').read_text());data=json.loads((source/'content.json').read_text())
    for document in (pack,data):
        document['version']='1.1.0'
        document['required_capabilities'] += ['personal_appointment_attendance_v1','visit_appointment_attendance_v1']
    scene=data['scenarios'][0];scene['id']='scenario.qingchuan_written_attendance';scene['enabled']['scenarios']=[scene['id']];scene['name_key']='name.scenario.written_attendance'
    scene['visit_appointment_attendance']={'version':1,'invitation_policy_ids':['invitation.swordswoman']}
    director=scene['dynamic_world']['director'];director['personal_appointment_attendance']={'version':1,'policies':[{'appointment_policy_id':'appointment.swordswoman','depart_before_ticks':4}]}
    templates=director['subjects']['society::actor.study_swordswoman']['templates'];templates.insert(templates.index('await_adjustment'),'personal_appointment_attendance')
    data['texts']['name.scenario.written_attendance']='青川拜帖：按本人发出的约期赴约'
    emit(target/'pack.json',pack);emit(target/'content.json',data)
    emit(repo/'content/packs/selections/visit-appointments-1.1.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.visit_appointments','version':'1.1.0'}]})
    (target/'README.md').write_text('''# 青川书面赴约 1.1

选择 visit-appointments-1.1.0.json，场景 visit_appointments::scenario.qingchuan_written_attendance。沿原1.0的实际托帖、收帖、发出固定邀约、领回、本人接受和当面叙谈流程；原1.0文件不覆盖。

本版明确选原模板15及原会面政策：习剑人在原窗口前四个半日开始复评本人已经发出的日期承诺。承诺存在才会赴原学舍；原Task责任、正在进行的行动和真实会话先行。移动仍用原邻地/道路行旅及原费用，不保证赶到、不延长日期。到场只留空闲等待，不替学徒领回或接受，不自己创建双人会面。是否已被远方领回/接受不影响发邀人自己的承诺，也不显示给她。

没有预置邀请、答复、Action、到场事实或未来NPC登记。新赴约目录只选择原invitation.swordswoman，提前量来自原personal_appointment_attendance政策；停止使用新目录时旧模板15仍只消费旧当面预约。新显式能力与Native完整共享组配对后才能正式准入；本包当前仅源码，未运行自然World或GUI。
''')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);build(p.parse_args().repo)
