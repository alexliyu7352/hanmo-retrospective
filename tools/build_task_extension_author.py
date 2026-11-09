"""复用青川真实私信原约生成独立延期作者，不更改旧包或预置受领/同意事实。"""
import argparse
import copy
import json
from pathlib import Path


def emit(path, value):
    """仅写未存在的新源文件；不compose、不复制资产、不创建运行银行。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        raise ValueError('refuse existing author: ' + str(path))
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def build(repo):
    """独立场景沿原学师受领、剑院交信和返报，只增加本人当面问延期的声明。"""
    root = repo / 'content/packs/authored'
    source = root / 'task_contacts_1_11'
    target = root / 'task_extensions_1_0'
    manifest = json.loads((source / 'pack.json').read_text())
    content = json.loads((source / 'content.json').read_text())
    manifest.update(id='hanmo.task_extensions', version='1.0.0', namespace='extensions')
    content.update(id='task_extensions', version='1.0.0')
    for value in (manifest, content):
        value['required_capabilities'].append('task_extensions_v1')
    scene = copy.deepcopy(next(s for s in content['scenarios'] if s['id'] == 'scenario.qingchuan_letter_returns'))
    scene.update(id='scenario.qingchuan_letter_extension', name_key='name.scenario.extension')
    scene['enabled']['scenarios'] = [scene['id']]
    scene['task_extensions'] = {'version': 1, 'profile': 'private_actor_deadline_v1', 'policies': [{
        'id': 'extension.letter_once', 'offer_id': 'offer.letter', 'dialogue_id': 'dialog.sender',
        'node_id': 'extension_consent', 'additional_ticks': 8}]}
    content['scenarios'] = [scene]
    content['texts'].update({
        'name.scenario.extension': '青川私信：当面延期一次',
        'option.ask_extension': '问问这封私信能否宽限几日。',
        'line.sender.extension': '这封问安书，我同意在原约定上再宽限八个半日，只此一次。须在原期限还没过、事情还没结清时由你确认；交信和回报仍照原约，报酬也不改。'})
    dialogue = next(d for d in content['dialogues'] if d['id'] == 'dialog.sender')
    menu = next(n for n in dialogue['nodes'] if n['id'] == 'menu')
    menu['options'].insert(-1, {'id': 'ask_extension', 'text_key': 'option.ask_extension',
        'conditions': [{'op': 'any', 'terms': [
            {'op': 'task_state', 'offer_id': 'offer.letter', 'state': state} for state in ('active', 'ready')]}],
        'next': 'extension_consent'})
    dialogue['nodes'].append({'id': 'extension_consent', 'kind': 'say', 'speaker': '$provider',
        'text_key': 'line.sender.extension', 'next': 'menu'})
    emit(target / 'pack.json', manifest)
    emit(target / 'content.json', content)
    emit(repo / 'content/packs/selections/task-extensions-1.0.0.json', {
        'format': 'HM-PACK-SELECTION', 'format_version': 1, 'packs': [
            {'id': 'hanmo.local_society', 'version': '1.11.0'},
            {'id': 'hanmo.task_extensions', 'version': '1.0.0'}]})
    (target / 'README.md').write_text('''# 青川私人委托延期 1.0

选择 task-extensions-1.0.0.json，场景 extensions::scenario.qingchuan_letter_extension。

由原青川私信实交/返报/原物返还情景复用人物、初态与约定。新独立包不与 task_contacts 同选，不改旧版本。学师本人受托说话与原付款仍保持：先本人受领送信，在原期限内当面询问宽限，停在学师明确同意的说话节点，打开本人延期核对后确认。一次增加八个半日，原本金、报酬、收件人和交付责任不改。只听台词或选中选项不会自动延期。

该首片只启用一份私人送信政策，采购可由同一目录表达；护送、公约、动态采购和已经逾期/结清任务不可借此修改。实交与回报继续走原Task，取消仍原物返还与原钱账。初态不含任务、延期或送达事实。

源码作者与本域诊断不代表正常compose/Installed或完整World/GUI通过。本批没有复制资产、创建stage或预造成功银行；必要运行待匹配正式产物。
''')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', required=True, type=Path)
    build(parser.parse_args().repo)
