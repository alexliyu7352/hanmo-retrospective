"""从原会面预约1.0生成独立1.1作者；只增加原provider有限赴约意向，不预制承诺或复制素材。"""
import argparse
import json
from pathlib import Path
from build_personal_appointment_author import emit


def build(repo):
    """原导师在原Director预算内遵守真实已接受约定；不替玩家到场或开始会面。"""
    root = repo / 'content/packs/authored'
    old = root / 'personal_appointments_1_0'
    target = root / 'personal_appointments_1_1'
    manifest = json.loads((old / 'pack.json').read_text())
    content = json.loads((old / 'content.json').read_text())
    manifest['version'] = '1.1.0'
    content['version'] = '1.1.0'
    for value in (manifest, content):
        value['required_capabilities'].append('personal_appointment_attendance_v1')
    scene = content['scenarios'][0]
    scene['id'] = 'scenario.qingchuan_personal_attendance'
    scene['name_key'] = 'name.scenario.appointment_attendance'
    scene['enabled']['scenarios'] = [scene['id']]
    scene['dynamic_world']['director']['personal_appointment_attendance'] = {
        'version': 1, 'policies': [{'appointment_policy_id': 'appointment.school_visit', 'depart_before_ticks': 1}]}
    scene['dynamic_world']['director']['subjects']['society::actor.tutor'] = {
        'templates': ['maintain_commitment', 'personal_appointment_attendance'],
        'request_arrear_after_ticks': 1, 'decline_work_after_ticks': 2, 'leave_after_ticks': 3,
        'report_source_keys': []}
    content['texts']['name.scenario.appointment_attendance'] = '青川会面：守候原约'
    emit(target / 'pack.json', manifest)
    emit(target / 'content.json', content)
    emit(repo / 'content/packs/selections/personal-appointments-1.1.0.json', {
        'format': 'HM-PACK-SELECTION', 'format_version': 1, 'packs': [
            {'id': 'hanmo.local_society', 'version': '1.11.0'}, {'id': 'hanmo.personal_appointments', 'version': '1.1.0'}]})
    (target / 'README.md').write_text('''# 守候原约 1.1

沿 personal-appointments-1.1.0.json 选择 appointments::scenario.qingchuan_personal_attendance。

保原1.0当面答允、取消、赴会三节点。导师没有预置预约；只有玩家真实接受后，原导师才在原Director预算内从开始窗口前一个半日复评原地点。原已受领任务、军旅与既有Action优先；无关会话不会被强行关闭。

到场等待不创建Wait Action，因此玩家仍可与本人真实交谈并开始半日会面。离开原地点时只提原相邻移动或正常行旅，不瞬移、不读取对方远方坐标、不保证克服路费或军事占用。窗口过后继续普通生活，不补造失约通知或惩罚。当前控制者若变成导师，则停止替玩家自主赴约。

这是显式赴约意向作者，尚未compose、Installed或运行自然World；所有输入都是声明，没有预置成功或额外资产。
''')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    build(parser.parse_args().repo)
