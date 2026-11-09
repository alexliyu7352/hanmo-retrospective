"""固定共享基准的私人预约接线候选；实际中央写入、构建和发布仍由中央串行处理。"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
from task_extension_registration_candidate import replace, after
BASE='3dba21d9441eb06446ab5c681017dcc929382ef9'
ROOT='native/world/src/resident/domains/interaction/appointments/'
INCLUDE='#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"\n'
PROFILE='witnessed_personal_meetings_v1'
CAP='personal_appointments_v1'
CPP=['catalog','validation','rules','codec','profile','view','boundary','payloads','world']
HEADERS=['','_state','_detail','_view','_boundary','_world']
SHARED=['native/world/src/resident/domains/interaction/interaction_model.hpp','native/world/include/hanmo/resident/model_contract.hpp',
'native/world/src/resident/kernel/command_types.hpp','native/world/src/resident/kernel/dispatcher.cpp',
'native/world/src/resident/checkpoint/command_boundary.cpp','native/world/src/resident/kernel/phase_plan.cpp',
'native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp',
'native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/storage/common_codec.cpp',
'native/world/src/resident/kernel/live_view_queries.cpp','game/ui/native_resident_backend.gd',
'game/ui/resident_page_requests.gd','game/ui/screen_composer.gd','schemas/content.schema.json',
'game/data/rule_capabilities.json','tools/content/links.py','tools/verify.py','native/world/resident.cmake']


def transform(path,s):
    """所有锚点精确唯一；原Interaction/Task/回执variant布局不添加空持久字段。"""
    if path.endswith('/interaction_model.hpp'):
        s=replace(s,'#pragma once','#pragma once\n#include "appointments/personal_appointment_state.hpp"')
        return replace(s,'std::optional<VisitReceptionState> visit_reception_claims;','std::optional<VisitReceptionState> visit_reception_claims;\n    std::optional<PersonalAppointmentState> personal_appointments;')
    if path.endswith('/model_contract.hpp'):
        s=replace(s,'struct TaskExtensionCatalog;','struct TaskExtensionCatalog;struct PersonalAppointmentCatalog;')
        return replace(s,'std::shared_ptr<const TaskExtensionCatalog> task_extensions;','std::shared_ptr<const TaskExtensionCatalog> task_extensions;\n    std::shared_ptr<const PersonalAppointmentCatalog> personal_appointments;')
    if path.endswith('/command_types.hpp'):
        s=replace(INCLUDE+s,'TaskExtensionCommand>;','TaskExtensionCommand,PersonalAppointmentCommand>;')
        return replace(s,'TaskExtensionQuote>;','TaskExtensionQuote,PersonalAppointmentQuote>;')
    if path.endswith('/dispatcher.cpp'):
        s=INCLUDE+s
        s=after(s,'const auto* extension=std::get_if<TaskExtensionCommand>(&command.data);','    const auto* appointment=std::get_if<PersonalAppointmentCommand>(&command.data);if(is_personal_appointment_command(command.kind)!=(appointment!=nullptr)||(appointment&&personal_appointment_command_kind(*appointment)!=command.kind))return reject("InvalidPersonalAppointmentCommand");')
        s=after(s,'if(extension){auto quote=prepare_task_extension_world','    if(appointment){auto quote=prepare_personal_appointment_world(w,id,*appointment);if(!quote.ok)return {false,{},quote.error};return ready(w,id,command,std::move(quote.value));}')
        s=after(s,'const auto* extension=std::get_if<TaskExtensionCommand>(&op.command.data);','    const auto* appointment=std::get_if<PersonalAppointmentCommand>(&op.command.data);if(is_personal_appointment_command(op.command.kind)!=(appointment!=nullptr)||(appointment&&(personal_appointment_command_kind(*appointment)!=op.command.kind||!std::holds_alternative<PersonalAppointmentQuote>(op.quote.data))))return failed<ReceiptDetails>(common_failure("InvalidPersonalAppointmentCommand"));')
        return after(s,'if(extension){auto result=commit_task_extension_world','    if(appointment){auto result=commit_personal_appointment_world(w,id,*appointment,std::get<PersonalAppointmentQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};if(result.value.action_started)return {true,ReceiptDetails{ActionStarted{result.value.source_id},{}},{}};InteractionEffect effect;effect.settlement_result=result.value.source_id;return {true,ReceiptDetails{std::move(effect),{}},{}};}')
    if path.endswith('/command_boundary.cpp'):
        s=INCLUDE+s
        s=after(s,'if(kind==task_extension_command_kind){auto decoded=','    if(is_personal_appointment_command(kind)){auto decoded=decode_personal_appointment_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}')
        s=after(s,'else if constexpr(std::is_same_v<T,TaskExtensionQuote>)return','                else if constexpr(std::is_same_v<T,PersonalAppointmentQuote>)return encode_personal_appointment_quote(quote,preview.quote->kind==personal_appointment_action_kind);')
        s=replace(s,'else if(visit_message_command_kind(kind)||kind==task_extension_command_kind){boundary::fields','else if(visit_message_command_kind(kind)||kind==task_extension_command_kind||kind=="personal_appointment_accept"||kind=="personal_appointment_cancel"){boundary::fields')
        s=replace(s,'if((visit_message_command_kind(kind)||kind==task_extension_command_kind)&&','if((visit_message_command_kind(kind)||kind==task_extension_command_kind||kind=="personal_appointment_accept"||kind=="personal_appointment_cancel")&&')
        return replace(s,'if(visit_message_command_kind(r.kind)||r.kind==task_extension_command_kind){const auto* effect=','if(visit_message_command_kind(r.kind)||r.kind==task_extension_command_kind||r.kind=="personal_appointment_accept"||r.kind=="personal_appointment_cancel"){const auto* effect=')
    if path.endswith('/phase_plan.cpp'):
        s=INCLUDE+s
        s=replace(s,'auto interaction=(shop_command||','auto interaction=(std::holds_alternative<PersonalAppointmentCommand>(command.data)||shop_command||')
        return replace(s,'}else if(action.kind=="task_shop_order_deliver"){','}else if(action.kind==personal_appointment_action_kind){\n                    if(action.remaining_ticks!=1){r=common_failure("PersonalAppointmentActionMismatch",id);break;}ActionMutation mutation;mutation.action=action;mutation.remaining_ticks=0;r=w.common.apply_action(mutation);if(!r.ok)break;action.remaining_ticks=0;auto step=step_personal_appointment_world(w,action,dynamic_input);if(!step.ok){r={false,{},step.error};break;}completed=true;outcome=personal_appointment_completion(step.value);\n                }else if(action.kind=="task_shop_order_deliver"){')
    if path.endswith('/native_initial_state.cpp'):
        s=after(INCLUDE+s,'if(task_extensions.value)admitted.module_versions.emplace_back','    auto appointments=decode_personal_appointment_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!appointments.ok)return {false,{},appointments.error};if(appointments.value)admitted.module_versions.emplace_back("personal_appointments","1");')
        return replace(s,'auto interaction=std::make_shared<InteractionState>();','auto interaction=std::make_shared<InteractionState>();if(mutable_catalog->personal_appointments)interaction->personal_appointments.emplace();')
    if path.endswith('/world_codec.cpp'):
        s=INCLUDE+s
        s=replace(s,'if(catalog.task_extensions)domains.push_back({"task_extensions",1});return domains;','if(catalog.task_extensions)domains.push_back({"task_extensions",1});if(catalog.personal_appointments)domains.push_back({"personal_appointments",1});return domains;')
        s=replace(s,'{"task_extensions",{"1"}}','{"task_extensions",{"1"}}, {"personal_appointments",{"1"}}')
        s=after(s,'if(has("task_extensions")){','        if(has("personal_appointments")){if(!dynamic)fail("PersonalAppointmentProfileMismatch","checkpoint/module_versions");domains.push_back({"personal_appointments",1});}')
        s=after(s,'if(has("task_extensions","1")!=','            if(has("personal_appointments","1")!=(version("personal_appointments")==1)||(!dynamic&&version("personal_appointments")))return {false,{},common_failure("PersonalAppointmentProfileMismatch").error};')
        s=after(s,'auto task_extensions=decode_task_extension_profile(scenario','    auto appointments=decode_personal_appointment_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!appointments.ok)return {false,{},appointments.error};if(appointments.value){auto decoded=decode_personal_appointment_catalog(field(scenario,"personal_appointments","scenario"),*result->interaction);if(!decoded.ok)return {false,{},decoded.error};result->personal_appointments=std::make_shared<const PersonalAppointmentCatalog>(std::move(decoded.value));}')
        s=after(s,'if(result->task_extensions)id.module_versions','    if(result->personal_appointments)id.module_versions.emplace_back("personal_appointments","1");')
        s=replace(s,'result.interaction=std::make_shared<const InteractionState>(std::move(i.value));','if(catalog.personal_appointments){auto decoded=decode_personal_appointments(field(source,"personal_appointments","state"));if(!decoded.ok)return {false,{},decoded.error};i.value.personal_appointments=std::move(decoded.value);}else if(source.find("personal_appointments"))fail("UnexpectedPersonalAppointmentDomain","personal_appointments");result.interaction=std::make_shared<const InteractionState>(std::move(i.value));')
        s=after(s,'// 本profile的wait没有业务params；','    {CommonStore view(result.common,catalog.common);auto checked=validate_personal_appointment_json_payloads(view,result.interaction&&result.interaction->personal_appointments?&*result.interaction->personal_appointments:nullptr,source);if(!checked.ok)return {false,{},checked.error};}')
        s=after(s,'auto actions=shop_action_payloads(w.common,','        auto appointments=personal_appointment_fact_payloads(w.common,w.interaction.personal_appointments?&*w.interaction.personal_appointments:nullptr);if(!appointments.ok)return appointments;merge_unique(facts,appointments.value);auto appointment_actions=personal_appointment_action_payloads(w.common,w.interaction.personal_appointments?&*w.interaction.personal_appointments:nullptr);if(!appointment_actions.ok)return appointment_actions;merge_unique(params,appointment_actions.value);')
        s=after(s,'set(output,"task_extensions",','    if(w.catalog->personal_appointments){if(!w.interaction.personal_appointments)fail("MissingPersonalAppointmentDomain","personal_appointments");set(output,"personal_appointments",encode_personal_appointments(*w.interaction.personal_appointments));}')
        s=after(s,'r=section("task_extensions",','    if(w.catalog->personal_appointments){if(!w.interaction.personal_appointments)return {false,{},common_failure("MissingPersonalAppointmentDomain").error};r=section("personal_appointments",[&](auto& c){return visit_personal_appointments_checkpoint(c,*w.interaction.personal_appointments);});if(!r.ok)return {false,{},r.error};}')
        return after(s,'else if(section.domain=="task_extensions")','        else if(section.domain=="personal_appointments"){i->personal_appointments.emplace();r=visit_personal_appointments_checkpoint(reader,*i->personal_appointments);}')
    if path.endswith('/world_bundle.cpp'):
        s=INCLUDE+s
        s=after(s,'if(w.tasks.deadline_extensions)for(const auto& row:','    if(w.interaction.personal_appointments){auto payloads=personal_appointment_fact_payloads(w.common,&*w.interaction.personal_appointments);if(!payloads.ok)return {false,{},payloads.error};for(const auto& row:boundary::object(payloads.value,"personal_appointments")){auto r=fact(row.first,boundary::get_string(row.second,"kind",row.first),personal_appointment_module);if(!r.ok)return r;}}')
        s=after(s,'if(w.tasks.shop_orders)for(const auto& row:w.tasks.shop_orders->actions.rows())','    {auto payloads=personal_appointment_action_payloads(w.common,w.interaction.personal_appointments?&*w.interaction.personal_appointments:nullptr);if(!payloads.ok)return {false,{},payloads.error};for(const auto& row:boundary::object(payloads.value,"personal_appointment_actions")){auto r=action(row.first,personal_appointment_action_kind);if(!r.ok)return r;}}')
        return after(s,'valid=validate_task_extension_world(*this);','    valid=validate_personal_appointment_world(*this);if(!valid.ok)return valid;')
    if path.endswith('/common_codec.cpp'):
        kinds='kind=="personal_appointment_accepted"||kind=="personal_appointment_cancelled"||kind=="personal_appointment_started"||kind=="personal_appointment_finished"'
        s=replace(s,'std::string domain=kind=="task_deadline_extended"','const bool appointment='+kinds+';std::string domain=appointment?"personal_appointments":kind=="task_deadline_extended"')
        s=replace(s,'if(kind=="task_deadline_extended"&&named!=','if(appointment&&named!="personal_appointments")fail("InvalidFactDomain",row.first);if(kind=="task_deadline_extended"&&named!=')
        return replace(s,'if(!(named=="task_extensions"','if(!(named=="personal_appointments"&&appointment)&&!(named=="task_extensions"')
    if path.endswith('/live_view_queries.cpp'):
        return after(INCLUDE+s,'if(query=="task_extensions"){','    if(query=="personal_appointments"){boundary::fields(args,{},{"cursor"},"args");return query_personal_appointment_world(world,args.find("cursor")?boundary::get_int(args,"cursor","args"):0);}')
    if path.endswith('/native_resident_backend.gd'):
        return after(s,'definition.has("task_extensions")','\tif definition.get("personal_appointment_profile","")=="witnessed_personal_meetings_v1": expected["module_versions"].append({"name":"personal_appointments","version":"1"})')
    if path.endswith('/resident_page_requests.gd'):
        s=after(s,'const TaskExtensionPanel=','const PersonalAppointmentPanel=preload("res://ui/personal_appointment_panel.gd")')
        return after(s,'if not TaskExtensionPanel.prepare(host):','\tif not PersonalAppointmentPanel.prepare(host): return false')
    if path.endswith('/screen_composer.gd'):
        s=after(s,'const TaskExtensionPanel=','const PersonalAppointmentPanel=preload("res://ui/personal_appointment_panel.gd")')
        lines=[x for x in s.splitlines() if 'TaskExtensionPanel.visible_here(m)' in x]
        if len(lines)!=2:raise ValueError('two original personal sidebars required')
        for line in lines:s=replace(s,'\n'+line+'\n','\n'+line+'\n'+line[:len(line)-len(line.lstrip())]+'if m._resident_mode and PersonalAppointmentPanel.visible_here(m): PersonalAppointmentPanel.build(m,_sidebar_section(m,stage,"PersonalAppointmentArea","本人会面约定"))\n')
        return s
    if path.endswith('/content.schema.json'):
        d=json.loads(s);p=d['$defs']['scenario']['properties'];assert 'personal_appointments' not in p
        fields={'id':{'type':'string','minLength':1,'maxLength':256},'provider_actor_id':{'$ref':'#/$defs/reference_actors'},'role_id':{'$ref':'#/$defs/reference_roles'},'dialogue_id':{'$ref':'#/$defs/reference_dialogues'}}
        fields.update({k:{'type':'string','minLength':1,'maxLength':256} for k in ['accept_node_id','cancel_node_id','meet_node_id']});fields.update({k:{'type':'integer','minimum':1,'maximum':720} for k in ['earliest_offset','latest_offset']});fields['duration_ticks']={'type':'integer','enum':[1]}
        p['personal_appointment_profile']={'type':'string','enum':[PROFILE]};p['personal_appointments']={'type':'object','additionalProperties':False,'required':['version','policies'],'properties':{'version':{'type':'integer','enum':[1]},'policies':{'type':'array','minItems':1,'maxItems':4096,'items':{'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}}}};d['properties']['required_capabilities']['items']['enum'].append(CAP);return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s);assert not any(x['capability_id']==CAP for x in d['capabilities']);d['capabilities'].append({'capability_id':CAP,'rule_version':PROFILE,'module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.personal_appointment_profile','equals':PROFILE}],'supported_semantics':[],'test_ids':['test_personal_appointment_panel_guard'],'evidence':[],'limitations':['首片仅原私人岗位本人当面答允、当面取消和一个半日实际会面；不含远程答复、代表授权、改约或自动NPC赴约。']});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/content/links.py':
        s=after(s,'from .task_extension_links import','    from .personal_appointment_links import check_personal_appointments')
        return replace(s,' + check_task_extensions(pack,index)',' + check_task_extensions(pack,index) + check_personal_appointments(pack,index)')
    if path=='tools/verify.py':return replace(s,"'test_task_extension_panel_guard',","'test_task_extension_panel_guard', 'test_personal_appointment_panel_guard',")
    if path.endswith('/resident.cmake'):
        anchor='    src/resident/domains/tasks/extensions/task_extension_world.cpp';assert s.count(anchor)==2
        s=s.replace(anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+'personal_appointment_'+x+'.cpp' for x in CPP))
        anchor='    src/resident/domains/tasks/extensions/task_extension_world.hpp';s=replace(s,anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+'personal_appointment'+x+'.hpp' for x in HEADERS))
        return s+'\n# 私人预约原会面来源、约期冲突、实际Action和坏档门，运行按风险另排。\nadd_executable(test_resident_personal_appointment tests/resident/personal_appointment_test.cpp)\ntarget_link_libraries(test_resident_personal_appointment PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_personal_appointment PRIVATE src)\ntarget_compile_options(test_resident_personal_appointment PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_personal_appointment COMMAND test_resident_personal_appointment)\n'
    raise ValueError(path)


def main():
    """全部锚点成功才写完整候选，并保精确基准/结果OID；不覆盖中央或旧候选。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.output.exists():raise ValueError('fresh output required')
    results={};rows=[];patch=[];oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in SHARED:
        before=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);s=transform(path,before.decode());data=s.encode();results[path]=data;rows.append({'path':path,'base_oid':oid(before),'result_oid':oid(data)});patch.extend(difflib.unified_diff(before.decode().splitlines(True),s.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
    for path,data in results.items():target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    (a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':a.base,'status':'SOURCE_ONLY_PENDING_SYNTAX','shared':rows,'module':'personal_appointments/1','old_visitors_unchanged':['Interaction','Task1','Task2','ReceiptData'],'build_cpp':[ROOT+'personal_appointment_'+x+'.cpp' for x in CPP],'build_headers':[ROOT+'personal_appointment'+x+'.hpp' for x in HEADERS],'external_publication':'central/root only; original shared publication restriction retained'},ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()
