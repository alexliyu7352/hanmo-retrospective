// 原Task真实accept/begin/finish与Common cache；S5来源和生命读明确夹具，未冒World注册或自然开店。
#define main original_shop_order_test_not_executed
#include "task_shop_order_test.cpp"
#undef main
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contacts.hpp"
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_receipts.hpp"
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_profile.hpp"
#include "resident/domains/commerce/private_shop_order_contact.hpp"
namespace {
struct ContactFixture:Fixture {
 ContactFixture(){common.state().metadata.ruleset_id="hanmo.native.local_society/1";common.state().identity.execution_epoch=1;}
 TaskShopOrderContactState contacts;
 Outcome<TaskEffect> finish(){
  const auto* current=common.action(action_id);need(current&&current->remaining_ticks==1,"real phase starts with one remaining delivery tick");
  ActionMutation step;step.action=*current;step.remaining_ticks=current->remaining_ticks-1;auto applied=common.apply_action(step);if(!applied.ok)return {false,{},applied.error};
  DynamicStepInput phase;phase.from_tick=common.tick();phase.to_tick=common.tick()+1;
  return finish_task_shop_order_delivery_in_candidate(common,tasks,orders,shop,interaction,dialogues,access,action_id,phase);
 }
 TaskShopOrderContactContext context(const DynamicStepInput* phase=nullptr){return {common,tasks,orders,contacts,dialogues,shop,phase,[this,phase](const StableId& actor,Integer tick,const StableId& fact){return read_actor_presence_before_fact(common,*definitions,{},actor,tick,fact,phase);},TaskShopOrderContactSubmitterMode::legacy_actor,{}};}
 StableId begin_contact(){auto quote=preview_task_shop_order_delivery(common,tasks,orders,shop,interaction,dialogues,access,"proprietor",task_id);need(quote.ok,"original provider SAY quote");auto started=begin_task_shop_order_delivery_in_candidate(common,tasks,orders,shop,interaction,dialogues,access,quote.value);need(started.ok,"original two-person Action starts");action_id=started.value;auto x=context();auto recorded=record_task_shop_order_contact_in_candidate(common,contacts,x,action_id,quote.value);need(recorded.ok,"start source immediately follows original action allocation");need(!contacts.starts.order().empty()&&contacts.starts.order().back()==recorded.value&&contacts.starts.find(recorded.value)&&contacts.starts.find(recorded.value)->fact_id==recorded.value,"actual start table key survives moving its source payload");return recorded.value;}
 StableId start_bound(Integer sequence=1){auto before=contacts;auto fact=begin_contact();ReceiptDetails details{ActionStarted{action_id},{}};auto fresh=task_shop_order_contact_new_result_source(&before,&contacts,"proprietor","task_shop_order_deliver",details,common.tick());if(!fresh.ok||fresh.value!=fact)std::cerr<<"contact result diff: code="<<(fresh.ok?StableId{}:fresh.error.original.code)<<" returned="<<fresh.value<<" expected="<<fact<<" table_key="<<contacts.starts.order().back()<<" payload_id="<<contacts.starts.rows().back().second.fact_id<<"\n";need(fresh.ok&&fresh.value==fact,"exact original new-source diff");need(!validate_task_shop_order_contacts(context()).ok,"before actual result cannot pass full cold");save_receipt("task_shop_order_deliver",sequence,ActionStarted{action_id});const auto& result=**common.state().results.find("proprietor:"+std::to_string(sequence));need(bind_task_shop_order_contact_result(common,contacts,fact,"proprietor",sequence,result).ok,"actual Common cache/history binds start");need(validate_task_shop_order_contacts(context()).ok,"active contact complete source");return fact;}
};
// 这些S5上下文只供实际成本前置纯读，未调用完整开店/市场史/成本写者。
struct UnusedContactClients final:TaskFulfilledClientRead {
 Outcome<TaskFulfilledClients> task_fulfilled_clients(const StableId&)const override{return {false,{},common_failure("UnexpectedContactClientRead").error};}
 Outcome<TaskFulfilledClients> task_fulfilled_clients_before_shop(const StableId&,const StableId&,Integer)const override{return {false,{},common_failure("UnexpectedContactClientRead").error};}
};
struct UnusedContactActivity final:ActivityRead {
 bool in_open_battle(const StableId&)const override{return false;}
 bool in_interaction(const StableId&,const StableId& ={})const override{return false;}
};
struct CostContactFixture:ContactFixture {
 FamilyCatalog family;LocalEconomyState economy;LocalEconomyCatalog economic_catalog;
 PrivateShopState shop_state;PrivateShopCatalog shop_catalog;UnusedContactClients clients;UnusedContactActivity activity;
 void accept_bound_order(){
  accept();const auto id=common.next_id("private_shop_order_bound");
  need(id.ok&&common.append_fact({id.value,"private_shop_order_bound","private_shops"}).ok,"S5 binding fixture allocated after actual Task acceptance");
  const auto& source=shop.source;PrivateShopOrderRecord order;order.id=source.terms.order_id;order.policy_id=source.policy_id;order.status=source.status;
  order.task_id=task_id;order.acceptance_fact_id=source.acceptance_fact_id;order.binding_fact_id=id.value;order.terms=source.terms;
  order.client_declaration=source.client_declaration;order.issued_tick=source.issued_tick;order.valid_through_tick=source.valid_through_tick;
  const auto order_id=order.id;need(shop_state.orders.orders.insert(order_id,std::move(order)),"S5 exact accepted order fixture");
 }
 Status cost_contact(const StableId& order_id,const DynamicStepInput* phase=nullptr){
  const auto* order=shop_state.orders.orders.find(order_id);if(!order)return common_failure("TestMissingOrder");
  TaskShopOrderExecutionAccess execution(common,tasks,orders,phase);auto done=execution.private_shop_order_execution(order->task_id);
  if(!done.ok)return {false,{},done.error};if(!done.value)return common_failure("TestMissingExecution");
  TaskShopOrderContactAccess contacts_read(common,orders,contacts,phase);
  PrivateShopContext x{common,*definitions,family,economy,economic_catalog,shop_state,shop_catalog,dialogues,clients,activity};
  x.phase=phase;x.order_contacts=&contacts_read;x.order_contacts_required=true;
  return validate_private_shop_order_delivery_contact(x,*order,*done.value);
 }
 void issue_second(){
  shop.earlier.emplace(shop.source.terms.order_id,shop.source);auto id=common.next_id("private_shop_order");
  need(id.ok&&common.append_fact({id.value,"private_shop_order_issued","private_shops"}).ok,"second S5 order fixture uses distinct real Common allocation");
  auto& source=shop.source;source.issue_fact_id=id.value;source.terms.order_id=id.value;source.status="pending_order";source.task_id.clear();source.acceptance_fact_id.clear();
  source.terms.accepted_tick=-1;source.issued_tick=common.tick();source.valid_through_tick=common.tick()+4;source.terms.quantity=2;
 }
};
void second_order_cost_contact(){
 CostContactFixture f;f.accept_bound_order();const auto first_order=f.shop.source.terms.order_id;f.start_bound();
 auto delivered=f.finish();need(delivered.ok&&delivered.value.success,"first order true price and stock transfer");
 DynamicStepInput phase;phase.from_tick=0;phase.to_tick=1;
 need(f.cost_contact(first_order,&phase).ok,"S5 cost prerequisite consumes actual Task execution before completion cache");
 need(!f.cost_contact(first_order).ok,"stable S5 cost prerequisite cannot read pre-clock completion");
 f.complete_phase();need(f.cost_contact(first_order).ok,"first completed cost prerequisite remains readable after release");
 f.issue_second();f.accept_bound_order();auto quote=preview_task_shop_order_delivery(f.common,f.tasks,f.orders,f.shop,f.interaction,f.dialogues,f.access,"proprietor",f.task_id);
 need(quote.ok,"second accepted order has original current provider SAY");
 auto started=begin_task_shop_order_delivery_in_candidate(f.common,f.tasks,f.orders,f.shop,f.interaction,f.dialogues,f.access,quote.value);
 need(started.ok,"second original Action starts");f.action_id=started.value;const auto next=f.common.state().next_instance_id;
 need(f.cost_contact(first_order).ok&&f.common.state().next_instance_id==next,"old successful cost source readable after new Action before new contact append, without ID allocation");
 auto before=f.contacts;auto x=f.context();auto contact=record_task_shop_order_contact_in_candidate(f.common,f.contacts,x,f.action_id,quote.value);
 need(contact.ok,"read-only old cost validation preserved Action/contact adjacency");
 need(!f.cost_contact(first_order).ok,"new unbound contact cannot pass full cost source validation early");
 f.save_receipt("task_shop_order_deliver",2,ActionStarted{f.action_id});const auto& actual=**f.common.state().results.find("proprietor:2");
 need(bind_task_shop_order_contact_result(f.common,f.contacts,contact.value,"proprietor",2,actual).ok,"second start binds its own actual cache result");
 need(f.cost_contact(first_order).ok,"old successful cost source readable after second actual result binding");
 need(validate_task_shop_order_contacts(f.context()).ok,"both contacts share original complete source validation");
 f.common.state().actions.find(f.action_id)->remaining_ticks=0;auto transient=f.cost_contact(first_order);need(!transient.ok&&transient.error.original.code=="InvalidTaskShopOrderAction","pre-clock zero-step Action is not a stable outer rollback boundary");
 f.common.state().actions.find(f.action_id)->remaining_ticks=quote.value.duration;need(f.cost_contact(first_order).ok,"outer savepoint begins at the real stable active Action boundary");f.common.state().next_instance_id=std::numeric_limits<Integer>::max()-2;
 const auto original=encode_common_state(f.common.state());const auto tasks=f.tasks;const auto orders=f.orders;const auto contacts=encode_task_shop_order_contacts(f.contacts);
 const auto save=f.common.begin_savepoint();need(save.ok,"original outer candidate opens for second late failure");
 auto failed=f.finish();need(!failed.ok&&failed.error.original.code=="InstanceIdOverflow"&&f.common.wallet("shop_cash")->balance==43,"actual second price and stock move before final settlement ID overflow");
 need(f.common.rollback_savepoint(save.value).ok,"original Common outer candidate rolls back late second delivery");f.tasks=tasks;f.orders=orders;
 need(compare_boundary(original,encode_common_state(f.common.state())).exact()&&compare_boundary(contacts,encode_task_shop_order_contacts(f.contacts)).exact(),"late failure restores asset IDs and keeps original bound contact history");
 need(f.cost_contact(first_order).ok,"first completed cost prerequisite survives second candidate rollback");
}
}
int main(){try{second_order_cost_contact();
 auto selected=std::vector<std::pair<StableId,StableId>>{{"task_shop_order_contacts","1"},{"task_shop_orders","1"},{"private_shops","1"},{"family_life","1"}};
 need(task_shop_order_contact_selected_version({}).ok&&task_shop_order_contact_selected_version({}).value==0,"legacy module identity remains unselected");
 need(task_shop_order_contact_selected_version(selected).ok&&task_shop_order_contact_selected_version(selected).value==1,"explicit original three-domain pair selects independent section");
 for(std::size_t i=1;i<selected.size();++i){auto missing=selected;missing.erase(missing.begin()+i);need(!task_shop_order_contact_selected_version(missing).ok,"each original source dependency is mandatory");}
 auto duplicate=selected;duplicate.push_back(selected.front());need(!task_shop_order_contact_selected_version(duplicate).ok,"duplicate contact module cannot select a second ledger");
 auto unsupported=selected;unsupported.front().second="2";need(!task_shop_order_contact_selected_version(unsupported).ok,"unknown contact version rejects");

 ContactFixture f;f.accept();auto contact=f.start_bound();auto controlled=f.context();controlled.submitter_mode=TaskShopOrderContactSubmitterMode::controlled_player;need(!validate_task_shop_order_contacts(controlled).ok,"controlled mode requires original historical player read");controlled.player_before=[](Integer,const StableId&)->Outcome<StableId>{return {true,"proprietor",{}};};need(validate_task_shop_order_contacts(controlled).ok,"controlled mode consumes exact original proprietor");controlled.player_before=[](Integer,const StableId&)->Outcome<StableId>{return {true,"outsider",{}};};need(!validate_task_shop_order_contacts(controlled).ok,"another historical player cannot authorize proprietor start");auto original=f.contacts;auto original_orders=encode_task_shop_orders(f.orders);auto encoded=encode_task_shop_order_contacts(f.contacts);auto restored=decode_task_shop_order_contacts(encoded);need(restored.ok&&compare_boundary(encoded,encode_task_shop_order_contacts(restored.value)).exact(),"independent contact JSON exact roundtrip");need(compare_boundary(original_orders,encode_task_shop_orders(f.orders)).exact(),"independent source serialization keeps original TaskShop1 bytes");CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);need(visit_task_shop_order_contacts_checkpoint(writer,f.contacts).ok&&writer.finish().ok,"standalone typed contact writes");TaskShopOrderContactState typed;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes));need(visit_task_shop_order_contacts_checkpoint(reader,typed).ok&&reader.finish().ok&&compare_boundary(encoded,encode_task_shop_order_contacts(typed)).exact(),"standalone typed contact reads");
 f.contacts.starts.find(contact)->client_actor_id="outsider";need(!validate_task_shop_order_contacts(f.context()).ok,"real unrelated actor cannot replace client");f.contacts=original;f.contacts.starts.find(contact)->session_id="interaction:1";need(!validate_task_shop_order_contacts(f.context()).ok,"active original session cannot be replaced");f.contacts=original;f.contacts.starts.find(contact)->node_id="other";need(!validate_task_shop_order_contacts(f.context()).ok,"another node cannot claim original client SAY");f.contacts=original;
 auto unrelated_fact=f.common.next_id("contact_test_fact");need(unrelated_fact.ok&&f.common.append_fact({unrelated_fact.value,"contact_test_fact","test"}).ok,"unrelated real Common fact allocated before delivery settlement");
 auto delivered=f.finish();need(delivered.ok&&delivered.value.success,"original actual two asset legs succeed");const auto settled=f.orders.settlements.order().back();DynamicStepInput phase;phase.from_tick=f.common.tick();phase.to_tick=f.common.tick()+1;need(validate_task_shop_order_contacts(f.context(&phase)).ok,"strict original phase validates complete contact before clock and release");TaskShopOrderContactAccess current(f.common,f.orders,f.contacts);need(!current.task_shop_order_delivery_contact(f.task_id,settled).ok,"future completion denied without actual phase");TaskShopOrderContactAccess projected(f.common,f.orders,f.contacts,&phase);auto proof=projected.task_shop_order_delivery_contact(f.task_id,settled);need(proof.ok&&proof.value&&proof.value->contact.fact_id==contact&&proof.value->completed_tick==phase.to_tick,"cost may consume exact delivery before completion result exists");need(f.common.wallet("shop_cash")->balance==23&&f.common.wallet("wallet:client")->balance==80,"contact creates no second money account");need(!projected.task_shop_order_delivery_contact("other-task",settled).ok,"exact settlement cannot be borrowed for another task");auto bad_phase=phase;++bad_phase.to_tick;TaskShopOrderContactAccess future(f.common,f.orders,f.contacts,&bad_phase);need(!future.task_shop_order_delivery_contact(f.task_id,settled).ok,"arbitrary phase projection rejects");f.complete_phase();need(validate_task_shop_order_contacts(f.context()).ok,"cold after real release and tick uses same source");
 auto saved=f.contacts;f.contacts.starts={};need(!validate_task_shop_order_contacts(f.context()).ok,"old successful settlement without original start not backfilled");f.contacts=saved;f.common.state().results={};f.common.state().history.clear();need(validate_task_shop_order_contacts(f.context()).ok,"all cache pruning preserves original contact");f.contacts.starts.find(contact)->actual_result->result_id="result:"+unrelated_fact.value.substr(unrelated_fact.value.find(':')+1);need(!validate_task_shop_order_contacts(f.context()).ok,"trimmed actual result cannot reuse another true Common fact instance");f.contacts=saved;f.contacts.starts.find(contact)->actual_result.reset();need(!validate_task_shop_order_contacts(f.context()).ok,"all cache pruning cannot erase permanent start result");f.contacts=saved;auto unknown=f.context();unknown.presence_before={};need(!validate_task_shop_order_contacts(unknown).ok,"actual historical life source is required");
 ContactFixture stopped;stopped.accept();auto stopped_contact=stopped.start_bound();stopped.common.state().inventories.find("shop_stock")->lots.clear();auto failed=stopped.finish();need(failed.ok&&!failed.value.success,"original missing goods genuinely interrupts action");DynamicStepInput failed_phase;failed_phase.from_tick=0;failed_phase.to_tick=1;need(validate_task_shop_order_contacts(stopped.context(&failed_phase)).ok,"interruption retains original contact and pending liability");stopped.complete_phase();need(validate_task_shop_order_contacts(stopped.context()).ok,"interrupted original source survives release");TaskShopOrderContactAccess no_delivery(stopped.common,stopped.orders,stopped.contacts);need(no_delivery.task_shop_order_delivery_contact(stopped.task_id,stopped.orders.interruptions.order().back()).ok&&!no_delivery.task_shop_order_delivery_contact(stopped.task_id,stopped.orders.interruptions.order().back()).value,"failed action is never successful delivery contact");
 ContactFixture overflow;overflow.accept();overflow.common.state().next_instance_id=std::numeric_limits<Integer>::max()-1;auto before=encode_common_state(overflow.common.state());auto orders_before=overflow.orders;auto save=overflow.common.begin_savepoint();need(save.ok,"real Common outer savepoint");auto quote=preview_task_shop_order_delivery(overflow.common,overflow.tasks,overflow.orders,overflow.shop,overflow.interaction,overflow.dialogues,overflow.access,"proprietor",overflow.task_id);need(quote.ok,"last ID original preview");auto action=begin_task_shop_order_delivery_in_candidate(overflow.common,overflow.tasks,overflow.orders,overflow.shop,overflow.interaction,overflow.dialogues,overflow.access,quote.value);need(action.ok,"original action allocated before contact overflow");auto x=overflow.context();auto missing=record_task_shop_order_contact_in_candidate(overflow.common,overflow.contacts,x,action.value,quote.value);need(!missing.ok,"contact last ID failure remains failure");need(overflow.common.rollback_savepoint(save.value).ok,"whole original action rollback");overflow.orders=orders_before;need(compare_boundary(before,encode_common_state(overflow.common.state())).exact()&&overflow.contacts.starts.empty(),"original Common and Task savepoints restore both occupations");
 std::cout<<checks<<" shop order contact source checks; World/author pending\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
