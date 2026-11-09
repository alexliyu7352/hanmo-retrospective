"""在原联系人包添加显式动态回应情景；保留原人物、Need、钱包与旧情景。"""
from __future__ import annotations
import argparse
from copy import deepcopy
import json
from pathlib import Path
import subprocess

BASE = 'e8d49bf62ca861ac1915b784da3359babca25b83'
BASE_PACK = 'content/packs/authored/task_contacts_1_3'


def generate(repo: Path, output: Path):
    """新场景使用同一县仓/松门真实需粮；不写拒绝历史或人为制造可领份额。"""
    if output.resolve() == (repo / BASE_PACK).resolve() or (output / '.git').exists():
        raise ValueError('不能覆盖原包或仓库根')
    original = {name: json.loads(subprocess.check_output(['git', 'show', BASE + ':' + BASE_PACK + '/' + name], cwd=repo)) for name in ('pack.json', 'content.json')}
    content, manifest = deepcopy(original['content.json']), deepcopy(original['pack.json'])
    content['version'] = manifest['version'] = '1.4.0'
    scenario = deepcopy(next(row for row in content['scenarios'] if row['id'] == 'scenario.qingchuan_revisit'))
    scenario.update(id='scenario.qingchuan_dynamic_responses', name_key='name.scenario.dynamic_responses', seed_text='qingchuan-runtime-response-v2', task_offer_response_profile='known_offers_v2')
    content['scenarios'].append(scenario)
    content['texts']['name.scenario.dynamic_responses'] = '青川公开委托与回应'
    for table in ('offers', 'wallets', 'inventories', 'roles', 'actors', 'items', 'relations', 'organizations'):
        if content[table] != original['content.json'][table]:
            raise ValueError('意外修改原作者权威表: ' + table)
    if content['scenarios'][:-1] != original['content.json']['scenarios']:
        raise ValueError('旧情景不应因安装新版而升级回应模块')
    output.mkdir(parents=True, exist_ok=True)
    for name, value in (('content.json', content), ('pack.json', manifest)):
        (output / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    return {'base': BASE, 'scenario_id': 'contacts::scenario.qingchuan_dynamic_responses', 'status': 'AUTHOR_SOURCE_ONLY'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(generate(args.repo.resolve(), args.output.resolve()), ensure_ascii=False))
