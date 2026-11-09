#!/usr/bin/env python3
"""仅真实玩家新接册在原提交尾分配result后绑定，中央两叶由唯一写者三方接入。"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

BASE = '9e8ad835feb9f866dcc3763807600c356bfd7a76'

def replace(text, old, new, path):
    if text.count(old) != 1:
        raise ValueError('exact receipt hook anchor: ' + path)
    return text.replace(old, new)

def candidate(repo, output):
    if output.exists():
        raise ValueError('output must be fresh')
    rows=[];patch=[]
    for path in ['native/world/src/resident/kernel/resident_world.cpp', 'native/world/src/resident/kernel/world_bundle.hpp']:
        before=subprocess.check_output(['git','-C',str(repo),'show',BASE+':'+path]).decode()
        after=before
        if path.endswith('resident_world.cpp'):
            after=replace(after,'#include "private_shop_integration.hpp"','#include "private_shop_integration.hpp"\n#include "political_shop_registry_integration.hpp"',path)
            old='auto completed=commit_tail(*candidate,command.actor_id,command.request_seq,committed_kind,applied.value,result,plan_execution);'
            new='StableId registry_acceptance_id;if(committed_kind==political_shop_registry_accept_command){auto source=political_shop_registry_new_player_acceptance(current.political_shop_registry_successions,candidate->political_shop_registry_successions,command.actor_id,candidate->common.tick());if(!source.ok)return {false,{},source.error};registry_acceptance_id=std::move(source.value);}auto completed=commit_tail(*candidate,command.actor_id,command.request_seq,committed_kind,applied.value,result,plan_execution,registry_acceptance_id);'
            after=replace(after,old,new,path)
            after=replace(after,'const PlayerPlanExecution* plan_execution){','const PlayerPlanExecution* plan_execution,const StableId& registry_acceptance_id){',path)
            old='state.history.push_back({w.common.tick(),kind,actor,id.value,{}});if(state.history.size()>2000)state.history.erase(state.history.begin());'
            new=old+'\n    // 本次实际新接册与已分配结果同候选绑定；不从裁剪历史或相邻ID猜原来源。\n    if(kind==political_shop_registry_accept_command||!registry_acceptance_id.empty()){if(kind!=political_shop_registry_accept_command||registry_acceptance_id.empty()||!w.political_shop_registry_successions_enabled())return common_failure("PoliticalShopRegistryNewPlayerSourceRequired");auto bound=bind_political_shop_registry_player_receipt(w.common,w.political_shop_registry_successions,registry_acceptance_id,actor,sequence,*receipt,[](const CommitResult& r){return r.details&&std::holds_alternative<Done>(r.details->data);});if(!bound.ok)return bound;}'
            after=replace(after,old,new,path)
        else:
            after=replace(after,'const PlayerPlanExecution* plan_execution=nullptr);','const PlayerPlanExecution* plan_execution=nullptr,const StableId& registry_acceptance_id={});',path)
        q=output/'candidate'/path;q.parent.mkdir(parents=True,exist_ok=True);q.write_text(after)
        oid=lambda value:subprocess.check_output(['git','hash-object','--stdin'],input=value.encode()).decode().strip()
        rows.append({'path':path,'before_oid':oid(before),'after_oid':oid(after),'sha256':hashlib.sha256(after.encode()).hexdigest()})
        import difflib
        patch.extend(difflib.unified_diff(before.splitlines(True),after.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
    (output/'candidate.patch').write_text(''.join(patch));(output/'manifest.json').write_text(json.dumps({'base':BASE,'status':'SOURCE_ONLY','shared':rows,'scope':'two exact player commit-tail hooks; existing Common format and NPC result-free path unchanged'},indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();candidate(a.repo,a.output)
