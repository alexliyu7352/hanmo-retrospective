"""生成独立文书交接段的完整共享登记候选；仅写新隔离目录，中央持唯一实际共享写权。"""
from __future__ import annotations
from pathlib import Path
import argparse,hashlib,json,subprocess
from task_static_public_candidate import replace,blob
from task_message_handoff_candidate import transform_model,transform_rules
BASE='7ccc2eb3a24389f9bdbe99d833256ec9dfa594d4'
DOMAIN='task_message_handoffs'
PROFILE='observed_letter_delivery_v1'
CAP='task_message_handoffs_v1'
INCLUDE='#include "resident/domains/tasks/message_handoffs/task_message_handoff.hpp"\n#include "resident/domains/tasks/message_handoffs/task_message_handoff_profile.hpp"\n'


def transform(path,source):
    """每个新增入口必须命中唯一具名锚；Task2/财政后继可串入，不覆盖对方整叶。"""
    if path.endswith('/task_model.hpp'):return transform_model(source)
    if path.endswith('/task_rules.cpp'):return transform_rules(source)
    if path.endswith('/model_contract.hpp'):
        return replace(source,'bool task_takeover_execution_enabled=false;','bool task_takeover_execution_enabled=false;\n    bool task_message_handoffs_enabled=false;')
    if path.endswith('/world_codec.cpp'):
        source=INCLUDE+source
        a='if(catalog.task_personal_advances)domains.push_back({"task_personal_advances",1});';source=replace(source,a,a+'if(catalog.task_message_handoffs_enabled)domains.push_back({"task_message_handoffs",1});')
        source=replace(source,'{"task_personal_advances",{"1"}}','{"task_personal_advances",{"1"}}, {"task_message_handoffs",{"1"}}')
        a='if (has("task_personal_advances")) domains.push_back({"task_personal_advances",1});';source=replace(source,a,a+'\n        if(has("task_message_handoffs")){if(!dynamic)fail("TaskMessageHandoffProfileMismatch","checkpoint/module_versions");domains.push_back({"task_message_handoffs",1});}')
        a='if(has("family_life","1")!=(version("family_life")==1)||(!dynamic&&version("family_life")))';source=replace(source,a,'if(has("task_message_handoffs","1")!=(version("task_message_handoffs")==1)||(!dynamic&&version("task_message_handoffs")))return {false,{},common_failure("TaskMessageHandoffProfileMismatch").error};\n            '+a)
        a='result->task_offer_responses=responses.value!=0;result->task_offer_response_version=responses.value;';source=replace(source,a,a+'\n    auto handoffs=decode_task_message_handoff_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!handoffs.ok)return {false,{},handoffs.error};result->task_message_handoffs_enabled=handoffs.value;')
        a='if(result->task_personal_advances)id.module_versions.emplace_back("task_personal_advances","1");';source=replace(source,a,a+'\n    if(result->task_message_handoffs_enabled)id.module_versions.emplace_back("task_message_handoffs","1");')
        a='}else if(source.find("task_personal_advances"))fail("UnexpectedTaskAdvanceDomain","task_personal_advances");';source=replace(source,a,a+'\n    if(catalog.task_message_handoffs_enabled){auto decoded=decode_task_message_handoffs(field(source,"task_message_handoffs","state"));if(!decoded.ok)return {false,{},decoded.error};auto extended=std::make_shared<TaskState>(*result.tasks);extended->message_handoffs=std::move(decoded.value);result.tasks=std::move(extended);}else if(source.find("task_message_handoffs"))fail("UnexpectedTaskMessageHandoffDomain","task_message_handoffs");')
        a='||header->kind=="task_delivery"||header->kind=="task_transfer"';source=replace(source,a,a+'||(w.catalog->task_message_handoffs_enabled&&w.tasks.message_handoffs&&header->domain=="tasks"&&header->kind==task_message_handoff_kind&&w.tasks.message_handoffs->receipts.find(row.first))')
        a='set(output,"task_personal_advances",encode_task_personal_advance_state(*w.tasks.personal_advances));}';source=replace(source,a,a+'\n    if(w.catalog->task_message_handoffs_enabled){if(!w.tasks.message_handoffs)fail("MissingTaskMessageHandoffDomain","task_message_handoffs");set(output,"task_message_handoffs",encode_task_message_handoffs(*w.tasks.message_handoffs));}')
        a='r=section("task_personal_advances",[&](auto& c){return visit_task_personal_advance_checkpoint(c,*w.tasks.personal_advances);});if(!r.ok)return {false,{},r.error};}';source=replace(source,a,a+'\n    if(w.catalog->task_message_handoffs_enabled){if(!w.tasks.message_handoffs)return {false,{},common_failure("MissingTaskMessageHandoffDomain").error};r=section("task_message_handoffs",[&](auto& c){return visit_task_message_handoffs_checkpoint(c,*w.tasks.message_handoffs);});if(!r.ok)return {false,{},r.error};}')
        a='else if(section.domain=="task_takeover_execution"){tasks->takeover_execution.emplace();r=visit_task_public_account_execution_checkpoint(reader,*tasks->takeover_execution);}';source=replace(source,a,a+'\n        else if(section.domain=="task_message_handoffs"){tasks->message_handoffs.emplace();r=visit_task_message_handoffs_checkpoint(reader,*tasks->message_handoffs);}')
        return source
    if path.endswith('/world_bundle.cpp'):
        source=INCLUDE+source
        a='Status WorldBundle::validate() const {';source=replace(source,a,a+'\n    auto handoff_layout=validate_task_message_handoff_layout(common,catalog->task_message_handoffs_enabled,catalog->identity.module_versions,tasks.message_handoffs?&*tasks.message_handoffs:nullptr);if(!handoff_layout.ok)return handoff_layout;if(catalog->task_message_handoffs_enabled){if(!dynamic_enabled())return common_failure("TaskMessageHandoffProfileMismatch");auto valid=validate_task_message_handoffs(common,tasks,*catalog->tasks,*catalog->common,*tasks.message_handoffs);if(!valid.ok)return valid;}')
        a='if(w.tasks.takeover_execution)for(const auto& row:w.tasks.takeover_execution->payments.rows()){auto r=fact(row.first,task_contract_payment_fact_kind,"tasks");if(!r.ok)return r;}';return replace(source,a,a+'\n    if(w.tasks.message_handoffs)for(const auto& row:w.tasks.message_handoffs->receipts.rows()){auto r=fact(row.first,task_message_handoff_kind,"tasks");if(!r.ok)return r;}')
    if path.endswith('/native_initial_state.cpp'):
        source=INCLUDE+source
        a='if(advances.value)admitted.module_versions.emplace_back("task_personal_advances","1");';source=replace(source,a,a+'\n    auto handoffs=decode_task_message_handoff_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!handoffs.ok)return {false,{},handoffs.error};if(handoffs.value)admitted.module_versions.emplace_back("task_message_handoffs","1");')
        a='if(mutable_catalog->task_personal_advances)tasks->personal_advances.emplace();';return replace(source,a,a+'if(mutable_catalog->task_message_handoffs_enabled)tasks->message_handoffs.emplace();')
    if path.endswith('/common_codec.cpp'):
        source=replace(source,'(kind=="escort_arrival"||','(kind=="escort_arrival"||kind=="task_message_handoff"||');return replace(source,'(named=="tasks"&&(', '(named=="tasks"&&(kind=="task_message_handoff"||')
    if path.endswith('/live_view_queries.cpp'):
        source=INCLUDE+source;a='if(query=="task_public_account_execution")';return replace(source,a,'if(query=="task_message_handoffs"){boundary::fields(args,{}, {"cursor"},"args");if(!world.catalog->task_message_handoffs_enabled)return failure("TaskMessageHandoffProfileRequired",query,ErrorKind::unsupported_capability);if(!world.tasks.message_handoffs)return failure("MissingTaskMessageHandoffDomain",query);return query_task_message_handoffs(world.common,world.tasks,*world.catalog->tasks,*world.catalog->common,*world.tasks.message_handoffs,args.find("cursor")?boundary::get_int(args,"cursor","args"):0);}\n    '+a)
    if path.endswith('/native_resident_backend.gd'):
        a='if definition.get("dynamic_world",{}).has("task_personal_advances"): expected["module_versions"].append({"name":"task_personal_advances","version":"1"})';return replace(source,a,a+'\n\tif definition.has("task_message_handoff_profile"): expected["module_versions"].append({"name":"task_message_handoffs","version":"1"})')
    if path.endswith('/resident_page_requests.gd'):
        a='const TaskPublicAccountPanel=preload("res://ui/task_public_account_panel.gd")';source=replace(source,a,a+'\nconst TaskMessageHandoffPanel=preload("res://ui/task_message_handoff_panel.gd")');a='if not TaskPublicAccountPanel.prepare(host): return false';return replace(source,a,a+'\n\tif not TaskMessageHandoffPanel.prepare(host): return false')
    if path.endswith('/screen_composer.gd'):
        a='const TaskPublicAccountPanel=preload("res://ui/task_public_account_panel.gd")';source=replace(source,a,a+'\nconst TaskMessageHandoffPanel=preload("res://ui/task_message_handoff_panel.gd")');a='if m._resident_mode and TaskPublicAccountPanel.visible_here(m): TaskPublicAccountPanel.build(m,_sidebar_section(m,stage,"TaskPublicAccountArea","公账履责"))';assert source.count(a)==2
        lines=[]
        for line in source.splitlines(True):
            lines.append(line)
            if a in line:lines.append(line[:len(line)-len(line.lstrip())]+'if m._resident_mode and TaskMessageHandoffPanel.visible_here(m): TaskMessageHandoffPanel.build(m,_sidebar_section(m,stage,"TaskMessageHandoffArea","文书交接"))\n')
        return ''.join(lines)
    if path=='schemas/content.schema.json':
        data=json.loads(source);properties=data['$defs']['scenario']['properties'];assert 'task_message_handoff_profile' not in properties;properties['task_message_handoff_profile']={'type':'string','enum':[PROFILE]};caps=data['properties']['required_capabilities']['items']['enum'];assert CAP not in caps;caps.append(CAP);return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    if path=='game/data/rule_capabilities.json':
        data=json.loads(source);records=data['capabilities'];assert not any(x['capability_id']==CAP for x in records);records.append({'capability_id':CAP,'rule_version':PROFILE,'module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.task_message_handoff_profile','equals':PROFILE}],'supported_semantics':[],'test_ids':['test_interaction_tasks_panel','test_interaction_tasks_prepare'],'evidence':[]});return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/content/links.py':
        a='    from .task_offer_response_links import check_task_offer_responses';source=replace(source,a,a+'\n    from .task_message_handoff_links import check_task_message_handoffs');return replace(source,' + check_task_offer_responses(pack,index)',' + check_task_offer_responses(pack,index) + check_task_message_handoffs(pack,index)')
    raise ValueError('unknown shared leaf '+path)


PATHS=['native/world/src/resident/domains/tasks/task_model.hpp','native/world/src/resident/domains/tasks/task_rules.cpp','native/world/include/hanmo/resident/model_contract.hpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/storage/common_codec.cpp','native/world/src/resident/kernel/live_view_queries.cpp','game/ui/native_resident_backend.gd','game/ui/resident_page_requests.gd','game/ui/screen_composer.gd','schemas/content.schema.json','game/data/rule_capabilities.json','tools/content/links.py']

def main():
    """同时保原Git基准及串合输入OID，要求所有锚成功后才一次写完整候选；不写构建清单。"""
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);parser.add_argument('--source',type=Path);parser.add_argument('--base',default=BASE);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    if args.output.exists():raise ValueError('output must be fresh')
    rows=[];results={}
    for path in PATHS:
        base=subprocess.check_output(['git','-C',str(args.repo),'show',args.base+':'+path]);before=(args.source/path).read_bytes() if args.source else base;after=transform(path,before.decode()).encode();results[path]=after;rows.append({'path':path,'base_oid':blob(base),'source_oid':blob(before),'candidate_oid':blob(after),'sha256':hashlib.sha256(after).hexdigest()})
    for path,raw in results.items():
        target=args.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
    (args.output/'manifest.json').write_text(json.dumps({'base':args.base,'status':'SOURCE_CANDIDATE_NOT_WORLD_VERIFIED','shared':rows,'checkpoint_domain':'task_message_handoffs/1','old_task_bytes_unchanged':True,'build_required_sources':['native/world/src/resident/domains/tasks/message_handoffs/task_message_handoff_'+name+'.cpp'for name in ['codec','profile','rules','validation','view']],'required_owned':['task message handoff domain leaves','tools/content/task_message_handoff_links.py','game/ui/task_message_handoff_panel.gd'],'pending':['same-generation World cold/transaction and actual UI flow']},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
