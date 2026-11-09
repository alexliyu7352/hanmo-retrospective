#include "repair_service_physical_internal.hpp"
#include "resident/domains/craft/craft_facility_occupancy.hpp"
namespace hanmo::resident {
SourceRef repair_custody_item_transfer_source(const StableId&id){return {"commerce","repair_custody_item_transfer",id};}
SourceRef repair_return_item_transfer_source(const StableId&id){return {"commerce","repair_return_item_transfer",id};}
}
namespace hanmo::resident::repair_physical_detail {
const RepairItemIndependentSourceRead&independent(const RepairPhysicalReadContext&x){
 const auto&a=x.agreements;const auto&c=x.commercial;
 repair_service_agreement_detail::context(a);context(c);
 if(&a.common!=&c.common||&a.items!=&c.items||&a.clock!=&c.clock||!c.repair||&c.repair->conditions!=&a.conditions)fail("RepairPhysicalContextMismatch");
 if(!x.independent)fail("RepairOriginalItemTitleSpatialHistoryRequired",{},ErrorKind::unsupported_capability);
 if(!x.independent->bound_to(a.common,a.items))fail("RepairPhysicalOriginalCandidateMismatch");return *x.independent;
}
const RepairJob*job(const RepairServiceReadContext&x,const RepairServiceContractSource&t){
 const auto&a=accepted_scope(x,t);const RepairJob*found=nullptr;
 for(const auto&r:x.items.repair_jobs.jobs.rows())if(same(r.second.terms.accepted_service_ref,{repair_accepted_source(a.id),1})){
  if(found||r.first!=r.second.id)fail("RepairAmbiguousOriginalJob",t.id);found=&r.second;
 }return found;
}
ReturnTerminal terminal(const RepairServiceReadContext&x,const RepairServiceContractSource&t){
 if(!x.repair)fail("RepairReturnOriginalS9ReaderRequired",t.id,ErrorKind::unsupported_capability);
 const auto*a=&accepted_scope(x,t);const auto*j=job(x,t);
 if(j){
  if(j->completed_ref&&j->interrupted_ref)fail("RepairConflictingOriginalTerminal",j->id);
  if(j->completed_ref){const MomentOwnerSource s{repair_completed_source(*j->completed_ref),1};auto actual=take(read_repair_completion_for_financial_evidence(*x.repair,s));if(actual.job!=j)fail("RepairReturnCompletionScopeMismatch",j->id);return {j,s,actual.completion->occurred_at};}
  if(j->interrupted_ref){const MomentOwnerSource s{repair_interrupted_source(*j->interrupted_ref),1};const auto*r=take(read_repair_interruption_source(*x.repair,s));if(r->job_id!=j->id)fail("RepairReturnInterruptionScopeMismatch",j->id);return {j,s,r->occurred_at};}
  fail("RepairReturnActualTerminalRequired",j->id,ErrorKind::rule);
 }
 const RepairUnstartedInterruptionSource*found=nullptr;
 for(const auto&r:x.items.repair_jobs.unstarted_interruptions.rows())if(same(r.second.accepted_service_ref,{repair_accepted_source(a->id),1})){
  if(found)fail("RepairAmbiguousUnstartedTerminal",a->id);found=take(read_repair_unstarted_interruption(*x.repair,{repair_unstarted_interrupted_source(r.first),1}));
 }
 if(!found)fail("RepairReturnActualTerminalRequired",a->id,ErrorKind::rule);
 return {nullptr,{repair_unstarted_interrupted_source(found->id),1},found->occurred_at};
}
void contact(const RepairPhysicalReadContext&x,const RepairServiceContractSource&t,const StableId&actor,const MomentOwnerSource&s,const WorldMoment&at,const SourceRef*cut,bool returning){
 const auto&expected=returning?t.terms.return_terms.receiver:t.terms.customer;
 if(expected.kind!=Owner::Kind::actor||expected.extra||t.terms.provider.kind!=Owner::Kind::actor||t.terms.provider.extra)fail("RepairRepresentedItemHandoverSourceRequired",t.id,ErrorKind::unsupported_capability);
 if(actor!=expected.id)fail("RepairActualItemCommandActorMismatch",t.id);
 const auto*c=take(read_face_to_face_contact_source(x.agreements.common,x.agreements.interactions,x.agreements.interaction_catalog,s.source_ref,s.source_schema));
 if(compare(c->occurred_at,at)>0||!((c->session_actor_id==actor&&c->provider_actor_id==t.terms.provider.id)||(c->provider_actor_id==actor&&c->session_actor_id==t.terms.provider.id)))fail("RepairActualItemContactMismatch",t.id);
 if(cut&&header(x.agreements.common,s.source_ref)>=header(x.agreements.common,*cut))fail("RepairItemContactOrderMismatch",t.id);
}
void transfer_shape(const RepairServiceReadContext&x,const RepairServiceContractSource&t,const OriginalUniqueItemTransferEffect&e,const CargoSelection&item,const ItemConditionHead&head,const WorldMoment&at,const SourceRef*cut){
 take(validate_original_unique_item_transfer_effect_shape(e));
 if(e.instance_id!=t.terms.instance_id||!owner(e.legal_owner,t.terms.customer)||item.inventory_id!=e.destination_inventory_id||item.lot_id!=e.lot_id||item.quantity!=1||head.instance_id!=e.instance_id||compare(at,t.accepted_at)<0||compare(at,now(x))>0)fail("RepairOriginalTransferScopeMismatch",t.id);
 if(!x.repair)fail("RepairItemConditionSourcesRequired",t.id,ErrorKind::unsupported_capability);
 const auto actual=cut?take(read_item_condition_before_common_source(x.repair->conditions,head,*cut,at)):take(read_item_condition_at_head(x.repair->conditions,head));
 if(!actual.original_instance||actual.original_instance->item_definition_id!=e.item_definition_id)fail("RepairTransferOriginalDefinitionMismatch",t.id);
 if(cut&&header(x.common,*cut)<=header(x.common,repair_service_contract_source(t.id)))fail("RepairTransferBeforeActualAcceptance",t.id);
}
}
namespace hanmo::resident {using namespace repair_physical_detail;
bool NativeRepairServicePhysicalOriginalRead::bound_to(const CommonStore&c,const ItemInstancesState&i)const noexcept{return &c==&x_.agreements.common&&&i==&x_.agreements.items&&&c==&x_.commercial.common&&&i==&x_.commercial.items;}
Status NativeRepairServicePhysicalOriginalRead::validate_original_repair_custody(const RepairServiceContractSource&t,const RepairCustodyReceivedSource&r)const{try{
 const auto&sources=independent(x_);const auto actual=MomentOwnerSource{repair_custody_received_source(r.id),1};const auto&c=x_.commercial;
 if(c.items.repair_service_contracts.custody_receipts.find(r.id)!=&r||!same(r.contract_ref,{repair_service_contract_source(t.id),1})||r.actual_transfer.destination_inventory_id!=t.terms.custody_terms.inventory_id||r.original_resource_effect_refs.size()!=1||!same(r.original_resource_effect_refs.front(),{repair_custody_item_transfer_source(r.id),1}))fail("RepairCustodyOriginalSourceMismatch",r.id);
 transfer_shape(c,t,r.actual_transfer,r.actual_item_selection,r.actual_condition_head,r.occurred_at,&actual.source_ref);
 contact(x_,t,r.actual_actor_id,r.actual_contact_source,r.occurred_at,&actual.source_ref,false);
 repair_service_agreement_detail::command(x_.agreements,r.actual_actor_id,"deliver_repair_item",actual,r.actual_handover_source,r.occurred_at);
 if(!repair_service_agreement_detail::same_route(r.actor_route,repair_service_agreement_detail::route(x_.agreements,r.actual_actor_id,actual,r.actual_handover_source,r.occurred_at)))fail("RepairCustodyActorRouteMismatch",r.id);
 const auto*j=job(c,t);if(j&&header(c.common,repair_job_source(j->id))<=header(c.common,actual.source_ref))fail("RepairDeliveryAfterActualStart",r.id);
 for(const auto&v:c.items.repair_jobs.unstarted_interruptions.rows())if(same(v.second.accepted_service_ref,{repair_accepted_source(accepted_scope(c,t).id),1})&&header(c.common,repair_unstarted_interrupted_source(v.first))<=header(c.common,actual.source_ref))fail("RepairDeliveryAfterUnstartedTerminal",r.id);
 take(sources.validate_original_repair_item_physical_scope(t,r.actual_transfer,r.actual_actor_id,r.actual_contact_source,r.occurred_at,&actual.source_ref,false));return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status NativeRepairServicePhysicalOriginalRead::validate_original_repair_return(const RepairServiceContractSource&t,const RepairItemReturnedSource&r)const{try{
 const auto&sources=independent(x_);const auto actual=MomentOwnerSource{repair_item_returned_source(r.id),1};const auto&c=x_.commercial;
 if(c.items.repair_service_contracts.returns.find(r.id)!=&r||!same(r.contract_ref,{repair_service_contract_source(t.id),1})||r.actual_transfer.source_inventory_id!=t.terms.custody_terms.inventory_id||r.original_resource_effect_refs.size()!=1||!same(r.original_resource_effect_refs.front(),{repair_return_item_transfer_source(r.id),1})||!same(r.actual_receiver_authority,t.terms.return_terms.receiver_authority))fail("RepairReturnOriginalSourceMismatch",r.id);
 transfer_shape(c,t,r.actual_transfer,r.actual_item_selection,r.actual_condition_head,r.occurred_at,&actual.source_ref);
 contact(x_,t,r.actual_actor_id,r.actual_contact_source,r.occurred_at,&actual.source_ref,true);
 repair_service_agreement_detail::command(x_.agreements,r.actual_actor_id,"receive_repair_item",actual,r.actual_receiver_acceptance,r.occurred_at);
 if(!repair_service_agreement_detail::same_route(r.actor_route,repair_service_agreement_detail::route(x_.agreements,r.actual_actor_id,actual,r.actual_receiver_acceptance,r.occurred_at)))fail("RepairReturnActorRouteMismatch",r.id);
 const auto end=terminal(c,t);if(compare(end.at,r.occurred_at)>0||header(c.common,end.source.source_ref)>=header(c.common,actual.source_ref))fail("RepairReturnBeforeActualTerminal",r.id);
 if(end.job){
  if(!c.repair->occupancy)fail("RepairReturnOriginalClearanceRequired",r.id,ErrorKind::unsupported_capability);
  const CraftFacilityOccupancyEvent*last=nullptr;for(const auto&id:end.job->facility_occupancy_event_refs){const auto*event=take(read_craft_facility_occupancy(*c.repair->occupancy,craft_facility_occupancy_source(id),1)).original_event;if(!event)fail("RepairReturnOriginalOccupancyMissing",id);if(header(c.common,craft_facility_occupancy_source(id))<header(c.common,actual.source_ref))last=event;}
  if(!last||last->occupancy_kind!=CraftFacilityOccupancyKind::cleared)fail("RepairReturnActualClearanceRequired",r.id,ErrorKind::unsupported_capability);
 }
 const RepairCustodyReceivedSource*held=nullptr;for(const auto&v:c.items.repair_service_contracts.custody_receipts.rows())if(same(v.second.contract_ref,r.contract_ref)){if(held)fail("RepairAmbiguousHeldItem",r.id);held=take(read_repair_custody_received(c,{repair_custody_received_source(v.first),1}));}
 if(!held||held->actual_transfer.lot_id!=r.actual_transfer.lot_id||header(c.common,repair_custody_received_source(held->id))>=header(c.common,actual.source_ref))fail("RepairOriginalHeldItemRequired",r.id);
 const auto destination=take(sources.read_original_repair_return_destination(t,r.actual_transfer.destination_inventory_id,r.occurred_at,&actual.source_ref));if(!same(destination,r.actual_destination_source))fail("RepairReturnDestinationSourceMismatch",r.id);
 take(sources.validate_original_repair_item_physical_scope(t,r.actual_transfer,r.actual_actor_id,r.actual_contact_source,r.occurred_at,&actual.source_ref,true));return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<std::vector<MomentOwnerSource>>NativeRepairServicePhysicalOriginalRead::read_original_repair_terminal_continuity(const RepairServiceContractSource&t,const MomentOwnerSource&actual)const{try{
 independent(x_);const auto&c=x_.commercial;terminal_moment(c,t,actual);const auto cut=header(c.common,actual.source_ref);std::vector<MomentOwnerSource>out;
 const RepairCustodyReceivedSource*held=nullptr;for(const auto&v:c.items.repair_service_contracts.custody_receipts.rows())if(same(v.second.contract_ref,{repair_service_contract_source(t.id),1})&&header(c.common,repair_custody_received_source(v.first))<cut){if(held)fail("RepairAmbiguousTerminalCustody",t.id);held=take(read_repair_custody_received(c,{repair_custody_received_source(v.first),1}));}
 if(held){bool returned=false;for(const auto&v:c.items.repair_service_contracts.returns.rows())if(same(v.second.contract_ref,held->contract_ref)&&header(c.common,repair_item_returned_source(v.first))<cut){take(read_repair_item_returned(c,{repair_item_returned_source(v.first),1}));returned=true;}if(!returned)out.push_back({repair_custody_received_source(held->id),1});}
 return {true,std::move(out),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
