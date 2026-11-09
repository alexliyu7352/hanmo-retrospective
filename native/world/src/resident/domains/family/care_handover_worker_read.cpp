#include "care_handover_worker_read.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/actor_foundation/character_capability_replay.hpp"
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void check(Status status){if(!status.ok)throw Failure{std::move(status.error)};}
bool same_head(const ActorConditionHead& a,const ActorConditionHead& b){return a.actor_id==b.actor_id&&a.condition_revision==b.condition_revision&&a.initial_condition_seed_ref.seed_key==b.initial_condition_seed_ref.seed_key&&bool(a.latest_receipt_ref)==bool(b.latest_receipt_ref)&&(!a.latest_receipt_ref||a.latest_receipt_ref->result_id==b.latest_receipt_ref->result_id);}
const CareHandoverExecutionBoundary1& opening(const CareHandoverSourceContext& x,const CareHandoverExecution& execution,const StableId& transition_id){
 const FixedServiceTransition* original=nullptr;for(const auto& row:x.elapsed.fixed_service_transitions)if(row.transition_id==transition_id){if(original)fail("DuplicateCareHandoverFSLTransition",transition_id);original=&row;}
 if(!original||original->action_ref!=execution.original_action_ref||original->transition_ordinal<0||static_cast<std::size_t>(original->transition_ordinal)>=execution.handover_execution_boundaries.size())fail("CareHandoverServiceOpeningMissing",transition_id);
 const auto& result=execution.handover_execution_boundaries[static_cast<std::size_t>(original->transition_ordinal)];take(read_care_handover_boundary_for_original_effect(x,care_handover_boundary_source(result.boundary_id),1));return result;
}
void service(const CareHandoverSourceContext& x,const CareHandoverExecution& e,const ActualServiceIntervalView& view){if(!view.complete)fail("CareHandoverFullServiceHistoryRequired",e.handover_id);for(const auto& span:view.intervals){const auto& adopted=opening(x,e,span.opening_transition_ref);if(adopted.worker_capability_inputs.empty())fail("CareHandoverActualServiceWorkersMissing",e.handover_id);for(const auto& frame:adopted.worker_capability_inputs)check(validate_frozen_character_capability_original_span(x.acceptance.body,frame,{span.from,span.until}));}}
}
Outcome<FrozenFacilityCapabilityInputs> capture_care_handover_worker(const CharacterConditionSources& body,const StableId& worker,const ReadStamp& stamp,const ReadStampAuthority& authority,const DomainReadVersionSource& versions){try{
 const auto* binding=body.catalog.capability_purpose_bindings.find("facility_assisted_clearance_worker");if(!binding||binding->purpose_ref.version!=1||binding->purpose_ref.catalog_digest!=body.catalog.identity.content_digest)fail("CareHandoverWorkerPurposeRequired",worker,ErrorKind::unsupported_capability);
 auto actual=take(read_character_capability(body,worker,binding->purpose_ref,std::nullopt,stamp,authority,versions));return freeze_character_capability(actual);
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_handover_worker_history(const CareHandoverSourceContext& x,const StableId& id){try{
 const auto* e=x.executions.find(id);if(!e||e->handover_id!=id)fail("CareHandoverExecutionMissing",id);check(validate_care_handover_execution_shape(*e,5));const auto now=take(x.acceptance.goals.clock.read_current_moment());const auto* elapsed=find_fixed_elapsed(x.elapsed,e->original_action_ref);if(!elapsed)fail("CareHandoverFixedEntryMissing",id);
 for(const auto& boundary:e->handover_execution_boundaries){take(read_care_handover_boundary_for_original_effect(x,care_handover_boundary_source(boundary.boundary_id),1));for(const auto& frame:boundary.worker_capability_inputs)check(validate_frozen_character_capability_original_prefix(x.acceptance.body,frame));}
 const bool due=std::holds_alternative<ElapsedRunning>(elapsed->timing)&&take(compare_fractions(elapsed->credited_units.value,elapsed->required_units.value))==0;
 auto supplied=take(due?read_fixed_service_intervals_at_completion_due(x.elapsed,e->original_action_ref,elapsed->started_at,elapsed->last_settled_at,now,x.time_scale):read_fixed_service_intervals(x.elapsed,e->original_action_ref,elapsed->started_at,elapsed->last_settled_at,now,x.time_scale));service(x,*e,supplied);
 const auto* accepted=x.acceptance.accepted.find(e->accepted_care_terms_ref);if(!accepted)fail("CareHandoverAcceptanceMissing",id);const auto& action=std::get<ActorActionWorkBinding>(e->execution_binding);
 for(const auto& period:e->work_period_sources){take(read_care_handover_period_for_original_effect(x,care_handover_period_source(period.source_id),2));auto actual=take(read_fixed_service_work_period2(x.elapsed,action,period.work_period_ref.period_ordinal,accepted->terms.handover_half_days,now,x.time_scale));if(actual.actual_service.intervals.empty())fail("CareHandoverOriginalPeriodEmpty",period.source_id);const auto& adopted=opening(x,*e,actual.actual_service.intervals.back().opening_transition_ref);
  if(period.worker_capability_inputs.size()!=adopted.worker_capability_inputs.size())fail("CareHandoverPeriodWorkerSetMismatch",period.source_id);
  for(std::size_t i=0;i<period.worker_capability_inputs.size();++i){const auto& frame=period.worker_capability_inputs[i];const auto& before=adopted.worker_capability_inputs[i];if(frame.actor_id!=before.actor_id||!same_head(frame.condition_head_snapshot,before.condition_head_snapshot)||take(compare_world_moments(frame.captured_at,before.captured_at))!=0||!actor_foundation_rule_equal(frame.purpose_ref,before.purpose_ref))fail("CareHandoverPeriodAdoptedHeadMismatch",period.source_id);check(validate_frozen_character_capability_original_prefix(x.acceptance.body,frame));}
 }
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}
