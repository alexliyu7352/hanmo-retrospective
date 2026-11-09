"""当面问去向完整共享候选；固定Git来源，不写中央或改旧Interaction/Task访客字节。"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
from task_extension_registration_candidate import replace, after
BASE='c6bdbcf538f6ed4dabc3f599c006336054ed83d7'
ROOT='native/world/src/resident/domains/interaction/contact_inquiries/'
INCLUDE='#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"\n'
PROFILE='witnessed_personal_inquiries_v1'
CAP='contact_inquiries_v1'
CPP=['catalog','codec','rules','validation','receipts','progress','boundary','view','payloads','world']
HEADERS=['','_state','_detail','_receipts','_progress','_boundary','_view','_world']
SHARED=['native/world/src/resident/domains/interaction/interaction_model.hpp','native/world/include/hanmo/resident/model_contract.hpp',
'native/world/src/resident/kernel/command_types.hpp','native/world/src/resident/kernel/dispatcher.cpp',
'native/world/src/resident/checkpoint/command_boundary.cpp','native/world/src/resident/kernel/phase_plan.cpp',
'native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp',
'native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/kernel/world_bundle.hpp',
'native/world/src/resident/kernel/resident_world.cpp','native/world/src/resident/storage/common_codec.cpp',
'native/world/src/resident/kernel/live_view_queries.cpp','game/ui/native_resident_backend.gd',
'game/ui/resident_page_requests.gd','game/ui/screen_composer.gd','schemas/content.schema.json',
'game/data/rule_capabilities.json','tools/content/links.py','tools/verify.py','native/world/resident.cmake']

def append_variant(s,name,value):
    """只追加现存命令/报价C++类型，不改任何持久ReceiptData序号。"""
    lines=[x for x in s.splitlines() if x.startswith('using '+name+'=std::variant<')]
    if len(lines)!=1 or not lines[0].endswith('>;'):raise ValueError('variant '+name)
    return replace(s,lines[0],lines[0][:-2]+','+value+'>;')

def transform(path,s):
    """逐处具名锚点保当前法律/清店/预约尾；晚冷验在原人物和亲见来源完整校验之后。"""
    if path.endswith('/interaction_model.hpp'):
        s=replace(s,'#pragma once','#pragma once\n#include "contact_inquiries/contact_inquiry_state.hpp"')
        return replace(s,'std::optional<PersonalAppointmentState> personal_appointments;','std::optional<PersonalAppointmentState> personal_appointments;\n    std::optional<ContactInquiryState> contact_inquiries;')
    if path.endswith('/model_contract.hpp'):
        s=replace(s,'struct PersonalAppointmentCatalog;','struct PersonalAppointmentCatalog;struct ContactInquiryCatalog;')
        return replace(s,'std::shared_ptr<const PersonalAppointmentCatalog> personal_appointments;','std::shared_ptr<const PersonalAppointmentCatalog> personal_appointments;\n    std::shared_ptr<const ContactInquiryCatalog> contact_inquiries;')
    if path.endswith('/command_types.hpp'):
        return append_variant(append_variant(INCLUDE+s,'CommandData','ContactInquiryRequest'),'QuoteData','ContactInquiryQuote')
    if path.endswith('/dispatcher.cpp'):
        s=INCLUDE+s
        s=after(s,'const auto* appointment=std::get_if<PersonalAppointmentCommand>(&command.data);','    const auto* inquiry=std::get_if<ContactInquiryRequest>(&command.data);if((command.kind==contact_inquiry_action_kind)!=(inquiry!=nullptr))return reject("InvalidContactInquiryCommand");')
        s=after(s,'if(appointment){auto quote=prepare_personal_appointment_world','    if(inquiry){auto quote=prepare_contact_inquiry_world(w,id,*inquiry);if(!quote.ok)return {false,{},quote.error};return ready(w,id,command,std::move(quote.value));}')
        s=after(s,'const auto* appointment=std::get_if<PersonalAppointmentCommand>(&op.command.data);','    const auto* inquiry=std::get_if<ContactInquiryRequest>(&op.command.data);if((op.command.kind==contact_inquiry_action_kind)!=(inquiry!=nullptr)||(inquiry&&!std::holds_alternative<ContactInquiryQuote>(op.quote.data)))return failed<ReceiptDetails>(common_failure("InvalidContactInquiryCommand"));')
        return after(s,'if(appointment){auto result=commit_personal_appointment_world','    if(inquiry){auto result=commit_contact_inquiry_world(w,id,*inquiry,std::get<ContactInquiryQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{ActionStarted{result.value},{}},{}};}')
    if path.endswith('/command_boundary.cpp'):
        s=after(INCLUDE+s,'if(is_personal_appointment_command(kind)){auto decoded=','    if(kind==contact_inquiry_action_kind){auto decoded=decode_contact_inquiry_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}')
        return after(s,'else if constexpr(std::is_same_v<T,PersonalAppointmentQuote>)return','                else if constexpr(std::is_same_v<T,ContactInquiryQuote>)return encode_contact_inquiry_quote(quote);')
    if path.endswith('/phase_plan.cpp'):
        s=replace(INCLUDE+s,'std::holds_alternative<PersonalAppointmentCommand>(command.data)||','std::holds_alternative<PersonalAppointmentCommand>(command.data)||std::holds_alternative<ContactInquiryRequest>(command.data)||')
        s=after(s,'Status try_npc_command(WorldBundle& w,','    applied=false;if(std::holds_alternative<ContactInquiryRequest>(command.data)||command.kind==contact_inquiry_action_kind)return common_failure("ContactInquiryPlayerCommandRequired");')
        return replace(s,'}else if(action.kind==personal_appointment_action_kind){','}else if(action.kind==contact_inquiry_action_kind){\n                    if(action.remaining_ticks!=1){r=common_failure("ContactInquiryActionMismatch",id);break;}ActionMutation mutation;mutation.action=action;mutation.remaining_ticks=0;r=w.common.apply_action(mutation);if(!r.ok)break;action.remaining_ticks=0;auto step=step_contact_inquiry_world(w,action,dynamic_input);if(!step.ok){r={false,{},step.error};break;}completed=true;outcome=contact_inquiry_completion(step.value);\n                }else if(action.kind==personal_appointment_action_kind){')
    if path.endswith('/native_initial_state.cpp'):
        s=after(INCLUDE+s,'auto liquidations=decode_private_shop_liquidation_profile(scenario,','    auto inquiries=decode_contact_inquiry_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!inquiries.ok)return {false,{},inquiries.error};if(inquiries.value)admitted.module_versions.emplace_back("contact_inquiries","1");')
        return replace(s,'auto interaction=std::make_shared<InteractionState>();','auto interaction=std::make_shared<InteractionState>();if(mutable_catalog->contact_inquiries)interaction->contact_inquiries.emplace();')
    if path.endswith('/world_codec.cpp'):
        s=INCLUDE+s
        s=replace(s,'domains.push_back({"task_shop_order_estate_cancellations",1});}return domains;','domains.push_back({"task_shop_order_estate_cancellations",1});}if(catalog.contact_inquiries)domains.push_back({"contact_inquiries",1});return domains;')
        s=replace(s,'{"personal_appointments",{"1"}}','{"personal_appointments",{"1"}}, {"contact_inquiries",{"1"}}')
        s=after(s,'{const bool estate=has("private_shop_liquidations");','        if(has("contact_inquiries")){if(!dynamic||(!has("personal_appointments")&&!has("task_message_handoffs")))fail("ContactInquiryProfileMismatch","checkpoint/module_versions");domains.push_back({"contact_inquiries",1});}')
        s=after(s,'if(has("personal_appointments","1")!=','            if(has("contact_inquiries","1")!=(version("contact_inquiries")==1)||(version("contact_inquiries")&&(!dynamic||(!has("personal_appointments","1")&&!has("task_message_handoffs","1")&&!has("task_message_handoffs","2")))))return {false,{},common_failure("ContactInquiryProfileMismatch").error};')
        s=after(s,'auto appointments=decode_personal_appointment_profile(scenario,','    auto inquiries=decode_contact_inquiry_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!inquiries.ok)return {false,{},inquiries.error};if(inquiries.value){auto decoded=decode_contact_inquiry_catalog(field(scenario,"contact_inquiries","scenario"),*result->interaction);if(!decoded.ok)return {false,{},decoded.error};result->contact_inquiries=std::make_shared<const ContactInquiryCatalog>(std::move(decoded.value));}')
        s=after(s,'if(result->private_shop_liquidations){id.module_versions.emplace_back','    if(result->contact_inquiries)id.module_versions.emplace_back("contact_inquiries","1");')
        s=replace(s,'result.interaction=std::make_shared<const InteractionState>(std::move(i.value));','if(catalog.contact_inquiries){auto decoded=decode_contact_inquiries(field(source,"contact_inquiries","state"));if(!decoded.ok)return {false,{},decoded.error};i.value.contact_inquiries=std::move(decoded.value);}else if(source.find("contact_inquiries"))fail("UnexpectedContactInquiryDomain","contact_inquiries");result.interaction=std::make_shared<const InteractionState>(std::move(i.value));')
        s=after(s,'auto checked=validate_personal_appointment_json_payloads(','    {CommonStore view(result.common,catalog.common);auto checked=validate_contact_inquiry_json_payloads(view,result.interaction&&result.interaction->contact_inquiries?&*result.interaction->contact_inquiries:nullptr,source);if(!checked.ok)return {false,{},checked.error};}')
        s=after(s,'auto appointments=personal_appointment_fact_payloads(','        auto inquiries=contact_inquiry_fact_payloads(w.common,w.interaction.contact_inquiries?&*w.interaction.contact_inquiries:nullptr);if(!inquiries.ok)return inquiries;merge_unique(facts,inquiries.value);auto inquiry_actions=contact_inquiry_action_payloads(w.common,w.interaction.contact_inquiries?&*w.interaction.contact_inquiries:nullptr);if(!inquiry_actions.ok)return inquiry_actions;merge_unique(params,inquiry_actions.value);')
        s=after(s,'set(output,"task_shop_order_estate_cancellations",','    if(w.catalog->contact_inquiries){if(!w.interaction.contact_inquiries)fail("MissingContactInquiryDomain","contact_inquiries");set(output,"contact_inquiries",encode_contact_inquiries(*w.interaction.contact_inquiries));}')
        s=after(s,'r=section("task_shop_order_estate_cancellations",','    if(w.catalog->contact_inquiries){if(!w.interaction.contact_inquiries)return {false,{},common_failure("MissingContactInquiryDomain").error};r=section("contact_inquiries",[&](auto& c){return visit_contact_inquiries_checkpoint(c,*w.interaction.contact_inquiries);});if(!r.ok)return {false,{},r.error};}')
        return after(s,'else if(section.domain=="personal_appointments")','        else if(section.domain=="contact_inquiries"){i->contact_inquiries.emplace();r=visit_contact_inquiries_checkpoint(reader,*i->contact_inquiries);}')
    if path.endswith('/world_bundle.cpp'):
        s=INCLUDE+s
        s=after(s,'auto payloads=personal_appointment_fact_payloads(','    {auto payloads=contact_inquiry_fact_payloads(w.common,w.interaction.contact_inquiries?&*w.interaction.contact_inquiries:nullptr);if(!payloads.ok)return {false,{},payloads.error};for(const auto& row:boundary::object(payloads.value,"contact_inquiries")){auto r=fact(row.first,boundary::get_string(row.second,"kind",row.first),contact_inquiry_module);if(!r.ok)return r;}}')
        s=after(s,'auto payloads=personal_appointment_action_payloads(','    {auto payloads=contact_inquiry_action_payloads(w.common,w.interaction.contact_inquiries?&*w.interaction.contact_inquiries:nullptr);if(!payloads.ok)return {false,{},payloads.error};for(const auto& row:boundary::object(payloads.value,"contact_inquiry_actions")){auto r=action(row.first,contact_inquiry_action_kind);if(!r.ok)return r;}}')
        # 原早Plan先跳过此显式新段；完整亲见和人物来源后，才准Plan读取新完成源。
        lines=s.splitlines(True)
        for n,line in enumerate(lines):
            if 'validate_player_plan_world(*this)' in line:
                if 'validate_player_plan(common,player_plan)' in line:
                    lines[n]=line.replace('if(!catalog->personal_appointments', 'if(!catalog->contact_inquiries&&!catalog->personal_appointments',1)
                else:
                    lines[n]=line.replace('if(player_plan_enabled()', 'if(player_plan_enabled()&&!catalog->contact_inquiries',1)
        s=''.join(lines)
        s=after(s,'valid=validate_personal_appointment_world(*this);','    valid=validate_contact_inquiry_world(*this);if(!valid.ok)return valid;')
        return after(s,'if(player_plan_enabled()&&!catalog->contact_inquiries&&catalog->personal_appointments','    if(player_plan_enabled()&&catalog->contact_inquiries){valid=validate_player_plan_world(*this);if(!valid.ok)return valid;}')
    if path.endswith('/resident_world.cpp'):
        s=INCLUDE+s
        s=replace(s,'auto appointment_sources=personal_appointment_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);','auto inquiry_sources=contact_inquiry_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!inquiry_sources.ok)return {false,{},inquiry_sources.error};auto appointment_sources=personal_appointment_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);')
        s=replace(s,'?&*current.tasks.shop_orders->estate_cancellations:nullptr);','?&*current.tasks.shop_orders->estate_cancellations:nullptr,inquiry_sources.value);')
        s=replace(s,'const TaskShopOrderEstateCancellationState* estate_orders_before){','const TaskShopOrderEstateCancellationState* estate_orders_before,const std::vector<StableId>& inquiry_sources){')
        return after(s,'auto appointment_bound=bind_personal_appointment_result_world','    auto inquiry_bound=bind_contact_inquiry_result_world(w,inquiry_sources,actor,sequence,*receipt);if(!inquiry_bound.ok)return inquiry_bound;')
    if path.endswith('/world_bundle.hpp'):
        return replace(s,'const TaskShopOrderEstateCancellationState* estate_orders_before=nullptr);','const TaskShopOrderEstateCancellationState* estate_orders_before=nullptr,const std::vector<StableId>& inquiry_sources={});')
    if path.endswith('/common_codec.cpp'):
        s=replace(s,'const bool appointment=','const bool inquiry=kind=="contact_inquiry_started"||kind=="contact_inquiry_finished";const bool appointment=')
        s=replace(s,'std::string domain=liquidation?','std::string domain=inquiry?"contact_inquiries":liquidation?')
        s=replace(s,'if(appointment&&named!=','if(inquiry&&named!="contact_inquiries")fail("InvalidFactDomain",row.first);if(appointment&&named!=')
        return replace(s,'&&!(named=="personal_appointments"','&&!(named=="contact_inquiries"&&inquiry)&&!(named=="personal_appointments"')
    if path.endswith('/live_view_queries.cpp'):
        return after(INCLUDE+s,'if(query=="personal_appointments"){','    if(query=="contact_inquiries"){boundary::fields(args,{},{"cursor"},"args");return query_contact_inquiry_world(world,args.find("cursor")?boundary::get_int(args,"cursor","args"):0);}')
    if path.endswith('/native_resident_backend.gd'):
        s=after(s,'expected["module_versions"].append({"name":"task_shop_order_estate_cancellations"','\tif definition.get("contact_inquiry_profile","")=="witnessed_personal_inquiries_v1": expected["module_versions"].append({"name":"contact_inquiries","version":"1"})')
        return s+'\n## 只显式同版本目录启用问去向；原亲见模块另由Native完整配对。\nfunc contact_inquiries_enabled() -> bool:\n\treturn _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("contact_inquiry_profile","")=="witnessed_personal_inquiries_v1"\n'
    if path.endswith('/resident_page_requests.gd'):
        s=after(s,'const PersonalAppointmentPanel=','const ContactInquiriesPanel=preload("res://ui/contact_inquiries_panel.gd")')
        return after(s,'if not PersonalAppointmentPanel.prepare(host):','\tif host.kernel.has_method("contact_inquiries_enabled") and host.kernel.contact_inquiries_enabled() and not ContactInquiriesPanel.prepare(host): return false')
    if path.endswith('/screen_composer.gd'):
        s=after(s,'const PersonalAppointmentPanel=','const ContactInquiriesPanel=preload("res://ui/contact_inquiries_panel.gd")')
        lines=[x for x in s.splitlines() if 'PersonalAppointmentPanel.visible_here(m)' in x]
        if len(lines)!=2:raise ValueError('two actual appointment sidebars required')
        for line in lines:s=replace(s,'\n'+line+'\n','\n'+line+'\n'+line[:len(line)-len(line.lstrip())]+'if m._resident_mode and m.kernel.has_method("contact_inquiries_enabled") and m.kernel.contact_inquiries_enabled() and ContactInquiriesPanel.visible_here(m): ContactInquiriesPanel.build(m,_sidebar_section(m,stage,"ContactInquiriesArea","当面问去向"))\n')
        return s
    if path.endswith('/content.schema.json'):
        d=json.loads(s);p=d['$defs']['scenario']['properties'];assert 'contact_inquiries' not in p
        fields={k:{'type':'string','minLength':1,'maxLength':256} for k in ['id','node_id']}
        fields.update({k:{'$ref':'#/$defs/reference_'+v} for k,v in [('provider_actor_id','actors'),('subject_actor_id','actors'),('role_id','roles'),('dialogue_id','dialogues')]});fields.update(disclosure={'type':'string','enum':['share_last_seen','decline']},duration_ticks={'type':'integer','enum':[1]})
        p['contact_inquiry_profile']={'type':'string','enum':[PROFILE]};p['contact_inquiries']={'type':'object','additionalProperties':False,'required':['version','policies'],'properties':{'version':{'type':'integer','enum':[1]},'policies':{'type':'array','minItems':1,'maxItems':4096,'items':{'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}}}};d['properties']['required_capabilities']['items']['enum'].append(CAP);return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s);assert not any(x['capability_id']==CAP for x in d['capabilities']);d['capabilities'].append({'capability_id':CAP,'rule_version':PROFILE,'module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.contact_inquiry_profile','equals':PROFILE}],'supported_semantics':[],'test_ids':['test_contact_inquiries_panel_guard'],'evidence':[],'limitations':['原私人本人say后消耗一个半日；只答复本人在开工前的原亲见，明确未知或拒答。无远方实时定位、转述递归、NPC自动问话或社交奖励。']});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/content/links.py':
        s=after(s,'from .personal_appointment_links import','    from .contact_inquiry_links import check_contact_inquiries')
        return replace(s,' + check_personal_appointments(pack,index)',' + check_personal_appointments(pack,index) + check_contact_inquiries(pack,index)')
    if path=='tools/verify.py':return replace(s,"'test_personal_appointment_panel_guard',","'test_personal_appointment_panel_guard', 'test_contact_inquiries_panel_guard',")
    if path.endswith('/resident.cmake'):
        anchor='    src/resident/domains/interaction/appointments/personal_appointment_world.cpp';assert s.count(anchor)==2
        s=s.replace(anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+'contact_inquiry_'+x+'.cpp' for x in CPP))
        anchor='    src/resident/domains/interaction/appointments/personal_appointment_world.hpp';s=replace(s,anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+'contact_inquiry'+x+'.hpp' for x in HEADERS)+'\n    src/resident/domains/interaction/known_contacts/known_contact_location_contract.hpp')
        for test in ['source','action','receipts']:
            name='contact_inquiry_'+test;s+='\n# 问去向具名原亲见、实际Action及永久请求来源门；当前运行后置。\nadd_executable(test_resident_'+name+' tests/resident/'+name+'_test.cpp)\ntarget_link_libraries(test_resident_'+name+' PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_'+name+' PRIVATE src)\ntarget_compile_options(test_resident_'+name+' PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_'+name+' COMMAND test_resident_'+name+')\n'
        return s
    raise ValueError(path)

def main():
    """先内存变换全部白名单，成功后写新目录和真实blob OID；旧候选禁止覆写。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.base!=BASE:raise ValueError('fixed reviewed base required')
    if a.output.exists():raise ValueError('fresh output required')
    results={};rows=[];patch=[];oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in SHARED:
        before=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);s=transform(path,before.decode());data=s.encode();results[path]=data;rows.append({'path':path,'base_oid':oid(before),'result_oid':oid(data)});patch.extend(difflib.unified_diff(before.decode().splitlines(True),s.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
    for path,data in results.items():target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    (a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':a.base,'status':'SOURCE_ONLY_PENDING_SYNTAX','shared':rows,'module':'contact_inquiries/1','old_visitors_unchanged':['Interaction','Task1','Task2','ReceiptData'],'build_cpp':[ROOT+'contact_inquiry_'+x+'.cpp' for x in CPP],'build_headers':[ROOT+'contact_inquiry'+x+'.hpp' for x in HEADERS]+['native/world/src/resident/domains/interaction/known_contacts/known_contact_location_contract.hpp'],'plan':'Parent holds exact-source Continue integration using contact_inquiry_progress.hpp; do not enable half closure','external_publication':'central/root only; prior shared publication restriction retained'},ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
