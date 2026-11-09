#include "care_coverage.hpp"
#include "care_participant_capacity.hpp"
#include "resident/domains/travel/care_dependent_placement.hpp"
namespace hanmo::resident {
Status validate_care_handover_actual_placement(const CareCoverageSources& x,
 const CareHandoverExecution& e,const AcceptedCaregiver& accepted,
 const CareBinding& binding,const CareTemporalEvent& event){
 const auto& c=x.handovers.acceptance.goals.common;
 if(!x.placements||!x.placements->bound_to(c))return common_failure("CareActualS1PlacementReadRequired",e.handover_id,ErrorKind::unsupported_capability);
 const auto* completed=std::get_if<CareHandoverCompletedEvent>(&event.body);
 if(!completed||completed->handover_id!=e.handover_id||completed->requirement_id!=e.requirement_id||completed->binding_id!=binding.binding_id||
  accepted.accepted_id!=e.accepted_care_terms_ref||binding.accepted_ref!=accepted.accepted_id||binding.terms_ref!=accepted.accepted_id||
  binding.child_actor_id!=accepted.subject_actor_id||binding.carer_actor_id!=accepted.provider.carer_actor_id||
  binding.placement_ref.source_schema!=completed->placement_result_ref.source_schema||
  !same_source_ref(binding.placement_ref.source_ref,completed->placement_result_ref.source_ref))
  return common_failure("CareActualPlacementOriginalBindingMismatch",e.handover_id,ErrorKind::internal);
 auto actual=x.placements->read_placement_result(binding.placement_ref);if(!actual.ok)return {false,{},std::move(actual.error)};
 const auto* row=actual.value.original;
 if(!row||row->id!=binding.placement_ref.source_ref.id||row->child_actor_id!=binding.child_actor_id||row->carer_actor_id!=binding.carer_actor_id||
  row->actual_handover_source.source_schema!=3||!same_source_ref(row->actual_handover_source.source_ref,care_handover_execution_source(e.handover_id))||
  actual.value.placement_result.source_schema!=binding.placement_ref.source_schema||!same_source_ref(actual.value.placement_result.source_ref,binding.placement_ref.source_ref))
  return common_failure("CareActualPlacementOriginalResultMismatch",e.handover_id,ErrorKind::internal);
 auto same=compare_world_moments(row->occurred_at,event.occurred_at);if(!same.ok)return {false,{},std::move(same.error)};
 if(same.value!=0)return common_failure("CareActualPlacementOriginalMomentMismatch",e.handover_id,ErrorKind::internal);
 const auto& g=x.handovers.acceptance.goals;
 if(&x.handovers.acceptance.body.common!=&c)return common_failure("CareHandoverParticipantSameCandidateRequired",e.handover_id,ErrorKind::internal);
 return validate_care_participants_at_actual_handover(x.handovers.acceptance.body,binding.child_actor_id,binding.carer_actor_id,binding.placement_ref.source_ref,event.occurred_at,g.clock);
}
}
