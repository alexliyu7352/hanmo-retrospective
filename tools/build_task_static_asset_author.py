"""基于原可玩委托追加显式货物收据作者版本；不改旧包、任务条款、人物或初态事实。"""
import argparse
import copy
import json
from pathlib import Path
PROFILE='original_task_assets_v1'
CAP='task_static_assets_v1'


def emit(path, value):
    """仅写新版本作者原件，拒绝误覆已有工作。"""
    path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists():raise ValueError('refuse existing author: '+str(path))
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')


def build(repo):
    """采购复用Task2资金原约；私信仅新增选择场景，原六场景保持旧profile。"""
    root=repo/'content/packs/authored'
    for old,new,version in [('task_takeover_1_1','task_takeover_1_2','1.2.0'),('task_contacts_1_7','task_contacts_1_8','1.8.0')]:
        manifest=json.loads((root/old/'pack.json').read_text());content=json.loads((root/old/'content.json').read_text())
        manifest['version']=version;content['version']=version
        for value in (manifest,content):
            if CAP in value['required_capabilities']:raise ValueError('unexpected enabled source')
            value['required_capabilities'].append(CAP)
        if new=='task_takeover_1_2':
            for scenario in content['scenarios']:scenario['task_static_asset_profile']=PROFILE
        else:
            scenario=copy.deepcopy(next(s for s in content['scenarios'] if s['id']=='scenario.qingchuan_letter_reports'))
            scenario['id']='scenario.qingchuan_letter_returns';scenario['name_key']='name.scenario.letter_returns';scenario['enabled']['scenarios']=[scenario['id']];scenario['task_static_asset_profile']=PROFILE
            content['texts']['name.scenario.letter_returns']='青川私信：原物返还';content['scenarios'].append(scenario)
        emit(root/new/'pack.json',manifest);emit(root/new/'content.json',content)
    selection={'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'qingxi.campaign','version':'1.2.0'},{'id':'hanmo.political_takeover','version':'1.0.0'},{'id':'hanmo.task_takeover','version':'1.2.0'}]}
    emit(repo/'content/packs/selections/task-static-assets-1.2.0.json',selection)
    emit(repo/'content/packs/selections/task-letter-returns-1.8.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.task_contacts','version':'1.8.0'}]})


def main():
    """不准备stage或生成银行；正式compose和Installed由原共享入口另行运行。"""
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);args=parser.parse_args();build(args.repo)

if __name__=='__main__':main()
