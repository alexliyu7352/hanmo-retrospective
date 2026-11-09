"""通知独立段/原受领/UI严格候选；固定基准逐锚点，不写中央或CMake。"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from task_message_notice_task_hooks import replace, model, rules
BASE='40126740dd0ba45b499ac4d79e8ba7a120fc9a87'
INCLUDE='#include "resident/domains/tasks/message_notices/task_message_notice_world.hpp"\n'
PROFILE='witnessed_death_letters_v1'
CAP='task_death_notices_v1'


def after_line(source, anchor, extra):
    """同一源只有一行可承担该登记，漂移或重影立即拒绝。"""
    lines=[line for line in source.splitlines() if anchor in line]
    if len(lines)!=1:raise ValueError(anchor)
    return replace(source,lines[0],lines[0]+'\n'+extra)


def transform(path, source):
    """保旧Task/信件格式与中央并行领域，只有新显式profile追加独立段。"""
    if path.endswith('/task_model.hpp'):return model(source)
    if path.endswith('/task_rules.cpp'):return rules(source)
    if path.endswith('/model_contract.hpp'):
        source=replace(source,'struct PreparedCatalog {','struct TaskMessageNoticeCatalog;\nstruct PreparedCatalog {')
        return replace(source,'bool task_message_handoffs_enabled=false;Integer task_message_handoff_version=0;','bool task_message_handoffs_enabled=false;Integer task_message_handoff_version=0;\n    std::shared_ptr<const TaskMessageNoticeCatalog> task_message_notices;')
    if path.endswith('/native_initial_state.cpp'):
        source=INCLUDE+source
        source=after_line(source,'admitted.module_versions.emplace_back("family_bequest","1")','    auto notices=decode_task_message_notice_catalog(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!notices.ok)return {false,{},notices.error};if(notices.value)admitted.module_versions.emplace_back("task_message_notices","1");')
        return replace(source,'if(mutable_catalog->task_message_handoffs_enabled){','if(mutable_catalog->task_message_notices)tasks->message_notices.emplace();if(mutable_catalog->task_message_handoffs_enabled){')
    if path.endswith('/world_codec.cpp'):
        source=INCLUDE+source
        source=replace(source,'if(family_bequest_enabled(catalog))domains.push_back({"family_bequest",1});return domains;','if(family_bequest_enabled(catalog))domains.push_back({"family_bequest",1});if(catalog.task_message_notices)domains.push_back({"task_message_notices",1});return domains;')
        source=replace(source,'{"task_message_handoffs",{"1","2"}},','{"task_message_handoffs",{"1","2"}}, {"task_message_notices",{"1"}},')
        source=after_line(source,'if (has("family_bequest")) domains.push_back','        if(has("task_message_notices")){if(!dynamic||!has("family_life")||!has("task_message_handoffs"))fail("TaskMessageNoticeProfileMismatch","checkpoint/module_versions");domains.push_back({"task_message_notices",1});}')
        source=after_line(source,'if(has("family_bequest","1")!=','            if(has("task_message_notices","1")!=(version("task_message_notices")==1)||(version("task_message_notices")&&(!dynamic||!has("family_life","1")||!version("task_message_handoffs"))))return {false,{},common_failure("TaskMessageNoticeProfileMismatch").error};')
        source=after_line(source,'result->task_message_handoff_version=handoffs.value;','    auto notices=decode_task_message_notice_catalog(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!notices.ok)return {false,{},notices.error};if(notices.value)result->task_message_notices=std::make_shared<const TaskMessageNoticeCatalog>(std::move(*notices.value));')
        source=after_line(source,'result->tasks=std::make_shared<const TaskCatalog>', '    if(result->task_message_notices){auto valid=validate_task_message_notice_catalog(*result->task_message_notices,*result->tasks,*result->common);if(!valid.ok)return {false,{},valid.error};}')
        source=after_line(source,'if(family_bequest_enabled(*result))id.module_versions','    if(result->task_message_notices)id.module_versions.emplace_back("task_message_notices","1");')
        source=after_line(source,'else if(source.find("task_message_handoffs"))','    if(catalog.task_message_notices){auto decoded=decode_task_message_notices(field(source,"task_message_notices","state"));if(!decoded.ok)return {false,{},decoded.error};auto extended=std::make_shared<TaskState>(*result.tasks);extended->message_notices=std::move(decoded.value);result.tasks=std::move(extended);}else if(source.find("task_message_notices"))fail("UnexpectedTaskMessageNoticeDomain","task_message_notices");')
        source=replace(source,'||(w.catalog->task_message_handoffs_enabled&&w.tasks.message_handoffs&&header->domain=="tasks"', '||(w.catalog->task_message_notices&&w.tasks.message_notices&&header->domain=="tasks"&&header->kind==task_death_notice_bound_kind&&w.tasks.message_notices->bindings.find(row.first))||(w.catalog->task_message_handoffs_enabled&&w.tasks.message_handoffs&&header->domain=="tasks"')
        source=after_line(source,'set(output,"task_message_handoffs",','    if(w.catalog->task_message_notices){if(!w.tasks.message_notices)fail("MissingTaskMessageNoticeDomain","task_message_notices");set(output,"task_message_notices",encode_task_message_notices(*w.tasks.message_notices));}')
        source=after_line(source,'r=section("family_bequest",','    if(w.catalog->task_message_notices){if(!w.tasks.message_notices)return {false,{},common_failure("MissingTaskMessageNoticeDomain").error};r=section("task_message_notices",[&](auto& c){return visit_task_message_notices_checkpoint(c,*w.tasks.message_notices);});if(!r.ok)return {false,{},r.error};}')
        source=after_line(source,'else if(section.domain=="task_message_handoffs")','        else if(section.domain=="task_message_notices"){tasks->message_notices.emplace();r=visit_task_message_notices_checkpoint(reader,*tasks->message_notices);}')
        return source
    if path.endswith('/world_bundle.cpp'):
        source=INCLUDE+source
        source=after_line(source,'if(w.tasks.message_handoffs)for(const auto& row:w.tasks.message_handoffs->receipts','    if(w.tasks.message_notices)for(const auto& row:w.tasks.message_notices->bindings.rows()){auto r=fact(row.first,task_death_notice_bound_kind,"tasks");if(!r.ok)return r;}')
        return after_line(source,'valid=validate_family_death_coverage(common,family_life,family_death_read(),family_life_enabled());','    {const bool enabled=bool(catalog->task_message_notices);Integer markers=0;for(const auto& marker:catalog->identity.module_versions)if(marker.first=="task_message_notices"){++markers;if(marker.second!="1")return common_failure("UnsupportedTaskMessageNoticeVersion");}if(markers!=(enabled?1:0)||enabled!=tasks.message_notices.has_value())return common_failure("TaskMessageNoticeProfileMismatch");if(!enabled){for(const auto& row:common.state().facts.rows())if(row.second.kind==task_death_notice_bound_kind)return common_failure("TaskMessageNoticeProfileRequired",row.first);}else{auto notice=make_task_message_notice_services(*this);if(!notice.ok)return {false,{},notice.error};if(!notice.value)return common_failure("TaskMessageNoticeServicesMismatch");valid=validate_task_message_notices(common,tasks,*catalog->tasks,*catalog->common,*catalog->task_message_notices,*tasks.message_notices,*tasks.message_handoffs,notice.value->validate);if(!valid.ok)return valid;}}')
    if path.endswith('/common_codec.cpp'):
        anchor='kind=="task_message_handoff"||';assert source.count(anchor)==2;return source.replace(anchor,'kind=="task_death_notice_bound"||'+anchor)
    if path.endswith('/dispatcher.cpp'):
        source=INCLUDE+source
        source=replace(source,'auto q=preview_task(w.common,w.tasks,','auto notice=make_task_message_notice_services(w);if(!notice.ok)return {false,{},notice.error};auto q=preview_task(w.common,w.tasks,')
        return replace(source,'known?&*known:nullptr,takeover.value->tasks());','known?&*known:nullptr,takeover.value->tasks(),notice.value?&*notice.value:nullptr);')
    if path.endswith('/phase_plan.cpp'):
        source=INCLUDE+source
        source=replace(source,'auto end=finish_task_action(w.common,w.tasks,','auto notice=make_task_message_notice_services(w,w.dynamic_enabled()?&dynamic_input:nullptr);if(!notice.ok){r={false,{},notice.error};break;}auto end=finish_task_action(w.common,w.tasks,')
        return replace(source,'ia,action,to_tick,w.dynamic_enabled()?&w.affairs:nullptr,takeover.value->tasks());','ia,action,to_tick,w.dynamic_enabled()?&w.affairs:nullptr,takeover.value->tasks(),notice.value?&*notice.value:nullptr);')
    if path.endswith('/live_view_queries.cpp'):
        return after_line(INCLUDE+source,'if(query=="task_message_handoffs")','    if(query=="task_death_notices"){boundary::fields(args,{}, {"cursor"},"args");if(!world.catalog->task_message_notices)return failure("TaskMessageNoticeProfileRequired",query,ErrorKind::unsupported_capability);auto notice=make_task_message_notice_services(world);if(!notice.ok)return {false,{},notice.error};if(!notice.value)return failure("MissingTaskMessageNoticeDomain",query);return query_delivered_task_death_notices(world.common,world.tasks,*world.catalog->tasks,*world.catalog->common,*world.catalog->task_message_notices,*world.tasks.message_notices,*world.tasks.message_handoffs,notice.value->validate,args.find("cursor")?boundary::get_int(args,"cursor","args"):0);}')
    if path.endswith('/native_resident_backend.gd'):
        return after_line(source,'definition.get("family_bequest",{}).get("profile")','\tif definition.has("task_message_notices"): expected["module_versions"].append({"name":"task_message_notices","version":"1"})')
    if path.endswith('/resident_page_requests.gd'):
        source=after_line(source,'const TaskMessageHandoffPanel=','const TaskDeathNoticePanel=preload("res://ui/task_death_notice_panel.gd")')
        return after_line(source,'if not TaskMessageHandoffPanel.prepare(host):','\tif not TaskDeathNoticePanel.prepare(host): return false')
    if path.endswith('/screen_composer.gd'):
        source=after_line(source,'const TaskMessageHandoffPanel=','const TaskDeathNoticePanel=preload("res://ui/task_death_notice_panel.gd")');lines=source.splitlines();out=[]
        for line in lines:
            out.append(line)
            if 'TaskMessageHandoffPanel.build(' in line:out.append(line.replace('TaskMessageHandoff','TaskDeathNotice').replace('文书交接','已收死亡文书'))
        assert sum('TaskDeathNoticePanel.build(' in row for row in out)==2
        return '\n'.join(out)+'\n'
    if path.endswith('/links.py'):
        source=after_line(source,'from .task_message_handoff_links import','    from .task_message_notice_links import check_task_message_notices')
        return replace(source,' + check_task_message_handoffs(pack,index)',' + check_task_message_handoffs(pack,index) + check_task_message_notices(pack,index)')
    if path.endswith('/content.schema.json'):
        data=json.loads(source);assert 'task_message_notices' not in data['$defs']['scenario']['properties'];data['$defs']['scenario']['properties']['task_message_notices']={'type':'object','additionalProperties':False,'required':['version','profile','bindings'],'properties':{'version':{'type':'integer','enum':[1]},'profile':{'type':'string','enum':[PROFILE]},'bindings':{'type':'array','minItems':1,'maxItems':4096,'items':{'type':'object','additionalProperties':False,'required':['offer_id','decedent_actor_id'],'properties':{'offer_id':{'$ref':'#/$defs/reference_offers'},'decedent_actor_id':{'$ref':'#/$defs/reference_actors'}}}}}};caps=data['properties']['required_capabilities']['items']['enum'];assert CAP not in caps;caps.append(CAP);return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        data=json.loads(source);rows=data['capabilities'];assert not any(x['capability_id']==CAP for x in rows);rows.append({'capability_id':CAP,'rule_version':PROFILE,'module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.task_message_notices.profile','equals':PROFILE}],'supported_semantics':[],'test_ids':['test_interaction_tasks_panel','test_interaction_tasks_prepare'],'evidence':[]});return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    raise ValueError(path)

PATHS=['native/world/src/resident/domains/tasks/task_model.hpp','native/world/src/resident/domains/tasks/task_rules.cpp','native/world/include/hanmo/resident/model_contract.hpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/kernel/dispatcher.cpp','native/world/src/resident/kernel/phase_plan.cpp','native/world/src/resident/kernel/live_view_queries.cpp','native/world/src/resident/storage/common_codec.cpp','game/ui/native_resident_backend.gd','game/ui/resident_page_requests.gd','game/ui/screen_composer.gd','tools/content/links.py','schemas/content.schema.json','game/data/rule_capabilities.json']

def main():
    """原生共享只写候选，源码清单和外部S4依赖独立交中央登记。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);entries=[]
    for path in PATHS:
        before=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);after=transform(path,before.decode()).encode();dest=a.output/'candidate'/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(after);oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest();entries.append({'path':path,'base_oid':oid(before),'result_oid':oid(after)})
    (a.output/'manifest.json').write_text(json.dumps({'base':a.base,'files':entries,'external_source_commit':'c67989f41efed2167ecffa82d49a7d77c6261faf','external_source_leaves':['native/world/src/resident/domains/family/'+name for name in ['death_notice_source.hpp','death_notice_source.cpp','death_notice_source_codec.cpp','death_notice_military_read.hpp','death_notice_military_read.cpp']],'status':'SOURCE_ONLY_PENDING_SYNTAX_AND_INTEGRATION'},indent=2))

if __name__=='__main__':main()
