"""预约原业务事实绑定本次真实result；严格保留S6既有接册尾钩，不直接写中央。"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
from task_extension_registration_candidate import replace, after
BASE='02b054991342bfd4b6115a6e008b4691a315f7b0'
PATHS=['native/world/src/resident/kernel/resident_world.cpp','native/world/src/resident/kernel/world_bundle.hpp','native/world/resident.cmake']


def transform(path,s):
    """原共享签名末尾加瞬时来源列表，无新公共持久字段或替代缓存。"""
    if path.endswith('/resident_world.cpp'):
        s='#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"\n'+s
        s=replace(s,'auto completed=commit_tail(*candidate,command.actor_id,command.request_seq,committed_kind,applied.value,result,plan_execution,registry_acceptance_id);','auto appointment_sources=personal_appointment_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!appointment_sources.ok)return {false,{},appointment_sources.error};auto completed=commit_tail(*candidate,command.actor_id,command.request_seq,committed_kind,applied.value,result,plan_execution,registry_acceptance_id,appointment_sources.value);')
        s=replace(s,'const StableId& registry_acceptance_id){','const StableId& registry_acceptance_id,const std::vector<StableId>& appointment_sources){')
        return after(s,'if(kind==political_shop_registry_accept_command||!registry_acceptance_id.empty()){','    auto appointment_bound=bind_personal_appointment_result_world(w,appointment_sources,actor,sequence,*receipt);if(!appointment_bound.ok)return appointment_bound;')
    if path.endswith('/world_bundle.hpp'):
        return replace(s,'const StableId& registry_acceptance_id={});','const StableId& registry_acceptance_id={},const std::vector<StableId>& appointment_sources={});')
    if path.endswith('/resident.cmake'):
        anchor='    src/resident/domains/interaction/appointments/personal_appointment_boundary.cpp'
        if s.count(anchor)!=2:raise ValueError('two actual native source lists required')
        return s.replace(anchor,anchor+'\n    src/resident/domains/interaction/appointments/personal_appointment_receipts.cpp')
    raise ValueError(path)


def main():
    """每叶记录精确Git基准与结果OID，候选目录不可覆写。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.output.exists():raise ValueError('fresh output required')
    oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest();values={};rows=[];patch=[]
    for path in PATHS:
        b=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);s=transform(path,b.decode());data=s.encode();values[path]=data;rows.append({'path':path,'base_oid':oid(b),'result_oid':oid(data)});patch.extend(difflib.unified_diff(b.decode().splitlines(True),s.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
    for path,data in values.items():target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    (a.output/'manifest.json').write_text(json.dumps({'base':a.base,'shared':rows,'status':'SOURCE_ONLY_PENDING_SYNTAX','scope':'actual command/Advance result binding; existing registry hook retained; unpublished personal_appointments/1 payload closure'},ensure_ascii=False,indent=2)+'\n');(a.output/'candidate.patch').write_text(''.join(patch))


if __name__=='__main__':main()
