#include "repair_detail.hpp"
#include "repair_service_sources.hpp"
#include "craft_facility_occupancy.hpp"
#include "resident/domains/commerce/repair_service/repair_service.hpp"
#include <limits>
namespace hanmo::resident {using namespace repair_detail;
namespace {
bool selection(const CargoSelection&a,const CargoSelection&b){return a.inventory_id==b.inventory_id&&a.lot_id==b.lot_id&&a.quantity==b.quantity;}
void completed_materials(const RepairReadContext&x,const RepairJob&j,const RepairCompletedSource&r){
 material_history(x,j,true);if(r.consumed_materials.size()!=j.material_reservations.size())fail("RepairConsumedMaterialCoverageMismatch",r.id);
 for(std::size_t n=0;n<r.consumed_materials.size();++n){const auto&reserved=j.material_reservations[n];if(!selection(r.consumed_materials[n],reserved.held))fail("RepairConsumedMaterialSourceMismatch",r.id);if(reserved.properties_ref){const auto*properties=x.properties.properties.find(reserved.properties_ref->properties_id);const auto*definition=properties?x.item_catalog.property_policies.find(properties->property_policy_id):nullptr;if(!properties||!definition||properties->properties_id!=reserved.properties_ref->properties_id||properties->definition_id!=reserved.definition_id||properties->produced_tick>r.occurred_at.half_day_tick||(properties->expires_at_tick&&r.occurred_at.half_day_tick>=*properties->expires_at_tick)||definition->version!=properties->property_policy_version)fail("RepairConsumedPropertyHistoryMismatch",r.id);}}
}
void erase_action(CommonStore&c,const RepairJob&j){const auto*action=c.action(j.action_id);const auto*actor=c.actor(j.terms.worker_id);if(!action||action->actor_id!=j.terms.worker_id||action->participants!=std::vector<StableId>{j.terms.worker_id}||!actor||actor->action_id!=j.action_id)fail("RepairTerminalActionBindingMismatch",j.id);ActorMutation free;free.actor_id=j.terms.worker_id;free.action_id=StableId{};take(c.apply_actor(free));ActionMutation end;end.kind=ActionMutation::Kind::erase;end.action.id=j.action_id;take(c.apply_action(end));}
}
Outcome<RepairCompletionFinancialEvidence> read_repair_completion_for_financial_evidence(const RepairReadContext&x,const MomentOwnerSource&actual){try{
 context(x);const auto&source=actual.source_ref;if(actual.source_schema!=1||!same_source_ref(source,repair_completed_source(source.id)))fail("UnsupportedRepairCompletionSource",source.id,ErrorKind::unsupported_capability);const auto*r=x.items.repair_jobs.completions.find(source.id);if(!r||r->id!=source.id)fail("RepairCompletedSourceRequired",source.id);const auto&j=*take(read_original_repair_job(x,r->job_id));
 if(j.completed_ref!=std::optional<StableId>{r->id}||j.interrupted_ref||j.phase!=RepairJobPhase::completed||header(x.common,source)<=header(x.common,repair_job_source(j.id))||compare(r->occurred_at,j.started_at)<0||compare(r->occurred_at,now(x))>0)fail("RepairCompletedOriginalMismatch",r->id);
 const auto*e=find_fixed_elapsed(x.elapsed,j.action_id);if(!e||compare(e->last_settled_at,r->occurred_at)||take(compare_fractions(e->credited_units.value,e->required_units.value,x.arithmetic_budget)))fail("RepairCompletedServiceRequired",r->id);
 validate_elapsed_history(x,j,*e,transitions(x,j));completed_materials(x,j,*r);
 // This prefix deliberately stops before this completion's condition result.
 // The full item-condition validator independently consumes the returned source.
 const StableId*predecessor=nullptr;bool own_result=false;
 for(const auto&condition:x.items.condition_results){if(condition.instance_id!=j.terms.instance_id)continue;if(condition.source_kind==ItemConditionSourceKind::repair_completed&&same_source_ref(condition.source_fact_ref,source)){if(own_result)fail("DuplicateRepairConditionResult",r->id);own_result=true;continue;}if(!own_result)predecessor=&condition.result_id;}
 // Exact prior condition prefix also makes original-source recursion finite:
 // a completion can never cite itself or a later condition as its adopted head.
 if(j.terms.condition_head.last_condition_result_ref.has_value()!=bool(predecessor)||(predecessor&&*j.terms.condition_head.last_condition_result_ref!=*predecessor))fail("RepairConditionAdoptionPredecessorMismatch",r->id);
 take(read_item_condition_at_head(x.conditions,j.terms.condition_head));
 return {true,{&j,r},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<ActualItemRepairCompletion> read_actual_repair_completed(const RepairReadContext&x,const SourceRef&source,const StableId&instance){try{
 const auto evidence=take(read_repair_completion_for_financial_evidence(x,{source,1}));const auto&j=*evidence.job;const auto*r=evidence.completion;
 if(j.terms.instance_id!=instance)fail("RepairCompletedInstanceMismatch",instance);
 refs(r->actual_fee_sources,r->id);if(!x.commercial_sources)fail("RepairOriginalCommercialReaderRequired",r->id,ErrorKind::unsupported_capability);take(validate_original_repair_completion_fee(*x.commercial_sources,j,*r));take(x.original.validate_repair_completion_fees(j,*r));
 const auto before=take(read_item_condition_at_head(x.conditions,j.terms.condition_head));const auto&p=policy(x,j.terms);const auto target=take(read_repair_condition_target(p,*before.original_instance,before.max_durability));
 // Preserve the original first-occurrence union and physical order. The
 // temporary tracked buffer owns only borrowed SourceRef components.
 CheckpointVector<SourceRefBuildView> effects;
 if(r->actual_fee_sources.size()==std::numeric_limits<std::size_t>::max())throw std::bad_alloc();
 effects.reserve(r->actual_fee_sources.size()+(!r->consumed_materials.empty()?1:0));
 if(!r->consumed_materials.empty())effects.push_back({source.domain,source.kind,source.id});
 for(const auto&fee:r->actual_fee_sources)if(std::none_of(effects.begin(),effects.end(),[&](const auto&ref){return ref.domain==fee.source_ref.domain&&ref.kind==fee.source_ref.kind&&ref.id==fee.source_ref.id;}))effects.push_back({fee.source_ref.domain,fee.source_ref.kind,fee.source_ref.id});
 const ActualItemRepairCompletionBuildView fields{{source.domain,source.kind,source.id},{"crafting","repair_job",j.id},instance,p.policy_id,before.instance_version,target.durability,target.max_durability,r->occurred_at,{effects}};
 return {true,ActualItemRepairCompletion(fields),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<StableId> complete_repair_in_candidate(CommonStore&c,ItemInstancesState&i,ActionElapsedStore&e,const RepairWriteContext&x,const StableId&id){try{
 writable(c,i,e,x);auto*j=i.repair_jobs.jobs.find(id);if(!j)fail("RepairJobRequired",id);
 if(j->completed_ref){take(read_actual_repair_completed(x.read,repair_completed_source(*j->completed_ref),j->terms.instance_id));const auto*done=i.repair_jobs.completions.find(*j->completed_ref);if(!done->condition_result_ref)fail("RepairConditionResultRequired",id);take(read_item_condition_result(x.read.conditions,*done->condition_result_ref));return {true,*j->completed_ref,{}};}
 take(read_original_repair_job(x.read,id));const auto at=now(x.read);auto*elapsed=find_fixed_elapsed(e,j->action_id);if(!elapsed||j->phase!=RepairJobPhase::working||!std::holds_alternative<ElapsedRunning>(elapsed->timing)||compare(elapsed->last_settled_at,at))fail("RepairActualCompletionBoundaryRequired",id);
 take(validate_fixed_elapsed_at_completion_due(*elapsed,at,x.read.scale,x.read.arithmetic_budget));if(compare(std::get<ElapsedRunning>(elapsed->timing).completion_due_at,at)||take(compare_fractions(elapsed->credited_units.value,elapsed->required_units.value,x.read.arithmetic_budget)))fail("RepairServiceNotCompleted",id);
 const auto history=transitions(x.read,*j);validate_elapsed_history(x.read,*j,*elapsed,history);material_history(x.read,*j,false);
 const auto head_owner=take(read_current_item_condition_head(x.read.conditions,j->terms.instance_id));const auto&head=head_owner.value();if(!same_head(head,j->terms.condition_head))fail("RepairItemChangedDuringService",id);
 const auto&item=lot(c,j->terms.original_item);if(item.quantity!=1||item.item_instance_id!=std::optional<StableId>{j->terms.instance_id}||item.reserved_by!=j->terms.service_holder||!owner(item.owner,j->terms.legal_owner))fail("RepairCompletionCustodyMismatch",id);
 RepairCompletedSource done;done.id=take(c.next_id("repair_completed"));done.job_id=id;done.occurred_at=at;
 for(const auto&reserved:j->material_reservations){const auto&actual=lot(c,reserved.held);take(check_item_material_use(i,x.read.properties,x.read.item_catalog,actual,{reserved.definition_id,{},{},false,false},at.half_day_tick));const auto&draft=apply(c,ConsumeSelectedLots{reserved.held.inventory_id,{reserved.held},{reserved.legal_owner},j->terms.service_holder},reserved.held.inventory_id);if(draft.lot_flows.size()!=1||draft.lot_flows.front().source_lot_id!=reserved.held.lot_id||draft.lot_flows.front().quantity!=reserved.held.quantity)fail("RepairConsumptionFlowMismatch",id);done.consumed_materials.push_back(reserved.held);}
 const auto completed=done.id;take(c.append_fact({completed,"repair_completed","crafting"}));if(!i.repair_jobs.completions.insert(completed,std::move(done)))fail("DuplicateRepairCompletion",completed);auto*stored=i.repair_jobs.completions.find(completed);
 // The actual full-service/material result is already owned before its fee
 // trigger consumes it. Any fee or later failure discards this whole candidate.
 j->completed_ref=completed;j->phase=RepairJobPhase::completed;
 take(read_repair_completion_for_financial_evidence(x.read,{repair_completed_source(completed),1}));
 stored->actual_fee_sources=take(consume_original_repair_completion_fee_in_candidate(c,i,*x.original_economy,*x.read.commercial_sources,*j,*stored));
 take(read_actual_repair_completed(x.read,repair_completed_source(completed),j->terms.instance_id));
 auto stamp=take(x.stamps.capture({{ResourceKey::Kind::inventory,j->terms.original_item.inventory_id}},{{"craft","item_instance",j->terms.instance_id}},{completed}));const auto*condition=take(apply_item_repair_completed_in_candidate(c,i,x.read.conditions,{repair_completed_source(completed),j->terms.instance_id,head.instance_version,*stamp},x.stamp_authority,x.versions));stored->condition_result_ref=condition->result_id;
 if(!x.read.occupancy||!x.facility_occupancy_owner)fail("RepairOccupancyProviderRequired",id,ErrorKind::unsupported_capability);take(craft_occupancy_detail::clear_completed_repair_in_candidate(c,*x.facility_occupancy_owner,i,*x.read.occupancy,completed));
 take(x.effects.apply_repair_terminal_facility_disposition_in_candidate(*j,repair_completed_source(completed),at));
 FixedServiceTransition terminal;terminal.transition_id=completed;terminal.action_ref=j->action_id;terminal.transition_ordinal=add(history.back().transition_ordinal,1);terminal.previous_transition_ref=history.back().transition_id;terminal.kind=FixedTransitionKind::terminal;terminal.occurred_at=at;terminal.credited_at=elapsed->credited_units;terminal.execution_source_ref=repair_job_source(id);terminal.execution_source_schema=1;terminal.cause=FixedOwnerTerminated{repair_completed_source(completed),1};terminal.previous_input_basis_refs=history.back().input_basis_refs;terminal.terminal_disposition=FixedTerminalDisposition::completed;
 NativeRepairActionTimeRead original(x.read);auto prepared=take(prepare_fixed_service_change(e,j->action_id,terminal,{},repair_action_time_registration(),x.read.scale,original,x.read.arithmetic_budget));take(apply_fixed_service_change_in_candidate(e,std::move(prepared),repair_action_time_registration(),x.read.scale,original,x.read.arithmetic_budget));erase_action(c,*j);
 // Custody and original unique lot remain in the accepted service holder until
 // the separate real S7 return or settlement writer performs that handover.
 return {true,completed,{}};
}catch(Failure&error){return {false,{},std::move(error.error)};}}
}
