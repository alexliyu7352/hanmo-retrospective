"""固定Git公共候选：原实际会面链追加具名Known模板，旧Born与Interaction字节不变。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
from task_extension_registration_candidate import replace,after
from contact_inquiry_registration_candidate import append_variant
BASE='822f497357830ae18fc2baa4ccefdfc3147561c3'
ROOT='native/world/src/resident/domains/interaction/personal_dialogues/'
INCLUDE='#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"\n'
MODULE='known_personal_dialogues'; CAP=MODULE+'_v1'
CPP=['known_personal_actor_source','known_personal_dialogue_catalog','known_personal_dialogue_codec','known_personal_dialogue_proof_codec','known_personal_dialogue_rules','known_personal_dialogue_validation','known_personal_dialogue_access','known_personal_dialogue_control','known_personal_dialogue_receipts','known_personal_dialogue_boundary','known_personal_dialogue_view','known_personal_dialogue_world']
HEADERS=['known_personal_actor_source','known_personal_dialogue','known_personal_dialogue_catalog','known_personal_dialogue_state','known_personal_dialogue_contract','known_personal_dialogue_codec_detail','known_personal_dialogue_detail','known_personal_dialogue_control','known_personal_dialogue_receipts','known_personal_dialogue_boundary','known_personal_dialogue_view','known_personal_dialogue_world']
SHARED=['native/world/include/hanmo/resident/model_contract.hpp','native/world/src/resident/domains/interaction/interaction_model.hpp','native/world/src/resident/domains/interaction/interaction_rules.cpp','native/world/src/resident/domains/interaction/dialogue_node_source.cpp','native/world/src/resident/domains/interaction/regional_life_dialogue.cpp','native/world/src/resident/kernel/command_types.hpp','native/world/src/resident/kernel/dispatcher.cpp','native/world/src/resident/checkpoint/command_boundary.cpp','native/world/src/resident/kernel/phase_plan.cpp','native/world/src/resident/kernel/dynamic_integration.cpp','native/world/src/resident/kernel/world_services.cpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/kernel/world_bundle.hpp','native/world/src/resident/kernel/resident_world.cpp','native/world/src/resident/storage/common_codec.cpp','native/world/src/resident/kernel/live_view_queries.cpp','game/ui/native_resident_backend.gd','game/ui/resident_page_requests.gd','game/ui/screen_composer.gd','schemas/content.schema.json','game/data/rule_capabilities.json','tools/content/links.py','tools/verify.py','native/world/resident.cmake']
def transform(path,s):
 if path.endswith('/model_contract.hpp'):
  s=replace(s,'struct BornPersonalDialogueCatalog;','struct BornPersonalDialogueCatalog;struct KnownPersonalDialogueCatalog;')
  return after(s,'std::shared_ptr<const BornPersonalDialogueCatalog> born_personal_dialogues;','    std::shared_ptr<const KnownPersonalDialogueCatalog> known_personal_dialogues;')
 if path.endswith('/interaction_model.hpp'):
  s=after(s,'#include "born_dialogues/born_personal_dialogue_state.hpp"','#include "personal_dialogues/known_personal_dialogue_state.hpp"')
  s=after(s,'std::optional<BornPersonalDialogueState> born_personal_dialogues;','    std::optional<KnownPersonalDialogueState> known_personal_dialogues;')
  return after(s,'std::function<Outcome<bool>(const InteractionSession&)> born_personal_session;','    std::function<Outcome<bool>(const InteractionSession&)> known_personal_session;')
 if path.endswith('/interaction_rules.cpp'):
  return after(s,'bool born_bound=false;', '        if(!born_bound&&a.known_personal_session){for(const auto& current:s.interactions)if(current.actor_id==actor&&current.context.provider_actor_id==ctx.provider_actor_id&&current.context.place_id==ctx.place_id){auto bound=a.known_personal_session(current);if(!bound.ok)return {false,{},bound.error};born_bound=bound.value;}}')
 if path.endswith('/dialogue_node_source.cpp'):
  return replace(s,'if(!bound)return {false,{},common_failure("InvalidAuthoredNodeSource").error};','if(!bound&&access.known_personal_session){auto actual=access.known_personal_session(*current);if(!actual.ok)return {false,{},actual.error};bound=actual.value;}if(!bound)return {false,{},common_failure("InvalidAuthoredNodeSource").error};')
 if path.endswith('/regional_life_dialogue.cpp'):
  return replace(s,'if(s.regional_action_committed||((s.context.kind','if(!born&&access&&access->known_personal_session){auto bound=access->known_personal_session(s);if(!bound.ok)return {false,{},bound.error};born=bound.value;}if(s.regional_action_committed||((s.context.kind')
 if path.endswith('/command_types.hpp'):return append_variant(append_variant(INCLUDE+s,'CommandData','OpenKnownPersonalDialogue'),'QuoteData','KnownPersonalDialogueQuote')
 if path.endswith('/dispatcher.cpp'):
  s=after(INCLUDE+s,'const auto* born_dialogue=std::get_if<OpenBornPersonalDialogue>(&command.data);','    const auto* known_dialogue=std::get_if<OpenKnownPersonalDialogue>(&command.data);if((command.kind=="open_known_personal_dialogue")!=(known_dialogue!=nullptr))return reject("InvalidKnownPersonalDialogueCommand");')
  s=after(s,'if(born_dialogue){auto quote=prepare_born_personal_dialogue_world','    if(known_dialogue){auto quote=prepare_known_personal_dialogue_world(w,id,*known_dialogue);if(!quote.ok)return {false,{},quote.error};return ready(w,id,command,std::move(quote.value));}')
  s=after(s,'const auto* born_dialogue=std::get_if<OpenBornPersonalDialogue>(&op.command.data);','    const auto* known_dialogue=std::get_if<OpenKnownPersonalDialogue>(&op.command.data);if((op.command.kind=="open_known_personal_dialogue")!=(known_dialogue!=nullptr)||(known_dialogue&&!std::holds_alternative<KnownPersonalDialogueQuote>(op.quote.data)))return failed<ReceiptDetails>(common_failure("InvalidKnownPersonalDialogueCommand"));')
  return after(s,'if(born_dialogue){auto result=commit_born_personal_dialogue_world','    if(known_dialogue){auto result=commit_known_personal_dialogue_world(w,id,*known_dialogue,std::get<KnownPersonalDialogueQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{std::move(result.value),{}},{}};}')
 if path.endswith('/command_boundary.cpp'):
  s=after(INCLUDE+s,'if(kind=="open_born_personal_dialogue"){','    if(kind=="open_known_personal_dialogue"){auto decoded=decode_known_personal_dialogue_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}')
  return after(s,'else if constexpr(std::is_same_v<T,BornPersonalDialogueQuote>)','                else if constexpr(std::is_same_v<T,KnownPersonalDialogueQuote>)return encode_known_personal_dialogue_quote(quote);')
 if path.endswith('/phase_plan.cpp'):
  s=after(INCLUDE+s,'Status try_npc_command(WorldBundle& w,','    applied=false;if(std::holds_alternative<OpenKnownPersonalDialogue>(command.data)||command.kind=="open_known_personal_dialogue")return common_failure("KnownPersonalDialoguePlayerRequired");')
  return replace(s,'std::holds_alternative<PersonalAppointmentCommand>(command.data)||','std::holds_alternative<PersonalAppointmentCommand>(command.data)||std::holds_alternative<OpenKnownPersonalDialogue>(command.data)||')
 if path.endswith('/dynamic_integration.cpp'):return after(INCLUDE+s,'Status adopt_director_proposal(', '    if(proposal.command&&(std::holds_alternative<OpenKnownPersonalDialogue>(proposal.command->data)||proposal.command->kind=="open_known_personal_dialogue"))return common_failure("KnownPersonalDialoguePlayerRequired");')
 if path.endswith('/world_services.cpp'):return after(INCLUDE+s,'access.born_personal_session=','    access.known_personal_session=[this](const InteractionSession& session){return match_known_personal_dialogue_session_world(world_,session);};')
 if path.endswith('/native_initial_state.cpp'):
  s=after(INCLUDE+s,'if(lessons.value)admitted.module_versions','    auto known_dialogues=decode_known_personal_dialogue_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!known_dialogues.ok)return {false,{},known_dialogues.error};if(known_dialogues.value)admitted.module_versions.emplace_back("known_personal_dialogues","1");')
  return replace(s,'auto interaction=std::make_shared<InteractionState>();','auto interaction=std::make_shared<InteractionState>();if(mutable_catalog->known_personal_dialogues)interaction->known_personal_dialogues.emplace();')
 if path.endswith('/world_codec.cpp'):
  s=INCLUDE+s
  line=next(x for x in s.splitlines() if x.startswith('std::vector<CheckpointDomain> domains_for('));s=replace(s,line,line.replace('return domains;}','if(catalog.known_personal_dialogues)domains.push_back({"known_personal_dialogues",1});return domains;}'))
  s=replace(s,'{"growth_tactic_lessons",{"1"}}','{"growth_tactic_lessons",{"1"}}, {"known_personal_dialogues",{"1"}}')
  s=after(s,'if(has("growth_tactic_lessons")){','        {auto selected=known_personal_dialogue_selected(identity.catalog.module_versions);if(!selected.ok)return {false,{},selected.error};if(selected.value){if(!dynamic)fail("KnownPersonalDialogueProfileMismatch","checkpoint/module_versions");domains.push_back({"known_personal_dialogues",1});}}')
  s=after(s,'if(has("growth_tactic_lessons","1")!=','            {auto selected=known_personal_dialogue_selected(versions);if(!selected.ok)return {false,{},selected.error};if(selected.value!=(version("known_personal_dialogues")==1)||(version("known_personal_dialogues")&&!dynamic))return {false,{},common_failure("KnownPersonalDialogueProfileMismatch").error};}')
  s=after(s,'auto tactic_lessons=decode_growth_tactic_lesson_profile(','    auto known_dialogues=decode_known_personal_dialogue_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!known_dialogues.ok)return {false,{},known_dialogues.error};if(known_dialogues.value){if(!result->family||!result->family_life)return {false,{},common_failure("KnownPersonalDialogueLifeProfileRequired").error};auto parsed=decode_known_personal_dialogue_catalog(field(field(scenario,"dynamic_world","scenario"),"known_personal_dialogues","dynamic_world"),*result->interaction,*result->family,result->family_birth?*result->family_birth:BirthArrangementCatalog{});if(!parsed.ok)return {false,{},parsed.error};result->known_personal_dialogues=std::make_shared<const KnownPersonalDialogueCatalog>(std::move(parsed.value));}')
  s=after(s,'if(result->growth_tactic_lessons)id.module_versions','    if(result->known_personal_dialogues)id.module_versions.emplace_back("known_personal_dialogues","1");')
  s=replace(s,'result.interaction=std::make_shared<const InteractionState>(std::move(i.value));','if(catalog.known_personal_dialogues){auto decoded=decode_known_personal_dialogues(field(source,"known_personal_dialogues","state"));if(!decoded.ok)return {false,{},decoded.error};i.value.known_personal_dialogues=std::move(decoded.value);}else if(source.find("known_personal_dialogues"))fail("UnexpectedKnownPersonalDialogueDomain","known_personal_dialogues");result.interaction=std::make_shared<const InteractionState>(std::move(i.value));')
  s=after(s,'auto checked=validate_born_personal_dialogue_json_payloads(','    {CommonStore view(result.common,catalog.common);auto checked=validate_known_personal_dialogue_json_payloads(view,result.interaction&&result.interaction->known_personal_dialogues?&*result.interaction->known_personal_dialogues:nullptr,source);if(!checked.ok)return {false,{},checked.error};}')
  s=after(s,'auto born_dialogues=born_personal_dialogue_fact_payloads(','        auto known_dialogues=known_personal_dialogue_fact_payloads(w.common,w.interaction.known_personal_dialogues?&*w.interaction.known_personal_dialogues:nullptr);if(!known_dialogues.ok)return known_dialogues;merge_unique(facts,known_dialogues.value);')
  s=after(s,'set(output,"growth_tactic_lessons",','    if(w.catalog->known_personal_dialogues){if(!w.interaction.known_personal_dialogues)fail("MissingKnownPersonalDialogueDomain","known_personal_dialogues");set(output,"known_personal_dialogues",encode_known_personal_dialogues(*w.interaction.known_personal_dialogues));}')
  s=after(s,'r=section("growth_tactic_lessons",','    if(w.catalog->known_personal_dialogues){if(!w.interaction.known_personal_dialogues)return {false,{},common_failure("MissingKnownPersonalDialogueDomain").error};r=section("known_personal_dialogues",[&](auto& c){return visit_known_personal_dialogues_checkpoint(c,*w.interaction.known_personal_dialogues);});if(!r.ok)return {false,{},r.error};}')
  return after(s,'else if(section.domain=="growth_tactic_lessons")','        else if(section.domain=="known_personal_dialogues"){i->known_personal_dialogues.emplace();r=visit_known_personal_dialogues_checkpoint(reader,*i->known_personal_dialogues);}')
 if path.endswith('/world_bundle.cpp'):
  s=after(INCLUDE+s,'auto payloads=born_personal_dialogue_fact_payloads(','    {auto payloads=known_personal_dialogue_fact_payloads(w.common,w.interaction.known_personal_dialogues?&*w.interaction.known_personal_dialogues:nullptr);if(!payloads.ok)return {false,{},payloads.error};for(const auto& row:boundary::object(payloads.value,"known_personal_dialogues")){auto r=fact(row.first,boundary::get_string(row.second,"kind",row.first),known_personal_dialogue_module);if(!r.ok)return r;}}')
  return after(s,'valid=validate_born_personal_dialogue_world(*this);','    valid=validate_known_personal_dialogue_world(*this);if(!valid.ok)return valid;')
 if path.endswith('/world_bundle.hpp'):return replace(s,'Integer tactic_lesson_from_tick=-1);','Integer tactic_lesson_from_tick=-1,const StableId& known_dialogue_source={});')
 if path.endswith('/resident_world.cpp'):
  s=replace(INCLUDE+s,'auto born_dialogue_source=born_personal_dialogue_new_result_world','auto known_dialogue_source=known_personal_dialogue_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!known_dialogue_source.ok)return {false,{},known_dialogue_source.error};auto born_dialogue_source=born_personal_dialogue_new_result_world')
  s=replace(s,'nullptr,current.common.tick());if(!completed.ok)','nullptr,current.common.tick(),known_dialogue_source.value);if(!completed.ok)')
  s=replace(s,'Integer tactic_lesson_from_tick){','Integer tactic_lesson_from_tick,const StableId& known_dialogue_source){')
  return after(s,'auto born_dialogue_bound=bind_born_personal_dialogue_result_world','    auto known_dialogue_bound=bind_known_personal_dialogue_result_world(w,known_dialogue_source,actor,sequence,*receipt);if(!known_dialogue_bound.ok)return known_dialogue_bound;')
 if path.endswith('/common_codec.cpp'):
  s=replace(s,'const bool born_dialogue=','const bool known_dialogue=kind=="known_personal_dialogue_opened";const bool born_dialogue=')
  s=replace(s,'std::string domain=','std::string domain=known_dialogue?"known_personal_dialogues":')
  s=replace(s,'if(born_dialogue&&named!=','if(known_dialogue&&named!="known_personal_dialogues")fail("InvalidFactDomain",row.first);if(born_dialogue&&named!=')
  return replace(s,'&&!(named=="born_personal_dialogues"','&&!(named=="known_personal_dialogues"&&known_dialogue)&&!(named=="born_personal_dialogues"')
 if path.endswith('/live_view_queries.cpp'):return after(INCLUDE+s,'if(query=="born_personal_dialogues"){','    if(query=="known_personal_dialogues"){boundary::fields(args,{},{"cursor"},"args");return query_known_personal_dialogue_world(world,args.find("cursor")?boundary::get_int(args,"cursor","args"):0);}')
 if path.endswith('/native_resident_backend.gd'):
  s=after(s,'expected["module_versions"].append({"name":"growth_tactic_lessons"','\tif definition.get("dynamic_world",{}).has("known_personal_dialogues"): expected["module_versions"].append({"name":"known_personal_dialogues","version":"1"})')
  return s+'\nfunc known_personal_dialogues_enabled() -> bool:\n\treturn _installed and _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("known_personal_dialogues",{}).get("version",0)==1\n'
 if path.endswith('/resident_page_requests.gd'):
  s=after(s,'const BornPersonalDialoguesPanel=','const KnownPersonalDialoguesPanel=preload("res://ui/known_personal_dialogues_panel.gd")')
  return after(s,'and not BornPersonalDialoguesPanel.prepare(host):','\tif host.kernel.has_method("known_personal_dialogues_enabled") and host.kernel.known_personal_dialogues_enabled() and not KnownPersonalDialoguesPanel.prepare(host): return false')
 if path.endswith('/screen_composer.gd'):
  s=after(s,'const BornPersonalDialoguesPanel=','const KnownPersonalDialoguesPanel=preload("res://ui/known_personal_dialogues_panel.gd")')
  lines=[x for x in s.splitlines() if 'BornPersonalDialoguesPanel.visible_here(m)' in x]
  if len(lines)!=2:raise ValueError('two actual scene sidebars required')
  for line in lines:s=replace(s,'\n'+line+'\n','\n'+line+'\n'+line[:len(line)-len(line.lstrip())]+'if m._resident_mode and m.kernel.has_method("known_personal_dialogues_enabled") and m.kernel.known_personal_dialogues_enabled() and KnownPersonalDialoguesPanel.visible_here(m): KnownPersonalDialoguesPanel.build(m,_sidebar_section(m,stage,"KnownPersonalDialoguesArea","当面私人交谈"))\n')
  return s
 if path.endswith('/content.schema.json'):
  d=json.loads(s);p=d['$defs']['dynamic_world']['properties'];assert MODULE not in p
  selector={'type':'object','additionalProperties':False,'required':['kind','source_id'],'properties':{'kind':{'type':'string','enum':['installed_actor','born_prototype']},'source_id':{'$ref':'#/$defs/reference_actors'}}}
  fields={'id':{'type':'string','minLength':1,'maxLength':256},'dialogue_id':{'$ref':'#/$defs/reference_dialogues'},'listener':selector,'provider':selector}
  p[MODULE]={'type':'object','additionalProperties':False,'required':['version','templates'],'properties':{'version':{'type':'integer','enum':[1]},'templates':{'type':'array','minItems':1,'maxItems':256,'items':{'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}}}};d['properties']['required_capabilities']['items']['enum'].append(CAP);return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
 if path.endswith('/rule_capabilities.json'):
  d=json.loads(s);d['capabilities'].append({'capability_id':CAP,'rule_version':MODULE+'/1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.known_personal_dialogues.version','equals':1}],'supported_semantics':[],'test_ids':['test_known_personal_dialogues_panel_guard'],'evidence':[],'limitations':['实际本人与眼前真实成年人的显式Installed/Birth模板；不读亲缘、代人同意或授控制权。完整World尚待。']});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
 if path=='tools/content/links.py':
  s=after(s,'from .born_personal_dialogue_links import','    from .known_personal_dialogue_links import check_known_personal_dialogues,known_personal_dialogue_bindings')
  s=replace(s,'| born_personal_dialogue_bindings(pack)','| born_personal_dialogue_bindings(pack) | known_personal_dialogue_bindings(pack)')
  return replace(s,' + check_born_personal_dialogues(pack,index)',' + check_born_personal_dialogues(pack,index) + check_known_personal_dialogues(pack,index)')
 if path=='tools/verify.py':return replace(s,"'test_born_personal_dialogues_panel_guard',","'test_born_personal_dialogues_panel_guard', 'test_known_personal_dialogues_panel_guard',")
 if path.endswith('/resident.cmake'):
  anchor='    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_world.cpp';assert s.count(anchor)==2;s=s.replace(anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+n+'.cpp' for n in CPP))
  anchor='    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp';s=replace(s,anchor,anchor+'\n'+'\n'.join('    '+ROOT.removeprefix('native/world/')+n+'.hpp' for n in HEADERS))
  return s+'\n# Real known-personal origins, original session, permanent actual result; runtime gates deferred.\nadd_executable(test_resident_known_personal_dialogue tests/resident/known_personal_dialogue_source_test.cpp)\ntarget_link_libraries(test_resident_known_personal_dialogue PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_known_personal_dialogue PRIVATE src)\ntarget_compile_options(test_resident_known_personal_dialogue PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_known_personal_dialogue COMMAND test_resident_known_personal_dialogue)\n'
 raise ValueError(path)
def main():
 p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 if a.output.exists():raise ValueError('fresh output required')
 rows=[];patch=[];files={};oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
 for path in SHARED:
  before=subprocess.check_output(['git','-C',str(a.repo),'show',BASE+':'+path]);data=transform(path,before.decode()).encode();files[path]=data;rows.append({'path':path,'base_oid':oid(before),'result_oid':oid(data)});patch+=difflib.unified_diff(before.decode().splitlines(True),data.decode().splitlines(True),fromfile='a/'+path,tofile='b/'+path)
 for path,data in files.items():target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
 (a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':BASE,'shared':rows,'module':MODULE+'/1','status':'SOURCE_ONLY_PENDING_SYNTAX','cold_order':'complete Life/Creation/Birth -> Known original opening/actual result/pure control fold -> later collateral consent source','old_visitors_unchanged':['Interaction','Task1','Task2','Born1','ReceiptData']},indent=2)+'\n')
if __name__=='__main__':main()
