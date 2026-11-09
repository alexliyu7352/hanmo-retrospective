// 永久提交对应先核原全史，再逐真实cache核原业务；history裁剪不把首项下界冒成0。
#include "private_shop_command_result_sources.hpp"
#include "resident/domains/commerce/private_shop_payload.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_state.hpp"
#include "resident/domains/commerce/liquidation/private_shop_liquidation.hpp"
#include <map>
#include <set>
namespace hanmo::resident {namespace {
std::optional<Integer> serial(const CommonStore& c,const StableId& id){auto at=id.rfind(':');if(at==StableId::npos)return {};auto n=organization_instance_sequence(id,id.substr(0,at));return n&&*n<c.state().next_instance_id?n:std::nullopt;}
// 实际advance_world仅这些原入口可推进一次半日；确认对白只可沿原duel_start内嵌该时钟。
bool clock_writer(const StableId& kind){return kind=="advance"||kind=="wait"||kind=="travel_wait"||kind=="duel_start"||kind=="encounter_fight"||kind=="dialogue_confirm";}
StableId control_writer(const StableId& kind){if(kind=="family_succession")return "family_retire";if(kind=="family_voluntary_succession")return "family_continue_as";if(kind=="family_legacy_control_activated")return "family_legacy_activate";return {};}
bool related_instant(const StableId& kind){return kind=="task_shop_order_accept"||kind=="task_shop_order_cancel"||kind=="task_shop_order_estate_cancel"||kind=="private_shop_estate_close";}
Status writer_groups(const PrivateShopState& original,const PrivateShopCommandResultsState& state){
 std::map<StableId,Integer> controls,instant_sources;std::set<StableId> shops;
 for(const auto& row:state.bindings.rows()){const auto& b=row.second;shops.insert(b.result_id);if(clock_writer(b.result_kind)){if(b.from_tick==INT64_MAX||b.result_tick!=b.from_tick+1)return common_failure("PrivateShopCommandClockWriterMismatch",row.first);}
  else if(private_shop_command_kind(b.result_kind)||related_instant(b.result_kind)){if(b.from_tick!=b.result_tick||b.source_tick!=b.result_tick)return common_failure("PrivateShopCommandInstantWriterMismatch",row.first);}
  else return common_failure("PrivateShopCommandWriterUnavailable",row.first);
  if(related_instant(b.result_kind)){if(++instant_sources[b.result_id]!=1)return common_failure("PrivateShopRelatedWriterSourceMismatch",row.first);
   if(b.result_kind=="task_shop_order_accept"){const auto* bound=original.orders.bindings.find(row.first);const auto* order=bound?original.orders.orders.find(bound->order_id):nullptr;if(!bound||!order||order->terms.proprietor_actor_id!=b.submitter_actor_id)return common_failure("PrivateShopRelatedWriterSourceMismatch",row.first);}
   else if(b.result_kind=="private_shop_estate_close"){if(!original.closings.find(row.first))return common_failure("PrivateShopRelatedWriterSourceMismatch",row.first);}
   else{const auto* settled=original.order_settlements.find(row.first);const auto* order=settled?original.orders.orders.find(settled->order_id):nullptr;if(!settled||!order||settled->outcome!="cancelled"||(b.result_kind=="task_shop_order_cancel"&&order->terms.proprietor_actor_id!=b.submitter_actor_id))return common_failure("PrivateShopRelatedWriterSourceMismatch",row.first);}
  }
 }
 for(const auto& row:state.control_bindings.rows()){const auto& b=row.second;const auto kind=control_writer(b.source_kind);if(kind.empty()||b.result_kind!=kind||b.from_tick!=b.source_tick||b.source_tick!=b.result_tick||++controls[b.result_id]!=1||shops.count(b.result_id))return common_failure("PrivateShopControlWriterMismatch",row.first);}
 return common_success();
}
bool same_commit(const PrivateShopCommandResultBinding& a,const PrivateShopCommandResultBinding& b){return a.submitter_actor_id==b.submitter_actor_id&&a.result_id==b.result_id&&a.result_kind==b.result_kind&&a.from_tick==b.from_tick&&a.result_tick==b.result_tick&&a.allocation_begin==b.allocation_begin&&a.request_seq==b.request_seq&&a.revision==b.revision;}
// 原直接命令只认其确切原业务主事实，补货保原Trade Action回执并转查真实入库事实。
Outcome<StableId> direct_source(const PrivateShopState& s,const CommitResult& r){
 auto fail=[&]()->Outcome<StableId>{return {false,{},common_failure("PrivateShopBoundReceiptMismatch",r.result_id).error};};
 if(!r.ok||!r.details)return fail();StableId id;const auto* effect=std::get_if<InteractionEffect>(&r.details->data);const auto* start=std::get_if<ActionStarted>(&r.details->data);
 if(r.kind=="private_shop_operate"){if(!start||!s.operations.find(start->action_id))return fail();id=start->action_id;}
 else if(r.kind=="private_shop_restock"){if(!start)return fail();for(const auto& p:s.purchases.rows())if(p.second.trade_fact_id==start->action_id){if(!id.empty())return fail();id=p.first;}}
 else{if(!effect||!effect->encounter_id.empty()||effect->party_id||effect->person_id||effect->news_ids)return fail();id=effect->settlement_result;bool found=false;
  if(r.kind=="private_shop_open")found=s.shops.find(id);
  else if(r.kind=="private_shop_fund")found=s.capital.find(id);
  else if(r.kind=="private_shop_set_markup")found=s.markups.find(id);
  else if(r.kind=="private_shop_cancel_operation"){const auto* v=s.operating_results.find(id);found=v&&!v->completed&&v->reason=="cancelled";}
  else if(r.kind=="private_shop_employ"||r.kind=="private_shop_staff_accept"){const auto* v=s.staff.contracts.find(id);found=v&&v->accepted_by_worker==(r.kind=="private_shop_staff_accept");}
  else if(r.kind=="private_shop_staff_decide"){const auto* v=s.staff.decisions.find(id);found=v&&(v->decision=="leave"||v->decision=="dismiss");}
  else if(r.kind=="private_shop_request_order")found=s.orders.orders.find(id);
  else if(r.kind=="private_shop_close")found=s.closings.find(id);
  if(!found)return fail();
 }
 return id.empty()?fail():Outcome<StableId>{true,id,{}};
}
// 直接玩家命令还须是该原主事实中的真正本人；NPC附带事实不走这个主回执选择。
StableId direct_actor(const PrivateShopState& s,const StableId& id){
 if(const auto* v=s.shops.find(id))return v->terms.proprietor_actor_id;
 if(const auto* v=s.capital.find(id))return v->actor_id;
 if(const auto* v=s.markups.find(id))return v->actor_id;
 if(const auto* v=s.purchases.find(id)){const auto* shop=s.shops.find(v->shop_id);return shop?shop->terms.proprietor_actor_id:StableId{};}
 if(const auto* v=s.operations.find(id))return v->actor_id;
 if(const auto* v=s.operating_results.find(id))return v->actor_id;
 if(const auto* v=s.staff.contracts.find(id))return v->accepted_by_worker?v->terms.worker_actor_id:v->terms.employer_actor_id;
 if(const auto* v=s.staff.decisions.find(id))return v->actor_id;
 if(const auto* v=s.orders.orders.find(id))return v->terms.proprietor_actor_id;
 if(const auto* v=s.closings.find(id))return v->actor_id;
 return {};
}
// 缓存全裁剪后直接命令仍须恰一原主事实及真正业务本人，永久直接命令不得只凭自存种类/本人串互相认证。
Status direct_group(const PrivateShopState& shops,const PrivateShopCommandResultsState& state,const PrivateShopCommandResultBinding& group){
 if(!private_shop_command_kind(group.result_kind))return common_success();StableId primary;
 for(const auto& row:state.bindings.rows()){const auto& b=row.second;if(b.result_id!=group.result_id)continue;const auto& id=b.id;bool found=false;
  if(group.result_kind=="private_shop_open")found=shops.shops.find(id);
  else if(group.result_kind=="private_shop_fund")found=shops.capital.find(id);
  else if(group.result_kind=="private_shop_set_markup")found=shops.markups.find(id);
  else if(group.result_kind=="private_shop_restock")found=shops.purchases.find(id);
  else if(group.result_kind=="private_shop_operate")found=shops.operations.find(id);
  else if(group.result_kind=="private_shop_cancel_operation"){const auto* v=shops.operating_results.find(id);found=v&&!v->completed&&v->reason=="cancelled";}
  else if(group.result_kind=="private_shop_employ"||group.result_kind=="private_shop_staff_accept"){const auto* v=shops.staff.contracts.find(id);found=v&&v->accepted_by_worker==(group.result_kind=="private_shop_staff_accept");}
  else if(group.result_kind=="private_shop_staff_decide"){const auto* v=shops.staff.decisions.find(id);found=v&&(v->decision=="leave"||v->decision=="dismiss");}
  else if(group.result_kind=="private_shop_request_order")found=shops.orders.orders.find(id);
  else if(group.result_kind=="private_shop_close")found=shops.closings.find(id);
  if(found){if(!primary.empty()||direct_actor(shops,id)!=group.submitter_actor_id||b.source_tick!=group.result_tick)return common_failure("PrivateShopBoundCommandActorMismatch",id);primary=id;}
 }
 if(primary.empty())return common_failure("PrivateShopBoundCommandSourceMissing",group.id);
 for(const auto& row:state.bindings.rows()){if(row.second.result_id!=group.result_id||row.first==primary)continue;bool allowed=false;
  if(group.result_kind=="private_shop_cancel_operation"){const auto* day=shops.operating_results.find(primary);const auto* wage=shops.staff.services.find(row.first);allowed=day&&wage&&wage->operation_id==day->operation_id&&wage->closing_fraction;}
  else if(group.result_kind=="private_shop_staff_decide"){const auto* decision=shops.staff.decisions.find(primary);if(const auto* day=shops.operating_results.find(row.first)){const auto* operation=shops.operations.find(day->operation_id);allowed=decision&&operation&&operation->staff_contract_id==decision->contract_id&&!day->completed;}if(const auto* wage=shops.staff.services.find(row.first))allowed=decision&&wage->contract_id==decision->contract_id&&wage->closing_fraction;}
  if(!allowed)return common_failure("PrivateShopUnexpectedInstantDerivedSource",row.first);
 }
 return common_success();
}
// 一个原提交可含多个NPC派生业务，但所有来源只能归这个真实cache key/result。
Status match(const PrivateShopCommandResultsState& state,const StableId& id,const StableId& key,const CommitResult& r){
 const auto* b=state.bindings.find(id);if(!b||key!=b->submitter_actor_id+":"+std::to_string(b->request_seq)||b->result_id!=r.result_id||b->result_kind!=r.kind||b->revision!=r.revision||b->result_tick!=r.tick)return common_failure("PrivateShopBoundCacheMismatch",id);return common_success();
}
}
Status validate_private_shop_command_results(const CommonStore& c,const PrivateShopState& shops,const PrivateShopCommandResultsState* state,const FamilyState* family,const FamilyLegacyState* legacy,ProjectionWriter* projection){
 if(!state)return common_success();if(state->version!=1)return common_failure("UnsupportedPrivateShopCommandResultsVersion");auto sources=read_private_shop_command_fact_sources(c,shops,projection);if(!sources.ok)return {false,{},sources.error};if(sources.value.size()!=state->bindings.size())return common_failure("PrivateShopCommandResultCoverageMismatch");
 auto controls=read_private_shop_command_control_sources(c,family,legacy);if(!controls.ok)return {false,{},controls.error};if(controls.value.size()!=state->control_bindings.size())return common_failure("PrivateShopCommandControlCoverageMismatch");
 std::set<Integer> actual_facts;for(const auto& h:c.state().facts.rows()){auto n=serial(c,h.first);if(n)actual_facts.insert(*n);}
 std::map<Integer,const PrivateShopCommandResultBinding*> groups;std::map<std::pair<StableId,Integer>,StableId> requests;
 auto collect=[&](const auto& values,const auto& table)->Status{for(const auto& source:values){const PrivateShopCommandResultBinding* b=table.find(source.id);auto id=serial(c,source.id);auto result=b?organization_instance_sequence(b->result_id,"result"):std::nullopt;const auto* sequence=b?c.state().request_sequences.find(b->submitter_actor_id):nullptr;
  if(!b||b->id!=source.id||b->source_kind!=source.kind||b->source_tick!=source.tick||!id||!result||*result>=c.state().next_instance_id||!c.actor(b->submitter_actor_id)||!sequence||b->request_seq<1||b->request_seq>2147483647||b->request_seq>*sequence||b->revision<1||b->revision>c.revision()||b->result_kind.empty()||actual_facts.count(*result)||b->from_tick<0||b->from_tick>b->source_tick||b->source_tick>b->result_tick||b->result_tick>c.tick()||b->allocation_begin<1||b->allocation_begin>*id||*id>=*result)return common_failure("InvalidPrivateShopCommandResultBinding",source.id);
  const auto g=groups.emplace(*result,b);if(!g.second&&!same_commit(*g.first->second,*b))return common_failure("PrivateShopCommandResultGroupMismatch",source.id);const auto q=requests.emplace(std::make_pair(b->submitter_actor_id,b->request_seq),b->result_id);if(!q.second&&q.first->second!=b->result_id)return common_failure("PrivateShopCommandResultRequestReused",source.id);
 }return common_success();};auto collected=collect(sources.value,state->bindings);if(!collected.ok)return collected;collected=collect(controls.value,state->control_bindings);if(!collected.ok)return collected;auto writers=writer_groups(shops,*state);if(!writers.ok)return writers;
 Integer prior_result=0,prior_revision=0,prior_tick=0;std::map<StableId,Integer> prior_request;
 for(const auto& group:groups){const auto& b=*group.second;auto direct=direct_group(shops,*state,b);if(!direct.ok)return direct;if(b.allocation_begin<=prior_result||b.revision<=prior_revision||b.from_tick<prior_tick||b.request_seq<=prior_request[b.submitter_actor_id])return common_failure("PrivateShopCommandResultOrderMismatch",b.id);prior_result=group.first;prior_revision=b.revision;prior_tick=b.result_tick;prior_request[b.submitter_actor_id]=b.request_seq;
  bool found=false;for(const auto& h:c.state().history){auto n=organization_instance_sequence(h.result_id,"result");if(!n)return common_failure("PrivateShopBoundHistoryMismatch",h.result_id);if(h.result_id==b.result_id){if(found||h.actor_id!=b.submitter_actor_id||h.kind!=b.result_kind||h.tick!=b.result_tick)return common_failure("PrivateShopBoundHistoryMismatch",b.id);found=true;}else if(*n>=b.allocation_begin&&*n<group.first)return common_failure("PrivateShopCommandResultCrossedCommit",b.id);}
  const auto key=b.submitter_actor_id+":"+std::to_string(b.request_seq);const auto* cached=c.state().results.find(key);if(cached&&(!*cached||!(*cached)->ok||(*cached)->result_id!=b.result_id||(*cached)->kind!=b.result_kind||(*cached)->tick!=b.result_tick||(*cached)->revision!=b.revision))return common_failure("PrivateShopBoundCacheMismatch",b.id);
  for(const auto& row:c.state().results.rows())if(row.second&&row.second->result_id==b.result_id&&row.first!=key)return common_failure("PrivateShopBoundCacheMismatch",b.id);
  if(cached&&*cached&&state->control_bindings.find(b.id)){const auto* effect=(*cached)->details?std::get_if<FamilyEffect>(&(*cached)->details->data):nullptr;if(!effect||effect->fact_id!=b.id)return common_failure("PrivateShopControlResultSourceMismatch",b.id);}
 }
 for(const auto& row:c.state().results.rows())if(row.second){const auto& r=*row.second;if(private_shop_command_kind(r.kind)){auto source=direct_source(shops,r);if(!source.ok)return {false,{},source.error};auto valid=match(*state,source.value,row.first,r);if(!valid.ok)return valid;const auto* binding=state->bindings.find(source.value);if(!binding||direct_actor(shops,source.value)!=binding->submitter_actor_id||binding->source_tick!=r.tick)return common_failure("PrivateShopBoundCommandActorMismatch",source.value);}
  const auto* advance=r.details?std::get_if<AdvanceDetails>(&r.details->data):nullptr;if(!advance)continue;for(const auto& done:advance->completed_actions.rows()){const auto* operation=shops.operations.find(done.first);if(!operation)continue;const auto* day=shops.operating_results.find(operation->result_fact_id);if(!day||(r.kind!="advance"&&r.kind!="wait"&&r.kind!="travel_wait"))return common_failure("PrivateShopBoundCompletionMismatch",done.first);auto valid=match(*state,day->id,row.first,r);if(!valid.ok)return valid;}
 }
 return common_success();
}
// 真Task/清算全史之后的原来源交叉；不把自动到期改叫本人取消，也不复制原执行权账。
Status validate_private_shop_command_related_sources(const PrivateShopState& shops,const PrivateShopCommandResultsState* state,const TaskShopOrderState* tasks,const PrivateShopLiquidationState* estate){
 if(!state)return common_success();for(const auto& row:state->bindings.rows()){const auto& b=row.second;if(!related_instant(b.result_kind))continue;
  auto bad=[&](){return common_failure("PrivateShopRelatedWriterSourceMismatch",row.first);};
  if(b.result_kind=="private_shop_estate_close"){const auto* closed=shops.closings.find(row.first);if(!closed||!estate)return bad();const PrivateShopEstateClosingRecord* source=nullptr;for(const auto& record:estate->closings.rows())if(record.second.closing_fact_id==row.first){if(source)return bad();source=&record.second;}const auto* bound=source?estate->result_bindings.find(source->id):nullptr;if(!source||!bound||source->executor_actor_id!=b.submitter_actor_id||source->tick!=b.source_tick||bound->actor_id!=b.submitter_actor_id||bound->command_kind!=b.result_kind||bound->result_id!=b.result_id||bound->request_seq!=b.request_seq||bound->revision!=b.revision||bound->tick!=b.result_tick)return bad();continue;}
  if(!tasks)return bad();
  if(b.result_kind=="task_shop_order_accept"){const auto* bound=shops.orders.bindings.find(row.first);const auto* accepted=bound?tasks->bindings.find(bound->acceptance_fact_id):nullptr;if(!bound||!accepted||accepted->order_id!=bound->order_id||accepted->task_id!=bound->task_id||accepted->terms.proprietor_actor_id!=b.submitter_actor_id||accepted->terms.accepted_tick!=b.source_tick)return bad();continue;}
  const auto* cost=shops.order_settlements.find(row.first);const auto* settled=cost?tasks->settlements.find(cost->receipt_fact_id):nullptr;if(!cost||!settled||settled->task_id!=cost->task_id||settled->order_id!=cost->order_id||settled->tick!=b.source_tick||settled->outcome!="cancelled"||settled->paid_price||settled->refunded_price)return bad();
  if(b.result_kind=="task_shop_order_cancel"){if(settled->termination!="cancelled_by_proprietor")return bad();continue;}
  if(settled->termination!="cancelled_by_estate_executor"||!tasks->estate_cancellations)return bad();const TaskShopOrderEstateCancellation* source=nullptr;for(const auto& record:tasks->estate_cancellations->cancellations.rows())if(record.second.settlement_fact_id==settled->fact_id){if(source)return bad();source=&record.second;}if(!source||!source->player_receipt||source->request.executor_actor_id!=b.submitter_actor_id||source->tick!=b.source_tick||source->player_receipt->result_id!=b.result_id||source->player_receipt->request_sequence!=b.request_seq||source->player_receipt->result_revision!=b.revision)return bad();
 }return common_success();
}

}
