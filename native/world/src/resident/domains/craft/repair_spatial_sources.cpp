#include "resident/kernel/common_fact_prefix.hpp"
#include "repair_spatial_sources.hpp"
#include "repair_character_sources.hpp"
#include "repair_detail.hpp"
#include "craft_facility_occupancy.hpp"
namespace hanmo::resident {using namespace repair_detail;
namespace {
WorldMoment identity(const RepairSpatialSourceContext&x,const RepairJob&j){
 const auto&r=x.repair;context(r);const auto at=now(r);
 if(r.spatial_sources!=&x||!r.occupancy||r.occupancy->repair!=&r||&x.original.common!=&r.common||&x.original.common_catalog!=&r.common_catalog||compare(at,take(x.original.spatial.current_moment()))||r.items.repair_jobs.jobs.find(j.id)!=&j||j.id.empty()||j.terms.worker_id.empty())fail("RepairSpatialContextMismatch",j.id);
 if(!r.character_sources||&r.character_sources->body.common!=&r.common||&r.character_sources->body.state!=&x.original.foundation||&r.character_sources->body.catalog!=&x.original.actors)fail("RepairSpatialCharacterContextMismatch",j.id);
 header(r.common,repair_job_source(j.id));if(compare(j.started_at,at)>0)fail("RepairSpatialFutureStart",j.id);return at;
}
MovementAnchor workstation(const RepairSpatialSourceContext&x,const RepairJob&j,const FacilityClaim2&claim,const FacilityClaimMutationReceipt&receipt){
 const auto&o=*x.repair.occupancy;
 if(claim.slot_ref!=j.terms.workstation||claim.holder!=j.terms.service_holder||claim.units!=1||std::find(j.claim_refs.begin(),j.claim_refs.end(),claim.claim_id)==j.claim_refs.end()||!same_source_ref(claim.source_result_ref,receipt.source_result_ref))fail("RepairSpatialOriginalClaimMismatch",claim.claim_id);
 const EconomicFacilityCapacitySource*capacity=nullptr;for(const auto&v:o.facility_sources.capacity_sources)if(v.slot_ref==claim.slot_ref&&v.source_revision==claim.capacity_revision){if(capacity)fail("DuplicateRepairSpatialCapacity",claim.claim_id);capacity=&v;}
 if(!capacity||capacity->source_schema!=1||compare(capacity->occurred_at,receipt.occurred_at)>0)fail("RepairSpatialCapacitySourceRequired",claim.claim_id);
 take(o.physical.read_facility_slot_definition_at_source({{claim.slot_ref.provider_domain,"economic_facility_capacity",capacity->source_id},capacity->source_schema},claim.slot_ref));
 const PhysicalServiceFacilityExtension*facility=nullptr;for(const auto&v:o.facility_sources.facilities)if(v.facility_id==claim.slot_ref.facility_id){if(facility||v.asset_owner_domain!=claim.slot_ref.provider_domain||capacity->facility_extension_ref!=v.facility_id)fail("RepairSpatialFacilityIdentityMismatch",claim.claim_id);facility=&v;}
 if(!facility||facility->place_ref.empty())fail("RepairSpatialFacilityPlaceRequired",claim.claim_id);
 return take(read_original_spatial_site_anchor(x.repair.common_catalog,x.travel_catalog,{"common_catalog","place",facility->place_ref}));
}
void at_workstation(const RepairSpatialSourceContext&x,const MovementAnchor&actual,const MovementAnchor&expected,const StableId&actor){if(!take(same_actual_movement_position(actual,expected,x.repair.arithmetic_budget)))fail("RepairWorkerNotAtActualWorkstation",actor);}
}
Outcome<FixedAppliedInput> capture_repair_worker_spatial_input(const RepairSpatialSourceContext&x,const RepairJob&j){try{
 const auto at=identity(x,j);const auto&o=*x.repair.occupancy;const FacilityClaim2*current=nullptr;
 for(const auto&c:o.claims.claims)if(c.slot_ref==j.terms.workstation&&c.holder==j.terms.service_holder&&compare(c.from,at)<=0&&compare(at,c.until)<0){if(current)fail("AmbiguousRepairCurrentWorkstationClaim",j.id);current=&c;}
 if(!current)fail("RepairCurrentWorkstationClaimRequired",j.id);const auto episode=take(read_facility_claim_episode(o.claims,current->claim_id,at,x.repair.arithmetic_budget));if(episode.close_receipt_ref)fail("RepairCurrentClaimAlreadyClosed",j.id);const auto receipt=take(read_facility_claim_receipt(o.claims,episode.admission_receipt_ref));if(compare(receipt.occurred_at,at)>0)fail("RepairSpatialFutureClaim",j.id);const auto expected=workstation(x,j,episode.original_claim,receipt);
 auto prefix_read = read_current_common_fact_prefix(x.repair.common);
 if (!prefix_read.ok) {
   if (prefix_read.error.original.code == "CommonCurrentFactPrefixOverflow")
     fail("RepairSpatialFactPrefixOverflow", j.id);
   if (prefix_read.error.original.code == "CommonCurrentFactPrefixMismatch")
     fail("RepairSpatialFactPrefixMismatch", j.id);
   take(std::move(prefix_read));
 }
 ActorSpatialActualFactPrefix prefix;
 prefix.common_fact_count = prefix_read.value.common_fact_count;
 if (const auto *h = prefix_read.value.last_common_fact)
   prefix.last_common_fact = SourceRef{h->domain, h->kind, h->id};
 const auto&v=x.original;const auto actual=take(read_actor_spatial_location_at_actual_fact_prefix(v.common,v.common_catalog,v.movement,v.carriers,v.registrations,v.foundation,v.actors,v.spatial,j.terms.worker_id,at,prefix,x.military_handoffs,v.budget,v.family_catalog,v.births,v.military_actions));
 if(actual.actor_id!=j.terms.worker_id||actual.actual_location_sources.size()!=1)fail("RepairSpatialCanonicalHeadRequired",j.id);at_workstation(x,*take(require_actor_ordinary_movement_anchor(actual)),expected,j.terms.worker_id);const auto&head=actual.actual_location_sources.front();if(!is_actor_spatial_head_source_kind(head.source)||take(actor_spatial_history_source_schema(head.source))!=head.schema)fail("RepairSpatialCanonicalSourceRequired",j.id);return {true,{head.source,head.schema},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_repair_worker_spatial_span(const RepairSpatialSourceContext&x,const RepairJob&j,const std::vector<FixedServiceInputRef>&inputs,const MomentSpan&span,const std::vector<StableId>&claims){try{
 const auto at=identity(x,j);if(compare(span.from,span.to)>=0||compare(span.from,j.started_at)<0||compare(span.to,at)>0||claims.size()!=1)fail("RepairSpatialServiceWindowMismatch",j.id);
 const auto&o=*x.repair.occupancy;const auto episode=take(read_facility_claim_episode(o.claims,claims.front(),at,x.repair.arithmetic_budget));const auto&claim=episode.original_claim;const auto receipt=take(read_facility_claim_receipt(o.claims,episode.admission_receipt_ref));if(!episode.actual_reserved_span||compare(episode.actual_reserved_span->from,span.from)>0||compare(episode.actual_reserved_span->to,span.to)<0||compare(claim.from,span.from)>0||compare(claim.until,span.to)<0||compare(receipt.occurred_at,span.from)>0)fail("RepairSpatialClaimSpanNotCovered",j.id);const auto expected=workstation(x,j,claim,receipt);
 const FixedAppliedInput*head=nullptr;for(const auto&input:inputs){const auto*s=std::get_if<FixedAppliedInput>(&input);if(!s||!is_actor_spatial_head_source_kind(s->source_ref))continue;if(head)fail("AmbiguousRepairSpatialInput",j.id);if(take(actor_spatial_history_source_schema(s->source_ref))!=s->source_schema||!take(read_actor_spatial_head_source_membership(x.original,j.terms.worker_id,{s->source_ref,s->source_schema})))fail("RepairSpatialWorkerSourceMismatch",j.id);head=s;}
 if(!head)fail("RepairSpatialAdoptedInputRequired",j.id,ErrorKind::unsupported_capability);
 const FixedServiceTransition*adoption=nullptr;for(const auto&t:x.repair.elapsed.fixed_service_transitions)if(t.action_ref==j.action_id&&compare(t.occurred_at,span.from)<=0){if(adoption&&t.transition_ordinal<=adoption->transition_ordinal)fail("RepairSpatialInputAdoptionOrderMismatch",j.id);adoption=&t;}
 if(!adoption||(adoption->kind!=FixedTransitionKind::started&&adoption->kind!=FixedTransitionKind::resumed&&adoption->kind!=FixedTransitionKind::input_rebound)||adoption->input_basis_refs.size()!=inputs.size())fail("RepairSpatialInputAdoptionRequired",j.id);for(std::size_t n=0;n<inputs.size();++n)if(!same_fixed_input_ref(inputs[n],adoption->input_basis_refs[n]))fail("RepairSpatialInputAdoptionMismatch",j.id);
 SourceRef adoption_source;if(const auto*cause=std::get_if<FixedOwnerAdopted>(&adoption->cause)){if(cause->owner_source_schema!=1||!same_source_ref(cause->owner_source_ref,repair_job_source(j.id)))fail("RepairSpatialOriginalAdoptionMismatch",j.id);adoption_source=cause->owner_source_ref;}else if(const auto*cause=std::get_if<FixedOwnerStateChanged>(&adoption->cause)){const auto*c=x.repair.items.repair_jobs.controls.find(adoption->transition_id);if(cause->owner_source_schema!=1||!c||c->job_id!=j.id||!same_source_ref(cause->owner_source_ref,repair_control_source(c->id))||compare(c->occurred_at,adoption->occurred_at))fail("RepairSpatialOriginalAdoptionMismatch",j.id);adoption_source=cause->owner_source_ref;}else fail("RepairSpatialOriginalAdoptionRequired",j.id);
 const auto&v=x.original;const auto initial=take(read_actor_spatial_location_before_source(v.common,v.common_catalog,v.movement,v.carriers,v.registrations,v.foundation,v.actors,v.spatial,j.terms.worker_id,head->source_ref,adoption_source,adoption->occurred_at,x.military_handoffs,v.budget,v.family_catalog,v.births,v.military_actions));if(initial.actor_id!=j.terms.worker_id)fail("RepairSpatialOriginalWorkerMismatch",j.id);at_workstation(x,initial.actual_anchor,expected,j.terms.worker_id);
 const auto adopted=take(read_actor_spatial_location_at_source_head(v.common,v.common_catalog,v.movement,v.carriers,v.registrations,v.foundation,v.actors,v.spatial,j.terms.worker_id,{{head->source_ref,head->source_schema}},span.from,x.military_handoffs,v.budget,v.family_catalog,v.births,v.military_actions));if(adopted.actor_id!=j.terms.worker_id)fail("RepairSpatialWorkerMismatch",j.id);at_workstation(x,*take(require_actor_ordinary_movement_anchor(adopted)),expected,j.terms.worker_id);
 const auto actual=take(read_actor_stationary_location_span(v.common,v.common_catalog,v.movement,v.carriers,v.registrations,v.foundation,v.actors,v.spatial,j.terms.worker_id,head->source_ref,span,x.military_handoffs,v.budget,v.family_catalog,v.births,v.military_actions));if(actual.actor_id!=j.terms.worker_id||compare(actual.span.from,span.from)||compare(actual.span.to,span.to))fail("RepairSpatialActualServiceMismatch",j.id);at_workstation(x,actual.actual_anchor,expected,j.terms.worker_id);return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
