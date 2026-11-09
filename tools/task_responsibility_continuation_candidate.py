"""已受领特殊责任v2的固定共享候选；旧v1目录、访客字节和原专业writer不变。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
BASE = '48ba224'
ROOT = 'native/world/src/resident/'
PATHS = [ROOT+p for p in ['domains/director/director_model.hpp','domains/director/director_rules.cpp',
    'kernel/dynamic_integration.cpp','kernel/native_initial_state.cpp','kernel/world_codec.cpp','kernel/world_bundle.cpp']]+[
    'game/ui/native_resident_backend.gd','schemas/content.schema.json','game/data/rule_capabilities.json']
OWNED = [ROOT+'domains/tasks/'+p for p in ['task_responsibility_continuation.hpp','task_responsibility_continuation.cpp',
    'task_responsibility_director.hpp','task_responsibility_director.cpp','task_responsibility_director_world.hpp','task_responsibility_director_world.cpp',
    'task_authored_continuation.hpp','task_authored_continuation.cpp','task_authored_continuation_catalog.hpp','task_authored_continuation_catalog.cpp']]+[
    'native/world/tests/resident/task_responsibility_continuation_test.cpp','native/world/tests/resident/task_authored_continuation_test.cpp',
    'tools/content/task_authored_continuation_links.py','tools/test_task_authored_continuation_author.py','tools/task_responsibility_continuation_candidate.py']


def replace(s, old, new, count=1):
    """精确锚点失配即停；后继公共变更交中央三方保留。"""
    if s.count(old) != count:
        raise ValueError((old, s.count(old), count))
    return s.replace(old, new)


def transform(path, s):
    """同一v2选择绑定三目录与完整冷验，不增加状态或修改原Task保存集。"""
    if path.endswith('director_model.hpp'):
        s=replace(s,'struct SocialState;','struct TaskResponsibilityDirectorContext;struct SocialState;')
        return replace(s,'const DirectorDiplomaticContext* diplomacy);','const DirectorDiplomaticContext* diplomacy,const TaskResponsibilityDirectorContext* task_responsibility=nullptr);')
    if path.endswith('director_rules.cpp'):
        s='#include "resident/domains/tasks/task_responsibility_director.hpp"\n'+s
        s=replace(s,'const DirectorDiplomaticContext* diplomacy){','const DirectorDiplomaticContext* diplomacy,const TaskResponsibilityDirectorContext* task_responsibility){')
        lines=[l for l in s.splitlines() if l.strip().startswith('const TaskRecord* obligation=nullptr;')]
        assert len(lines)==1
        s=replace(s,lines[0]+'\n','')
        anchor='    if(view.interaction_busy){'
        new=lines[0]+'''\n    // 先按真实已受领侧账分流；不得把authority/proposal ID伪装成普通Offer。
    if(obligation&&catalog.authored_task_continuation&&catalog.authored_task_continuation->version==2){auto special=propose_task_responsibility_continuation(c,tasks,common_catalog,travel,view,obligation->id,p,catalog.authored_task_continuation.get(),task_responsibility);if(!special.ok)return {false,{},special.error};if(special.value)return {true,{std::move(*special.value)},{}};}
'''+anchor
        return replace(s,anchor,new)
    if path.endswith('dynamic_integration.cpp'):
        s='#include "resident/domains/tasks/task_responsibility_director_world.hpp"\n'+s
        s=replace(s,'        auto growth_access=w.services.growth_access();auto proposed=','        std::unique_ptr<TaskResponsibilityDirectorWorldAccess> task_responsibility;if(w.catalog->director->authored_task_continuation&&w.catalog->director->authored_task_continuation->version==2){auto read=TaskResponsibilityDirectorWorldAccess::create(w,authority);if(!read.ok)return {false,{},read.error};task_responsibility=std::move(read.value);}\n        auto growth_access=w.services.growth_access();auto proposed=')
        return replace(s,'diplomatic?&*diplomatic:nullptr);if(!proposed.ok)','diplomatic?&*diplomatic:nullptr,task_responsibility?&task_responsibility->context():nullptr);if(!proposed.ok)')
    if path.endswith('native_initial_state.cpp'):
        s='#include \"resident/domains/tasks/task_authored_continuation_catalog.hpp\"\n'+s
        return replace(s,'admitted.module_versions.emplace_back("authored_task_continuation","1")','admitted.module_versions.emplace_back("authored_task_continuation",std::to_string(response_director.value.authored_task_continuation->version))')
    if path.endswith('world_codec.cpp'):
        s=replace(s,'{"authored_task_continuation",{"1"}}','{"authored_task_continuation",{"1","2"}}')
        s=replace(s,'!has("authored_task_continuation","1")','!(has("authored_task_continuation","1")||has("authored_task_continuation","2"))')
        s=replace(s,'std::find(caps.begin(),caps.end(),"task_authored_continuation_v1")','std::find(caps.begin(),caps.end(),result->director->authored_task_continuation->version==2?"task_authored_continuation_v2":"task_authored_continuation_v1")')
        return replace(s,'id.module_versions.emplace_back("authored_task_continuation","1")','id.module_versions.emplace_back("authored_task_continuation",std::to_string(result->director->authored_task_continuation->version))')
    if path.endswith('world_bundle.cpp'):
        return replace(s,'if(marker.second!="1")return common_failure("UnsupportedAuthoredTaskContinuationVersion")','if(!catalog->director->authored_task_continuation||marker.second!=std::to_string(catalog->director->authored_task_continuation->version))return common_failure("UnsupportedAuthoredTaskContinuationVersion")')
    if path.endswith('native_resident_backend.gd'):
        return replace(s,'{"name":"authored_task_continuation","version":"1"}','{"name":"authored_task_continuation","version":str(definition["dynamic_world"]["director"]["authored_task_continuation"]["version"])}')
    if path.endswith('content.schema.json'):
        data=json.loads(s)
        for key in ['dynamic_director','dynamic_director_legacy','dynamic_director_registry']:
            node=data['$defs'][key]['properties']['authored_task_continuation']['properties']['version'];assert node['enum']==[1];node['enum']=[1,2]
        data['properties']['required_capabilities']['items']['enum'].append('task_authored_continuation_v2')
        return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('rule_capabilities.json'):
        data=json.loads(s);assert not any(x['capability_id']=='task_authored_continuation_v2' for x in data['capabilities'])
        data['capabilities'].append({'capability_id':'task_authored_continuation_v2','rule_version':'authored_task_continuation_v2','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.director.authored_task_continuation.version','equals':2}],'supported_semantics':[],'test_ids':['test_dynamic_actor_schedule'],'evidence':[]})
        return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    raise ValueError(path)


def main():
    """每叶记录基准及结果OID；不触碰正式源、CMake或实际中央。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--owned-repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    sha=lambda data:hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
    base=subprocess.check_output(['git','-C',str(a.repo),'rev-parse',a.base],text=True).strip();head=subprocess.check_output(['git','-C',str(a.owned_repo),'rev-parse','HEAD'],text=True).strip();files=[];owned=[]
    for name in PATHS:
        before=subprocess.check_output(['git','-C',str(a.repo),'show',base+':'+name]);after=transform(name,before.decode()).encode();dst=a.output/'candidate'/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(after);files.append({'path':name,'base_oid':sha(before),'result_oid':sha(after)})
    for name in OWNED:
        data=(a.owned_repo/name).read_bytes();dst=a.output/'candidate'/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(data);owned.append({'path':name,'result_oid':sha(data)})
    (a.output/'manifest.json').write_text(json.dumps({'base':base,'source_commit':head,'files':files,'owned_files':owned,
        'build_cpp':[ROOT+'domains/tasks/'+f for f in ['task_responsibility_continuation.cpp','task_responsibility_director.cpp','task_responsibility_director_world.cpp']],
        'build_headers':[ROOT+'domains/tasks/'+f for f in ['task_responsibility_continuation.hpp','task_responsibility_director.hpp','task_responsibility_director_world.hpp']],
        'tests':['native/world/tests/resident/task_responsibility_continuation_test.cpp'],
        'status':'SOURCE_WIP_SAME_GENERATION_DIRECTOR_GATE_PENDING',
        'merge_note':'S5 issued_supply tail is parallel: preserve both new final arguments in Director declaration, definition and kernel call; each access owns its same-candidate readers.',
        'evidence':'52f typed source20/0; proposer/world5TU syntax0; fixed48 shared5TU syntax0 (two original missing-picosha2 include diagnostics retained, corrected compile include only). Old23 support-library expanded probe exited-11 with incompatible InteractionQuote/CommandPayload layouts, retained and stopped; no new World claim.',
        'semantics':'v1 unchanged; v2 continues only already accepted governance/envoy responsibilities. No automatic acceptance, ordinary Offer substitution, remote holder tracking, session close, resource distribution or new persistent state.'},ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()
