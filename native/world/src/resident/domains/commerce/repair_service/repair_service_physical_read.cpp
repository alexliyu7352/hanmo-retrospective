#include "repair_service_physical_internal.hpp"
#include "resident/domains/craft/craft_facility_occupancy.hpp"
namespace hanmo::resident {using namespace repair_service_detail;
Outcome<const RepairCustodyReceivedSource*>read_repair_custody_received(const RepairServiceReadContext&x,const MomentOwnerSource&s){try{
 context(x);if(s.source_schema!=1||!same_source_ref(s.source_ref,repair_custody_received_source(s.source_ref.id)))fail("UnsupportedRepairCustodySource",s.source_ref.id,ErrorKind::unsupported_capability);
 const auto*r=state(x).custody_receipts.find(s.source_ref.id);if(!r||r->id!=s.source_ref.id)fail("RepairOriginalCustodyRequired",s.source_ref.id);header(x.common,s.source_ref);const auto&t=contract(x,r->contract_ref);accepted_scope(x,t);take(x.original.validate_original_repair_custody(t,*r));return {true,r,{}};
}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Outcome<const RepairItemReturnedSource*>read_repair_item_returned(const RepairServiceReadContext&x,const MomentOwnerSource&s){try{
 context(x);if(s.source_schema!=1||!same_source_ref(s.source_ref,repair_item_returned_source(s.source_ref.id)))fail("UnsupportedRepairReturnSource",s.source_ref.id,ErrorKind::unsupported_capability);
 const auto*r=state(x).returns.find(s.source_ref.id);if(!r||r->id!=s.source_ref.id)fail("RepairOriginalReturnRequired",s.source_ref.id);header(x.common,s.source_ref);const auto&t=contract(x,r->contract_ref);accepted_scope(x,t);take(x.original.validate_original_repair_return(t,*r));return {true,r,{}};
}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Outcome<const OriginalUniqueItemTransferEffect*>read_repair_original_item_transfer(const RepairServiceReadContext&x,const MomentOwnerSource&s){try{
 if(s.source_schema!=1||s.source_ref.domain!="commerce")fail("UnsupportedRepairOwnedTransferSource",s.source_ref.id,ErrorKind::unsupported_capability);
 if(same_source_ref(s.source_ref,repair_custody_item_transfer_source(s.source_ref.id)))return {true,&take(read_repair_custody_received(x,{repair_custody_received_source(s.source_ref.id),1}))->actual_transfer,{}};
 if(same_source_ref(s.source_ref,repair_return_item_transfer_source(s.source_ref.id)))return {true,&take(read_repair_item_returned(x,{repair_item_returned_source(s.source_ref.id),1}))->actual_transfer,{}};
 fail("UnsupportedRepairOwnedTransferSource",s.source_ref.id,ErrorKind::unsupported_capability);
}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Status validate_repair_service_physical_sources(const RepairServiceReadContext&x){try{
 const auto&s=state(x);
 for(const auto&r:s.custody_receipts.rows()){
  if(r.first!=r.second.id)fail("RepairCustodyKeyMismatch",r.first);const auto*v=take(read_repair_custody_received(x,{repair_custody_received_source(r.first),1}));
  for(const auto&other:s.custody_receipts.rows())if(other.first!=r.first&&(same(other.second.contract_ref,v->contract_ref)||same(other.second.actual_handover_source,v->actual_handover_source)))fail("DuplicateRepairActualDelivery",r.first);
 }
 for(const auto&r:s.returns.rows()){
  if(r.first!=r.second.id)fail("RepairReturnKeyMismatch",r.first);const auto*v=take(read_repair_item_returned(x,{repair_item_returned_source(r.first),1}));
  for(const auto&other:s.returns.rows())if(other.first!=r.first&&(same(other.second.contract_ref,v->contract_ref)||same(other.second.actual_receiver_acceptance,v->actual_receiver_acceptance)))fail("DuplicateRepairActualReturn",r.first);
  const auto&t=contract(x,v->contract_ref);if(const auto*j=repair_physical_detail::job(x,t))if(j->returned_ref!=std::optional<StableId>{r.first})fail("RepairReturnS9AdoptionRequired",r.first);
 }
 for(const auto&r:x.items.repair_jobs.jobs.rows()){
  const auto&j=r.second;const auto*held=take(read_repair_custody_received(x,j.terms.custody_source_ref));
  if(!same(held->contract_ref,j.terms.fee_terms_ref)||held->actual_transfer.instance_id!=j.terms.instance_id||held->actual_item_selection.inventory_id!=j.terms.original_item.inventory_id||held->actual_item_selection.lot_id!=j.terms.original_item.lot_id)fail("RepairJobActualCustodyMismatch",r.first);
  if(j.returned_ref){const auto*back=take(read_repair_item_returned(x,{repair_item_returned_source(*j.returned_ref),1}));if(!same(back->contract_ref,held->contract_ref))fail("RepairJobReturnReverseMismatch",r.first);}
 }
 for(const auto&r:x.common.state().results.rows())if(r.second&&r.second->ok&&(r.second->kind=="deliver_repair_item"||r.second->kind=="receive_repair_item")){
  const auto*p=r.second->details?repair_service_command_receipt(*r.second->details):nullptr;if(!p)fail("RepairSealedItemCommandPayloadRequired",r.second->result_id);const MomentOwnerSource seal{{"common","command_result",r.second->result_id},1};
  if(r.second->kind=="deliver_repair_item"){const auto*v=take(read_repair_custody_received(x,p->actual_source));if(!same(v->actual_handover_source,seal))fail("RepairDeliveryCommandReverseMismatch",r.second->result_id);}
  else{const auto*v=take(read_repair_item_returned(x,p->actual_source));if(!same(v->actual_receiver_acceptance,seal))fail("RepairReturnCommandReverseMismatch",r.second->result_id);}
 }
 // Owned-field transfer identities have no separate Common headers.
 for(const auto&r:x.common.state().facts.rows())if(r.second.domain=="commerce"&&(r.second.kind=="repair_custody_item_transfer"||r.second.kind=="repair_return_item_transfer"))fail("RepairOwnedTransferCannotBeCommonHeader",r.first);
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status visit_repair_service_physical_pins(const RepairPhysicalReadContext&x,const MomentOwnerSource&s,RepairServiceAgreementPinSink&sink){try{
 if(same_source_ref(s.source_ref,repair_custody_received_source(s.source_ref.id))){
  const auto*r=take(read_repair_custody_received(x.commercial,s));take(sink.source(r->contract_ref));take(sink.source(r->actual_handover_source));take(sink.source(r->actual_contact_source));take(sink.item_condition(r->actual_condition_head));for(const auto&e:r->original_resource_effect_refs)take(sink.source(e));take(repair_service_agreement_detail::original_route_pins(x.agreements,r->actor_route,r->actual_actor_id,sink));
  const auto&t=contract(x.commercial,r->contract_ref);take(sink.source({repair_accepted_source(accepted_scope(x.commercial,t).id),1}));take(repair_physical_detail::independent(x).visit_original_repair_item_physical_pins(t,r->actual_transfer,r->occurred_at,s.source_ref,false,sink));
 }else if(same_source_ref(s.source_ref,repair_item_returned_source(s.source_ref.id))){
  const auto*r=take(read_repair_item_returned(x.commercial,s));for(const auto*p:{&r->contract_ref,&r->actual_receiver_acceptance,&r->actual_receiver_authority,&r->actual_destination_source,&r->actual_contact_source})take(sink.source(*p));take(sink.item_condition(r->actual_condition_head));for(const auto&e:r->original_resource_effect_refs)take(sink.source(e));take(repair_service_agreement_detail::original_route_pins(x.agreements,r->actor_route,r->actual_actor_id,sink));
  const auto&t=contract(x.commercial,r->contract_ref);take(sink.source({repair_accepted_source(accepted_scope(x.commercial,t).id),1}));
  for(const auto&held:x.commercial.items.repair_service_contracts.custody_receipts.rows())if(same(held.second.contract_ref,r->contract_ref))take(sink.source({repair_custody_received_source(held.first),1}));
  const auto end=repair_physical_detail::terminal(x.commercial,t);take(sink.source(end.source));
  if(end.job){const StableId*last=nullptr;for(const auto&id:end.job->facility_occupancy_event_refs)if(header(x.commercial.common,craft_facility_occupancy_source(id))<header(x.commercial.common,s.source_ref))last=&id;if(!last)fail("RepairReturnClearancePinRequired",r->id);take(sink.source({craft_facility_occupancy_source(*last),1}));}
  take(repair_physical_detail::independent(x).visit_original_repair_item_physical_pins(t,r->actual_transfer,r->occurred_at,s.source_ref,true,sink));
 }else{
  take(read_repair_original_item_transfer(x.commercial,s));
  // An owned-field address has no Common header, but its actual immutable row
  // must remain live when another original owner references only this effect.
  // Emit one direct edge; do not recursively invoke the parent's pin callback.
  // The central collector does not infer ownership from the shared id.
  if(same_source_ref(s.source_ref,repair_custody_item_transfer_source(s.source_ref.id)))
   take(sink.source({repair_custody_received_source(s.source_ref.id),1}));
  else take(sink.source({repair_item_returned_source(s.source_ref.id),1}));
 }
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
