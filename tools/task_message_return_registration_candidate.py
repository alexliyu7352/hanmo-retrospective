"""Q03完整候选只读取固定公共Git对象；原Task唯一writer与独立returns尾同步注册。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
from task_extension_registration_candidate import replace,after
from task_message_return_task_hooks_candidate import transform as task_hooks,BASE,MODEL,RULES
ROOT='native/world/src/resident/domains/tasks/message_returns/'
INCLUDE='#include "resident/domains/tasks/message_returns/task_message_return_world.hpp"\n#include "resident/domains/tasks/message_returns/task_message_return_boundary.hpp"\n'
SHARED=[MODEL,RULES,'native/world/include/hanmo/resident/model_contract.hpp','native/world/src/resident/kernel/command_types.hpp','native/world/src/resident/kernel/dispatcher.cpp','native/world/src/resident/checkpoint/command_boundary.cpp','native/world/src/resident/kernel/phase_plan.cpp','native/world/src/resident/kernel/dynamic_integration.cpp','native/world/src/resident/kernel/world_bundle.hpp','native/world/src/resident/kernel/resident_world.cpp','native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/storage/common_codec.cpp','native/world/src/resident/kernel/live_view_queries.cpp','game/ui/native_resident_backend.gd','game/ui/resident_page_requests.gd','game/ui/screen_composer.gd','schemas/content.schema.json','game/data/rule_capabilities.json','tools/content/links.py','tools/verify.py','native/world/resident.cmake']
def transform(path,s):
 if path in [MODEL,RULES]:return task_hooks(path,s)
 if path.endswith('/model_contract.hpp'):
  s=replace(s,'struct TaskMessageNoticeCatalog;','struct TaskMessageReturnCatalog;struct TaskMessageNoticeCatalog;')
  return after(s,'std::shared_ptr<const TaskMessageNoticeCatalog> task_message_notices;','    std::shared_ptr<const TaskMessageReturnCatalog> task_message_returns;')
 if path.endswith('/command_types.hpp'):
  s=INCLUDE+s
  s=replace(s,'OpenKnownPersonalDialogue,PoliticalOfficeReceptionCommand>;','OpenKnownPersonalDialogue,PoliticalOfficeReceptionCommand,TaskMessageReturnCollect>;')
  return replace(s,'KnownPersonalDialogueQuote,PoliticalOfficeReceptionQuote>;','KnownPersonalDialogueQuote,PoliticalOfficeReceptionQuote,TaskMessageReturnQuote>;')
 if path.endswith('/dispatcher.cpp'):
  s=INCLUDE+s
  s=after(s,'Prepared prepare_command(const WorldBundle& w,','    const auto* message_return=std::get_if<TaskMessageReturnCollect>(&command.data);if((command.kind==task_message_return_collect_command)!=(message_return!=nullptr))return reject("InvalidTaskMessageReturnCommand");')
  s=after(s,'if(inquiry){auto quote=prepare_contact_inquiry_world','    if(message_return){auto quote=preview_task_message_return_world(w,id,*message_return);if(!quote.ok)return {false,{},quote.error};return ready(w,id,command,std::move(quote.value));}')
  s=replace(s,'auto q=preview_task(w.common,w.tasks,','auto returns=TaskMessageReturnWorldAccess::create(w);if(!returns.ok)return {false,{},returns.error};auto q=preview_task(w.common,w.tasks,')
  s=replace(s,'takeover.value->tasks(),notice.value?&*notice.value:nullptr);if(!q.ok)','takeover.value->tasks(),notice.value?&*notice.value:nullptr,returns.value?&returns.value->services():nullptr);if(!q.ok)')
  line=next(x for x in s.splitlines() if 'else if(auto* task=std::get_if<TaskQuote>(&q.data))status=start_task_action' in x)
  s=replace(s,line,line.replace(')status=start_task_action','){auto returns=TaskMessageReturnWorldAccess::create(w);if(!returns.ok)return {false,{},returns.error};status=start_task_action').replace('takeover.value->tasks());','takeover.value->tasks(),returns.value?&returns.value->services():nullptr);}'))
  s=after(s,'const auto* inquiry=std::get_if<ContactInquiryRequest>(&op.command.data);','    const auto* message_return=std::get_if<TaskMessageReturnCollect>(&op.command.data);if((op.command.kind==task_message_return_collect_command)!=(message_return!=nullptr)||(message_return&&!std::holds_alternative<TaskMessageReturnQuote>(op.quote.data)))return failed<ReceiptDetails>(common_failure("InvalidTaskMessageReturnCommand"));')
  return after(s,'if(inquiry){auto result=commit_contact_inquiry_world','    if(message_return){auto result=commit_task_message_return_world(w,id,*message_return,std::get<TaskMessageReturnQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{ActionStarted{result.value},{}},{}};}')
 if path.endswith('/command_boundary.cpp'):
  s=INCLUDE+s;s=after(s,'if(kind==contact_inquiry_action_kind){auto decoded=','    if(kind==task_message_return_collect_command){auto decoded=decode_task_message_return_collect(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}')
  s=after(s,'else if constexpr(std::is_same_v<T,ContactInquiryQuote>)return','                else if constexpr(std::is_same_v<T,TaskMessageReturnQuote>)return encode_task_message_return_quote(quote);')
  return after(s,'if(r.kind=="task_personal_advance"&&','        if(r.kind==task_message_return_collect_command&&(!r.details||!std::holds_alternative<ActionStarted>(r.details->data)||std::get<ActionStarted>(r.details->data).action_id.empty()))failed("InvalidTaskMessageReturnReceipt","receipt/details",ErrorKind::rule);')
 if path.endswith('/phase_plan.cpp'):
  s='#include "resident/model/procurement_timing.hpp"\n'+INCLUDE+s;s=after(s,'Status try_npc_command(WorldBundle& w,','    {auto allowed=task_message_return_npc_command_allowed(w,command.kind,std::get_if<TaskCommand>(&command.data));if(!allowed.ok)return allowed;}')
  s=replace(s,'const bool task_write=','const bool task_write=std::holds_alternative<TaskMessageReturnCollect>(command.data)||')
  s=replace(s,'                }else if(action.kind=="accept_offer"||action.kind=="report_offer"||action.kind=="deliver_message"){','''                }else if(action.kind==task_message_return_collect_command){
                    if(action.remaining_ticks!=task_handover_ticks){r=common_failure("TaskMessageReturnActiveActionRequired",id);break;}ActionMutation mutation;mutation.action=action;mutation.remaining_ticks=0;r=w.common.apply_action(mutation);if(!r.ok)break;auto end=finish_task_message_return_world(w,id,dynamic_input);if(!end.ok){r={false,{},end.error};break;}outcome.task=std::move(end.value);completed=true;
                }else if(action.kind=="accept_offer"||action.kind=="report_offer"||action.kind=="deliver_message"){''')
  s=replace(s,'auto end=finish_task_action(w.common,w.tasks,','auto returns=TaskMessageReturnWorldAccess::create(w,&dynamic_input);if(!returns.ok){r={false,{},returns.error};break;}auto end=finish_task_action(w.common,w.tasks,')
  return replace(s,'takeover.value->tasks(),notice.value?&*notice.value:nullptr);if(!end.ok)','takeover.value->tasks(),notice.value?&*notice.value:nullptr,returns.value?&returns.value->services():nullptr);if(!end.ok)')
 if path.endswith('/dynamic_integration.cpp'):
  return after(INCLUDE+s,'Status adopt_director_proposal(WorldBundle& w,','    if(proposal.command){auto allowed=task_message_return_npc_command_allowed(w,proposal.command->kind,std::get_if<TaskCommand>(&proposal.command->data));if(!allowed.ok)return allowed;}')
 if path.endswith('/world_bundle.hpp'):
  return replace(INCLUDE+s,'const CollateralControlState* collateral_before=nullptr);','const CollateralControlState* collateral_before=nullptr,const TaskMessageReturnResultBatch& task_return_sources={});')
 if path.endswith('/resident_world.cpp'):
  s=INCLUDE+s;s=replace(s,'auto shop_order_contact_source=task_shop_order_contact_new_result_world','auto task_return_sources=task_message_return_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!task_return_sources.ok)return {false,{},task_return_sources.error};auto shop_order_contact_source=task_shop_order_contact_new_result_world')
  s=replace(s,'current.family_collateral_legacy_enabled()?&current.family_collateral_legacy:nullptr);if(!completed.ok)','current.family_collateral_legacy_enabled()?&current.family_collateral_legacy:nullptr,task_return_sources.value);if(!completed.ok)')
  s=replace(s,'const CollateralControlState* collateral_before){','const CollateralControlState* collateral_before,const TaskMessageReturnResultBatch& task_return_sources){')
  return after(s,'auto contact_bound=bind_task_shop_order_contact_result_world','    auto returns_bound=bind_task_message_return_result_world(w,task_return_sources,actor,sequence,*receipt);if(!returns_bound.ok)return returns_bound;')
 if path.endswith('/world_bundle.cpp'):
  s=INCLUDE+s;s=after(s,'auto payloads=task_shop_order_contact_fact_payloads(', '    {auto payloads=task_message_return_fact_payloads(w.common,w.tasks.message_returns?&*w.tasks.message_returns:nullptr);if(!payloads.ok)return {false,{},payloads.error};for(const auto& row:boundary::object(payloads.value,"task_message_returns")){auto r=fact(row.first,boundary::get_string(row.second,"kind",row.first),task_message_return_module);if(!r.ok)return r;}}')
  s=after(s,'auto payloads=contact_inquiry_action_payloads(', '    {auto payloads=task_message_return_action_payloads(w.common,w.tasks.message_returns?&*w.tasks.message_returns:nullptr);if(!payloads.ok)return {false,{},payloads.error};for(const auto& row:boundary::object(payloads.value,"task_message_return_actions")){auto r=action(row.first,task_message_return_collect_command);if(!r.ok)return r;}}')
  s=after(s,'valid=validate_task_extension_world(*this);','    valid=validate_task_message_return_layout(common,catalog->identity.module_versions,tasks.message_returns?&*tasks.message_returns:nullptr);if(!valid.ok)return valid;')
  # 原未选择路径相同；新完整人物/控制和回书全史先于私铺履约资格、继产及Plan消费者。
  s=s.replace('!catalog->task_shop_order_contacts_enabled','(!catalog->task_shop_order_contacts_enabled&&!catalog->task_message_returns)')
  s=s.replace('if(catalog->task_shop_order_contacts_enabled){','if(catalog->task_shop_order_contacts_enabled||catalog->task_message_returns){')
  return after(s,'valid=validate_family_collateral_world(*this);','    valid=validate_task_message_return_world(*this);if(!valid.ok)return valid;')
 if path.endswith('/world_codec.cpp'):
  from task_message_return_codec_candidate import transform_codec
  return transform_codec(s)
 if path.endswith('/native_initial_state.cpp'):
  s=INCLUDE+s;s=after(s,'if(const auto* config=scenario.find("family_collateral_legacy")){','    auto message_returns=decode_task_message_return_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!message_returns.ok)return {false,{},message_returns.error};if(message_returns.value)admitted.module_versions.emplace_back(task_message_return_module,"1");auto return_modules=task_message_return_selected_version(admitted.module_versions);if(!return_modules.ok)return {false,{},return_modules.error};')
  return replace(s,'if(mutable_catalog->task_extensions)tasks->deadline_extensions.emplace();','if(mutable_catalog->task_extensions)tasks->deadline_extensions.emplace();if(mutable_catalog->task_message_returns)tasks->message_returns.emplace();')
 if path.endswith('/common_codec.cpp'):
  s=replace(s,'const bool shop_contact=','const bool message_return=kind=="task_message_return_started"||kind=="task_message_return_collected"||kind=="task_message_return_interrupted"||kind=="task_message_return_report_started"||kind=="task_message_return_report_interrupted"||kind=="task_message_return_delivered";const bool shop_contact=')
  s=replace(s,'std::string domain=','std::string domain=message_return?"task_message_returns":')
  s=replace(s,'if(shop_contact&&named!=','if(message_return&&named!="task_message_returns")fail("InvalidFactDomain",row.first);if(shop_contact&&named!=')
  return replace(s,'&&!(named=="task_shop_order_contacts"','&&!(named=="task_message_returns"&&message_return)&&!(named=="task_shop_order_contacts"')
 if path.endswith('/live_view_queries.cpp'):return after(INCLUDE+s,'if(query=="task_extensions"){','    if(query=="task_message_returns"){boundary::fields(args,{},{"cursor"},"args");return query_task_message_returns_world(world,args.find("cursor")?boundary::get_int(args,"cursor","args"):0);}')
 if path.endswith('/native_resident_backend.gd'):return after(s,'expected["module_versions"].append({"name":"family_collateral_legacy"','\tif definition.has("task_message_returns"): expected["module_versions"].append({"name":"task_message_returns","version":"1"})')
 if path.endswith('/resident_page_requests.gd'):
  return after(after(s,'const TaskExtensionPanel=','const TaskMessageReturnsPanel=preload("res://ui/task_message_returns_panel.gd")'),'if not TaskExtensionPanel.prepare(host):','\tif not TaskMessageReturnsPanel.prepare(host): return false')
 if path.endswith('/screen_composer.gd'):
  s=after(s,'const TaskExtensionPanel=','const TaskMessageReturnsPanel=preload("res://ui/task_message_returns_panel.gd")');lines=[x for x in s.splitlines() if 'TaskExtensionPanel.visible_here(m)' in x];assert len(lines)==2
  for line in lines:s=replace(s,'\n'+line+'\n','\n'+line+'\n'+line[:len(line)-len(line.lstrip())]+'if m._resident_mode and TaskMessageReturnsPanel.visible_here(m): TaskMessageReturnsPanel.build(m,_sidebar_section(m,stage,"TaskMessageReturnsArea","原任务回书"))\n')
  return s
 if path.endswith('/content.schema.json'):
  d=json.loads(s);fields={'id':{'type':'string','minLength':1,'maxLength':256},'offer_id':{'$ref':'#/$defs/reference_offers'},'reply_item_id':{'$ref':'#/$defs/reference_items'},'recipient_role_id':{'$ref':'#/$defs/reference_roles'},'dialogue_id':{'$ref':'#/$defs/reference_dialogues'},'node_id':{'type':'string','minLength':1,'maxLength':256}}
  d['$defs']['scenario']['properties']['task_message_returns']={'type':'object','additionalProperties':False,'required':['version','profile','policies'],'properties':{'version':{'type':'integer','enum':[1]},'profile':{'type':'string','enum':['private_round_trip_letter_v1']},'policies':{'type':'array','minItems':1,'maxItems':4096,'items':{'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}}}};d['properties']['required_capabilities']['items']['enum'].append('task_message_returns_v1');return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
 if path.endswith('/rule_capabilities.json'):
  d=json.loads(s);d['capabilities'].append({'capability_id':'task_message_returns_v1','rule_version':'task_message_returns/1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.task_message_returns.profile','equals':'private_round_trip_letter_v1'}],'supported_semantics':[],'test_ids':['test_task_message_returns_catalog','test_task_message_returns_panel_guard','test_task_message_return_task_hooks','test_task_message_returns_registration'],'evidence':[],'limitations':['Q03仅原私人静态送信；真实收件人托回、原报告同recipe交付，旧段字节不变。完整World行为待验。']});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
 if path=='tools/content/links.py':return replace(after(s,'from .task_extension_links import','    from .task_message_return_links import check_task_message_returns'),' + check_task_extensions(pack,index)',' + check_task_extensions(pack,index) + check_task_message_returns(pack,index)')
 if path=='tools/verify.py':return replace(s,"'test_task_extension_panel_guard',","'test_task_extension_panel_guard', 'test_task_message_returns_catalog', 'test_task_message_returns_panel_guard', 'test_task_message_return_task_hooks', 'test_task_message_returns_registration',")
 if path.endswith('/resident.cmake'):
  cpp=sorted(p.name for p in Path(ROOT).glob('*.cpp'));headers=sorted(p.name for p in Path(ROOT).glob('*.hpp'));anchor='    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_control.cpp';assert s.count(anchor)==2;s=s.replace(anchor,anchor+'\n'+'\n'.join('    src/resident/domains/tasks/message_returns/'+p for p in cpp));s=after(s,'src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_control.hpp','\n'.join('    src/resident/domains/tasks/message_returns/'+p for p in headers))
  for name in ['catalog','codec','receipts','profile','transaction']:
   target='test_resident_task_message_return_'+name;s+='\nadd_executable('+target+' tests/resident/task_message_return_'+name+'_test.cpp)\ntarget_link_libraries('+target+' PRIVATE hanmo_resident)\ntarget_include_directories('+target+' PRIVATE src)\ntarget_compile_options('+target+' PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_task_message_return_'+name+' COMMAND '+target+')\n'
  return s
 raise ValueError(path)
def main():
 p=argparse.ArgumentParser();p.add_argument('--base',default=BASE);p.add_argument('--output',required=True);a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=True);rows=[];patch=[]
 for path in SHARED:
  before=subprocess.check_output(['git','show',a.base+':'+path]).decode();changed=transform(path,before);dest=out/'candidate'/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(changed);oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest();rows.append({'path':path,'base_oid':oid(before.encode()),'result_oid':oid(changed.encode())});patch.extend(difflib.unified_diff(before.splitlines(True),changed.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
 (out/'candidate.patch').write_text(''.join(patch));(out/'manifest.json').write_text(json.dumps({'base':a.base,'status':'SOURCE_ONLY_UNVERIFIED','shared':rows},indent=2)+'\n')
if __name__=='__main__':main()
