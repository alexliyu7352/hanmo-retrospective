"""从原联系人1.4派生明确垫款情景；保持原人物、预算、货物和全部旧情景。"""
from copy import deepcopy
from pathlib import Path
import argparse, json, subprocess

BASE = 'b97055ec922de482ad2e843a1983f88684897b46'
PACK = 'content/packs/authored/task_contacts_1_4'

def generate(repo, output):
    if output.resolve() == (repo / PACK).resolve() or (output / '.git').exists():
        raise ValueError('不能覆盖原作者包或工程根')
    original = {name: json.loads(subprocess.check_output(['git', 'show', BASE+':'+PACK+'/'+name], cwd=repo)) for name in ('pack.json', 'content.json')}
    manifest, content = deepcopy(original['pack.json']), deepcopy(original['content.json'])
    manifest['version'] = content['version'] = '1.5.0'
    scenario = deepcopy(next(row for row in content['scenarios'] if row['id'] == 'scenario.qingchuan_dynamic_responses'))
    scenario.update(id='scenario.qingchuan_task_advances', name_key='name.scenario.task_advances', seed_text='qingchuan-explicit-task-advances')
    scenario['dynamic_world']['task_personal_advances'] = {'enabled': True, 'version': 1, 'policies': [
        {'id': 'policy.city_grain_advance', 'version': 1, 'task_template_id': 'society::template.city_grain', 'maximum_cash': 40, 'repayment_policy': 'authorized_advance_first_v1'},
        {'id': 'policy.sect_grain_advance', 'version': 1, 'task_template_id': 'society::template.sect_grain', 'maximum_cash': 40, 'repayment_policy': 'authorized_advance_first_v1'}]}
    content['scenarios'].append(scenario);content['texts']['name.scenario.task_advances']='青川受托经费与自愿垫款'
    for table in ('actors','wallets','inventories','task_templates','offers','roles','items','organizations'):
        if content[table] != original['content.json'][table]:
            raise ValueError('意外修改原财物或合同表: '+table)
    if content['scenarios'][:-1] != original['content.json']['scenarios']:
        raise ValueError('旧情景不得隐式启用垫款')
    output.mkdir(parents=True,exist_ok=True)
    for name,value in [('pack.json',manifest),('content.json',content)]:
        (output/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
    return {'base':BASE,'scenario_id':'contacts::scenario.qingchuan_task_advances','status':'AUTHOR_SOURCE_ONLY'}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();print(json.dumps(generate(args.repo.resolve(),args.output.resolve()),ensure_ascii=False))
