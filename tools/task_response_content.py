"""由已验书信同行包生成明确婉拒的1.1作者源；不改旧包、不复制父资产或创建世界。"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import subprocess

BASE = '4f39ecf4c2b72f213cd92051c1eb604550f1769e'
BASE_PACK = 'content/packs/authored/task_contacts_1_0'


def generate(repo: Path, output: Path) -> dict:
    """只追加两项本人显式回应，原报酬、期限、知情来源和普通告辞均沿已验合同。"""
    old_root = (repo / BASE_PACK).resolve()
    if output.resolve() == old_root or (output / '.git').exists():
        raise ValueError('不能覆盖原1.0包或工作树根')
    originals = {}
    for name in ('content.json', 'pack.json'):
        originals[name] = subprocess.check_output(['git', 'show', BASE + ':' + BASE_PACK + '/' + name], cwd=repo)
    content = json.loads(originals['content.json'])
    manifest = json.loads(originals['pack.json'])
    content['version'] = manifest['version'] = '1.1.0'
    for document in (content, manifest):
        document['required_capabilities'].append('task_offer_responses_v1')

    scenario = content['scenarios'][0]
    scenario['id'] = 'scenario.qingchuan_responses'
    scenario['seed_text'] = 'qingchuan-task-responses-v1'
    scenario['task_offer_response_profile'] = 'known_offers_v1'
    content['texts']['name.scenario'] = '青川应承与回访'
    sender = next(row for row in content['dialogues'] if row['id'] == 'dialog.sender')
    menu = next(row for row in sender['nodes'] if row['id'] == 'menu')
    leave = deepcopy(next(row for row in menu['options'] if row['id'] == 'leave'))
    for suffix, label, answer in (
        ('letter', '送信这件事，这次暂不承接。', '那就先搁下，信仍由我保管。你若改了主意，可以再来问；只有实际应承之后才算受托。'),
        ('escort', '护送这件事，这次暂不承接。', '我记下了，这次不托你护送。以后若想承接，再来把约定说清；卫简是否同行仍须当面同本人说定。'),
    ):
        ident = 'decline_' + suffix
        content['texts']['option.' + ident] = label
        content['texts']['line.sender.' + ident] = answer
        # 只在本人已从原学师获知且尚未承诺时出现，action仍走原预览和确认。
        menu['options'].insert(-1, {'id': ident, 'text_key': 'option.' + ident,
            'conditions': [{'op': 'task_state', 'offer_id': 'offer.' + suffix, 'state': 'known'}], 'next': ident})
        sender['nodes'].extend([
            {'id': ident, 'kind': 'action', 'command': {'kind': 'decline_offer', 'offer_id': 'offer.' + suffix},
             'on_success': ident + '_recorded', 'on_failure': 'unavailable'},
            {'id': ident + '_recorded', 'kind': 'say', 'speaker': '$provider',
             'text_key': 'line.sender.' + ident, 'next': 'menu'},
        ])
    assert next(row for row in menu['options'] if row['id'] == 'leave') == leave
    output.mkdir(parents=True, exist_ok=True)
    for name, value in (('content.json', content), ('pack.json', manifest)):
        (output / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    return {'status': 'AUTHOR_SOURCE_ONLY', 'runtime_verified': False, 'base_commit': BASE,
            'scenario_id': 'contacts::scenario.qingchuan_responses', 'pack_version': '1.1.0',
            'originals': {name: hashlib.sha256(raw).hexdigest() for name, raw in originals.items()},
            'files': {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in originals}}


def main() -> int:
    """显式指定本仓库与新包输出；只有作者文本，没有安装、编译或后台世界行为。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(generate(args.repo.resolve(), args.output.resolve()), ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
