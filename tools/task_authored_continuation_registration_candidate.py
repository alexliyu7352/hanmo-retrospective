"""原静态责任续行的固定共享候选；只写隔离后像，旧Director冷字节不追加状态。"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
BASE = 'ef6cebcfa7af6d1143b5f4d9e598743add117491'
CAP = 'task_authored_continuation_v1'
INCLUDE = '#include "resident/domains/tasks/task_authored_continuation_catalog.hpp"\n'


def replace(source, old, new, count=1):
    """固定锚点数不相符即停，不向漂移公共源做模糊覆盖。"""
    if source.count(old) != count:
        raise ValueError((old, source.count(old), count))
    return source.replace(old, new)


def after_line(source, anchor, extra):
    """完整行后插入具名登记，保同一基准的其他新模块原顺序。"""
    rows = [line for line in source.splitlines() if anchor in line]
    if len(rows) != 1:
        raise ValueError((anchor, len(rows)))
    return replace(source, rows[0], rows[0] + '\n' + extra)


def transform(path, source):
    """共享组合只追加显式目录、原提案接点和三处相同身份marker。"""
    if path.endswith('/director_model.hpp'):
        source = replace(source, 'struct DirectorCatalog {', 'struct TaskAuthoredContinuationCatalog;\nstruct DirectorCatalog {')
        return replace(source, '    OrderedTable<DirectorAdmission> admissions;\n};', '    OrderedTable<DirectorAdmission> admissions;\n    // 仅显式作者续行已受领责任；空值保旧非采购等待，不生成新的任务或知识。\n    std::shared_ptr<const TaskAuthoredContinuationCatalog> authored_task_continuation;\n};')
    if path.endswith('/director_codec.cpp'):
        source = INCLUDE + source
        source = replace(source, '{"diplomatic_response_version"},"director");', '{"diplomatic_response_version","authored_task_continuation"},"director");', 2)
        source = after_line(source, 'legacy.emplace_back("diplomatic_response_version",*response);', '    if(auto* continuation=value.find("authored_task_continuation"))legacy.emplace_back("authored_task_continuation",*continuation);')
        return replace(source, 'DirectorCatalog c;c.treaty_passage=', 'DirectorCatalog c;if(auto* value_continuation=value.find("authored_task_continuation")){auto parsed=decode_task_authored_continuation_catalog(*value_continuation);if(!parsed.ok)return {false,{},parsed.error};c.authored_task_continuation=std::make_shared<const TaskAuthoredContinuationCatalog>(std::move(parsed.value));}c.treaty_passage=')
    if path.endswith('/director_rules.cpp'):
        source = '#include "resident/domains/tasks/task_authored_continuation.hpp"\n' + source
        return replace(source, 'if(definition->template_id!="procurement")return waiting_with_membership(std::move(p));', 'if(definition->template_id!="procurement"){auto continuation=propose_authored_task_continuation(c,tasks,task_catalog,common_catalog,travel,view,obligation->id,p,catalog.authored_task_continuation.get());if(!continuation.ok)return {false,{},continuation.error};if(continuation.value)return waiting_with_membership(std::move(*continuation.value));return waiting_with_membership(std::move(p));}')
    if path.endswith('/native_initial_state.cpp'):
        return after_line(source, 'if(response_director.value.diplomatic_response)admitted.module_versions', '    if(response_director.value.authored_task_continuation)admitted.module_versions.emplace_back("authored_task_continuation","1");')
    if path.endswith('/world_codec.cpp'):
        source = INCLUDE + source
        source = replace(source, '{"diplomatic_response",{"1"}},', '{"diplomatic_response",{"1"}}, {"authored_task_continuation",{"1"}},')
        source = after_line(source, 'const auto response_markers=', '            const auto continuation_markers=std::count_if(versions.begin(),versions.end(),[](const auto& value){return value.first=="authored_task_continuation";});if(continuation_markers>1||(continuation_markers&&(!dynamic||!has("authored_task_continuation","1"))))return {false,{},common_failure("TaskContinuationProfileMismatch").error};')
        source = after_line(source, 'result->director=std::make_shared<const DirectorCatalog>', '        if(result->director->authored_task_continuation){auto caps=strings(field(data,"required_capabilities","data"),"required_capabilities");if(std::find(caps.begin(),caps.end(),"'+CAP+'") == caps.end())unsupported("TaskContinuationCapabilityRequired","dynamic_world/director");auto valid=validate_task_authored_continuation_catalog(*result->director->authored_task_continuation,*result->tasks,*result->common);if(!valid.ok)return {false,{},valid.error};}')
        return after_line(source, 'if(result->director&&result->director->diplomatic_response)id.module_versions', '    if(result->director&&result->director->authored_task_continuation)id.module_versions.emplace_back("authored_task_continuation","1");')
    if path.endswith('/world_bundle.cpp'):
        source = INCLUDE + source
        return after_line(source, 'valid=validate_director(common,director,', '        {Integer markers=0;for(const auto& marker:catalog->identity.module_versions)if(marker.first=="authored_task_continuation"){++markers;if(marker.second!="1")return common_failure("UnsupportedAuthoredTaskContinuationVersion");}if(markers!=(catalog->director->authored_task_continuation?1:0))return common_failure("TaskContinuationProfileMismatch");if(catalog->director->authored_task_continuation){valid=validate_task_authored_continuation_catalog(*catalog->director->authored_task_continuation,*catalog->tasks,*catalog->common);if(!valid.ok)return valid;}}')
    if path.endswith('/native_resident_backend.gd'):
        return after_line(source, 'get("diplomatic_response_version",0)==1:', '\tif definition.get("dynamic_world",{}).get("director",{}).has("authored_task_continuation"): expected["module_versions"].append({"name":"authored_task_continuation","version":"1"})')
    if path.endswith('/links.py'):
        source = after_line(source, 'from .task_message_notice_links import', '    from .task_authored_continuation_links import check_task_authored_continuation')
        return replace(source, ' + check_task_message_notices(pack,index)', ' + check_task_message_notices(pack,index) + check_task_authored_continuation(pack,index)')
    if path.endswith('/content.schema.json'):
        data = json.loads(source)
        item = {'type':'object','additionalProperties':False,'required':['offer_id','recipient_role_id'],'properties':{'offer_id':{'$ref':'#/$defs/reference_offers'},'recipient_role_id':{'$ref':'#/$defs/reference_roles'}}}
        definition = {'type':'object','additionalProperties':False,'required':['version','message_addresses'],'properties':{'version':{'type':'integer','enum':[1]},'message_addresses':{'type':'array','maxItems':4096,'items':item}}}
        data['$defs']['dynamic_director'].setdefault('properties', {})['authored_task_continuation'] = definition
        for key in ('dynamic_director_legacy','dynamic_director_registry'):
            assert 'authored_task_continuation' not in data['$defs'][key]['properties']
            data['$defs'][key]['properties']['authored_task_continuation'] = definition
        data['properties']['required_capabilities']['items']['enum'].append(CAP)
        return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        data=json.loads(source);assert not any(row['capability_id']==CAP for row in data['capabilities'])
        data['capabilities'].append({'capability_id':CAP,'rule_version':'authored_task_continuation_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.director.authored_task_continuation.version','equals':1}],'supported_semantics':[],'test_ids':['test_interaction_tasks_prepare'],'evidence':[]})
        return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    raise ValueError(path)


PATHS = ['native/world/src/resident/domains/director/director_model.hpp','native/world/src/resident/domains/director/director_codec.cpp','native/world/src/resident/domains/director/director_rules.cpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/world_bundle.cpp','game/ui/native_resident_backend.gd','tools/content/links.py','schemas/content.schema.json','game/data/rule_capabilities.json']
OWNED = ['native/world/src/resident/domains/tasks/'+name for name in ['task_authored_continuation.hpp','task_authored_continuation.cpp','task_authored_continuation_catalog.hpp','task_authored_continuation_catalog.cpp']] + ['native/world/tests/resident/task_authored_continuation_test.cpp','tools/content/task_authored_continuation_links.py','tools/task_authored_continuation_registration_candidate.py','tools/test_task_authored_continuation_author.py']


def main():
    """固定Git对象产生候选清单；构建登记只交两CPP两头，不擅改中央CMake。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.base != BASE:raise ValueError('fixed baseline required')
    a.output.mkdir(parents=True,exist_ok=False);rows=[]
    oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in PATHS:
        original=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);result=transform(path,original.decode()).encode();out=a.output/'candidate'/path;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(result);rows.append({'path':path,'base_oid':oid(original),'result_oid':oid(result)})
    (a.output/'manifest.json').write_text(json.dumps({'base':a.base,'files':rows,'owned_paths':OWNED,'build_cpp':OWNED[1:2]+OWNED[3:4],'build_headers':[OWNED[0],OWNED[2]],'status':'SOURCE_ONLY_PENDING_COMBINED_SYNTAX'},indent=2))


if __name__=='__main__':main()
