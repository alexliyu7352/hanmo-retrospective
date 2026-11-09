"""固定Git对象的新出生本人模板完整候选；全部公共叶只写隔离输出，旧visitor字节不变。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
from task_extension_registration_candidate import replace,after
from contact_inquiry_registration_candidate import append_variant
BASE='9220a562e7480d24dff8c33040ca9bac2d6e654c'
MODULE='born_personal_dialogues'
CAP=MODULE+'_v1'
ROOT='native/world/src/resident/domains/interaction/born_dialogues/'
INCLUDE='#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp"\n'
CPP=['catalog','codec','proof_codec','rules','validation','access','control','receipts','boundary','view','world']
HEADERS=['','_state','_catalog','_contract','_detail','_control','_receipts','_boundary','_view','_world']
SHARED=['native/world/include/hanmo/resident/model_contract.hpp','native/world/src/resident/kernel/command_types.hpp','native/world/src/resident/kernel/dispatcher.cpp','native/world/src/resident/checkpoint/command_boundary.cpp','native/world/src/resident/kernel/phase_plan.cpp','native/world/src/resident/kernel/dynamic_integration.cpp','native/world/src/resident/kernel/world_services.cpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/kernel/world_bundle.hpp','native/world/src/resident/kernel/resident_world.cpp','native/world/src/resident/storage/common_codec.cpp','native/world/src/resident/kernel/live_view_queries.cpp','game/ui/native_resident_backend.gd','game/ui/resident_page_requests.gd','game/ui/screen_composer.gd','schemas/content.schema.json','game/data/rule_capabilities.json','tools/content/links.py','tools/verify.py','native/world/resident.cmake']

def transform(path,s):
    if path.endswith('/model_contract.hpp'):
        s=replace(s,'struct VisitReplyCatalog;','struct VisitReplyCatalog;struct BornPersonalDialogueCatalog;')
        return replace(s,'std::shared_ptr<const VisitReplyIntentCatalog> visit_reply_intents;','std::shared_ptr<const VisitReplyIntentCatalog> visit_reply_intents;\n    std::shared_ptr<const BornPersonalDialogueCatalog> born_personal_dialogues;')
    if path.endswith('/command_types.hpp'):return append_variant(append_variant(INCLUDE+s,'CommandData','OpenBornPersonalDialogue'),'QuoteData','BornPersonalDialogueQuote')
    if path.endswith('/dispatcher.cpp'):
        s=after(INCLUDE+s,'const auto* visit_reply=std::get_if<VisitReplyCommand>(&command.data);','    const auto* born_dialogue=std::get_if<OpenBornPersonalDialogue>(&command.data);if((command.kind=="open_born_personal_dialogue")!=(born_dialogue!=nullptr))return reject("InvalidBornPersonalDialogueCommand");')
        s=after(s,'if(visit_reply){auto quote=prepare_visit_reply_world','    if(born_dialogue){auto quote=prepare_born_personal_dialogue_world(w,id,*born_dialogue);if(!quote.ok)return {false,{},quote.error};return ready(w,id,command,std::move(quote.value));}')
        s=after(s,'const auto* visit_reply=std::get_if<VisitReplyCommand>(&op.command.data);','    const auto* born_dialogue=std::get_if<OpenBornPersonalDialogue>(&op.command.data);if((op.command.kind=="open_born_personal_dialogue")!=(born_dialogue!=nullptr)||(born_dialogue&&!std::holds_alternative<BornPersonalDialogueQuote>(op.quote.data)))return failed<ReceiptDetails>(common_failure("InvalidBornPersonalDialogueCommand"));')
        return after(s,'if(visit_reply){auto result=commit_visit_reply_world','    if(born_dialogue){auto result=commit_born_personal_dialogue_world(w,id,*born_dialogue,std::get<BornPersonalDialogueQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{std::move(result.value),{}},{}};}')
    if path.endswith('/command_boundary.cpp'):
        s=after(INCLUDE+s,'if(visit_reply_command_kind(kind)){auto decoded=','    if(kind=="open_born_personal_dialogue"){auto decoded=decode_born_personal_dialogue_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}')
        return after(s,'else if constexpr(std::is_same_v<T,VisitReplyQuote>)','                else if constexpr(std::is_same_v<T,BornPersonalDialogueQuote>)return encode_born_personal_dialogue_quote(quote);')
    if path.endswith('/phase_plan.cpp'):
        s=after(INCLUDE+s,'Status try_npc_command(WorldBundle& w,','    applied=false;if(std::holds_alternative<OpenBornPersonalDialogue>(command.data)||command.kind=="open_born_personal_dialogue")return common_failure("BornPersonalDialoguePlayerRequired");')
        return replace(s,'std::holds_alternative<PersonalAppointmentCommand>(command.data)||','std::holds_alternative<PersonalAppointmentCommand>(command.data)||std::holds_alternative<OpenBornPersonalDialogue>(command.data)||')
    if path.endswith('/dynamic_integration.cpp'):
        return after(INCLUDE+s,'Status adopt_director_proposal(', '    if(proposal.command&&(std::holds_alternative<OpenBornPersonalDialogue>(proposal.command->data)||proposal.command->kind=="open_born_personal_dialogue"))return common_failure("BornPersonalDialoguePlayerRequired");')
    if path.endswith('/world_services.cpp'):
        return after(INCLUDE+s,'InteractionAccess BundleServices::interaction_access() const {','    // 只绑定当前实际session；完整出生/控制来源由World晚门核验。\n') .replace('    InteractionAccess access;','    InteractionAccess access;\n    access.born_personal_session=[this](const InteractionSession& session){return match_born_personal_dialogue_session_world(world_,session);};',1)
    if path.endswith('/native_initial_state.cpp'):
        s=after(INCLUDE+s,'if(visit_replies.value==2)admitted.module_versions.emplace_back','    auto born_dialogues=decode_born_personal_dialogue_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!born_dialogues.ok)return {false,{},born_dialogues.error};if(born_dialogues.value)admitted.module_versions.emplace_back("born_personal_dialogues","1");')
        return replace(s,'auto interaction=std::make_shared<InteractionState>();','auto interaction=std::make_shared<InteractionState>();if(mutable_catalog->born_personal_dialogues)interaction->born_personal_dialogues.emplace();')
    if path.endswith('/world_codec.cpp'):
        s=INCLUDE+s
        # 新独立段严格在旧最后段后追加，三个producer与两个冷选择共用此顺序。
        line=next(x for x in s.splitlines() if x.startswith('std::vector<CheckpointDomain> domains_for('));s=replace(s,line,line.replace('return domains;}','if(catalog.born_personal_dialogues)domains.push_back({"born_personal_dialogues",1});return domains;}'))
        s=replace(s,'{"visit_reply_intents",{"1"}}','{"visit_reply_intents",{"1"}}, {"born_personal_dialogues",{"1"}}')
        s=after(s,'if(has("family_disciple_legacy")){','        if(has("born_personal_dialogues")){if(!dynamic||!has("family_birth")||!has("family_lineage")||!has("family_life"))fail("BornPersonalDialogueProfileMismatch","checkpoint/module_versions");domains.push_back({"born_personal_dialogues",1});}')
        s=after(s,'if(has("visit_message_replies","1")!=','            if(has("born_personal_dialogues","1")!=(version("born_personal_dialogues")==1)||(version("born_personal_dialogues")&&(!dynamic||!has("family_birth","1")||!has("family_lineage","1")||!has("family_life","1"))))return {false,{},common_failure("BornPersonalDialogueProfileMismatch").error};')
        s=after(s,'if(const auto* config=scenario.find("family_birth")){','    auto born_dialogues=decode_born_personal_dialogue_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!born_dialogues.ok)return {false,{},born_dialogues.error};if(born_dialogues.value){if(!result->family_birth||!result->family)return {false,{},common_failure("BornPersonalDialogueBirthProfileRequired").error};auto decoded=decode_born_personal_dialogue_catalog(field(field(scenario,"dynamic_world","scenario"),"born_personal_dialogues","dynamic_world"),*result->interaction,*result->family,*result->family_birth);if(!decoded.ok)return {false,{},decoded.error};result->born_personal_dialogues=std::make_shared<const BornPersonalDialogueCatalog>(std::move(decoded.value));}')
        s=after(s,'if(result->visit_reply_intents)id.module_versions','    if(result->born_personal_dialogues)id.module_versions.emplace_back("born_personal_dialogues","1");')
        s=replace(s,'result.interaction=std::make_shared<const InteractionState>(std::move(i.value));','if(catalog.born_personal_dialogues){auto decoded=decode_born_personal_dialogues(field(source,"born_personal_dialogues","state"));if(!decoded.ok)return {false,{},decoded.error};i.value.born_personal_dialogues=std::move(decoded.value);}else if(source.find("born_personal_dialogues"))fail("UnexpectedBornPersonalDialogueDomain","born_personal_dialogues");result.interaction=std::make_shared<const InteractionState>(std::move(i.value));')
        s=after(s,'auto checked=validate_visit_reply_json_payloads(','    {CommonStore view(result.common,catalog.common);auto checked=validate_born_personal_dialogue_json_payloads(view,result.interaction&&result.interaction->born_personal_dialogues?&*result.interaction->born_personal_dialogues:nullptr,source);if(!checked.ok)return {false,{},checked.error};}')
        s=after(s,'auto visit_replies=visit_reply_fact_payloads(','        auto born_dialogues=born_personal_dialogue_fact_payloads(w.common,w.interaction.born_personal_dialogues?&*w.interaction.born_personal_dialogues:nullptr);if(!born_dialogues.ok)return born_dialogues;merge_unique(facts,born_dialogues.value);')
        s=after(s,'set(output,"family_disciple_legacy",','    if(w.catalog->born_personal_dialogues){if(!w.interaction.born_personal_dialogues)fail("MissingBornPersonalDialogueDomain","born_personal_dialogues");set(output,"born_personal_dialogues",encode_born_personal_dialogues(*w.interaction.born_personal_dialogues));}')
        s=after(s,'r=section("family_disciple_legacy",','    if(w.catalog->born_personal_dialogues){if(!w.interaction.born_personal_dialogues)return {false,{},common_failure("MissingBornPersonalDialogueDomain").error};r=section("born_personal_dialogues",[&](auto& c){return visit_born_personal_dialogues_checkpoint(c,*w.interaction.born_personal_dialogues);});if(!r.ok)return {false,{},r.error};}')
        return after(s,'else if(section.domain=="visit_message_replies")','        else if(section.domain=="born_personal_dialogues"){i->born_personal_dialogues.emplace();r=visit_born_personal_dialogues_checkpoint(reader,*i->born_personal_dialogues);}')
    if path.endswith('/world_bundle.cpp'):
        s=after(INCLUDE+s,'auto payloads=visit_reply_fact_payloads(','    {auto payloads=born_personal_dialogue_fact_payloads(w.common,w.interaction.born_personal_dialogues?&*w.interaction.born_personal_dialogues:nullptr);if(!payloads.ok)return {false,{},payloads.error};for(const auto& row:boundary::object(payloads.value,"born_personal_dialogues")){auto r=fact(row.first,boundary::get_string(row.second,"kind",row.first),born_personal_dialogue_module);if(!r.ok)return r;}}')
        return after(s,'valid=validate_family_birth_world(*this);','    // 原出生/生命全验后，先核打开控制链与真实结果；Legacy2授权后继消费本来源。\n    valid=validate_born_personal_dialogue_world(*this);if(!valid.ok)return valid;')
    if path.endswith('/world_bundle.hpp'):return replace(s,'const DiscipleControlState* disciple_bindings_before=nullptr);','const DiscipleControlState* disciple_bindings_before=nullptr,const StableId& born_dialogue_source={});')
    if path.endswith('/resident_world.cpp'):
        s=replace(INCLUDE+s,'auto visit_reply_source=visit_reply_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);','auto born_dialogue_source=born_personal_dialogue_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!born_dialogue_source.ok)return {false,{},born_dialogue_source.error};auto visit_reply_source=visit_reply_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);')
        s=replace(s,'?&current.family_disciple_legacy:nullptr);','?&current.family_disciple_legacy:nullptr,born_dialogue_source.value);')
        s=replace(s,'const DiscipleControlState* disciple_bindings_before){','const DiscipleControlState* disciple_bindings_before,const StableId& born_dialogue_source){')
        return after(s,'auto visit_reply_bound=bind_visit_reply_result_world','    auto born_dialogue_bound=bind_born_personal_dialogue_result_world(w,born_dialogue_source,actor,sequence,*receipt);if(!born_dialogue_bound.ok)return born_dialogue_bound;')
    if path.endswith('/common_codec.cpp'):
        s=replace(s,'const bool appointment=','const bool born_dialogue=kind=="born_personal_dialogue_opened";const bool appointment=')
        s=replace(s,'std::string domain=','std::string domain=born_dialogue?"born_personal_dialogues":')
        s=replace(s,'if(appointment&&named!=','if(born_dialogue&&named!="born_personal_dialogues")fail("InvalidFactDomain",row.first);if(appointment&&named!=')
        return replace(s,'&&!(named=="personal_appointments"','&&!(named=="born_personal_dialogues"&&born_dialogue)&&!(named=="personal_appointments"')
    if path.endswith('/live_view_queries.cpp'):return after(INCLUDE+s,'if(query=="visit_message_replies"){','    if(query=="born_personal_dialogues"){boundary::fields(args,{},{"cursor"},"args");return query_born_personal_dialogue_world(world,args.find("cursor")?boundary::get_int(args,"cursor","args"):0);}')
    if path.endswith('/native_resident_backend.gd'):
        s=after(s,'if definition.get("visit_reply_profile","")=="witnessed_authored_visit_replies_v2": expected','\tif definition.get("dynamic_world",{}).has("born_personal_dialogues"): expected["module_versions"].append({"name":"born_personal_dialogues","version":"1"})')
        return s+'\n## 独立出生模板只开真实本人会话；完整Birth与来源配对仍由Native核。\nfunc born_personal_dialogues_enabled() -> bool:\n\treturn _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("born_personal_dialogues",{}).get("version",0)==1\n'
    if path.endswith('/resident_page_requests.gd'):
        s=after(s,'const VisitMessageRepliesPanel=','const BornPersonalDialoguesPanel=preload("res://ui/born_personal_dialogues_panel.gd")')
        return after(s,'and not VisitMessageRepliesPanel.prepare(host):','\tif host.kernel.has_method("born_personal_dialogues_enabled") and host.kernel.born_personal_dialogues_enabled() and not BornPersonalDialoguesPanel.prepare(host): return false')
    if path.endswith('/screen_composer.gd'):
        s=after(s,'const VisitMessageRepliesPanel=','const BornPersonalDialoguesPanel=preload("res://ui/born_personal_dialogues_panel.gd")')
        lines=[x for x in s.splitlines() if 'VisitMessageRepliesPanel.visible_here(m)' in x]
        if len(lines)!=2:raise ValueError('two actual scene sidebars required')
        for line in lines:s=replace(s,'\n'+line+'\n','\n'+line+'\n'+line[:len(line)-len(line.lstrip())]+'if m._resident_mode and m.kernel.has_method("born_personal_dialogues_enabled") and m.kernel.born_personal_dialogues_enabled() and BornPersonalDialoguesPanel.visible_here(m): BornPersonalDialoguesPanel.build(m,_sidebar_section(m,stage,"BornPersonalDialoguesArea","本人私人交谈"))\n')
        return s
    if path.endswith('/content.schema.json'):
        d=json.loads(s);p=d['$defs']['dynamic_world']['properties'];assert MODULE not in p
        fields={'id':{'type':'string','minLength':1,'maxLength':256},'prototype_id':{'$ref':'#/$defs/reference_actors'},'dialogue_id':{'$ref':'#/$defs/reference_dialogues'}}
        p[MODULE]={'type':'object','additionalProperties':False,'required':['version','templates'],'properties':{'version':{'type':'integer','enum':[1]},'templates':{'type':'array','minItems':1,'maxItems':256,'items':{'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}}}};d['properties']['required_capabilities']['items']['enum'].append(CAP);return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s);d['capabilities'].append({'capability_id':CAP,'rule_version':'born_personal_dialogues/1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.born_personal_dialogues.version','equals':1}],'supported_semantics':[],'test_ids':['test_born_personal_dialogues_panel_guard'],'evidence':[],'limitations':['实际成年出生者与其原亲生父母本人当面，精确原prototype模板与provider say；只开交谈，不自动授同意/控制/财产。完整World尚待。']});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/content/links.py':
        s=after(s,'from .visit_reply_links import','    from .born_personal_dialogue_links import check_born_personal_dialogues,born_personal_dialogue_bindings')
        s=replace(s,'actor_dialogues, need_dialogues = regional_dialogue_bindings(pack)','actor_dialogues, need_dialogues = regional_dialogue_bindings(pack)\n    actor_dialogues = set(actor_dialogues) | born_personal_dialogue_bindings(pack)')
        return replace(s,' + check_visit_reply_intents(pack,index)',' + check_visit_reply_intents(pack,index) + check_born_personal_dialogues(pack,index)')
    if path=='tools/verify.py':return replace(s,"'test_visit_message_replies_panel_guard',","'test_visit_message_replies_panel_guard', 'test_born_personal_dialogues_panel_guard',")
    if path.endswith('/resident.cmake'):
        anchor='    src/resident/domains/interaction/visit_replies/visit_reply_world.cpp';assert s.count(anchor)==2;s=s.replace(anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+'born_personal_dialogue_'+n+'.cpp' for n in CPP)+'\n    src/resident/domains/family/born_actor_source.cpp')
        anchor='    src/resident/domains/interaction/visit_replies/visit_reply_world.hpp';s=replace(s,anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+'born_personal_dialogue'+n+'.hpp' for n in HEADERS)+'\n    src/resident/domains/family/born_actor_source.hpp')
        return s+'\n# 原出生模板、会话与真实提交来源；仅登记，行为/World验证后置。\nadd_executable(test_resident_born_personal_dialogue tests/resident/born_personal_dialogue_test.cpp)\ntarget_link_libraries(test_resident_born_personal_dialogue PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_born_personal_dialogue PRIVATE src)\ntarget_compile_options(test_resident_born_personal_dialogue PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_born_personal_dialogue COMMAND test_resident_born_personal_dialogue)\n'
    raise ValueError(path)

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.base!=BASE or a.output.exists():raise ValueError('fixed base and fresh output required')
    results={};rows=[];patch=[];oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in SHARED:
        before=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);text=transform(path,before.decode());data=text.encode();results[path]=data;rows.append({'path':path,'base_oid':oid(before),'result_oid':oid(data)});patch.extend(difflib.unified_diff(before.decode().splitlines(True),text.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
    for path,data in results.items():target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    (a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':a.base,'status':'SOURCE_ONLY_PENDING_SYNTAX','shared':rows,'module':'born_personal_dialogues/1','dependency':{'head':'9ba4725b129cc5b62f75a29580ca4bb4776bfcfc','paths':['native/world/src/resident/domains/family/born_actor_source.hpp','native/world/src/resident/domains/family/born_actor_source.cpp']},'cold_order':'FullBirth/Creation/Lineage/Life -> Born opening pure player chain and actual results -> Legacy2 full authorization (S4 successor candidate)','old_visitors_unchanged':['Interaction','Task1','Task2','ReceiptData'],'external_writer':'central/root only'},ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
