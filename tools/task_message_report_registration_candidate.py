"""在已正式登记实交v1的共享树上严格追加返报v2选择；不改旧段字节、政治/Task2三配对或构建清单。"""
from pathlib import Path
import argparse,json,hashlib,subprocess
from task_static_public_candidate import replace,blob
from task_message_report_candidate import transform_rules
BASE='6f9441609d2dd874ad9744a0dad092f3ec521bf7'
PROFILE='observed_letter_delivery_and_report_v2'
CAP='task_message_reports_v2'


def transform(path,source):
    """每处原v1锚唯一核准；新完整候选只列本后继改变的登记叶。"""
    if path.endswith('/task_rules.cpp'):return transform_rules(source)
    if path.endswith('/model_contract.hpp'):return replace(source,'bool task_message_handoffs_enabled=false;','bool task_message_handoffs_enabled=false;Integer task_message_handoff_version=0;')
    if path.endswith('/world_codec.cpp'):
        source=replace(source,'if(catalog.task_message_handoffs_enabled)domains.push_back({"task_message_handoffs",1});','if(catalog.task_message_handoffs_enabled)domains.push_back({"task_message_handoffs",static_cast<std::uint32_t>(catalog.task_message_handoff_version)});')
        source=replace(source,'{"task_message_handoffs",{"1"}}','{"task_message_handoffs",{"1","2"}}')
        source=replace(source,'domains.push_back({"task_message_handoffs",1});','domains.push_back({"task_message_handoffs",modules.at("task_message_handoffs")=="2"?2u:1u});')
        source=replace(source,'if(has("task_message_handoffs","1")!=(version("task_message_handoffs")==1)||(!dynamic&&version("task_message_handoffs")))','if(has("task_message_handoffs","1")!=(version("task_message_handoffs")==1)||has("task_message_handoffs","2")!=(version("task_message_handoffs")==2)||(!dynamic&&version("task_message_handoffs")))')
        source=replace(source,'decode_task_message_handoff_profile(scenario,','decode_task_message_handoff_version(scenario,')
        source=replace(source,'result->task_message_handoffs_enabled=handoffs.value;','result->task_message_handoffs_enabled=handoffs.value!=0;result->task_message_handoff_version=handoffs.value;')
        source=replace(source,'id.module_versions.emplace_back("task_message_handoffs","1");','id.module_versions.emplace_back("task_message_handoffs",std::to_string(result->task_message_handoff_version));')
        source=replace(source,'header->kind==task_message_handoff_kind&&w.tasks.message_handoffs->receipts.find(row.first)','((header->kind==task_message_handoff_kind&&w.tasks.message_handoffs->receipts.find(row.first))||(w.catalog->task_message_handoff_version==2&&w.tasks.message_handoffs->version==2&&header->kind==task_message_report_kind&&w.tasks.message_handoffs->reports.find(row.first)))')
        return source
    if path.endswith('/world_bundle.cpp'):
        source=replace(source,'auto handoff_layout=validate_task_message_handoff_layout','if(catalog->task_message_handoffs_enabled!=(catalog->task_message_handoff_version!=0))return common_failure("TaskMessageHandoffProfileMismatch");auto handoff_layout=validate_task_message_handoff_layout')
        source=replace(source,'tasks.message_handoffs?&*tasks.message_handoffs:nullptr);','tasks.message_handoffs?&*tasks.message_handoffs:nullptr,catalog->task_message_handoffs_enabled?catalog->task_message_handoff_version:1);')
        a='if(w.tasks.message_handoffs)for(const auto& row:w.tasks.message_handoffs->receipts.rows()){auto r=fact(row.first,task_message_handoff_kind,"tasks");if(!r.ok)return r;}'
        return replace(source,a,a+'\n    if(w.tasks.message_handoffs&&w.tasks.message_handoffs->version==2)for(const auto& row:w.tasks.message_handoffs->reports.rows()){auto r=fact(row.first,task_message_report_kind,"tasks");if(!r.ok)return r;}')
    if path.endswith('/native_initial_state.cpp'):
        source=replace(source,'decode_task_message_handoff_profile(scenario,','decode_task_message_handoff_version(scenario,')
        source=replace(source,'if(handoffs.value)admitted.module_versions.emplace_back("task_message_handoffs","1");','if(handoffs.value)admitted.module_versions.emplace_back("task_message_handoffs",std::to_string(handoffs.value));')
        return replace(source,'if(mutable_catalog->task_message_handoffs_enabled)tasks->message_handoffs.emplace();','if(mutable_catalog->task_message_handoffs_enabled){tasks->message_handoffs.emplace();tasks->message_handoffs->version=mutable_catalog->task_message_handoff_version;}')
    if path.endswith('/common_codec.cpp'):
        a='kind=="task_message_handoff"||';assert source.count(a)==2;return source.replace(a,a+'kind=="task_message_report"||')
    if path.endswith('/native_resident_backend.gd'):
        original='\tif definition.has("task_message_handoff_profile"): expected["module_versions"].append({"name":"task_message_handoffs","version":"1"})\n'
        source=replace(source,original,'')
        anchor='if definition.get("dynamic_world",{}).has("task_personal_advances"): expected["module_versions"].append({"name":"task_personal_advances","version":"1"})'
        return replace(source,anchor,anchor+'\n\tif definition.has("task_message_handoff_profile"): expected["module_versions"].append({"name":"task_message_handoffs","version":"2" if definition.task_message_handoff_profile=="'+PROFILE+'" else "1"})')
    if path=='schemas/content.schema.json':
        data=json.loads(source);profile=data['$defs']['scenario']['properties']['task_message_handoff_profile'];assert profile=={'type':'string','enum':['observed_letter_delivery_v1']};profile['enum'].append(PROFILE);caps=data['properties']['required_capabilities']['items']['enum'];assert CAP not in caps;caps.append(CAP);return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    if path=='game/data/rule_capabilities.json':
        data=json.loads(source);caps=data['capabilities'];assert not any(x['capability_id']==CAP for x in caps);caps.append({'capability_id':CAP,'rule_version':PROFILE,'module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.task_message_handoff_profile','equals':PROFILE}],'supported_semantics':[],'test_ids':['test_interaction_tasks_panel','test_interaction_tasks_prepare'],'evidence':[]});return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    raise ValueError('unknown leaf '+path)


PATHS=['native/world/src/resident/domains/tasks/task_rules.cpp','native/world/include/hanmo/resident/model_contract.hpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/storage/common_codec.cpp','game/ui/native_resident_backend.gd','schemas/content.schema.json','game/data/rule_capabilities.json']


def main():
    """同时存基准OID与实际串合输入，所有锚满足才生成，既有输出或缺v1不自动猜补。"""
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);parser.add_argument('--source',type=Path);parser.add_argument('--base',default=BASE);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    if args.output.exists():raise ValueError('output must be fresh')
    rows=[];results={}
    for path in PATHS:
        base=subprocess.check_output(['git','-C',str(args.repo),'show',args.base+':'+path]);before=(args.source/path).read_bytes() if args.source else base;after=transform(path,before.decode()).encode();results[path]=after;rows.append({'path':path,'base_oid':blob(base),'source_oid':blob(before),'candidate_oid':blob(after),'sha256':hashlib.sha256(after).hexdigest()})
    for path,raw in results.items():
        target=args.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
    (args.output/'manifest.json').write_text(json.dumps({'base':args.base,'status':'SOURCE_CANDIDATE_NOT_WORLD_VERIFIED','shared':rows,'checkpoint_domain':'task_message_handoffs/2','old_v1_visitor_bytes_unchanged':True,'build_required_sources':['native/world/src/resident/domains/tasks/message_handoffs/task_message_report_'+x+'.cpp'for x in ['rules','validation']],'build_required_headers':['native/world/src/resident/domains/tasks/message_handoffs/task_message_report.hpp'],'pending':['same-generation World transaction and cold','formal Installed and actual UI']},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
