#include "care_handover_start.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_action_occupation.hpp"
#include "resident/domains/actor_foundation/character_capability_replay.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void check(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
WorldMoment context(const CareHandoverStartContext& x){const auto& a=x.command_sources.handovers.acceptance;const auto& c=a.goals.common;if((a.goals.rules.selected_care_version()!=5&&a.goals.rules.selected_care_version()!=6)||!x.originals.bound_to(c)||&a.body.common!=&c||!a.goals.rules.bound_to(c))fail("CareHandoverActualStartSourcesRequired",{},ErrorKind::unsupported_capability);auto now=take(a.goals.clock.read_current_moment());if(!same(now,a.body.actual_now)||!same(now,take(x.reads.read_current_moment())))fail("CareHandoverActualStartClockMismatch");return now;}
void source(const MomentOwnerSource& ref){if(ref.source_schema<1||ref.source_ref.domain.empty()||ref.source_ref.kind.empty()||ref.source_ref.id.empty())fail("CareHandoverActualStartSourceRequired",ref.source_ref.id);}
void refs(const std::vector<MomentOwnerSource>& values){if(values.size()>65536)fail("CareHandoverStartSourcesTooLarge");for(std::size_t i=0;i<values.size();++i){source(values[i]);for(std::size_t j=0;j<i;++j)if(same_source_ref(values[i].source_ref,values[j].source_ref))fail("CareHandoverStartSourceDuplicate",values[i].source_ref.id);}}
void rule(const ActorFoundationRuleRef& r,const CharacterConditionSources& b){if(r.id.empty()||r.version<1||r.catalog_digest!=b.catalog.identity.content_digest)fail("CareHandoverOriginalStartRuleRequired",r.id);}
const CareRequirement& goal(const CareHandoverStartContext& x,const AcceptedCaregiver& a,const WorldMoment& now){const auto& g=x.command_sources.handovers.acceptance.goals;const auto* r=g.goals.requirements.find(a.requirement_id);if(!r||r->subject_actor_id!=a.subject_actor_id)fail("CareHandoverAcceptedRequirementMismatch",a.accepted_id);take(read_care_requirement_goal(g,{r->id,r->subject_actor_id}));if(r->state!=StateGoalState::unsatisfied||r->current_care_binding_ref||r->accepted_care_terms_ref)fail("CareHandoverUnsatisfiedRequirementRequired",r->id,ErrorKind::rule);if(take(compare_world_moments(now,WorldMoment{a.terms.from_tick,{}}))<0||(a.terms.until_tick&&take(compare_world_moments(now,WorldMoment{*a.terms.until_tick,{}}))>=0))fail("CareHandoverOutsideAcceptedWindow",r->id,ErrorKind::rule);return *r;}
void workers(const CommonStore& c,const AcceptedCaregiver& a,const StableId& actor,const std::vector<StableId>& ids,bool occupied,const StableId& action={}){if(ids.empty()||ids.size()>65536||std::count(ids.begin(),ids.end(),actor)!=1||std::count(ids.begin(),ids.end(),a.provider.carer_actor_id)!=1||actor==a.subject_actor_id)fail("CareHandoverActualWorkersRequired",actor,ErrorKind::rule);for(std::size_t i=0;i<ids.size();++i){const auto* person=c.actor(ids[i]);if(ids[i]==a.subject_actor_id||!person||!person->alive||!person->available||(occupied?person->action_id!=action:!person->action_id.empty()))fail("CareHandoverWorkerUnavailable",ids[i],ErrorKind::rule);for(std::size_t j=0;j<i;++j)if(ids[i]==ids[j])fail("CareHandoverDuplicateWorker",ids[i]);}}
void header(const CommonStore& c,const CareHandoverExecution& e){const auto* head=c.state().facts.find(e.handover_id);if(!head||head->id!=e.handover_id||head->domain!="care_requirements"||head->kind!="care_handover_execution"||std::count(c.state().facts.order().begin(),c.state().facts.order().end(),e.handover_id)!=1)fail("CareHandoverActualExecutionHeaderRequired",e.handover_id);}
} // namespace
Outcome<PreparedCareHandoverStart> prepare_care_handover_start(const CareHandoverStartContext& x,const StableId& actor,const StartCareHandoverRequest& request){try{
 const auto now=context(x);const auto& h=x.command_sources.handovers;const auto& a=h.acceptance;const auto& c=a.goals.common;const auto* accepted=take(read_accepted_caregiver(a,care_acceptance_source(request.accepted_care_terms_ref)));const auto& requirement=goal(x,*accepted,now);
 for(const auto& pair:h.executions.rows())if(pair.second.requirement_id==requirement.id&&!pair.second.completion_event_ref&&!pair.second.terminal_source_ref)fail("CareHandoverAlreadyActive",requirement.id,ErrorKind::rule);
 auto original=take(x.originals.read_current_start(actor,*accepted,now));workers(c,*accepted,actor,original.worker_actor_ids,false);rule(original.handover_rule_ref,a.body);rule(original.transition_rule_ref,a.body);refs(original.original_input_sources);
 std::vector<ResourceKey> resources;std::vector<DomainReadKey> domains;std::vector<StableId> source_ids{accepted->accepted_id,requirement.current_transition_ref.id};
 for(const auto& id:original.worker_actor_ids){resources.push_back({ResourceKey::Kind::actor,id});domains.push_back({"actor_foundation","condition_head",id});}resources.push_back({ResourceKey::Kind::actor,accepted->subject_actor_id});
 for(const auto& ref:original.original_input_sources)if(std::find(source_ids.begin(),source_ids.end(),ref.source_ref.id)==source_ids.end())source_ids.push_back(ref.source_ref.id);
 auto stamp=take(x.reads.capture(resources,domains,source_ids));if(!stamp||(stamp->mode()!=ReadMode::committed_current&&stamp->mode()!=ReadMode::phase_current)||!same(stamp->span().to,now))fail("CareHandoverActualStartStampRequired");check(x.stamps.validate(*stamp,c.state().identity,stamp->mode(),c,x.versions));
 PreparedCareHandoverStart prepared;prepared.common_=&c;prepared.executions_=&h.executions;prepared.originals_=&x.originals;prepared.actor_=actor;prepared.accepted_=accepted->accepted_id;prepared.requirement_=requirement.id;prepared.requirement_transition_=requirement.current_transition_ref.id;prepared.at_=now;prepared.duration_=accepted->terms.handover_half_days;prepared.original_=std::move(original);prepared.stamp_=std::move(stamp);
 for(const auto& id:prepared.original_.worker_actor_ids){auto frozen=take(capture_care_handover_worker(a.body,id,*prepared.stamp_,x.stamps,x.versions));if(!frozen.result.allowed)fail("CareHandoverWorkerCapabilityDenied",id,ErrorKind::rule);prepared.workers_.push_back(std::move(frozen));}
 return {true,std::move(prepared),{}};
}catch(Failure& e){return {false,{},std::move(e.error)};}}
// 应用已准备的照护交接并逐项保留原始输入；身份、覆盖或身体变化均沿原错误拒绝。
Outcome<PendingCareHandoverStart> apply_care_handover_start_in_candidate(
    CommonStore &c, OrderedTable<CareHandoverExecution> &executions,
    const CareHandoverStartContext &x, CareHandoverStartOriginalWrite &original,
    PreparedCareHandoverStart &&prepared) {
  try {
    const auto now = context(x);
    const auto &h = x.command_sources.handovers;
    const auto &a = h.acceptance;
    if (prepared.common_ != &c || prepared.executions_ != &executions ||
        &executions != &h.executions || &c != &a.goals.common ||
        prepared.originals_ != &x.originals ||
        static_cast<const CareHandoverStartOriginalRead *>(&original) !=
            &x.originals ||
        !original.bound_to(c) || !prepared.stamp_ || !same(now, prepared.at_))
      fail("CareHandoverSamePreparedCandidateRequired");
    check(x.stamps.validate(*prepared.stamp_, c.state().identity,
                            prepared.stamp_->mode(), c, x.versions));
    const auto *accepted = take(
        read_accepted_caregiver(a, care_acceptance_source(prepared.accepted_)));
    const auto &requirement = goal(x, *accepted, now);
    if (requirement.id != prepared.requirement_ ||
        requirement.current_transition_ref.id !=
            prepared.requirement_transition_ ||
        accepted->terms.handover_half_days != prepared.duration_)
      fail("CareHandoverPreparedTermsChanged");
    workers(c, *accepted, prepared.actor_, prepared.original_.worker_actor_ids,
            false);
    for (const auto &pair : executions.rows())
      if (pair.second.requirement_id == requirement.id &&
          !pair.second.completion_event_ref && !pair.second.terminal_source_ref)
        fail("CareHandoverAlreadyActive", requirement.id, ErrorKind::rule);
    CareHandoverExecution e;
    e.handover_id = take(c.next_id("care_handover"));
    e.requirement_id = requirement.id;
    e.accepted_care_terms_ref = accepted->accepted_id;
    e.original_action_ref = take(c.next_id("action"));
    e.execution_binding =
        ActorActionWorkBinding{e.original_action_ref, "care_handover",
                               care_handover_execution_source(e.handover_id)};
    e.handover_rule_ref = prepared.original_.handover_rule_ref;
    ActionCoreView action{
        e.original_action_ref, "care_handover",
        prepared.actor_,       prepared.original_.worker_actor_ids,
        now.half_day_tick,     prepared.duration_};
    check(adopt_started_action_in_candidate(c, action));
    const auto id = e.handover_id;
    if (!executions.insert(id, std::move(e)))
      fail("DuplicateCareHandoverExecution", id);
    check(c.append_fact({id, "care_handover_execution", "care_requirements"}));
    auto *stored = executions.find(id);
    auto applied = take(original.reserve_started_resources_in_candidate(
        c, *stored, *accepted, prepared.original_));
    if (executions.find(id) != stored ||
        !stored->handover_execution_boundaries.empty() ||
        stored->original_action_ref != action.id)
      fail("CareHandoverStartOwnerChangedSource", id);
    refs(applied.actual_participation_source_refs);
    refs(applied.facility_use_binding_refs);
    refs(applied.authority_source_refs);
    if (applied.actual_participation_source_refs.empty())
      fail("CareHandoverActualParticipationRequired", id);
    for (const auto &input : prepared.original_.original_input_sources) {
      bool retained = false;
      // 按原次序检查各个真实来源容器，不合并类型，也不跳过后续覆盖检查。
      const auto retain_input = [&](const auto &group) {
        for (const auto &ref : group)
          if (ref.source_schema == input.source_schema &&
              same_source_ref(ref.source_ref, input.source_ref))
            retained = true;
      };
      retain_input(applied.actual_participation_source_refs);
      retain_input(applied.facility_use_binding_refs);
      retain_input(applied.authority_source_refs);
      retain_input(accepted->terms.allowed_dependency_refs);
      retain_input(accepted->terms.provision_arrangement_refs);
      retain_input(accepted->terms.fee_arrangement_refs);
      retain_input(accepted->terms.guardianship_permission_refs);
      if (accepted->provider.institutional_assignment_ref &&
          accepted->provider.institutional_assignment_ref->source_schema ==
              input.source_schema &&
          same_source_ref(
              accepted->provider.institutional_assignment_ref->source_ref,
              input.source_ref))
        retained = true;
      if (!retained)
        fail("CareHandoverActualStartInputNotRetained", input.source_ref.id);
    }

    // 来源所有者不得在真实捕获与本次开启之间更改身体输入。
    // 完整冻结来源核验与命令封存分别执行。
    for (const auto &frame : prepared.workers_) {
      check(
          validate_frozen_character_capability_original_prefix(a.body, frame));
      auto actual =
          take(read_current_character_condition(a.body, frame.actor_id));
      if (actual.head.condition_revision !=
              frame.condition_head_snapshot.condition_revision ||
          actual.head.initial_condition_seed_ref.seed_key !=
              frame.condition_head_snapshot.initial_condition_seed_ref
                  .seed_key ||
          bool(actual.head.latest_receipt_ref) !=
              bool(frame.condition_head_snapshot.latest_receipt_ref) ||
          (actual.head.latest_receipt_ref &&
           actual.head.latest_receipt_ref->result_id !=
               frame.condition_head_snapshot.latest_receipt_ref->result_id) ||
          actual.body.alive != frame.body_scalars.alive ||
          actual.body.hp != frame.body_scalars.hp ||
          actual.body.fatigue != frame.body_scalars.fatigue ||
          actual.body.injury != frame.body_scalars.injury)
        fail("CareHandoverStartBodyChanged", frame.actor_id);
    }
    PendingCareHandoverStart pending;
    pending.common_ = &c;
    pending.executions_ = &executions;
    pending.originals_ = &original;
    pending.actor_ = prepared.actor_;
    pending.handover_ = id;
    pending.action_ = action.id;
    pending.at_ = now;
    auto &b = pending.opening_;
    b.boundary_id = take(c.next_id("care_handover_boundary"));
    b.execution_ref = id;
    b.occurred_at = now;
    b.transition_rule_ref = prepared.original_.transition_rule_ref;
    b.worker_capability_inputs = std::move(prepared.workers_);
    b.actual_participation_source_refs =
        std::move(applied.actual_participation_source_refs);
    b.facility_use_binding_refs = std::move(applied.facility_use_binding_refs);
    CareHandoverActorCommand command;
    command.actor_id = prepared.actor_;
    command.accepted_terms_ref = accepted->accepted_id;
    command.authority_source_refs = std::move(applied.authority_source_refs);
    b.cause = std::move(command);
    prepared.common_ = nullptr;
    prepared.stamp_.reset();
    take(read_pending_care_handover_started_source(pending, x));
    return {true, std::move(pending), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<MomentOwnerSource> read_pending_care_handover_started_source(const PendingCareHandoverStart& p,const CareHandoverStartContext& x){try{
 const auto now=context(x);const auto& h=x.command_sources.handovers;const auto& c=h.acceptance.goals.common;if(p.common_!=&c||p.executions_!=&h.executions||p.originals_!=&x.originals||!same(p.at_,now))fail("CareHandoverLivePendingStartRequired",p.handover_);
 const auto* e=h.executions.find(p.handover_);const auto* action=c.action(p.action_);const auto* accepted=e?take(read_accepted_caregiver(h.acceptance,care_acceptance_source(e->accepted_care_terms_ref))):nullptr;
 if(!e||!accepted||e->original_action_ref!=p.action_||!e->handover_execution_boundaries.empty()||!e->work_period_sources.empty()||e->completion_event_ref||e->terminal_source_ref||!action||action->kind!="care_handover"||action->actor_id!=p.actor_||action->start_tick!=p.at_.half_day_tick||action->remaining_ticks!=accepted->terms.handover_half_days||p.opening_.execution_ref!=e->handover_id||!same(p.opening_.occurred_at,p.at_)||find_fixed_elapsed(h.elapsed,p.action_))fail("CareHandoverOriginalPendingStartMismatch",p.handover_);
 header(c,*e);workers(c,*accepted,p.actor_,action->participants,true,p.action_);if(action->participants.size()!=p.opening_.worker_capability_inputs.size())fail("CareHandoverPendingWorkerSetMismatch",p.handover_);for(std::size_t i=0;i<action->participants.size();++i)if(action->participants[i]!=p.opening_.worker_capability_inputs[i].actor_id)fail("CareHandoverPendingWorkerSetMismatch",p.handover_);
 for(const auto& frame:p.opening_.worker_capability_inputs){check(validate_frozen_character_capability_original_prefix(h.acceptance.body,frame));auto actual=take(read_current_character_condition(h.acceptance.body,frame.actor_id));if(actual.head.condition_revision!=frame.condition_head_snapshot.condition_revision||actual.body.alive!=frame.body_scalars.alive||actual.body.hp!=frame.body_scalars.hp||actual.body.fatigue!=frame.body_scalars.fatigue||actual.body.injury!=frame.body_scalars.injury)fail("CareHandoverPendingBodyChanged",frame.actor_id);}
 check(x.originals.validate_started_sources(*e,*accepted,p.opening_));return {true,{care_handover_execution_source(e->handover_id),3},{}};
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<StableId> bind_care_handover_started_command_in_candidate(CommonStore& c,OrderedTable<CareHandoverExecution>& executions,ActionElapsedStore& elapsed,const CareHandoverStartContext& x,const CareHandoverBusinessRead& business,PendingCareHandoverStart& pending,const StableId& actor,Integer sequence,const CommitResult& result){try{
 const auto now=context(x);const auto& h=x.command_sources.handovers;if(&c!=&h.acceptance.goals.common||&executions!=&h.executions||&elapsed!=&h.elapsed||pending.common_!=&c||pending.executions_!=&executions||pending.originals_!=&x.originals||actor!=pending.actor_||!same(now,pending.at_))fail("CareHandoverSamePendingCandidateRequired");
 const auto* cached=c.state().results.find(actor+":"+std::to_string(sequence));const auto* seq=c.state().request_sequences.find(actor);const auto* started=result.details?std::get_if<ActionStarted>(&result.details->data):nullptr;
 if(sequence<1||!cached||cached->get()!=&result||!seq||*seq!=sequence||!result.ok||result.kind!="start_care_handover"||result.revision!=c.revision()||result.tick!=now.half_day_tick||!started||started->action_id!=pending.action_||c.state().history.empty())fail("CareHandoverActualSealedResultRequired",result.result_id);const auto& history=c.state().history.back();if(history.actor_id!=actor||history.result_id!=result.result_id||history.kind!=result.kind||history.tick!=result.tick)fail("CareHandoverActualSealedHistoryRequired",result.result_id);
 auto* e=executions.find(pending.handover_);if(!e)fail("CareHandoverOriginalStartMissing",pending.handover_);
 if(!e->handover_execution_boundaries.empty()){const auto& first=e->handover_execution_boundaries.front();const auto* original=std::get_if<CareHandoverActorCommand>(&first.cause);if(first.boundary_id!=pending.opening_.boundary_id||!original||original->actor_id!=actor||original->command_binding.command_kind!=result.kind||original->command_binding.request_seq!=sequence||original->command_binding.result_id!=result.result_id)fail("CareHandoverConflictingCommandSeal",e->handover_id);check(validate_care_handover_command_source(x.command_sources,*e,first));return adopt_care_handover_boundary_in_candidate(c,executions,elapsed,h,business,first.boundary_id);}
 take(read_pending_care_handover_started_source(pending,x));auto& command=std::get<CareHandoverActorCommand>(pending.opening_.cause);command.command_binding={result.kind,sequence,result.result_id};e->handover_execution_boundaries.push_back(std::move(pending.opening_));auto& b=e->handover_execution_boundaries.back();pending.opening_.boundary_id=b.boundary_id; // Retain only the real id for exact same-result replay.
 auto& stored_command=std::get<CareHandoverActorCommand>(b.cause);stored_command.route_basis=take(capture_care_handover_command_route(x.command_sources,b.boundary_id));check(validate_care_handover_command_source(x.command_sources,*e,b));check(x.originals.validate_started_sources(*e,*take(read_accepted_caregiver(h.acceptance,care_acceptance_source(e->accepted_care_terms_ref))),b));
 return adopt_care_handover_boundary_in_candidate(c,executions,elapsed,h,business,b.boundary_id);
}catch(Failure& e){return {false,{},std::move(e.error)};}}
}
