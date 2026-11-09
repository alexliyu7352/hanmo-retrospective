#include "care_handover_period.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include <limits>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> x){if(!x.ok)throw Failure{std::move(x.error)};return std::move(x.value);}
void check(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
void bound(const CareHandoverSourceContext& x,const CareHandoverPeriodOriginalRead& original){if(!original.bound_to(x.acceptance.goals.common)||(x.acceptance.goals.rules.selected_care_version()!=5&&x.acceptance.goals.rules.selected_care_version()!=6))fail("CareHandoverPeriodOriginalSourcesRequired",{},ErrorKind::unsupported_capability);}
const AcceptedCaregiver& acceptance(const CareHandoverSourceContext& x,const CareHandoverExecution& e){auto value=take(read_accepted_caregiver(x.acceptance,care_acceptance_source(e.accepted_care_terms_ref)));if(value->requirement_id!=e.requirement_id)fail("CareHandoverAcceptedRequirementMismatch",e.handover_id);return *value;}
FixedServiceWorkPeriodRead2 actual(const CareHandoverSourceContext& x,const CareHandoverExecution& e,const AcceptedCaregiver& accepted,Integer ordinal){const auto* b=std::get_if<ActorActionWorkBinding>(&e.execution_binding);if(!b)fail("CareHandoverActorActionRequired",e.handover_id);return take(read_fixed_service_work_period2(x.elapsed,*b,ordinal,accepted.terms.handover_half_days,take(x.acceptance.goals.clock.read_current_moment()),x.time_scale));}
const CareHandoverExecutionBoundary1& final_adoption(const CareHandoverSourceContext& x,const CareHandoverExecution& e,const FixedServiceWorkPeriodRead2& period){if(!period.actual_service.complete||period.actual_service.intervals.empty())fail("CareHandoverActualServiceMissing",e.handover_id);const auto& id=period.actual_service.intervals.back().opening_transition_ref;const FixedServiceTransition* row=nullptr;for(const auto& candidate:x.elapsed.fixed_service_transitions)if(candidate.transition_id==id){if(row)fail("DuplicateCareHandoverFSLSource",id);row=&candidate;}if(!row||row->action_ref!=e.original_action_ref||row->transition_ordinal<0||static_cast<std::size_t>(row->transition_ordinal)>=e.handover_execution_boundaries.size())fail("CareHandoverPeriodAdoptionMissing",id);const auto& source=e.handover_execution_boundaries[static_cast<std::size_t>(row->transition_ordinal)];return *take(read_care_handover_boundary_for_original_effect(x,care_handover_boundary_source(source.boundary_id),1));}
void one(const CareHandoverSourceContext& x,const CareHandoverPeriodOriginalRead& originals,const CareHandoverExecution& e,const AcceptedCaregiver& accepted,const CareHandoverWorkPeriod2& period){
 take(read_care_handover_period_for_original_effect(x,care_handover_period_source(period.source_id),2));auto input=actual(x,e,accepted,period.work_period_ref.period_ordinal);check(originals.validate_period_inputs(e,accepted,input));check(originals.validate_period_effects(e,accepted,period));
 for(const auto& ref:period.worker_condition_receipt_refs){const auto* receipt=take(read_character_condition_receipt_provenance(x.acceptance.body,ref));bool worker=false;for(const auto& frame:period.worker_capability_inputs)if(frame.actor_id==receipt->actor_id)worker=true;if(!worker||receipt->effective_at.kind!=ActorFoundationAt::Kind::exact_moment||take(compare_world_moments(receipt->effective_at.exact_moment,period.provided_at))!=0)fail("CareHandoverActualPeriodReceiptMismatch",ref.result_id);}
}
}
Outcome<CareHandoverPeriodApplication> provide_care_handover_period_in_candidate(CommonStore& common,OrderedTable<CareHandoverExecution>& table,const CareHandoverSourceContext& x,const StableId& id,Integer ordinal,CareHandoverPeriodOriginalWrite& originals){try{
 if(&common!=&x.acceptance.goals.common||&table!=&x.executions)fail("CareHandoverSameCandidateRequired",id);bound(x,originals);auto* e=table.find(id);if(!e||e->handover_id!=id)fail("CareHandoverExecutionMissing",id);const auto& accepted=acceptance(x,*e);check(validate_care_handover_worker_history(x,id));
 if(ordinal<0||ordinal>static_cast<Integer>(e->work_period_sources.size()))fail("CareHandoverPeriodOrdinalGap",id);
 if(ordinal<static_cast<Integer>(e->work_period_sources.size())){const auto& row=e->work_period_sources[static_cast<std::size_t>(ordinal)];one(x,originals,*e,accepted,row);return {true,{care_handover_period_source(row.source_id),true},{}};}
 if(e->completion_event_ref||e->terminal_source_ref)fail("CareHandoverAlreadyTerminal",id);auto input=actual(x,*e,accepted,ordinal);const auto now=take(x.acceptance.goals.clock.read_current_moment());if(take(compare_world_moments(input.ref.until,now))!=0)fail("CareHandoverPeriodMustApplyAtActualGate",id);check(originals.validate_period_inputs(*e,accepted,input));const auto& adopted=final_adoption(x,*e,input);
 CareHandoverWorkPeriod2 row;row.source_id=take(common.next_id("care_handover_period"));row.execution_ref=id;row.work_period_ref=input.ref;row.provided_at=now;row.worker_capability_inputs=adopted.worker_capability_inputs;row.field_order={"source_id","source_schema","execution_ref","work_period_ref","provided_at","worker_capability_inputs","worker_condition_receipt_refs","field_order"};check(validate_care_handover_period_shape(row));const auto result=row.source_id;
 // One actual original row, no Common notification and no S0 Shared gate.
 e->work_period_sources.push_back(std::move(row));auto* stored=&e->work_period_sources.back();const auto count=e->work_period_sources.size();
 auto receipts=take(originals.apply_period_effects_in_candidate(common,*e,accepted,*stored));
 // A cost owner may write only its own effects, never relocate/replace this
 // unique source table. Any such breach poisons the entire private candidate.
 if(table.find(id)!=e||e->work_period_sources.size()!=count||&e->work_period_sources.back()!=stored||stored->source_id!=result)fail("CareHandoverEffectOwnerChangedSource",id);
 stored->worker_condition_receipt_refs=std::move(receipts);check(validate_care_handover_period_shape(*stored));one(x,originals,*e,accepted,*stored);return {true,{care_handover_period_source(result),false},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_handover_period_sources(const CareHandoverSourceContext& x,const CareHandoverPeriodOriginalRead& originals){try{
 bound(x,originals);check(validate_care_handover_time_source_closure(x));const auto now=take(x.acceptance.goals.clock.read_current_moment());for(const auto& pair:x.executions.rows()){const auto& e=pair.second;if(pair.first!=e.handover_id)fail("CareHandoverExecutionKeyMismatch",pair.first);const auto& accepted=acceptance(x,e);check(validate_care_handover_worker_history(x,pair.first));const auto* elapsed=find_fixed_elapsed(x.elapsed,e.original_action_ref);if(!elapsed)fail("CareHandoverFixedEntryMissing",pair.first);
  // Reverse coverage checks every full N actually earned, including a paused
  // or stopped execution. No missing period can be hidden by its final phase.
  for(Integer ordinal=0;ordinal<accepted.terms.handover_half_days;++ordinal){auto credit=take(fixed_duration_work_units(ordinal+1,x.time_scale.units_per_half_day));const bool earned=take(compare_fractions(credit.value,elapsed->credited_units.value))<=0;if(!earned){if(static_cast<std::size_t>(ordinal)!=e.work_period_sources.size())fail("CareHandoverUnprovidedPeriod",e.handover_id);break;}if(static_cast<std::size_t>(ordinal)>=e.work_period_sources.size())fail("CareHandoverActualPeriodMissing",e.handover_id);one(x,originals,e,accepted,e.work_period_sources[static_cast<std::size_t>(ordinal)]);}
  if(e.work_period_sources.size()>static_cast<std::size_t>(accepted.terms.handover_half_days))fail("CareHandoverExtraPeriod",e.handover_id);
 }
 // Internal sources must never be mirrored as reportable Common facts.
 for(const auto& pair:x.acceptance.goals.common.state().facts.rows())if(pair.second.domain=="care_requirements"&&(pair.second.kind=="care_handover_period"||pair.second.kind=="care_handover_execution_boundary"))fail("CareInternalSourceHasCommonHeader",pair.first);
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}
