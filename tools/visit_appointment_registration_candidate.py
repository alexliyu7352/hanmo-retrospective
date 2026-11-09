"""8035固定共享候选；只写隔离目录，不触中央。原当面约/普通回复/Interaction visitor不改。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
from task_extension_registration_candidate import replace,after
from contact_inquiry_registration_candidate import append_variant,SHARED as ORIGINAL_SHARED
BASE='8035dbd560af53cb69b3104e20dea97c44af9fdc'
ROOT='native/world/src/resident/domains/interaction/visit_appointments/'
INCLUDE='#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"\n'
PROFILE='witnessed_visit_appointments_v1'
CAP='visit_appointments_v1';NPC_CAP='visit_appointment_intents_v1'
SHARED=ORIGINAL_SHARED+['native/world/src/resident/kernel/dynamic_integration.cpp','native/world/src/resident/domains/interaction/appointments/personal_appointment_world.cpp','native/world/src/resident/domains/interaction/visit_messages/visit_reception_director.hpp','native/world/src/resident/domains/interaction/visit_messages/visit_reception_director.cpp','native/world/src/resident/domains/interaction/visit_messages/visit_reception_world.hpp','native/world/src/resident/domains/interaction/visit_messages/visit_reception_world.cpp']

def transform(path,s,repo):
    if path.endswith('/interaction_model.hpp'):
        s=replace(s,'#pragma once','#pragma once\n#include "visit_appointments/visit_appointment_state.hpp"\n#include "visit_appointments/visit_appointment_meeting_state.hpp"')
        return replace(s,'std::optional<VisitReplyState> visit_message_replies;','std::optional<VisitReplyState> visit_message_replies;\n    std::optional<VisitAppointmentState> visit_appointment_invitations;\n    std::optional<VisitAppointmentMeetingState> visit_appointment_meetings;')
    if path.endswith('/model_contract.hpp'):
        s=replace(s,'struct VisitReplyCatalog;','struct VisitReplyCatalog;struct VisitAppointmentCatalog;struct VisitAppointmentIntentCatalog;')
        return replace(s,'std::shared_ptr<const VisitReplyCatalog> visit_message_replies;','std::shared_ptr<const VisitReplyCatalog> visit_message_replies;\n    std::shared_ptr<const VisitAppointmentCatalog> visit_appointment_invitations;\n    std::shared_ptr<const VisitAppointmentIntentCatalog> visit_appointment_intents;')
    if path.endswith('/command_types.hpp'):
        s=INCLUDE+s
        for name,typ in [('CommandData','VisitAppointmentCommand'),('CommandData','VisitAppointmentMeetingCommand'),('CommandData','AuthoredVisitAppointment'),('QuoteData','VisitAppointmentQuote'),('QuoteData','VisitAppointmentMeetingQuote'),('QuoteData','AuthoredVisitAppointmentQuote')]:s=append_variant(s,name,typ)
        return s
    if path.endswith('/dispatcher.cpp'):
        s=INCLUDE+s
        for expression,op in [('command',''),('op.command','op.')]:
            tail='->operation' if op else ''
            guard='return failed<ReceiptDetails>(common_failure("InvalidVisitAppointmentCommand"));' if op else 'return reject("InvalidVisitAppointmentCommand");'
            anchor='const auto* visit_reply=std::get_if<VisitReplyCommand>(&'+expression+'.data);'
            code='    const auto* visit_invitation=std::get_if<VisitAppointmentCommand>(&'+expression+'.data);if(is_visit_appointment_command('+expression+'.kind)!=(visit_invitation!=nullptr)||(visit_invitation&&visit_appointment_command_kind(*visit_invitation)!='+expression+'.kind))'+guard
            code+='\n    const auto* visit_meeting=std::get_if<VisitAppointmentMeetingCommand>(&'+expression+'.data);if(is_visit_appointment_meeting_command('+expression+'.kind)!=(visit_meeting!=nullptr)||(visit_meeting&&visit_appointment_meeting_command_kind(*visit_meeting)!='+expression+'.kind))'+guard
            code+='\n    const auto* authored_invitation=std::get_if<AuthoredVisitAppointment>(&'+expression+'.data);if(('+expression+'.kind==authored_visit_appointment_command)!=(authored_invitation!=nullptr))'+guard
            if op:code+='\n    if((visit_invitation&&!std::holds_alternative<VisitAppointmentQuote>(op.quote.data))||(visit_meeting&&!std::holds_alternative<VisitAppointmentMeetingQuote>(op.quote.data))||(authored_invitation&&!std::holds_alternative<AuthoredVisitAppointmentQuote>(op.quote.data)))'+guard
            s=after(s,anchor,code)
        s=after(s,'if(visit_reply){auto quote=prepare_visit_reply_world','''    if(visit_invitation){auto q=prepare_visit_appointment_world(w,id,*visit_invitation);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}
    if(visit_meeting){auto q=prepare_visit_appointment_meeting_world(w,id,*visit_meeting);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}
    if(authored_invitation){auto q=prepare_authored_visit_appointment_world(w,id,*authored_invitation);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}''')
        return after(s,'if(visit_reply){auto result=commit_visit_reply_world','''    if(visit_invitation){auto result=commit_visit_appointment_world(w,id,*visit_invitation,std::get<VisitAppointmentQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{std::move(result.value),{}},{}};}
    if(authored_invitation){auto result=commit_authored_visit_appointment_world(w,id,*authored_invitation,std::get<AuthoredVisitAppointmentQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{std::move(result.value),{}},{}};}
    if(visit_meeting){auto result=commit_visit_appointment_meeting_world(w,id,*visit_meeting,std::get<VisitAppointmentMeetingQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};if(result.value.action_started)return {true,ReceiptDetails{ActionStarted{result.value.source_id},{}},{}};InteractionEffect effect;effect.settlement_result=result.value.source_id;return {true,ReceiptDetails{std::move(effect),{}},{}};}''')
    if path.endswith('/command_boundary.cpp'):
        s=after(INCLUDE+s,'if(visit_reply_command_kind(kind)){auto decoded=','''    if(is_visit_appointment_command(kind)){auto decoded=decode_visit_appointment_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}
    if(is_visit_appointment_meeting_command(kind)){auto decoded=decode_visit_appointment_meeting_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}
    if(kind==authored_visit_appointment_command)return {false,{},common_failure("VisitAppointmentNpcOnlyCommand").error};''')
        s=after(s,'else if constexpr(std::is_same_v<T,VisitReplyQuote>)','''                else if constexpr(std::is_same_v<T,VisitAppointmentQuote>)return encode_visit_appointment_quote(quote);
                else if constexpr(std::is_same_v<T,AuthoredVisitAppointmentQuote>)return encode_visit_appointment_quote(quote.invitation);
                else if constexpr(std::is_same_v<T,VisitAppointmentMeetingQuote>)return encode_visit_appointment_meeting_quote(quote,preview.quote->kind==visit_appointment_meeting_action_kind);''')
        for old in ['else if(visit_reply_command_kind(kind)||','if((visit_reply_command_kind(kind)||','if(visit_reply_command_kind(r.kind)||']:
            new=old.replace('visit_reply_command_kind(', 'is_visit_appointment_command(')
            arg='r.kind' if 'r.kind' in old else 'kind'
            new=new.replace('is_visit_appointment_command('+arg+')||','is_visit_appointment_command('+arg+')||'+arg+'=="cancel_visit_appointment"||visit_reply_command_kind('+arg+')||')
            s=replace(s,old,new)
        return s
    if path.endswith('/phase_plan.cpp'):
        s=after(INCLUDE+s,'Status try_npc_command(WorldBundle& w,','    applied=false;if(std::holds_alternative<VisitAppointmentCommand>(command.data)||std::holds_alternative<VisitAppointmentMeetingCommand>(command.data)||is_visit_appointment_command(command.kind)||is_visit_appointment_meeting_command(command.kind))return common_failure("VisitAppointmentPlayerCommandRequired");')
        s=replace(s,'std::holds_alternative<PersonalAppointmentCommand>(command.data)||','std::holds_alternative<PersonalAppointmentCommand>(command.data)||std::holds_alternative<VisitAppointmentCommand>(command.data)||std::holds_alternative<VisitAppointmentMeetingCommand>(command.data)||std::holds_alternative<AuthoredVisitAppointment>(command.data)||')
        return replace(s,'}else if(action.kind==personal_appointment_action_kind){','''}else if(action.kind==visit_appointment_meeting_action_kind){
                    if(action.remaining_ticks!=1){r=common_failure("VisitAppointmentActionMismatch",id);break;}ActionMutation mutation;mutation.action=action;mutation.remaining_ticks=0;r=w.common.apply_action(mutation);if(!r.ok)break;action.remaining_ticks=0;auto step=step_visit_appointment_meeting_world(w,action,dynamic_input);if(!step.ok){r={false,{},step.error};break;}completed=true;outcome.completed=true;outcome.success=step.value.success;outcome.code=step.value.success?StableId{"VisitAppointmentMeetingFulfilled"}:step.value.reason;
                }else if(action.kind==personal_appointment_action_kind){''')
    if path.endswith('/dynamic_integration.cpp'):
        return after(INCLUDE+s,'Status adopt_director_proposal(WorldBundle& w,','    if(proposal.command&&(std::holds_alternative<VisitAppointmentCommand>(proposal.command->data)||std::holds_alternative<VisitAppointmentMeetingCommand>(proposal.command->data)||is_visit_appointment_command(proposal.command->kind)||is_visit_appointment_meeting_command(proposal.command->kind)))return common_failure("VisitAppointmentPlayerCommandRequired");')
    if path.endswith('/personal_appointment_world.cpp'):
        s=INCLUDE+s
        original='if constexpr(std::is_same_v<T,PersonalAppointmentAccept>)return preview_personal_appointment_accept(w.common,w.interaction,*w.catalog->interaction,access,*w.catalog->personal_appointments,*w.interaction.personal_appointments,actor,c);'
        s=replace(s,original,'if constexpr(std::is_same_v<T,PersonalAppointmentAccept>){auto quote=preview_personal_appointment_accept(w.common,w.interaction,*w.catalog->interaction,access,*w.catalog->personal_appointments,*w.interaction.personal_appointments,actor,c);if(!quote.ok)return quote;auto conflict=check_visit_appointment_personal_accept_world(w,quote.value);if(!conflict.ok)return {false,{},conflict.error};return quote;}')
        return replace(s,'result=accept_personal_appointment_in_candidate(', 'auto conflict=check_visit_appointment_personal_accept_world(w,quote);if(!conflict.ok)return {false,{},conflict.error};result=accept_personal_appointment_in_candidate(')
    if path.endswith('/visit_reception_director.hpp'):
        s=replace(s,'struct VisitReplyNpcRead;','struct VisitReplyNpcRead;struct VisitAppointmentNpcRead;')
        return replace(s,'const VisitReplyNpcRead* replies=nullptr;','const VisitReplyNpcRead* replies=nullptr;const VisitAppointmentNpcRead* invitations=nullptr;')
    if path.endswith('/visit_reception_world.hpp'):
        s=replace(s,'#pragma once','#pragma once\n#include "resident/domains/interaction/visit_appointments/visit_appointment_director.hpp"')
        return replace(s,'std::optional<VisitReplyNpcRead> replies_;','std::optional<VisitReplyNpcRead> replies_;std::optional<VisitAppointmentNpcRead> invitations_;')
    if path.endswith('/visit_reception_world.cpp'):
        s=replace(s,'read_{w.common,w.interaction,','invitations_(w.catalog->visit_appointment_intents?std::optional<VisitAppointmentNpcRead>{{*w.catalog->visit_appointment_invitations,*w.interaction.visit_appointment_invitations,*w.catalog->visit_appointment_intents,*w.catalog->personal_appointments,*w.interaction.personal_appointments,w.catalog->visit_message_replies.get(),&*w.interaction.visit_appointment_meetings}}:std::nullopt),read_{w.common,w.interaction,')
        s=replace(s,'replies_?&*replies_:nullptr}{}','replies_?&*replies_:nullptr,invitations_?&*invitations_:nullptr}{}')
        return replace(s,'return {true,std::unique_ptr<VisitReceptionWorldAccess>', 'if(w.catalog->visit_appointment_intents&&(!w.catalog->visit_appointment_invitations||!w.interaction.visit_appointment_invitations||!w.interaction.visit_appointment_meetings||!w.catalog->personal_appointments||!w.interaction.personal_appointments))return {false,{},common_failure("VisitAppointmentProfileRequired").error};return {true,std::unique_ptr<VisitReceptionWorldAccess>')
    if path.endswith('/visit_reception_director.cpp'):
        s='#include "resident/domains/interaction/visit_appointments/visit_appointment_director.hpp"\n'+s
        s=replace(s,'if(finished(r,*i)&&!reply_pending)', 'if(r.invitations){auto pending=visit_appointment_reception_pending(r,*r.invitations,view.actor_id,intent_id);if(!pending.ok)return {false,{},pending.error};reply_pending=reply_pending||pending.value;}if(finished(r,*i)&&!reply_pending)')
        return after(s,'if(r.replies){auto answer=propose_received_visit_reply','    if(r.invitations){auto answer=propose_received_visit_appointment(r,*r.invitations,director,view,intent_id,proposal);if(!answer.ok)return {false,{},answer.error};if(answer.value){auto step=std::move(*answer.value);std::visit([&](const auto& request){step.proposal.command=std::make_shared<const CommandPayload>(CommandPayload{step.command_kind,request});},step.command);return {true,std::move(step.proposal),{}};}}')
    if path.endswith('/native_initial_state.cpp'):
        s=after(INCLUDE+s,'admitted.module_versions.emplace_back("family_biological_legacy","1");','    auto visit_appointments=decode_visit_appointment_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!visit_appointments.ok)return {false,{},visit_appointments.error};if(visit_appointments.value){admitted.module_versions.emplace_back("visit_appointment_invitations","1");admitted.module_versions.emplace_back("visit_appointment_meetings","1");if(scenario.find("visit_appointment_intents"))admitted.module_versions.emplace_back("visit_appointment_intents","1");}')
        return replace(s,'auto interaction=std::make_shared<InteractionState>();','auto interaction=std::make_shared<InteractionState>();if(mutable_catalog->visit_appointment_invitations){interaction->visit_appointment_invitations.emplace();interaction->visit_appointment_meetings.emplace();}')
    if path.endswith('/world_bundle.hpp'):
        return replace(s,'const BiologicalControlState* biological_bindings_before=nullptr);','const BiologicalControlState* biological_bindings_before=nullptr,const StableId& visit_invitation_source={},const std::vector<StableId>& visit_meeting_sources={});')
    if path.endswith('/resident_world.cpp'):
        s=replace(INCLUDE+s,'auto born_dialogue_source=born_personal_dialogue_new_result_world(', 'auto visit_invitation_source=visit_appointment_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!visit_invitation_source.ok)return {false,{},visit_invitation_source.error};auto visit_meeting_sources=visit_appointment_meeting_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!visit_meeting_sources.ok)return {false,{},visit_meeting_sources.error};auto born_dialogue_source=born_personal_dialogue_new_result_world(')
        s=replace(s,'?&current.family_biological_legacy:nullptr);if(!completed.ok)', '?&current.family_biological_legacy:nullptr,visit_invitation_source.value,visit_meeting_sources.value);if(!completed.ok)')
        s=replace(s,'const BiologicalControlState* biological_bindings_before){','const BiologicalControlState* biological_bindings_before,const StableId& visit_invitation_source,const std::vector<StableId>& visit_meeting_sources){')
        return after(s,'auto born_dialogue_bound=bind_born_personal_dialogue_result_world','    auto visit_bound=bind_visit_appointment_result_world(w,visit_invitation_source,visit_meeting_sources,actor,sequence,*receipt);if(!visit_bound.ok)return visit_bound;')
    if path.endswith('/world_bundle.cpp'):
        s=after(INCLUDE+s,'auto payloads=born_personal_dialogue_fact_payloads(','    {auto payloads=visit_appointment_fact_payloads(w.common,w.interaction.visit_appointment_invitations?&*w.interaction.visit_appointment_invitations:nullptr,w.interaction.visit_appointment_meetings?&*w.interaction.visit_appointment_meetings:nullptr);if(!payloads.ok)return {false,{},payloads.error};for(const auto& row:boundary::object(payloads.value,"visit_appointments")){auto r=fact(row.first,boundary::get_string(row.second,"kind",row.first),boundary::get_string(row.second,"domain",row.first));if(!r.ok)return r;}}')
        s=after(s,'auto payloads=personal_appointment_action_payloads(','    {auto payloads=visit_appointment_action_payloads(w.common,w.interaction.visit_appointment_meetings?&*w.interaction.visit_appointment_meetings:nullptr);if(!payloads.ok)return {false,{},payloads.error};for(const auto& row:boundary::object(payloads.value,"visit_appointment_actions")){auto r=action(row.first,visit_appointment_meeting_action_kind);if(!r.ok)return r;}}')
        return after(s,'valid=validate_family_biological_world(*this);','    // 原人物/全部控制链、原预约及拜帖先验；本域三源/会面/实际结果后最后合并承诺冲突。\n    valid=validate_visit_appointment_world(*this);if(!valid.ok)return valid;')
    if path.endswith('/world_codec.cpp'):return codec(s)
    if path.endswith('/common_codec.cpp'):
        s=replace(s,'const bool born_dialogue=', 'const bool visit_invitation=kind=="visit_appointment_invitation_offered"||kind=="visit_appointment_invitation_collected"||kind=="visit_appointment_invitation_accepted";const bool visit_meeting=kind=="visit_appointment_meeting_started"||kind=="visit_appointment_meeting_cancelled"||kind=="visit_appointment_meeting_finished";const bool born_dialogue=')
        s=replace(s,'std::string domain=','std::string domain=visit_invitation?"visit_appointment_invitations":visit_meeting?"visit_appointment_meetings":')
        s=replace(s,'if(born_dialogue&&named!=','if(visit_invitation&&named!="visit_appointment_invitations")fail("InvalidFactDomain",row.first);if(visit_meeting&&named!="visit_appointment_meetings")fail("InvalidFactDomain",row.first);if(born_dialogue&&named!=')
        return replace(s,'&&!(named=="born_personal_dialogues"','&&!(named=="visit_appointment_invitations"&&visit_invitation)&&!(named=="visit_appointment_meetings"&&visit_meeting)&&!(named=="born_personal_dialogues"')
    if path.endswith('/live_view_queries.cpp'):
        return after(INCLUDE+s,'if(query=="visit_message_replies"){','    if(query=="visit_appointments"){boundary::fields(args,{},{"cursor","offer_cursor"},"args");return query_visit_appointment_world(world,args.find("cursor")?boundary::get_int(args,"cursor","args"):0,args.find("offer_cursor")?boundary::get_int(args,"offer_cursor","args"):0);}')
    if path.endswith('/native_resident_backend.gd'):
        s=after(s,'if definition.has("family_biological_legacy"): expected','\tif definition.get("visit_appointment_profile","")=="witnessed_visit_appointments_v1":\n\t\texpected["module_versions"].append({"name":"visit_appointment_invitations","version":"1"})\n\t\texpected["module_versions"].append({"name":"visit_appointment_meetings","version":"1"})\n\t\tif definition.has("visit_appointment_intents"): expected["module_versions"].append({"name":"visit_appointment_intents","version":"1"})')
        return s+'\nfunc visit_appointments_enabled() -> bool:\n\treturn _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("visit_appointment_profile","")=="witnessed_visit_appointments_v1"\n'
    if path.endswith('/resident_page_requests.gd'):
        s=after(s,'const VisitMessageRepliesPanel=','const VisitAppointmentsPanel=preload("res://ui/visit_appointments_panel.gd")')
        return after(s,'and not VisitMessageRepliesPanel.prepare(host):','\tif host.kernel.has_method("visit_appointments_enabled") and host.kernel.visit_appointments_enabled() and not VisitAppointmentsPanel.prepare(host): return false')
    if path.endswith('/screen_composer.gd'):
        s=after(s,'const VisitMessageRepliesPanel=','const VisitAppointmentsPanel=preload("res://ui/visit_appointments_panel.gd")')
        lines=[l for l in s.splitlines() if 'VisitMessageRepliesPanel.visible_here(m)' in l];assert len(lines)==2
        for line in lines:s=replace(s,'\n'+line+'\n','\n'+line+'\n'+line[:len(line)-len(line.lstrip())]+'if m._resident_mode and m.kernel.has_method("visit_appointments_enabled") and m.kernel.visit_appointments_enabled() and VisitAppointmentsPanel.visible_here(m): VisitAppointmentsPanel.build(m,_sidebar_section(m,stage,"VisitAppointmentsArea","拜帖约期"))\n')
        return s
    if path.endswith('/content.schema.json'):return schema(s)
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s)
        for cap,version,selector in [(CAP,'visit_appointment_invitations/1',{'path':'scenarios.*.visit_appointment_profile','equals':PROFILE}),(NPC_CAP,'visit_appointment_intents/1',{'path':'scenarios.*.visit_appointment_intents.version','equals':1})]:d['capabilities'].append({'capability_id':cap,'rule_version':version,'module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[selector],'supported_semantics':[],'test_ids':['test_visit_appointments_panel_guard'],'evidence':[],'limitations':['原收件人实际领读后明确承诺固定约期；原发送人实领后自己接受。NPC只具名原认领会话意向；真实到场一期会面及当面取消。World/GUI待同代验证。']})
        return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/content/links.py':
        s=after(s,'from .visit_reply_links import','    from .visit_appointment_links import check_visit_appointments')
        return replace(s,' + check_visit_reply_intents(pack,index)',' + check_visit_reply_intents(pack,index) + check_visit_appointments(pack,index)')
    if path=='tools/verify.py':return replace(s,"'test_visit_message_replies_panel_guard',","'test_visit_message_replies_panel_guard', 'test_visit_appointments_panel_guard',")
    if path.endswith('/resident.cmake'):
        cpp=sorted(p.relative_to(repo/'native/world').as_posix() for p in (repo/ROOT).glob('*.cpp'));headers=sorted(p.relative_to(repo/'native/world').as_posix() for p in (repo/ROOT).glob('*.hpp'))
        a='    src/resident/domains/interaction/visit_replies/visit_reply_world.cpp';assert s.count(a)==2;s=s.replace(a,a+'\n'+'\n'.join('    '+p for p in cpp))
        a='    src/resident/domains/interaction/visit_replies/visit_reply_world.hpp';s=replace(s,a,a+'\n'+'\n'.join('    '+p for p in headers))
        for name in ['catalog','sources','authored','meeting','view']:
            s+=f'\nadd_executable(test_resident_visit_appointment_{name} tests/resident/visit_appointment_{name}_test.cpp)\ntarget_link_libraries(test_resident_visit_appointment_{name} PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_visit_appointment_{name} PRIVATE src)\ntarget_compile_options(test_resident_visit_appointment_{name} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_visit_appointment_{name} COMMAND test_resident_visit_appointment_{name})\n'
        return s
    raise ValueError(path)

def codec(s):
    s=INCLUDE+s
    s=replace(s,'if(catalog.family_biological_legacy)domains.push_back({"family_biological_legacy",1});return domains;','if(catalog.family_biological_legacy)domains.push_back({"family_biological_legacy",1});if(catalog.visit_appointment_invitations){domains.push_back({"visit_appointment_invitations",1});domains.push_back({"visit_appointment_meetings",1});}return domains;')
    s=replace(s,'{"born_personal_dialogues",{"1"}}','{"born_personal_dialogues",{"1"}}, {"visit_appointment_invitations",{"1"}}, {"visit_appointment_meetings",{"1"}}, {"visit_appointment_intents",{"1"}}')
    s=after(s,'if(has("family_biological_legacy")){','''        if(has("visit_appointment_invitations")!=has("visit_appointment_meetings")||(has("visit_appointment_invitations")&&(!dynamic||!has("visit_messages")||!has("personal_appointments")))||(has("visit_appointment_intents")&&(!has("visit_appointment_invitations")||!has("visit_reception_intents")||!has("visit_reception_claims"))))fail("VisitAppointmentProfileMismatch","checkpoint/module_versions");
        if(has("visit_appointment_invitations")){domains.push_back({"visit_appointment_invitations",1});domains.push_back({"visit_appointment_meetings",1});}''')
    s=after(s,'if(has("born_personal_dialogues","1")!=','''            if(has("visit_appointment_invitations","1")!=(version("visit_appointment_invitations")==1)||has("visit_appointment_meetings","1")!=(version("visit_appointment_meetings")==1)||has("visit_appointment_invitations","1")!=has("visit_appointment_meetings","1")||(has("visit_appointment_invitations","1")&&(!dynamic||!has("visit_messages","1")||!has("personal_appointments","1")))||(has("visit_appointment_intents","1")&&(!has("visit_appointment_invitations","1")||!has("visit_reception_intents","1")||!has("visit_reception_claims","1"))))return {false,{},common_failure("VisitAppointmentProfileMismatch").error};''')
    s=after(s,'auto visit_replies=decode_visit_reply_profile_version(','''    auto visit_appointments=decode_visit_appointment_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!visit_appointments.ok)return {false,{},visit_appointments.error};if(visit_appointments.value){if(!result->interaction->visit_messages||!result->personal_appointments)return {false,{},common_failure("VisitAppointmentOriginalProfileRequired").error};auto decoded=decode_visit_appointment_catalog(field(scenario,"visit_appointment_invitations","scenario"),*result->interaction->visit_messages,*result->personal_appointments,*result->interaction,result->visit_message_replies.get());if(!decoded.ok)return {false,{},decoded.error};result->visit_appointment_invitations=std::make_shared<const VisitAppointmentCatalog>(std::move(decoded.value));}''')
    s=after(s,'result->director=std::make_shared<const DirectorCatalog>','''        if(const auto* config=scenario.find("visit_appointment_intents")){if(!result->visit_appointment_invitations||!result->director->visit_reception)return {false,{},common_failure("VisitAppointmentAuthorshipRequired").error};auto decoded=decode_visit_appointment_intents(*config,*result->visit_appointment_invitations,*result->director->visit_reception,*result->interaction->visit_messages,*result->personal_appointments,*result->interaction,result->visit_message_replies.get());if(!decoded.ok)return {false,{},decoded.error};result->visit_appointment_intents=std::make_shared<const VisitAppointmentIntentCatalog>(std::move(decoded.value));}''')
    s=after(s,'if(result->family_biological_legacy)id.module_versions','    if(result->visit_appointment_invitations){id.module_versions.emplace_back("visit_appointment_invitations","1");id.module_versions.emplace_back("visit_appointment_meetings","1");if(result->visit_appointment_intents)id.module_versions.emplace_back("visit_appointment_intents","1");}')
    s=replace(s,'result.interaction=std::make_shared<const InteractionState>(std::move(i.value));','if(catalog.visit_appointment_invitations){auto a=decode_visit_appointments(field(source,"visit_appointment_invitations","state"));if(!a.ok)return {false,{},a.error};auto m=decode_visit_appointment_meetings(field(source,"visit_appointment_meetings","state"));if(!m.ok)return {false,{},m.error};i.value.visit_appointment_invitations=std::move(a.value);i.value.visit_appointment_meetings=std::move(m.value);}else if(source.find("visit_appointment_invitations")||source.find("visit_appointment_meetings"))fail("UnexpectedVisitAppointmentDomain","state");result.interaction=std::make_shared<const InteractionState>(std::move(i.value));')
    s=after(s,'auto checked=validate_born_personal_dialogue_json_payloads(','    {CommonStore view(result.common,catalog.common);auto checked=validate_visit_appointment_json_payloads(view,result.interaction&&result.interaction->visit_appointment_invitations?&*result.interaction->visit_appointment_invitations:nullptr,result.interaction&&result.interaction->visit_appointment_meetings?&*result.interaction->visit_appointment_meetings:nullptr,source);if(!checked.ok)return {false,{},checked.error};}')
    s=after(s,'auto appointments=personal_appointment_fact_payloads(','        auto visit_appointments=visit_appointment_fact_payloads(w.common,w.interaction.visit_appointment_invitations?&*w.interaction.visit_appointment_invitations:nullptr,w.interaction.visit_appointment_meetings?&*w.interaction.visit_appointment_meetings:nullptr);if(!visit_appointments.ok)return visit_appointments;merge_unique(facts,visit_appointments.value);auto visit_actions=visit_appointment_action_payloads(w.common,w.interaction.visit_appointment_meetings?&*w.interaction.visit_appointment_meetings:nullptr);if(!visit_actions.ok)return visit_actions;merge_unique(params,visit_actions.value);')
    s=after(s,'set(output,"family_biological_legacy",','    if(w.catalog->visit_appointment_invitations){if(!w.interaction.visit_appointment_invitations||!w.interaction.visit_appointment_meetings)fail("MissingVisitAppointmentDomain","state");set(output,"visit_appointment_invitations",encode_visit_appointments(*w.interaction.visit_appointment_invitations));set(output,"visit_appointment_meetings",encode_visit_appointment_meetings(*w.interaction.visit_appointment_meetings));}')
    s=after(s,'r=section("family_biological_legacy",','''    if(w.catalog->visit_appointment_invitations){if(!w.interaction.visit_appointment_invitations||!w.interaction.visit_appointment_meetings)return {false,{},common_failure("MissingVisitAppointmentDomain").error};r=section("visit_appointment_invitations",[&](auto& c){return visit_visit_appointments_checkpoint(c,*w.interaction.visit_appointment_invitations);});if(!r.ok)return {false,{},r.error};r=section("visit_appointment_meetings",[&](auto& c){return visit_visit_appointment_meetings_checkpoint(c,*w.interaction.visit_appointment_meetings);});if(!r.ok)return {false,{},r.error};}''')
    return after(s,'else if(section.domain=="born_personal_dialogues")','''        else if(section.domain=="visit_appointment_invitations"){i->visit_appointment_invitations.emplace();r=visit_visit_appointments_checkpoint(reader,*i->visit_appointment_invitations);}
        else if(section.domain=="visit_appointment_meetings"){i->visit_appointment_meetings.emplace();r=visit_visit_appointment_meetings_checkpoint(reader,*i->visit_appointment_meetings);}''')

def schema(s):
    d=json.loads(s);p=d['$defs']['scenario']['properties'];assert 'visit_appointment_profile' not in p
    ident=lambda:{'type':'string','minLength':1,'maxLength':256}
    def obj(fields):return {'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}
    def catalog(key,item):return obj({'version':{'type':'integer','enum':[1]},key:{'type':'array','minItems':1,'maxItems':4096,'items':item}})
    fields={k:ident() for k in ['id','message_policy_id','appointment_policy_id','offer_node_id','collect_node_id']};fields['inviter_commits_fixed_window']={'type':'boolean','enum':[True]};fields.update({k:{'type':'integer','minimum':1,'maximum':720} for k in ['accept_before_offset','earliest_offset','latest_offset']})
    p['visit_appointment_profile']={'type':'string','enum':[PROFILE]};p['visit_appointment_invitations']=catalog('policies',obj(fields))
    step=obj({'node_id':ident(),'option_id':ident()});step['required']=['node_id']
    p['visit_appointment_intents']=catalog('intents',obj({'id':ident(),'reception_intent_id':ident(),'invitation_policy_id':ident(),'route':{'type':'array','minItems':1,'maxItems':16,'items':step}}))
    d['properties']['required_capabilities']['items']['enum'] += [CAP,NPC_CAP]
    return json.dumps(d,ensure_ascii=False,indent=2)+'\n'

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.base!=BASE or a.output.exists():raise ValueError('fixed base and fresh candidate output required')
    a.output.mkdir(parents=True);rows=[];patch=[]
    def oid(b):return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in SHARED:
        old=subprocess.check_output(['git','show',a.base+':'+path],cwd=a.repo);new=transform(path,old.decode(),a.repo).encode();assert new!=old,path
        dest=a.output/'candidate'/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(new)
        rows.append({'path':path,'base_oid':oid(old),'result_oid':oid(new),'bytes':len(new)})
        patch+=difflib.unified_diff(old.decode().splitlines(True),new.decode().splitlines(True),fromfile='a/'+path,tofile='b/'+path)
    (a.output/'candidate.patch').write_text(''.join(patch))
    (a.output/'manifest.json').write_text(json.dumps({'base':a.base,'status':'SOURCE_CANDIDATE','shared':rows,'requirements':['actual Common result bindings after cache/history and before full validation','full Birth/Family/Legacy2 before new three sources/meeting/results/final cross commitment','old personal accept preview and commit same candidate guard','original template13 pending and authored command full Interaction savepoints','Plan companion remains central-owned'],'deferred':['product syntax until coherent header assembly','normal official source admission','C++ runtime','World','GUI']},ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'shared':len(rows),'manifest':str(a.output/'manifest.json')}))
if __name__=='__main__':main()
