"""中断后新重建的回复2共享候选；固定5566，不覆盖中央或冒遗失候选原OID。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
from task_extension_registration_candidate import replace,after
from contact_inquiry_registration_candidate import append_variant
BASE='5566d061522a51cf9bf596a4ac0a2ca7b7ac2e94'
ROOT='native/world/src/resident/domains/interaction/visit_replies/'
SHARED=['native/world/include/hanmo/resident/model_contract.hpp','native/world/src/resident/kernel/dispatcher.cpp','native/world/src/resident/checkpoint/command_boundary.cpp','native/world/src/resident/kernel/phase_plan.cpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/command_types.hpp','native/world/src/resident/kernel/world_bundle.cpp','game/ui/native_resident_backend.gd','schemas/content.schema.json','game/data/rule_capabilities.json','tools/content/links.py','native/world/resident.cmake']
PROFILE='witnessed_authored_visit_replies_v2'
CAP='visit_message_replies_v2'
INCLUDE='#include "resident/domains/interaction/visit_replies/visit_reply_intents.hpp"\n'

def transform(path,s):
    """原v1段位置不变，只升级显式v2；控制史全验后才消费NPC身份，不碰原实际result钩。"""
    if path.endswith('/model_contract.hpp'):
        s=replace(s,'struct VisitReplyCatalog;','struct VisitReplyCatalog;struct VisitReplyIntentCatalog;')
        return replace(s,'std::shared_ptr<const VisitReplyCatalog> visit_message_replies;','std::shared_ptr<const VisitReplyCatalog> visit_message_replies;\n    std::shared_ptr<const VisitReplyIntentCatalog> visit_reply_intents;')
    if path.endswith('/command_types.hpp'):return append_variant(append_variant(s,'CommandData','AuthoredVisitReply'),'QuoteData','AuthoredVisitReplyQuote')
    if path.endswith('/dispatcher.cpp'):
        s=after(s,'const auto* visit_reply=std::get_if<VisitReplyCommand>(&command.data);','    const auto* authored_reply=std::get_if<AuthoredVisitReply>(&command.data);if((command.kind==authored_visit_reply_command)!=(authored_reply!=nullptr))return reject("InvalidAuthoredVisitReplyCommand");')
        s=after(s,'if(visit_reply){auto quote=prepare_visit_reply_world','    if(authored_reply){auto quote=prepare_authored_visit_reply_world(w,id,*authored_reply);if(!quote.ok)return {false,{},quote.error};return ready(w,id,command,std::move(quote.value));}')
        s=after(s,'const auto* visit_reply=std::get_if<VisitReplyCommand>(&op.command.data);','    const auto* authored_reply=std::get_if<AuthoredVisitReply>(&op.command.data);if((op.command.kind==authored_visit_reply_command)!=(authored_reply!=nullptr)||(authored_reply&&!std::holds_alternative<AuthoredVisitReplyQuote>(op.quote.data)))return failed<ReceiptDetails>(common_failure("InvalidAuthoredVisitReplyCommand"));')
        return after(s,'if(visit_reply){auto result=commit_visit_reply_world','    if(authored_reply){auto result=commit_authored_visit_reply_world(w,id,*authored_reply,std::get<AuthoredVisitReplyQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{std::move(result.value),{}},{}};}')
    if path.endswith('/command_boundary.cpp'):
        s=after(s,'if(visit_reply_command_kind(kind)){auto decoded=','    if(kind==authored_visit_reply_command)return {false,{},common_failure("VisitReplyNpcOnlyCommand").error};')
        return after(s,'else if constexpr(std::is_same_v<T,VisitReplyQuote>)','                else if constexpr(std::is_same_v<T,AuthoredVisitReplyQuote>)return encode_visit_reply_quote(quote.reply);')
    if path.endswith('/phase_plan.cpp'):
        # 第一处是玩家命令拒门，不得把新NPC命令也加到该拒门。
        anchor='std::holds_alternative<PersonalAppointmentCommand>(command.data)||std::holds_alternative<VisitReplyCommand>(command.data)||'
        return replace(s,anchor,'std::holds_alternative<PersonalAppointmentCommand>(command.data)||std::holds_alternative<AuthoredVisitReply>(command.data)||std::holds_alternative<VisitReplyCommand>(command.data)||')
    if path.endswith('/native_initial_state.cpp'):
        s=replace(INCLUDE+s,'auto visit_replies=decode_visit_reply_profile(','auto visit_replies=decode_visit_reply_profile_version(')
        s=replace(s,'if(visit_replies.value)admitted.module_versions.emplace_back("visit_message_replies","1");','if(visit_replies.value)admitted.module_versions.emplace_back("visit_message_replies",std::to_string(visit_replies.value));')
        s=after(s,'if(const auto* config=scenario.find("family_disciple_legacy"))','    if(visit_replies.value==2)admitted.module_versions.emplace_back("visit_reply_intents","1");')
        return replace(s,'if(mutable_catalog->visit_message_replies)interaction->visit_message_replies.emplace();','if(mutable_catalog->visit_message_replies){interaction->visit_message_replies.emplace();interaction->visit_message_replies->version=mutable_catalog->visit_message_replies->version;}')
    if path.endswith('/world_codec.cpp'):
        s=INCLUDE+s;s=replace(s,'domains.push_back({"visit_message_replies",1});return domains;', 'domains.push_back({"visit_message_replies",static_cast<std::uint32_t>(catalog.visit_message_replies->version)});return domains;') if 'domains.push_back({"visit_message_replies",1});return domains;' in s else replace(s,'if(catalog.visit_message_replies)domains.push_back({"visit_message_replies",1});','if(catalog.visit_message_replies)domains.push_back({"visit_message_replies",static_cast<std::uint32_t>(catalog.visit_message_replies->version)});')
        s=replace(s,'{"visit_message_replies",{"1"}}','{"visit_message_replies",{"1","2"}}, {"visit_reply_intents",{"1"}}')
        s=replace(s,'if(has("visit_message_replies")){if(!dynamic||!has("visit_messages"))fail("VisitReplyProfileMismatch","checkpoint/module_versions");domains.push_back({"visit_message_replies",1});}','if(has("visit_message_replies")){if(!dynamic||!has("visit_messages"))fail("VisitReplyProfileMismatch","checkpoint/module_versions");domains.push_back({"visit_message_replies",modules.at("visit_message_replies")=="2"?2u:1u});}\n        if(has("visit_reply_intents")!=(has("visit_message_replies")&&modules.at("visit_message_replies")=="2")||(has("visit_reply_intents")&&(!has("visit_reception_intents")||!has("visit_reception_claims"))))fail("VisitReplyAuthorshipRequired","checkpoint/module_versions");')
        s=replace(s,'if(has("visit_message_replies","1")!=(version("visit_message_replies")==1)||(version("visit_message_replies")&&(!dynamic||!has("visit_messages","1"))))return {false,{},common_failure("VisitReplyProfileMismatch").error};','if(has("visit_message_replies","1")!=(version("visit_message_replies")==1)||has("visit_message_replies","2")!=(version("visit_message_replies")==2)||(version("visit_message_replies")&&(!dynamic||!has("visit_messages","1")))||has("visit_reply_intents","1")!=(version("visit_message_replies")==2)||(has("visit_reply_intents","1")&&(!has("visit_reception_intents","1")||!has("visit_reception_claims","1"))))return {false,{},common_failure("VisitReplyProfileMismatch").error};')
        s=replace(s,'auto visit_replies=decode_visit_reply_profile(','auto visit_replies=decode_visit_reply_profile_version(')
        s=replace(s,'result->visit_message_replies=std::make_shared<const VisitReplyCatalog>(std::move(decoded.value));','if(decoded.value.version!=visit_replies.value)return {false,{},common_failure("VisitReplyProfileMismatch").error};result->visit_message_replies=std::make_shared<const VisitReplyCatalog>(std::move(decoded.value));')
        s=after(s,'result->director=std::make_shared<const DirectorCatalog>','        if(visit_replies.value==2){if(!result->director->visit_reception||!result->visit_message_replies)return {false,{},common_failure("VisitReplyAuthorshipRequired").error};auto parsed=decode_visit_reply_intents(field(scenario,"visit_reply_intents","scenario"),*result->visit_message_replies,*result->director->visit_reception,*result->interaction->visit_messages,*result->interaction);if(!parsed.ok)return {false,{},parsed.error};result->visit_reply_intents=std::make_shared<const VisitReplyIntentCatalog>(std::move(parsed.value));}')
        s=replace(s,'if(result->visit_message_replies)id.module_versions.emplace_back("visit_message_replies","1");','if(result->visit_message_replies)id.module_versions.emplace_back("visit_message_replies",std::to_string(result->visit_message_replies->version));')
        return after(s,'if(result->family_disciple_legacy)id.module_versions','    if(result->visit_reply_intents)id.module_versions.emplace_back("visit_reply_intents","1");')
    if path.endswith('/world_bundle.cpp'):
        s=replace(s,'    valid=validate_visit_reply_world(*this);if(!valid.ok)return valid;','    if(!catalog->visit_message_replies||catalog->visit_message_replies->version==1){valid=validate_visit_reply_world(*this);if(!valid.ok)return valid;}')
        return after(s,'valid=validate_family_disciple_legacy_world(*this);','    // 回复2真实producer要借原全部控制史，必须在弟子Legacy全验后消费。\n    if(catalog->visit_message_replies&&catalog->visit_message_replies->version==2){valid=validate_visit_reply_world(*this);if(!valid.ok)return valid;}')
    if path.endswith('/native_resident_backend.gd'):
        s=replace(s,'if definition.get("visit_reply_profile","")=="witnessed_visit_replies_v1": expected["module_versions"].append({"name":"visit_message_replies","version":"1"})','if definition.get("visit_reply_profile","") in ["witnessed_visit_replies_v1","witnessed_authored_visit_replies_v2"]: expected["module_versions"].append({"name":"visit_message_replies","version":"2" if definition.get("visit_reply_profile","")=="witnessed_authored_visit_replies_v2" else "1"})')
        s=after(s,'if definition.has("family_disciple_legacy")','\tif definition.get("visit_reply_profile","")=="witnessed_authored_visit_replies_v2": expected["module_versions"].append({"name":"visit_reply_intents","version":"1"})')
        return replace(s,'get("visit_reply_profile","")=="witnessed_visit_replies_v1"','get("visit_reply_profile","") in ["witnessed_visit_replies_v1","witnessed_authored_visit_replies_v2"]')
    if path.endswith('/content.schema.json'):
        d=json.loads(s);p=d['$defs']['scenario']['properties'];p['visit_reply_profile']['enum'].append(PROFILE);p['visit_message_replies']['properties']['version']['enum']=[1,2];fields={key:{'type':'string','minLength':1,'maxLength':256} for key in ['id','reception_intent_id','reply_policy_id']};fields['response']={'type':'string','enum':['declined','contact_in_person']};fields['route']={'type':'array','minItems':1,'maxItems':16,'items':{'type':'object','additionalProperties':False,'required':['node_id'],'properties':{key:{'type':'string','minLength':1,'maxLength':256} for key in ['node_id','option_id']}}};p['visit_reply_intents']={'type':'object','additionalProperties':False,'required':['version','intents'],'properties':{'version':{'type':'integer','enum':[1]},'intents':{'type':'array','minItems':1,'maxItems':4096,'items':{'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}}}};d['properties']['required_capabilities']['items']['enum'].append(CAP);return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s);d['capabilities'].append({'capability_id':CAP,'rule_version':PROFILE,'module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.visit_reply_profile','equals':PROFILE}],'supported_semantics':[],'test_ids':['test_visit_message_replies_panel_guard'],'evidence':[],'limitations':['仅原已领读及真正认领会话的具名NPC托回固定答复；回复当时控制身份按原Family全史精确证明。原发信人仍需亲领，不授预约/位置/关系。自然World尚待。']});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/content/links.py':
        s=replace(s,'from .visit_reply_links import check_visit_message_replies','from .visit_reply_links import check_visit_message_replies,check_visit_reply_intents')
        return replace(s,' + check_visit_message_replies(pack,index)',' + check_visit_message_replies(pack,index) + check_visit_reply_intents(pack,index)')
    if path.endswith('/resident.cmake'):
        anchor='    src/resident/domains/interaction/visit_replies/visit_reply_world.cpp';assert s.count(anchor)==2;s=s.replace(anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+'visit_reply_'+n+'.cpp' for n in ['intents','authored','director','control']))
        anchor='    src/resident/domains/interaction/visit_replies/visit_reply_world.hpp';s=replace(s,anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+'visit_reply_'+n+'.hpp' for n in ['intents','authored','director','control']))
        return s+'\n# 原NPC认领与回复控制史；仅登记，不冒自然World或旧银行已验。\nadd_executable(test_resident_visit_reply_authored tests/resident/visit_reply_authored_test.cpp)\ntarget_link_libraries(test_resident_visit_reply_authored PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_visit_reply_authored PRIVATE src)\ntarget_compile_options(test_resident_visit_reply_authored PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_visit_reply_authored COMMAND test_resident_visit_reply_authored)\n'
    raise ValueError(path)

def main():
    """全部固定锚点闭合才写新隔离候选；清单包含新重建状态与精确原/新OID。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.base!=BASE or a.output.exists():raise ValueError('fixed base and fresh output required')
    results={};rows=[];patch=[];oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in SHARED:
        before=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);text=transform(path,before.decode());data=text.encode();results[path]=data;rows.append({'path':path,'base_oid':oid(before),'result_oid':oid(data)});patch.extend(difflib.unified_diff(before.decode().splitlines(True),text.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
    for path,data in results.items():target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    (a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':a.base,'status':'NEWLY_REBUILT_SOURCE_ONLY','shared':rows,'cold_order':'originalFamily/Life/Birth/apprenticeship/discipleLegacy before reply2; reply1 old position','original_visitors_unchanged':['Interaction','visit_messages/1','Task1','Task2','ReceiptData'],'external_writer':'central/root only'},ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
