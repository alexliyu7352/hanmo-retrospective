"""固定828后的原拜帖真实结果独立尾；公共叶仅生成隔离候选。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
from task_extension_registration_candidate import replace,after
BASE='828bd693de4ff017da379ec9cc9c66cbd6318a3f'
ROOT='native/world/src/resident/domains/interaction/visit_messages/'
INCLUDE='#include "resident/domains/interaction/visit_messages/visit_message_receipt_world.hpp"\n#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"\n'
SHARED=['native/world/include/hanmo/resident/model_contract.hpp','native/world/src/resident/domains/interaction/interaction_model.hpp','native/world/src/resident/kernel/world_bundle.hpp','native/world/src/resident/kernel/resident_world.cpp','native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/kernel/phase_plan.cpp','native/world/src/resident/kernel/dynamic_integration.cpp','game/ui/native_resident_backend.gd','schemas/content.schema.json','game/data/rule_capabilities.json','tools/content/links.py','tools/verify.py','native/world/resident.cmake']

def transform(path,s,repo):
    if path.endswith('/model_contract.hpp'):
        return after(s,'std::shared_ptr<const VisitAppointmentAttendanceCatalog> visit_appointment_attendance;','    bool visit_message_receipts_enabled=false;')
    if path.endswith('/interaction_model.hpp'):
        s='#include "visit_messages/visit_message_receipt_state.hpp"\n'+s
        return after(s,'std::optional<VisitMessageState> visit_messages;','    std::optional<VisitMessageReceiptState> visit_message_receipts;')
    if path.endswith('/world_bundle.hpp'):
        return replace(s,'const StableId& political_head_handover_source_id={});','const StableId& political_head_handover_source_id={},const VisitMessageResultSources& visit_message_sources={});')
    if path.endswith('/resident_world.cpp'):
        s=INCLUDE+s
        s=replace(s,'auto visit_invitation_source=visit_appointment_new_result_world(', 'auto visit_message_sources=visit_message_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!visit_message_sources.ok)return {false,{},visit_message_sources.error};auto visit_invitation_source=visit_appointment_new_result_world(')
        s=replace(s,'visit_meeting_sources.value,political_head_handover_source_id);','visit_meeting_sources.value,political_head_handover_source_id,visit_message_sources.value);')
        s=replace(s,'const StableId& political_head_handover_source_id){','const StableId& political_head_handover_source_id,const VisitMessageResultSources& visit_message_sources){')
        return after(s,'auto visit_bound=bind_visit_appointment_result_world(w,','    auto messages_bound=bind_visit_message_result_world(w,visit_message_sources,actor,sequence,*receipt);if(!messages_bound.ok)return messages_bound;')
    if path.endswith('/world_bundle.cpp'):
        return after(INCLUDE+s,'valid=validate_family_biological_world(*this);','    valid=validate_visit_message_receipt_world(*this);if(!valid.ok)return valid;')
    if path.endswith('/phase_plan.cpp'):
        return after(INCLUDE+s,'Status try_npc_command(WorldBundle& w,','    if(w.catalog->visit_message_receipts_enabled&&actor==w.common.state().metadata.player_actor_id&&(std::holds_alternative<VisitMessageCommand>(command.data)||visit_message_command_kind(command.kind)))return common_failure("VisitMessageNpcActorRequired");')
    if path.endswith('/dynamic_integration.cpp'):
        return after(INCLUDE+s,'Status adopt_director_proposal(WorldBundle& w,','    if(w.catalog->visit_message_receipts_enabled&&proposal.command&&proposal.subject_id==w.common.state().metadata.player_actor_id&&(std::holds_alternative<VisitMessageCommand>(proposal.command->data)||visit_message_command_kind(proposal.command->kind)))return common_failure("VisitMessageNpcActorRequired");')
    if path.endswith('/world_codec.cpp'):
        s=INCLUDE+s
        s=replace(s,'if(catalog.political_head_handovers)domains.push_back({political_head_handover_domain,1});return domains;}','if(catalog.political_head_handovers)domains.push_back({political_head_handover_domain,1});if(catalog.visit_message_receipts_enabled)domains.push_back({visit_message_receipt_module,1});return domains;}')
        s=replace(s,'{"visit_messages",{"1"}}','{"visit_messages",{"1"}}, {"visit_message_receipts",{"1"}}')
        s=after(s,'auto handover_version=political_head_handover_selected_version(identity.catalog.module_versions);','        auto message_receipts=visit_message_receipt_selected_version(identity.catalog.module_versions);if(!message_receipts.ok)return {false,{},message_receipts.error};if(message_receipts.value){if(!dynamic)fail("VisitMessageReceiptProfileMismatch","checkpoint/module_versions");domains.push_back({visit_message_receipt_module,1});}')
        s=after(s,'auto handover_version=political_head_handover_selected_version(versions);','            auto message_receipts=visit_message_receipt_selected_version(versions);if(!message_receipts.ok)return {false,{},message_receipts.error};if(message_receipts.value!=version(visit_message_receipt_module)||(message_receipts.value&&!dynamic))return {false,{},common_failure("VisitMessageReceiptProfileMismatch").error};')
        s=after(s,'auto visit_messages=decode_visit_message_profile(scenario,','    auto message_receipts=decode_visit_message_receipt_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!message_receipts.ok)return {false,{},message_receipts.error};result->visit_message_receipts_enabled=message_receipts.value;')
        s=after(s,'if(result->visit_appointment_attendance)id.module_versions.emplace_back','    if(result->visit_message_receipts_enabled)id.module_versions.emplace_back(visit_message_receipt_module,"1");auto message_receipt_modules=visit_message_receipt_selected_version(id.module_versions);if(!message_receipt_modules.ok)return {false,{},message_receipt_modules.error};')
        s=replace(s,'result.interaction=std::make_shared<const InteractionState>(std::move(i.value));','if(catalog.visit_message_receipts_enabled){auto decoded=decode_visit_message_receipts(field(source,visit_message_receipt_module,"state"));if(!decoded.ok)return {false,{},decoded.error};i.value.visit_message_receipts=std::move(decoded.value);}else if(source.find(visit_message_receipt_module))fail("UnexpectedVisitMessageReceiptDomain",visit_message_receipt_module);result.interaction=std::make_shared<const InteractionState>(std::move(i.value));')
        s=after(s,'if(w.catalog->political_head_handovers){if(!w.political_head_handovers)fail(', '    if(w.catalog->visit_message_receipts_enabled){if(!w.interaction.visit_message_receipts)fail("MissingVisitMessageReceiptDomain",visit_message_receipt_module);set(output,visit_message_receipt_module,encode_visit_message_receipts(*w.interaction.visit_message_receipts));}')
        s=after(s,'if(w.catalog->political_head_handovers){if(!w.political_head_handovers)return', '    if(w.catalog->visit_message_receipts_enabled){if(!w.interaction.visit_message_receipts)return {false,{},common_failure("MissingVisitMessageReceiptDomain").error};r=section(visit_message_receipt_module,[&](auto& c){return visit_visit_message_receipts_checkpoint(c,*w.interaction.visit_message_receipts);});if(!r.ok)return {false,{},r.error};}')
        return after(s,'else if(section.domain==political_head_handover_domain){','        else if(section.domain==visit_message_receipt_module){i->visit_message_receipts.emplace();r=visit_visit_message_receipts_checkpoint(reader,*i->visit_message_receipts);}')
    if path.endswith('/native_initial_state.cpp'):
        s=after(INCLUDE+s,'auto written_attendance=decode_visit_appointment_attendance_profile(scenario,','    auto message_receipts=decode_visit_message_receipt_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!message_receipts.ok)return {false,{},message_receipts.error};if(message_receipts.value)admitted.module_versions.emplace_back(visit_message_receipt_module,"1");auto message_receipt_modules=visit_message_receipt_selected_version(admitted.module_versions);if(!message_receipt_modules.ok)return {false,{},message_receipt_modules.error};')
        return replace(s,'auto interaction=std::make_shared<InteractionState>();','auto interaction=std::make_shared<InteractionState>();if(mutable_catalog->visit_message_receipts_enabled)interaction->visit_message_receipts.emplace();')
    if path.endswith('/native_resident_backend.gd'):
        return after(s,'if definition.has("visit_appointment_attendance"):', '\tif str(definition.get("visit_message_receipt_profile", "")) == "permanent_visit_message_results_v1": expected["module_versions"].append({"name":"visit_message_receipts","version":"1"})')
    if path.endswith('/content.schema.json'):
        d=json.loads(s);p=d['$defs']['scenario']['properties'];assert 'visit_message_receipt_profile' not in p;p['visit_message_receipt_profile']={'type':'string','enum':['permanent_visit_message_results_v1']};d['properties']['required_capabilities']['items']['enum'].append('visit_message_receipts_v1');return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s);assert not any(c['capability_id']=='visit_message_receipts_v1' for c in d['capabilities']);d['capabilities'].append({'capability_id':'visit_message_receipts_v1','rule_version':'original_visit_message_actual_results_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.visit_message_receipt_profile','equals':'permanent_visit_message_results_v1'}],'supported_semantics':[],'test_ids':['test_visit_message_receipts_catalog'],'evidence':[]});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/links.py'):
        s=after(s,'from .visit_appointment_links import check_visit_appointments','    from .visit_message_receipt_links import check_visit_message_receipts');return replace(s,'+ check_visit_appointments(pack,index) +','+ check_visit_appointments(pack,index) + check_visit_message_receipts(pack,index) +')
    if path.endswith('/verify.py'):
        return after(s,"'test_visit_appointment_attendance_catalog':", "    'test_visit_message_receipts_catalog': '须正规拜帖1.2作者索引与匹配schema；只核原Snapshot与独立回执声明，不冒历史裁剪或实际NPC运行。',")
    if path.endswith('/resident.cmake'):
        anchor='    src/resident/domains/interaction/visit_messages/visit_message_world.cpp';assert s.count(anchor)==2
        cpp=['visit_message_receipt_codec','visit_message_receipt_control','visit_message_receipt_profile','visit_message_receipt_world','visit_message_receipts'];s=s.replace(anchor,anchor+''.join('\n    src/resident/domains/interaction/visit_messages/'+n+'.cpp' for n in cpp))
        anchor='    src/resident/domains/interaction/visit_messages/visit_message_world.hpp';heads=['visit_message_receipt_state','visit_message_receipt_control','visit_message_receipt_profile','visit_message_receipt_world','visit_message_receipts'];s=replace(s,anchor,anchor+''.join('\n    src/resident/domains/interaction/visit_messages/'+n+'.hpp' for n in heads))
        start=s.index('add_executable(test_resident_visit_message tests/resident/visit_message_test.cpp)');end=s.index('add_test(NAME resident_visit_message COMMAND test_resident_visit_message)',start)+len('add_test(NAME resident_visit_message COMMAND test_resident_visit_message)');block=s[start:end];return replace(s,block,block+'\n'+block.replace('test_resident_visit_message','test_resident_visit_message_receipts').replace('tests/resident/visit_message_test.cpp','tests/resident/visit_message_receipts_test.cpp').replace('NAME resident_visit_message ','NAME resident_visit_message_receipts '))
    raise ValueError(path)

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.output.exists():raise ValueError('fresh isolated output required')
    a.output.mkdir(parents=True);rows=[];patch=[]
    def oid(b):return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in SHARED:
        old=subprocess.check_output(['git','show',BASE+':'+path],cwd=a.repo);new=transform(path,old.decode(),a.repo).encode();assert new!=old,path;out=a.output/'candidate'/path;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(new);rows.append({'path':path,'base_oid':oid(old),'result_oid':oid(new),'bytes':len(new)});patch+=difflib.unified_diff(old.decode().splitlines(True),new.decode().splitlines(True),fromfile='a/'+path,tofile='b/'+path)
    (a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':BASE,'status':'SOURCE_CANDIDATE','shared':rows,'boundary':['original visit_messages visitor unchanged; separate optional receipts/1 selected explicitly','actual cache/history then bind; old source and binding rows immutable','complete Family/Birth/Legacy/Bio before receipt history; same-tick original control pauses NPC hot preview/commit','Interaction savepoints already cover original VisitMessageCommand; NPC cannot submit as current player','no new Common fact, no payload copy, no implicit Shop dependency'],'deferred':['behavior','World','GUI']},ensure_ascii=False,indent=2)+'\n');print(json.dumps({'shared':len(rows),'manifest':str(a.output/'manifest.json')}))
if __name__=='__main__':main()
