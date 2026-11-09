"""固定询问之后的本人回帖闭包候选；仅输出隔离文件，中央保留全部后继共享主权。"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
from task_extension_registration_candidate import replace,after
from contact_inquiry_registration_candidate import append_variant,SHARED
BASE='80d2b1c1f08448f618fc5c7a88b562f1777f7657'
ROOT='native/world/src/resident/domains/interaction/visit_replies/'
INCLUDE='#include "resident/domains/interaction/visit_replies/visit_reply_world.hpp"\n'
PROFILE='witnessed_visit_replies_v1'
CAP='visit_message_replies_v1'
CPP=['catalog','codec','rules','validation','receipts','read','view','boundary','world']
HEADERS=['','_state','_detail','_receipts','_read','_view','_boundary','_world']

def transform(path,s):
    """新段排询问末尾；不重写旧visitor、师承/安民/问去向实际结果钩子和基础Plan门。"""
    if path.endswith('/interaction_model.hpp'):
        s=replace(s,'#pragma once','#pragma once\n#include "visit_replies/visit_reply_state.hpp"')
        return replace(s,'std::optional<ContactInquiryState> contact_inquiries;','std::optional<ContactInquiryState> contact_inquiries;\n    std::optional<VisitReplyState> visit_message_replies;')
    if path.endswith('/model_contract.hpp'):
        s=replace(s,'struct ContactInquiryCatalog;','struct ContactInquiryCatalog;struct VisitReplyCatalog;')
        return replace(s,'std::shared_ptr<const ContactInquiryCatalog> contact_inquiries;','std::shared_ptr<const ContactInquiryCatalog> contact_inquiries;\n    std::shared_ptr<const VisitReplyCatalog> visit_message_replies;')
    if path.endswith('/command_types.hpp'):return append_variant(append_variant(INCLUDE+s,'CommandData','VisitReplyCommand'),'QuoteData','VisitReplyQuote')
    if path.endswith('/dispatcher.cpp'):
        s=INCLUDE+s
        s=after(s,'const auto* inquiry=std::get_if<ContactInquiryRequest>(&command.data);','    const auto* visit_reply=std::get_if<VisitReplyCommand>(&command.data);if(visit_reply_command_kind(command.kind)!=(visit_reply!=nullptr)||(visit_reply&&visit_reply_command_kind(*visit_reply)!=command.kind))return reject("InvalidVisitReplyCommand");')
        s=after(s,'if(inquiry){auto quote=prepare_contact_inquiry_world','    if(visit_reply){auto quote=prepare_visit_reply_world(w,id,*visit_reply);if(!quote.ok)return {false,{},quote.error};return ready(w,id,command,std::move(quote.value));}')
        s=after(s,'const auto* inquiry=std::get_if<ContactInquiryRequest>(&op.command.data);','    const auto* visit_reply=std::get_if<VisitReplyCommand>(&op.command.data);if(visit_reply_command_kind(op.command.kind)!=(visit_reply!=nullptr)||(visit_reply&&(visit_reply_command_kind(*visit_reply)!=op.command.kind||!std::holds_alternative<VisitReplyQuote>(op.quote.data))))return failed<ReceiptDetails>(common_failure("InvalidVisitReplyCommand"));')
        return after(s,'if(inquiry){auto result=commit_contact_inquiry_world','    if(visit_reply){auto result=commit_visit_reply_world(w,id,*visit_reply,std::get<VisitReplyQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{std::move(result.value),{}},{}};}')
    if path.endswith('/command_boundary.cpp'):
        s=after(INCLUDE+s,'if(kind==contact_inquiry_action_kind){auto decoded=','    if(visit_reply_command_kind(kind)){auto decoded=decode_visit_reply_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}')
        s=after(s,'else if constexpr(std::is_same_v<T,ContactInquiryQuote>)return','                else if constexpr(std::is_same_v<T,VisitReplyQuote>)return encode_visit_reply_quote(quote);')
        s=replace(s,'else if(visit_message_command_kind(kind)||','else if(visit_reply_command_kind(kind)||visit_message_command_kind(kind)||')
        s=replace(s,'if((visit_message_command_kind(kind)||','if((visit_reply_command_kind(kind)||visit_message_command_kind(kind)||')
        return replace(s,'if(visit_message_command_kind(r.kind)||','if(visit_reply_command_kind(r.kind)||visit_message_command_kind(r.kind)||')
    if path.endswith('/phase_plan.cpp'):
        s=replace(INCLUDE+s,'applied=false;if(std::holds_alternative<ContactInquiryRequest>','applied=false;if(std::holds_alternative<VisitReplyCommand>(command.data)||visit_reply_command_kind(command.kind))return common_failure("VisitReplyPlayerCommandRequired");if(std::holds_alternative<ContactInquiryRequest>')
        return replace(s,'std::holds_alternative<PersonalAppointmentCommand>(command.data)||std::holds_alternative<ContactInquiryRequest>(command.data)||','std::holds_alternative<PersonalAppointmentCommand>(command.data)||std::holds_alternative<VisitReplyCommand>(command.data)||std::holds_alternative<ContactInquiryRequest>(command.data)||')
    if path.endswith('/native_initial_state.cpp'):
        s=after(INCLUDE+s,'auto inquiries=decode_contact_inquiry_profile(scenario,','    auto visit_replies=decode_visit_reply_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!visit_replies.ok)return {false,{},visit_replies.error};if(visit_replies.value)admitted.module_versions.emplace_back("visit_message_replies","1");')
        return replace(s,'auto interaction=std::make_shared<InteractionState>();','auto interaction=std::make_shared<InteractionState>();if(mutable_catalog->visit_message_replies)interaction->visit_message_replies.emplace();')
    if path.endswith('/world_codec.cpp'):
        s=replace(INCLUDE+s,'if(catalog.contact_inquiries)domains.push_back({"contact_inquiries",1});return domains;','if(catalog.contact_inquiries)domains.push_back({"contact_inquiries",1});if(catalog.visit_message_replies)domains.push_back({"visit_message_replies",1});return domains;')
        s=replace(s,'{"contact_inquiries",{"1"}}','{"contact_inquiries",{"1"}}, {"visit_message_replies",{"1"}}')
        s=after(s,'if(has("contact_inquiries")){','        if(has("visit_message_replies")){if(!dynamic||!has("visit_messages"))fail("VisitReplyProfileMismatch","checkpoint/module_versions");domains.push_back({"visit_message_replies",1});}')
        s=after(s,'if(has("contact_inquiries","1")!=','            if(has("visit_message_replies","1")!=(version("visit_message_replies")==1)||(version("visit_message_replies")&&(!dynamic||!has("visit_messages","1"))))return {false,{},common_failure("VisitReplyProfileMismatch").error};')
        s=after(s,'auto inquiries=decode_contact_inquiry_profile(scenario,','    auto visit_replies=decode_visit_reply_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!visit_replies.ok)return {false,{},visit_replies.error};if(visit_replies.value){if(!result->interaction->visit_messages)return {false,{},common_failure("VisitReplyOriginalProfileRequired").error};auto decoded=decode_visit_reply_catalog(field(scenario,"visit_message_replies","scenario"),*result->interaction->visit_messages,*result->interaction);if(!decoded.ok)return {false,{},decoded.error};result->visit_message_replies=std::make_shared<const VisitReplyCatalog>(std::move(decoded.value));}')
        s=after(s,'if(result->contact_inquiries)id.module_versions','    if(result->visit_message_replies)id.module_versions.emplace_back("visit_message_replies","1");')
        s=replace(s,'result.interaction=std::make_shared<const InteractionState>(std::move(i.value));','if(catalog.visit_message_replies){auto decoded=decode_visit_replies(field(source,"visit_message_replies","state"));if(!decoded.ok)return {false,{},decoded.error};i.value.visit_message_replies=std::move(decoded.value);}else if(source.find("visit_message_replies"))fail("UnexpectedVisitReplyDomain","visit_message_replies");result.interaction=std::make_shared<const InteractionState>(std::move(i.value));')
        s=after(s,'auto checked=validate_contact_inquiry_json_payloads(','    {CommonStore view(result.common,catalog.common);auto checked=validate_visit_reply_json_payloads(view,result.interaction&&result.interaction->visit_message_replies?&*result.interaction->visit_message_replies:nullptr,source);if(!checked.ok)return {false,{},checked.error};}')
        s=after(s,'auto inquiries=contact_inquiry_fact_payloads(','        auto visit_replies=visit_reply_fact_payloads(w.common,w.interaction.visit_message_replies?&*w.interaction.visit_message_replies:nullptr);if(!visit_replies.ok)return visit_replies;merge_unique(facts,visit_replies.value);')
        s=after(s,'set(output,"contact_inquiries",','    if(w.catalog->visit_message_replies){if(!w.interaction.visit_message_replies)fail("MissingVisitReplyDomain","visit_message_replies");set(output,"visit_message_replies",encode_visit_replies(*w.interaction.visit_message_replies));}')
        s=after(s,'r=section("contact_inquiries",','    if(w.catalog->visit_message_replies){if(!w.interaction.visit_message_replies)return {false,{},common_failure("MissingVisitReplyDomain").error};r=section("visit_message_replies",[&](auto& c){return visit_visit_replies_checkpoint(c,*w.interaction.visit_message_replies);});if(!r.ok)return {false,{},r.error};}')
        return after(s,'else if(section.domain=="contact_inquiries")','        else if(section.domain=="visit_message_replies"){i->visit_message_replies.emplace();r=visit_visit_replies_checkpoint(reader,*i->visit_message_replies);}')
    if path.endswith('/world_bundle.cpp'):
        s=after(INCLUDE+s,'auto payloads=contact_inquiry_fact_payloads(','    {auto payloads=visit_reply_fact_payloads(w.common,w.interaction.visit_message_replies?&*w.interaction.visit_message_replies:nullptr);if(!payloads.ok)return {false,{},payloads.error};for(const auto& row:boundary::object(payloads.value,"visit_message_replies")){auto r=fact(row.first,boundary::get_string(row.second,"kind",row.first),visit_reply_module);if(!r.ok)return r;}}')
        return after(s,'valid=validate_contact_inquiry_world(*this);','    valid=validate_visit_reply_world(*this);if(!valid.ok)return valid;')
    if path.endswith('/world_bundle.hpp'):return replace(s,'const std::vector<StableId>& inquiry_sources={});','const std::vector<StableId>& inquiry_sources={},const StableId& visit_reply_source={});')
    if path.endswith('/resident_world.cpp'):
        s=replace(INCLUDE+s,'auto inquiry_sources=contact_inquiry_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);','auto visit_reply_source=visit_reply_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!visit_reply_source.ok)return {false,{},visit_reply_source.error};auto inquiry_sources=contact_inquiry_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);')
        s=replace(s,'governance_continuation_id,inquiry_sources.value);','governance_continuation_id,inquiry_sources.value,visit_reply_source.value);')
        s=replace(s,'const std::vector<StableId>& inquiry_sources){','const std::vector<StableId>& inquiry_sources,const StableId& visit_reply_source){')
        return after(s,'auto inquiry_bound=bind_contact_inquiry_result_world','    auto visit_reply_bound=bind_visit_reply_result_world(w,visit_reply_source,actor,sequence,*receipt);if(!visit_reply_bound.ok)return visit_reply_bound;')
    if path.endswith('/common_codec.cpp'):
        s=replace(s,'const bool inquiry=','const bool visit_reply=kind=="visit_message_reply_posted"||kind=="visit_message_reply_collected";const bool inquiry=')
        s=replace(s,'inquiry?"contact_inquiries":','visit_reply?"visit_message_replies":inquiry?"contact_inquiries":')
        s=replace(s,'if(inquiry&&named!=','if(visit_reply&&named!="visit_message_replies")fail("InvalidFactDomain",row.first);if(inquiry&&named!=')
        return replace(s,'&&!(named=="contact_inquiries"','&&!(named=="visit_message_replies"&&visit_reply)&&!(named=="contact_inquiries"')
    if path.endswith('/live_view_queries.cpp'):return after(INCLUDE+s,'if(query=="contact_inquiries"){','    if(query=="visit_message_replies"){boundary::fields(args,{},{"cursor"},"args");return query_visit_reply_world(world,args.find("cursor")?boundary::get_int(args,"cursor","args"):0);}')
    if path.endswith('/native_resident_backend.gd'):
        s=after(s,'expected["module_versions"].append({"name":"contact_inquiries"','\tif definition.get("visit_reply_profile","")=="witnessed_visit_replies_v1": expected["module_versions"].append({"name":"visit_message_replies","version":"1"})')
        return s+'\n## 本人回信独立启用，原消息及独立冷段由Native严格配对。\nfunc visit_message_replies_enabled() -> bool:\n\treturn _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("visit_reply_profile","")=="witnessed_visit_replies_v1"\n'
    if path.endswith('/resident_page_requests.gd'):
        s=after(s,'const ContactInquiriesPanel=','const VisitMessageRepliesPanel=preload("res://ui/visit_message_replies_panel.gd")')
        return after(s,'and not ContactInquiriesPanel.prepare(host):','\tif host.kernel.has_method("visit_message_replies_enabled") and host.kernel.visit_message_replies_enabled() and not VisitMessageRepliesPanel.prepare(host): return false')
    if path.endswith('/screen_composer.gd'):
        s=after(s,'const ContactInquiriesPanel=','const VisitMessageRepliesPanel=preload("res://ui/visit_message_replies_panel.gd")')
        lines=[line for line in s.splitlines() if 'ContactInquiriesPanel.visible_here(m)' in line]
        if len(lines)!=2:raise ValueError('two original sidebars required')
        for line in lines:s=replace(s,'\n'+line+'\n','\n'+line+'\n'+line[:len(line)-len(line.lstrip())]+'if m._resident_mode and m.kernel.has_method("visit_message_replies_enabled") and m.kernel.visit_message_replies_enabled() and VisitMessageRepliesPanel.visible_here(m): VisitMessageRepliesPanel.build(m,_sidebar_section(m,stage,"VisitMessageRepliesArea","拜帖答复"))\n')
        return s
    if path.endswith('/content.schema.json'):
        d=json.loads(s);p=d['$defs']['scenario']['properties'];assert 'visit_message_replies' not in p
        fields={k:{'type':'string','minLength':1,'maxLength':256} for k in ['id','message_policy_id','reply_node_id','collect_node_id']}
        p['visit_reply_profile']={'type':'string','enum':[PROFILE]};p['visit_message_replies']={'type':'object','additionalProperties':False,'required':['version','policies'],'properties':{'version':{'type':'integer','enum':[1]},'policies':{'type':'array','minItems':1,'maxItems':4096,'items':{'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}}}};d['properties']['required_capabilities']['items']['enum'].append(CAP);return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s);assert not any(x['capability_id']==CAP for x in d['capabilities']);d['capabilities'].append({'capability_id':CAP,'rule_version':PROFILE,'module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.visit_reply_profile','equals':PROFILE}],'supported_semantics':[],'test_ids':['test_visit_message_replies_panel_guard'],'evidence':[],'limitations':['首片仅受控本人对已领原拜帖答复，并由原发信人真实到原接待者处领取；没有NPC自动回复、远程即时通知、预约承诺、关系或去向奖励。']});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/content/links.py':
        s=after(s,'from .contact_inquiry_links import','    from .visit_reply_links import check_visit_message_replies')
        return replace(s,' + check_contact_inquiries(pack,index)',' + check_contact_inquiries(pack,index) + check_visit_message_replies(pack,index)')
    if path=='tools/verify.py':return replace(s,"'test_contact_inquiries_panel_guard',","'test_contact_inquiries_panel_guard', 'test_visit_message_replies_panel_guard',")
    if path.endswith('/resident.cmake'):
        anchor='    src/resident/domains/interaction/contact_inquiries/contact_inquiry_world.cpp';assert s.count(anchor)==2;s=s.replace(anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+'visit_reply_'+x+'.cpp' for x in CPP))
        anchor='    src/resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp';s=replace(s,anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+'visit_reply'+x+'.hpp' for x in HEADERS))
        return s+'\n# 原拜帖领读/本人回信/实领、冷历史与永久结果门；运行单独按风险排期。\nadd_executable(test_resident_visit_reply tests/resident/visit_reply_test.cpp)\ntarget_link_libraries(test_resident_visit_reply PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_visit_reply PRIVATE src)\ntarget_compile_options(test_resident_visit_reply PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_visit_reply COMMAND test_resident_visit_reply)\n'
    raise ValueError(path)

def main():
    """所有具名锚点先成功再写新候选，清单含每个原/新blob OID，旧候选不覆。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.base!=BASE or a.output.exists():raise ValueError('fixed reviewed base and fresh output required')
    results={};rows=[];patch=[];oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in SHARED:
        before=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);s=transform(path,before.decode());data=s.encode();results[path]=data;rows.append({'path':path,'base_oid':oid(before),'result_oid':oid(data)});patch.extend(difflib.unified_diff(before.decode().splitlines(True),s.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
    for path,data in results.items():target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    (a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':a.base,'status':'SOURCE_ONLY_PENDING_SYNTAX','shared':rows,'module':'visit_message_replies/1','old_visitors_unchanged':['Interaction','visit_messages/1','Task1','Task2','ReceiptData'],'build_cpp':[ROOT+'visit_reply_'+x+'.cpp' for x in CPP],'build_headers':[ROOT+'visit_reply'+x+'.hpp' for x in HEADERS],'external_publication':'central/root only; prior shared publication restriction retained'},ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
