"""本人实物凭据页只接既有查询/预读/侧栏，旧任务及实物段协议不变。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
BASE='6d596ffa482298b0f5ff23e7ed43b74e02aef336'
PATHS=['native/world/src/resident/kernel/live_view_queries.cpp','game/ui/resident_page_requests.gd','game/ui/screen_composer.gd']
OWNED=['native/world/src/resident/domains/tasks/static_assets/'+x for x in ['task_static_asset_state.hpp','task_static_asset_codec.cpp','task_static_asset_rules.cpp','task_static_asset_validation.cpp','task_static_asset_view.hpp','task_static_asset_view.cpp']]+['native/world/tests/resident/task_static_asset_test.cpp','game/ui/task_static_asset_panel.gd','game/tests/test_task_static_asset_panel_guard.gd','tools/task_static_asset_view_registration_candidate.py']


def replace(source, before, after, count=1):
    """只接受具名唯一锚点，漂移即停而不覆盖中央新增入口。"""
    if source.count(before)!=count:raise ValueError((before,source.count(before)))
    return source.replace(before,after)


def transform(path, source):
    """原有限页同候选读；query不创建命令、事实、ID或新的保存域。"""
    if path.endswith('.cpp'):
        before='#include "resident/domains/tasks/task_public_account_view.hpp"'
        source=replace(source,before,before+'\n#include "resident/domains/tasks/static_assets/task_static_asset_view.hpp"')
        line='    if(query=="task_message_handoffs")'
        inserted='    if(query=="task_static_assets"){boundary::fields(args,{}, {"cursor","receipt_id","lot_cursor"},"args");if(!world.catalog->task_static_assets_enabled)return failure("TaskStaticAssetProfileRequired",query,ErrorKind::unsupported_capability);if(!world.tasks.static_assets)return failure("MissingTaskStaticAssetDomain",query);return query_task_static_assets(world.common,world.tasks,*world.catalog->tasks,*world.tasks.static_assets,args.find("cursor")?boundary::get_int(args,"cursor","args"):0,args.find("receipt_id")?boundary::get_string(args,"receipt_id","args"):StableId{},args.find("lot_cursor")?boundary::get_int(args,"lot_cursor","args"):0);}\n'
        return replace(source,line,inserted+line)
    before='const TaskDeathNoticePanel=preload("res://ui/task_death_notice_panel.gd")'
    source=replace(source,before,before+'\nconst TaskStaticAssetPanel=preload("res://ui/task_static_asset_panel.gd")')
    if path.endswith('/resident_page_requests.gd'):
        line='\tif not TaskDeathNoticePanel.prepare(host): return false'
        return replace(source,line,line+'\n\tif not TaskStaticAssetPanel.prepare(host): return false')
    lines=[];count=0
    for line in source.splitlines():
        lines.append(line)
        if 'TaskDeathNoticePanel.build(' in line:
            count+=1;lines.append(line.replace('TaskDeathNoticePanel','TaskStaticAssetPanel').replace('TaskDeathNoticeArea','TaskStaticAssetArea').replace('已收死亡文书','本人实物明细'))
    if count!=2:raise ValueError(('screen mounts',count))
    return '\n'.join(lines)+'\n'


def main():
    """保存基准与结果OID；中央仍是三个共享叶的唯一实际写者。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--owned-repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);rows=[]
    oid=lambda data:hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
    source_commit=subprocess.check_output(['git','-C',str(a.owned_repo),'rev-parse','HEAD'],text=True).strip()
    for path in PATHS:
        original=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);result=transform(path,original.decode()).encode();target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(result);rows.append({'path':path,'base_oid':oid(original),'result_oid':oid(result)})
    owned=[]
    for path in OWNED:
        data=(a.owned_repo/path).read_bytes();frozen=subprocess.check_output(['git','-C',str(a.owned_repo),'show',source_commit+':'+path]);assert data==frozen,(path,'must freeze owned leaf first')
        target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);owned.append({'path':path,'result_oid':oid(data)})
    (a.output/'manifest.json').write_text(json.dumps({'base':a.base,'source_commit':source_commit,'files':rows,'owned_files':owned,'query':'task_static_assets','query_arguments':['cursor','receipt_id','lot_cursor'],'build_cpp':['native/world/src/resident/domains/tasks/static_assets/task_static_asset_view.cpp'],'build_headers':['native/world/src/resident/domains/tasks/static_assets/task_static_asset_view.hpp'],'tests':['native/world/tests/resident/task_static_asset_test.cpp','game/tests/test_task_static_asset_panel_guard.gd'],'persistence':'Original task_static_assets/1 visitor fields and bytes unchanged; only derived assignee index is rebuilt after successful decode. No issuer remote knowledge, new ledger, business command, or new savepoint.','status':'SOURCE_ONLY_WORLD_AND_VISUAL_ACCEPTANCE_DEFERRED'},ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()
