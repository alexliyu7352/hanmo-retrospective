#include "care_handover_time.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/domains/actor_foundation/character_capability_replay.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
void check(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
bool same(const SourceRef& a,const SourceRef& b){return same_source_ref(a,b);}
WorldMoment context(const CareHandoverSourceContext& x,const CareHandoverBusinessRead& original){
 if((x.acceptance.goals.rules.selected_care_version()!=5&&x.acceptance.goals.rules.selected_care_version()!=6)||!original.bound_to(x.acceptance.goals.common))fail("CareHandoverActualBusinessSourcesRequired",{},ErrorKind::unsupported_capability);
 const auto now=take(x.acceptance.goals.clock.read_current_moment());if(!same(now,x.acceptance.body.actual_now))fail("CareHandoverActualClockMismatch");check(validate_time_scale(x.time_scale));return now;
}
void registration(const ActionTimeRegistration& r){if(r.action_kind!="care_handover"||r.owner_domain!="care_requirements"||r.profile_id!="care_handover_fixed_service"||r.profile_version!=1||r.source_schema!=3||r.mode!=ActionTimeMode::fixed_duration||r.remaining_meaning!=ActionRemainingMeaning::service_estimate||r.shared_gate_kind||!r.retained_wait_phases.empty())fail("CareHandoverActionTimeRegistrationMismatch",r.action_kind);}
const CareHandoverExecution& execution(const CareHandoverSourceContext& x,const StableId& action){const CareHandoverExecution* found=nullptr;for(const auto& pair:x.executions.rows())if(pair.second.original_action_ref==action){if(found||pair.first!=pair.second.handover_id)fail("CareHandoverActionSourceNotUnique",action);found=&pair.second;}if(!found)fail("CareHandoverActionSourceMissing",action);return *found;}
const CareHandoverExecution& boundary_owner(const CareHandoverSourceContext& x,const StableId& id,const CareHandoverExecutionBoundary1*& boundary){const CareHandoverExecution* found=nullptr;for(const auto& pair:x.executions.rows())for(const auto& row:pair.second.handover_execution_boundaries)if(row.boundary_id==id){if(found||pair.first!=pair.second.handover_id)fail("CareHandoverBoundaryNotUnique",id);found=&pair.second;boundary=&row;}if(!found)fail("CareHandoverBoundaryMissing",id);return *found;}
const AcceptedCaregiver& accepted(const CareHandoverSourceContext& x,const CareHandoverExecution& e){const auto* value=take(read_accepted_caregiver(x.acceptance,care_acceptance_source(e.accepted_care_terms_ref)));if(value->requirement_id!=e.requirement_id)fail("CareHandoverAcceptedRequirementMismatch",e.handover_id);return *value;}
FixedTransitionKind kind(CareHandoverBoundaryKind v){switch(v){case CareHandoverBoundaryKind::started:return FixedTransitionKind::started;case CareHandoverBoundaryKind::suspended:return FixedTransitionKind::suspended;case CareHandoverBoundaryKind::resumed:return FixedTransitionKind::resumed;case CareHandoverBoundaryKind::input_rebound:return FixedTransitionKind::input_rebound;default:return FixedTransitionKind::terminal;}}
void original_chain(const CareHandoverSourceContext& x,const CareHandoverBusinessRead& original,const CareHandoverExecution& e,const AcceptedCaregiver& consent,const WorldMoment& now){
 check(validate_care_handover_execution_shape(e,5));const auto& first=e.handover_execution_boundaries.front();if(take(compare_world_moments(consent.accepted_at,first.occurred_at))>0)fail("CareHandoverBeforeConsent",e.handover_id);
 // Only the original boundary and its actual sources are checked here. The S0
 // proposal need not be installed yet; the full retained closure is separate.
 for(const auto& row:e.handover_execution_boundaries){if(take(compare_world_moments(row.occurred_at,now))>0)fail("CareHandoverFutureBoundary",row.boundary_id);for(const auto& frame:row.worker_capability_inputs){if(frame.actor_id==consent.subject_actor_id)fail("CareDependentCannotBeWorker",frame.actor_id);check(validate_frozen_character_capability_original_prefix(x.acceptance.body,frame));}check(original.validate_original_boundary(e,consent,row));}
 check(original.validate_original_input_change_coverage(e,now));
}
FixedTerminalDisposition terminal(const CareHandoverBusinessRead& original,const CareHandoverExecution& e,const CareHandoverExecutionBoundary1& b){const auto stop=take(original.read_original_terminal_stop(e,b));if(b.kind==CareHandoverBoundaryKind::completed){if(stop!=RunStop::completed)fail("CareHandoverCompletedOutcomeMismatch",b.boundary_id);return FixedTerminalDisposition::completed;}if(b.kind!=CareHandoverBoundaryKind::stopped)fail("CareHandoverTerminalSourceRequired",b.boundary_id);if(stop==RunStop::cancelled)return FixedTerminalDisposition::cancelled;if(stop==RunStop::action_replaced)return FixedTerminalDisposition::replaced_by_original_action;if(stop==RunStop::blocked)return FixedTerminalDisposition::interrupted;fail("CareHandoverOriginalStopMismatch",b.boundary_id);}
std::vector<FixedServiceInputRef> inputs(const CareHandoverSourceContext& x,const CareHandoverExecution& e,const AcceptedCaregiver& consent,const CareHandoverExecutionBoundary1& b){
 std::vector<FixedServiceInputRef> out;if(kind(b.kind)==FixedTransitionKind::terminal)return out;
 auto add=[&](FixedServiceInputRef value){for(const auto& previous:out)if(same_fixed_input_ref(previous,value))return;out.push_back(std::move(value));};
 auto source=[&](const SourceRef& ref,Integer schema){if(ref.domain.empty()||ref.kind.empty()||ref.id.empty()||schema<1)fail("CareHandoverFixedInputSourceRequired",b.boundary_id);add(FixedAppliedInput{ref,schema});};
 auto rule=[&](const ActorFoundationRuleRef& ref){if(ref.id.empty()||ref.version<1||ref.catalog_digest!=x.acceptance.body.catalog.identity.content_digest)fail("CareHandoverLockedRuleMismatch",b.boundary_id);add(FixedLockedRuleInput{x.acceptance.body.catalog.identity,ref.id});};
 source(care_handover_boundary_source(b.boundary_id),1);source(care_acceptance_source(consent.accepted_id),3);rule(e.handover_rule_ref);rule(b.transition_rule_ref);
 for(const auto& frame:b.worker_capability_inputs){source(initial_condition_seed_source(frame.condition_head_snapshot.initial_condition_seed_ref),take(read_initial_condition_seed_schema(x.acceptance.body.catalog)));if(frame.condition_head_snapshot.latest_receipt_ref)source(character_condition_receipt_source(*frame.condition_head_snapshot.latest_receipt_ref),2);rule(frame.purpose_ref);rule(frame.result.rule_ref);}
 for(const auto* refs:{&b.actual_participation_source_refs,&b.facility_use_binding_refs,&consent.terms.allowed_dependency_refs,&consent.terms.provision_arrangement_refs,&consent.terms.fee_arrangement_refs,&consent.terms.guardianship_permission_refs})for(const auto& ref:*refs)source(ref.source_ref,ref.source_schema);
 if(consent.provider.institutional_assignment_ref)source(consent.provider.institutional_assignment_ref->source_ref,consent.provider.institutional_assignment_ref->source_schema);
 if(const auto* command=std::get_if<CareHandoverActorCommand>(&b.cause))for(const auto& ref:command->authority_source_refs)source(ref.source_ref,ref.source_schema);
 else if(const auto* obligation=std::get_if<CareHandoverAcceptedObligation>(&b.cause)){source(obligation->original_obligation_ref,obligation->original_obligation_source_schema);rule(obligation->accepted_rule_ref);}
 else if(const auto* changed=std::get_if<CareHandoverDependencyChanged>(&b.cause)){rule(changed->continuation_rule_ref);std::visit([&](const auto& change){using T=std::decay_t<decltype(change)>;if constexpr(std::is_same_v<T,CareHandoverConditionChanged>)source(character_condition_receipt_source(change.condition_receipt_ref),2);else if constexpr(std::is_same_v<T,CareHandoverDeathChanged>)source({change.death_ref.domain,change.death_ref.kind,change.death_ref.id},4);else source(change.source_ref,change.source_schema);},changed->change);}
 return out;
}
void equal_inputs(const std::vector<FixedServiceInputRef>& a,const std::vector<FixedServiceInputRef>& b,const StableId& id){if(a.size()!=b.size())fail("CareHandoverInputCoverageMismatch",id);for(std::size_t i=0;i<a.size();++i)if(!same_fixed_input_ref(a[i],b[i]))fail("CareHandoverInputSourceMismatch",id);}
const SourceRef& cause(const FixedServiceTransition& t,Integer& schema){return std::visit([&](const auto& value)->const SourceRef&{using T=std::decay_t<decltype(value)>;if constexpr(std::is_same_v<T,FixedOwnerAdopted>||std::is_same_v<T,FixedOwnerStateChanged>){schema=value.owner_source_schema;return value.owner_source_ref;}else if constexpr(std::is_same_v<T,FixedOwnerTerminated>){schema=value.owner_result_schema;return value.owner_result_ref;}else{schema=value.original_cause_schema;return value.original_cause_ref;}},t.cause);}
void equal_proof(const ActionTimeOwnerProof& a,const ActionTimeOwnerProof& b){if(a.action_ref!=b.action_ref||a.action_kind!=b.action_kind||a.actor_id!=b.actor_id||a.participant_actor_ids!=b.participant_actor_ids||!same(a.original_execution_source,b.original_execution_source)||a.source_schema!=b.source_schema||a.phase!=b.phase||a.retained_wait_source||a.suspended_source||!a.retained_gate_result_refs.empty()||a.terminal_result_ref!=b.terminal_result_ref||bool(a.ended_source)!=bool(b.ended_source))fail("CareHandoverOwnerProofMismatch",a.action_ref);auto source=[&](const ElapsedTimeSource& x,const ElapsedTimeSource& y){const auto* p=std::get_if<MomentOwnerSource>(&x);const auto* q=std::get_if<MomentOwnerSource>(&y);if(!p||!q||p->source_schema!=q->source_schema||!same(p->source_ref,q->source_ref))fail("CareHandoverOwnerTimeSourceMismatch",a.action_ref);};source(a.started_source,b.started_source);if(a.ended_source)source(*a.ended_source,*b.ended_source);}
}
Outcome<ActionTimeRegistration> make_care_handover_action_time_registration(Integer version){if(version!=5&&version!=6)return {false,{},common_failure("CareHandoverHistoryVersion5Required",{},ErrorKind::unsupported_capability).error};ActionTimeRegistration r;r.action_kind="care_handover";r.owner_domain="care_requirements";r.profile_id="care_handover_fixed_service";r.profile_version=1;r.source_schema=3;r.mode=ActionTimeMode::fixed_duration;r.remaining_meaning=ActionRemainingMeaning::service_estimate;return {true,std::move(r),{}};}
Status CareHandoverActionTimeRead::enumerate_action_time_owners(const ActionTimeRegistration& r,ActionTimeOwnerIdSink& sink)const{try{registration(r);context(sources_,original_);for(const auto& pair:sources_.executions.rows()){if(pair.first!=pair.second.handover_id)fail("CareHandoverExecutionKeyMismatch",pair.first);take(read_action_time_owner(r,pair.second.original_action_ref));check(sink.append(pair.second.original_action_ref));}return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<ActionTimeOwnerProof> CareHandoverActionTimeRead::read_action_time_owner(const ActionTimeRegistration& r,const StableId& action)const{try{
 registration(r);const auto now=context(sources_,original_);const auto& e=execution(sources_,action);const auto& consent=accepted(sources_,e);original_chain(sources_,original_,e,consent,now);auto actors=take(original_.read_original_actors(e,consent));
 bool found=false;for(std::size_t i=0;i<actors.participant_actor_ids.size();++i){const auto& id=actors.participant_actor_ids[i];if(id.empty()||id==consent.subject_actor_id||!sources_.acceptance.goals.common.actor(id))fail("CareHandoverOriginalWorkerInvalid",id);for(std::size_t j=0;j<i;++j)if(actors.participant_actor_ids[j]==id)fail("CareHandoverOriginalWorkerRepeated",id);found=found||id==actors.actor_id;}if(!found)fail("CareHandoverOriginalInitiatorMissing",action);
 bool paused=false;for(const auto& row:e.handover_execution_boundaries){if(row.kind==CareHandoverBoundaryKind::suspended)paused=true;else if(row.kind==CareHandoverBoundaryKind::resumed)paused=false;}
 const auto& first=e.handover_execution_boundaries.front();const auto& last=e.handover_execution_boundaries.back();const bool ended=kind(last.kind)==FixedTransitionKind::terminal;
 std::optional<ElapsedTimeSourceBuildView> ended_source;std::optional<std::string_view> terminal_result;
 const auto* current=sources_.acceptance.goals.common.action(action);if(ended){if(current)fail("CareHandoverTerminalActionStillHeld",action);terminal(original_,e,last);ended_source=MomentOwnerSourceBuildView{{"care_requirements","care_handover_execution_boundary",last.boundary_id},1};terminal_result=std::get<CareHandoverOriginalOutcome>(last.cause).outcome_source_ref.id;}
 else if(!current||current->kind!="care_handover"||current->actor_id!=actors.actor_id||current->start_tick!=first.occurred_at.half_day_tick||(paused?!current->participants.empty():current->participants!=actors.participant_actor_ids))fail("CareHandoverOriginalActionMismatch",action);
 // Original actor-reader ownership is separate. Only borrowed views reach the
 // sole shared proof constructor, whose first member retains its fresh copy.
 CheckpointVector<std::string_view> participants,gates;participants.reserve(actors.participant_actor_ids.size());for(const auto& actor:actors.participant_actor_ids)participants.emplace_back(actor);
 const ActionTimeOwnerProofBuildView view{action,"care_handover",actors.actor_id,{participants},
  {"care_requirements","care_handover_execution",e.handover_id},3,
  MomentOwnerSourceBuildView{{"care_requirements","care_handover_execution_boundary",first.boundary_id},1},
  ended?ActionTimeOwnerPhase::terminal:paused?ActionTimeOwnerPhase::suspended:ActionTimeOwnerPhase::working,
  std::nullopt,ended_source,terminal_result,{gates},std::nullopt};
 ActionTimeOwnerProof proof(view);
 check(validate_action_time_owner_proof(r,proof));return {true,std::move(proof),{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<WorldMoment> CareHandoverActionTimeRead::read_action_time_source(const ElapsedTimeSource& value)const{try{const auto now=context(sources_,original_);const auto* ref=std::get_if<MomentOwnerSource>(&value);if(!ref||ref->source_schema!=1||ref->source_ref.domain!="care_requirements"||ref->source_ref.kind!="care_handover_execution_boundary")fail("CareHandoverExactBoundarySourceRequired",{},ErrorKind::unsupported_capability);const CareHandoverExecutionBoundary1* row=nullptr;const auto& e=boundary_owner(sources_,ref->source_ref.id,row);original_chain(sources_,original_,e,accepted(sources_,e),now);return {true,row->occurred_at,{}};}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status CareHandoverActionTimeRead::validate_fixed(const ActionTimeRegistration& r,const ActionTimeOwnerProof& proof,const ActionElapsedState& elapsed,const std::vector<FixedServiceTransition>& transitions,bool retained)const{try{
 registration(r);const auto actual=take(read_action_time_owner(r,proof.action_ref));equal_proof(proof,actual);const auto& e=execution(sources_,proof.action_ref);const auto& consent=accepted(sources_,e);const auto required=take(fixed_duration_work_units(consent.terms.handover_half_days,sources_.time_scale.units_per_half_day));
 if(elapsed.action_ref!=e.original_action_ref||elapsed.execution_source_ref!=e.handover_id||elapsed.owner_cost_state_ref!=e.handover_id||elapsed.duration_definition_ref!=e.handover_rule_ref.id||!same(elapsed.started_at,e.handover_execution_boundaries.front().occurred_at)||take(compare_fractions(required.value,elapsed.required_units.value))!=0)fail("CareHandoverOriginalElapsedMismatch",e.handover_id);
 std::size_t n=0;const FixedServiceTransition* previous=nullptr;const CareHandoverExecutionBoundary1* adopted=nullptr;bool running=false;
 auto supplied=[&](const WorldMoment& until){if(!running||!adopted)return;const auto order=take(compare_world_moments(adopted->occurred_at,until));if(order>0)fail("CareHandoverServiceChronologyMismatch",e.handover_id);if(!order)return;for(const auto& frame:adopted->worker_capability_inputs)check(validate_frozen_character_capability_original_span(sources_.acceptance.body,frame,{adopted->occurred_at,until}));};
 for(const auto& transition:transitions){if(transition.action_ref!=e.original_action_ref){if(!retained)fail("CareHandoverUnexpectedActionTransition",transition.action_ref);continue;}if(n>=e.handover_execution_boundaries.size())fail("CareHandoverExtraTimeBoundary",transition.transition_id);const auto& b=e.handover_execution_boundaries[n++];Integer schema=0;
  if(transition.transition_ordinal!=b.boundary_ordinal||transition.source_schema!=1||transition.kind!=kind(b.kind)||transition.execution_source_schema!=3||!same(transition.execution_source_ref,care_handover_execution_source(e.handover_id))||!same(transition.occurred_at,b.occurred_at)||!same(cause(transition,schema),care_handover_boundary_source(b.boundary_id))||schema!=1||transition.previous_transition_ref!=(previous?std::optional<StableId>{previous->transition_id}:std::nullopt))fail("CareHandoverTimeBoundaryMismatch",b.boundary_id);
  equal_inputs(transition.input_basis_refs,inputs(sources_,e,consent,b),b.boundary_id);if(previous)equal_inputs(transition.previous_input_basis_refs,previous->input_basis_refs,b.boundary_id);else if(!transition.previous_input_basis_refs.empty())fail("CareHandoverUnexpectedPredecessorInputs",b.boundary_id);
  if(transition.kind==FixedTransitionKind::started){if(!std::holds_alternative<FixedOwnerAdopted>(transition.cause))fail("CareHandoverStartedCauseMismatch",b.boundary_id);}
  else if(transition.kind==FixedTransitionKind::terminal){if(!std::holds_alternative<FixedOwnerTerminated>(transition.cause)||transition.terminal_disposition!=terminal(original_,e,b))fail("CareHandoverTerminalCauseMismatch",b.boundary_id);}
  else {const auto* changed=std::get_if<FixedOwnerStateChanged>(&transition.cause);if(!changed||changed->transition_rule_ref!=b.transition_rule_ref.id)fail("CareHandoverChangedCauseMismatch",b.boundary_id);}
  supplied(b.occurred_at);
  if(b.kind==CareHandoverBoundaryKind::started||b.kind==CareHandoverBoundaryKind::resumed)running=true;
  else if(b.kind==CareHandoverBoundaryKind::suspended||kind(b.kind)==FixedTransitionKind::terminal)running=false;
  // InputRebound adopts the new body but preserves an existing pause.
  adopted=&b;previous=&transition;
 }
 supplied(elapsed.last_settled_at);
 if(n!=e.handover_execution_boundaries.size())fail("CareHandoverTimeBoundaryMissing",e.handover_id);
 if((proof.phase==ActionTimeOwnerPhase::working)!=std::holds_alternative<ElapsedRunning>(elapsed.timing)||(proof.phase==ActionTimeOwnerPhase::suspended)!=std::holds_alternative<ElapsedSuspended>(elapsed.timing)||(proof.phase==ActionTimeOwnerPhase::terminal)!=std::holds_alternative<ElapsedTerminal>(elapsed.timing))fail("CareHandoverOriginalPhaseMismatch",e.handover_id);
 if(const auto* stopped=std::get_if<ElapsedTerminal>(&elapsed.timing);stopped&&(!proof.terminal_result_ref||stopped->outcome_ref!=*proof.terminal_result_ref))fail("CareHandoverElapsedOutcomeMismatch",e.handover_id);
 if(const auto* suspended=std::get_if<ElapsedSuspended>(&elapsed.timing)){const CareHandoverExecutionBoundary1* source=nullptr;for(const auto& b:e.handover_execution_boundaries)if(b.kind==CareHandoverBoundaryKind::suspended)source=&b;if(!source||suspended->reason_source_ref!=source->boundary_id)fail("CareHandoverElapsedSuspensionMismatch",e.handover_id);}
 return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status CareHandoverActionTimeRead::validate_fixed_action_sources(const ActionTimeRegistration& r,const ActionTimeOwnerProof& p,const ActionElapsedState& e,const std::vector<FixedServiceTransition>& t)const{return validate_fixed(r,p,e,t,false);}
Status CareHandoverActionTimeRead::validate_retained_fixed_sources(const ActionTimeRegistration& r,const StableId& action)const{try{auto p=take(read_action_time_owner(r,action));const auto* e=find_fixed_elapsed(sources_.elapsed,action);if(!e)fail("CareHandoverFixedEntryMissing",action);check(validate_fixed(r,p,*e,sources_.elapsed.fixed_service_transitions,true));take(read_fixed_service_credit_boundary(sources_.elapsed,action,e->credited_units,context(sources_,original_),sources_.time_scale));return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status CareHandoverActionTimeRead::validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const{return common_failure("CareHandoverIsFixedService",{},ErrorKind::unsupported_capability);}
Outcome<RunStop> CareHandoverActionTimeRead::read_action_continuation_stop(const ActionTimeRegistration& r,const ActionTimeOwnerProof& p,const WorldMoment& now)const{try{if(!same(now,context(sources_,original_)))fail("CareHandoverActualClockMismatch");auto actual=take(read_action_time_owner(r,p.action_ref));equal_proof(p,actual);if(actual.phase==ActionTimeOwnerPhase::terminal){const auto& e=execution(sources_,p.action_ref);terminal(original_,e,e.handover_execution_boundaries.back());return {true,take(original_.read_original_terminal_stop(e,e.handover_execution_boundaries.back())),{}};}return {true,actual.phase==ActionTimeOwnerPhase::suspended?RunStop::paused:RunStop::awaiting_permit,{}};}catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<OriginalActionRemaining> CareHandoverActionTimeRead::read_original_action_remaining(const ActionTimeRegistration& r,const ActionTimeOwnerProof& p,const WorldMoment& now)const{try{if(!same(now,context(sources_,original_)))fail("CareHandoverActualClockMismatch");auto actual=take(read_action_time_owner(r,p.action_ref));equal_proof(p,actual);check(validate_retained_fixed_sources(r,p.action_ref));const auto* e=find_fixed_elapsed(sources_.elapsed,p.action_ref);check(validate_fixed_elapsed_value(*e,now,sources_.time_scale));const auto remaining=take(subtract_work_units(e->required_units,e->credited_units));const auto duration=take(scale_units_to_duration(remaining,sources_.time_scale.units_per_half_day));return {true,OriginalActionRemaining{ActualServiceRemaining{ActualServiceRemainingBuildView{duration,{actual.original_execution_source.domain,actual.original_execution_source.kind,actual.original_execution_source.id},3}}},{}};}catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<StableId> CareHandoverActionTimeRead::read_gate_result_id(const SharedWorkGateResult&)const{return {false,{},common_failure("CareHandoverHasNoSharedGate",{},ErrorKind::unsupported_capability).error};}
Outcome<SharedWorkGateResult> CareHandoverActionTimeRead::read_original_gate_result(const StableId& id)const{return {false,{},common_failure("CareHandoverHasNoSharedGate",id,ErrorKind::unsupported_capability).error};}
Status CareHandoverActionTimeRead::validate_original_gate_result(const SharedWorkGateResult&)const{return common_failure("CareHandoverHasNoSharedGate",{},ErrorKind::unsupported_capability);}
// 仅在同一候选内采用当前照护边界；任一来源或准备失败仍交由外层撤回候选。
Outcome<StableId> adopt_care_handover_boundary_in_candidate(
    CommonStore &common, OrderedTable<CareHandoverExecution> &rows,
    ActionElapsedStore &elapsed, const CareHandoverSourceContext &x,
    const CareHandoverBusinessRead &original, const StableId &id) {
  try {
    if (&common != &x.acceptance.goals.common || &rows != &x.executions ||
        &elapsed != &x.elapsed)
      fail("CareHandoverSameCandidateRequired", id);
    const auto now = context(x, original);
    const auto reg = take(make_care_handover_action_time_registration(5));
    const CareHandoverExecutionBoundary1 *boundary = nullptr;
    const auto &e = boundary_owner(x, id, boundary);
    const auto &consent = accepted(x, e);
    CareHandoverActionTimeRead reader(x, original);
    for (const auto &t : elapsed.fixed_service_transitions) {
      Integer schema = 0;
      if (same(cause(t, schema), care_handover_boundary_source(id))) {
        if (schema != 1 || t.action_ref != e.original_action_ref)
          fail("CareHandoverConflictingTimeAdoption", id);
        check(
            reader.validate_retained_fixed_sources(reg, e.original_action_ref));
        return {true, t.transition_id, {}};
      }
    }
    if (&e.handover_execution_boundaries.back() != boundary ||
        !same(boundary->occurred_at, now))
      fail("CareHandoverActualCurrentBoundaryRequired", id);
    take(reader.read_action_time_owner(reg, e.original_action_ref));
    FixedServiceTransition t;
    t.transition_id = take(common.next_id("fixed_service_transition"));
    t.action_ref = e.original_action_ref;
    t.transition_ordinal = boundary->boundary_ordinal;
    t.kind = kind(boundary->kind);
    t.occurred_at = boundary->occurred_at;
    t.execution_source_ref = care_handover_execution_source(e.handover_id);
    t.execution_source_schema = 3;
    t.input_basis_refs = inputs(x, e, consent, *boundary);
    auto prepared = [&]() -> PreparedFixedServiceChange {
      if (t.kind == FixedTransitionKind::started) {
        const auto *action = common.action(e.original_action_ref);
        if (!action)
          fail("CareHandoverActualActionMissing", id);
        t.cause = FixedOwnerAdopted{care_handover_boundary_source(id), 1};
        auto entry = take(begin_fixed_elapsed(
            action->id, e.handover_id, e.handover_rule_ref.id, e.handover_id,
            now, consent.terms.handover_half_days, x.time_scale));
        return take(prepare_fixed_service_adoption(*action, entry, t, reg,
                                                   x.time_scale, reader));
      } else {
        const auto *entry = find_fixed_elapsed(elapsed, e.original_action_ref);
        if (!entry)
          fail("CareHandoverFixedEntryMissing", id);
        const FixedServiceTransition *previous = nullptr;
        for (const auto &row : elapsed.fixed_service_transitions)
          if (row.action_ref == e.original_action_ref &&
              (!previous ||
               row.transition_ordinal > previous->transition_ordinal))
            previous = &row;
        if (!previous ||
            previous->transition_ordinal ==
                std::numeric_limits<Integer>::max() ||
            previous->transition_ordinal + 1 != t.transition_ordinal)
          fail("CareHandoverTimePredecessorMissing", id);
        t.previous_transition_ref = previous->transition_id;
        t.previous_input_basis_refs = previous->input_basis_refs;
        t.credited_at = entry->credited_units;
        if (std::holds_alternative<ElapsedRunning>(entry->timing) &&
            !same(entry->last_settled_at, now))
          t.credited_at =
              take(prepare_fixed_elapsed_slice(
                       *entry, {entry->last_settled_at, now}, x.time_scale))
                  .credited_units;
        std::optional<StableId> suspended;
        if (t.kind == FixedTransitionKind::terminal) {
          t.cause = FixedOwnerTerminated{care_handover_boundary_source(id), 1};
          t.terminal_disposition = terminal(original, e, *boundary);
        } else {
          t.cause = FixedOwnerStateChanged{care_handover_boundary_source(id), 1,
                                           boundary->transition_rule_ref.id};
          if (t.kind == FixedTransitionKind::suspended)
            suspended = id;
        }
        return take(prepare_fixed_service_change(elapsed, e.original_action_ref,
                                                 t, suspended, reg,
                                                 x.time_scale, reader));
      }
    }();
    const auto result = t.transition_id;
    check(apply_fixed_service_change_in_candidate(elapsed, std::move(prepared),
                                                  reg, x.time_scale, reader));
    return {true, result, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
}
