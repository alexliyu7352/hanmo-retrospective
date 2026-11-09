"""复用青川真实学舍构建独立私人预约作者；不改旧包，不运行compose或复制素材。"""
import argparse
import copy
import json
from pathlib import Path


def emit(path, data):
    """新源只落未存在路径，保原作者与任何失败现场。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        raise ValueError('refuse existing author: ' + str(path))
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')


def build(repo):
    """当前师者只相约闲谈，未来实际会面仍用原角色、原地点和一个半日行动。"""
    root = repo / 'content/packs/authored'
    source, target = root / 'task_contacts_1_11', root / 'personal_appointments_1_0'
    manifest = json.loads((source / 'pack.json').read_text())
    content = json.loads((source / 'content.json').read_text())
    manifest.update(id='hanmo.personal_appointments', version='1.0.0', namespace='appointments')
    content.update(id='personal_appointments', version='1.0.0')
    for value in (manifest, content):
        value['required_capabilities'].append('personal_appointments_v1')
    scene = copy.deepcopy(next(s for s in content['scenarios'] if s['id'] == 'scenario.qingchuan_letter_returns'))
    scene.update(id='scenario.qingchuan_personal_appointment', name_key='name.scenario.appointment')
    scene['enabled']['scenarios'] = [scene['id']]
    scene['personal_appointment_profile'] = 'witnessed_personal_meetings_v1'
    scene['personal_appointments'] = {'version': 1, 'policies': [{
        'id': 'appointment.school_visit', 'provider_actor_id': 'society::actor.tutor',
        'role_id': 'role.sender', 'dialogue_id': 'dialog.sender',
        'accept_node_id': 'appointment_accept', 'cancel_node_id': 'appointment_cancel',
        'meet_node_id': 'appointment_meet', 'earliest_offset': 2, 'latest_offset': 4, 'duration_ticks': 1}]}
    content['scenarios'] = [scene]
    content['texts'].update({'name.scenario.appointment': '青川会面：有约而来',
        'option.appointment.accept': '另约个时候，再来与先生叙谈。',
        'option.appointment.cancel': '谈谈取消我先前的这次约定。',
        'option.appointment.meet': '我按先前约定来赴会。',
        'line.appointment.accept': '从现在起第二至第四个半日，可到这处学舍来寻我。你若愿意，我们届时叙谈半日；不收费，也不是授艺的约定。仍要你当面确认。',
        'line.appointment.cancel': '若你要取消先前这份还未开始的约定，我同意当面说清。请核对自己的那份约期再确认。',
        'line.appointment.meet': '按原先约定，若此刻正在约期内，我们就在这里叙谈半日。现在由你确认开始。'})
    dialogue = next(d for d in content['dialogues'] if d['id'] == 'dialog.sender')
    menu = next(n for n in dialogue['nodes'] if n['id'] == 'menu')
    content['texts'].update({'option.appointment.menu': '另谈私人会面约定。', 'line.appointment.menu': '约定、赴会或取消，请当面说清。', 'option.appointment.back': '回到其他事务。'})
    menu['options'].insert(-1, {'id': 'appointment_menu', 'text_key': 'option.appointment.menu', 'conditions': [], 'next': 'appointment_menu'})
    appointment_menu = {'id': 'appointment_menu', 'kind': 'choice', 'text_key': 'line.appointment.menu', 'options': []}
    dialogue['nodes'].append(appointment_menu)
    for purpose in ('accept', 'cancel', 'meet'):
        node = 'appointment_' + purpose
        appointment_menu['options'].append({'id': node, 'text_key': 'option.appointment.' + purpose, 'conditions': [], 'next': node})
        dialogue['nodes'].append({'id': node, 'kind': 'say', 'speaker': '$provider', 'text_key': 'line.appointment.' + purpose, 'next': 'appointment_menu'})
    appointment_menu['options'].append({'id': 'back', 'text_key': 'option.appointment.back', 'conditions': [], 'next': 'menu'})
    emit(target / 'pack.json', manifest)
    emit(target / 'content.json', content)
    emit(repo / 'content/packs/selections/personal-appointments-1.0.0.json', {'format': 'HM-PACK-SELECTION', 'format_version': 1, 'packs': [
        {'id': 'hanmo.local_society', 'version': '1.11.0'}, {'id': 'hanmo.personal_appointments', 'version': '1.0.0'}]})
    (target / 'README.md').write_text('''# 青川私人会面预约 1.0

选择 personal-appointments-1.0.0.json，场景 appointments::scenario.qingchuan_personal_appointment。

沿原学舍见原学师，选择另约叙谈，实际到达本人答允的说话节点后，在本人预约页确认。窗口为接受时刻后的第二至第四个半日，真正会面一个半日；接受不预占人物、不收费，也不获得教学或公职权利。

离开后按正常时间生活、再从原地点与同一本人交谈。到赴会节点才可开始；太早、错过、缺人或有原Action均由业务规则解释。取消也须同一本人当面到取消声明，不能远程假装消息已送达。相交约期拒绝；错过只保原窗口，不自动惩罚关系。普通任务和其他生活仍独立可选。

这里只是首片当面约定、实际赴会与当面取消。远程答复、正式改约、授权代表和NPC主动赴约仍属后继；不是完整预约系统验收通过。原初态无预约/完成事实。未compose、未Installed、未运行World或GUI，也没有复制资产。
''')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', required=True, type=Path)
    build(parser.parse_args().repo)
