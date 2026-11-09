#include "resident/domains/actor_foundation/long_rest_condition.hpp"
#include "growth_complete_validation.hpp"
#include "study_service_sources.hpp"
#include "growth_internal.hpp"
#include "growth_equal.hpp"
#include "growth_skill_qualification_v2.hpp"
#include "growth_fixed_input_changes.hpp"
#include <algorithm>
#include "resident/kernel/fixed_service_history.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
const ActionTimeRegistration& registration(const CharacterConditionSources& sources,const StudyFact& fact){if(!sources.action_registrations)fail("CompleteGrowthRegistrationsRequired");const auto kind=fact.terms.study_teaching_agreement?"teach_study":"study_skill";const ActionTimeRegistration* result=nullptr;for(const auto& row:*sources.action_registrations)if(row.owner_domain=="growth"&&row.action_kind==kind){if(result)fail("DuplicateCompleteGrowthRegistration",kind);result=&row;}if(!result)fail("CompleteGrowthRegistrationRequired",kind);return *result;}
// 按学习动作反读原边界；身份或 schema 不符时拒绝，不借通用 boundary 命名空间。
const LifeFixedServiceBoundary &study_boundary(const GrowthState &state,
                                               const SourceRef &ref,
                                               const StableId &action) {
  const auto *value = state.fixed_service_boundaries.find(ref.id);
  if (ref.domain != "growth" || ref.kind != "growth_fixed_service_boundary" ||
      !value || value->action_ref != action || value->source_schema != 1)
    fail("StudyActualBoundaryRequired", action);
  return *value;
}
const FrozenFacilityCapabilityInputs& capability(const LifeFixedServiceBoundary& source,const StableId& actor,const char* purpose){const FrozenFacilityCapabilityInputs* result=nullptr;for(const auto& person:source.input_snapshot.actors)if(person.actor_id==actor)for(const auto& frame:person.capability_frames)if(frame.purpose_ref.id==purpose){if(result)fail("DuplicateStudyBoundaryCapability",actor);result=&frame;}if(!result)fail("StudyBoundaryCapabilityRequired",actor);return *result;}
Integer provided_before(const GrowthState& state,const StableId& action,const WorldMoment& at){Integer count=0;for(const auto& source:state.lesson_capabilities.lesson_capability_sources)if(source.action_id==action&&take(compare_world_moments(source.provided_to,at))<=0)++count;return count;}
}
Status validate_complete_training_result_sources(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const StableId& id,const TrainingFact& fact,const GrowthCompleteValidationSources& exact){try{
 const auto& body=exact.condition;if(&body.common!=&common||body.growth!=&state||body.growth_catalog!=&catalog||body.shared!=&shared||!body.elapsed||!body.scale||!body.action_registrations||!catalog.complete_study_sources)fail("CompleteGrowthValidationCandidateRequired",id);
 const ActionTimeRegistration* selected=nullptr;for(const auto& row:*body.action_registrations)if(row.owner_domain=="growth"&&row.action_kind==fact.action_kind){if(selected)fail("DuplicateCompleteGrowthRegistration",fact.action_kind);selected=&row;}if(!selected)fail("CompleteGrowthRegistrationRequired",fact.action_kind);
 GrowthActionTimeRead timing(common,state,catalog,shared,*body.elapsed,*body.scale,body.catalog.identity,body.actual_now,{},body.interaction,body.interaction_catalog);const auto proof=take(timing.read_action_time_owner(*selected,id));const auto* elapsed=find_fixed_elapsed(*body.elapsed,id);if(!elapsed)fail("CompleteTrainingElapsedRequired",id);take(validate_fixed_elapsed_value(*elapsed,body.actual_now,*body.scale));take(timing.validate_retained_fixed_action_sources(*selected,proof,*elapsed));const auto started=take(timing.read_action_time_source(proof.started_source));if(fact.start_tick!=started.half_day_tick)fail("TrainingExactStartMismatch",id);
 if(fact.phase=="completed"){
  if(!proof.ended_source||proof.phase!=ActionTimeOwnerPhase::terminal)fail("TrainingExactTerminalRequired",id);const auto ended=take(timing.read_action_time_source(*proof.ended_source));if(fact.completed_tick!=ended.half_day_tick)fail("TrainingExactTerminalMismatch",id);take(timing.validate_adopted_terminal_source(*selected,id));
  // Full-service attempts keep the original requirement. An earlier terminal
  // is only an actual body/death/availability interruption, never a failed roll.
  if(take(compare_fractions(elapsed->credited_units.value,elapsed->required_units.value))){const auto* source=std::get_if<MomentOwnerSource>(&*proof.ended_source);if(fact.success||!source||source->source_schema!=1||source->source_ref.domain!="growth"||source->source_ref.kind!="growth_fixed_service_boundary")fail("TrainingActualServiceIncomplete",id);const auto* closed=state.fixed_service_boundaries.find(source->source_ref.id);const auto* terminal=closed?std::get_if<LifeOriginalTerminalApplication>(&closed->cause):nullptr;const auto* stop=terminal?std::get_if<LifeActualStopTrigger>(&terminal->trigger):nullptr;if(!closed||closed->kind!=LifeFixedBoundaryKind::stopped||!stop||!std::holds_alternative<LifeFixedInputChange>(stop->source))fail("TrainingActualInterruptionSourceRequired",id);take(validate_growth_fixed_input_change_source(body,state,closed->boundary_id));}
 }else if(fact.phase=="started"){
  const auto* action=common.action(id);if(!action||proof.phase!=ActionTimeOwnerPhase::working||action->start_tick!=started.half_day_tick||action->remaining_ticks!=fact.training.duration)fail("TrainingExactActiveProgressInvalid",id);
 }else fail("InvalidTrainingFact",id);
 // Success has exactly its original student/teacher writes; failed attempts have none. This replays real sources, not current fatigue.
 take(validate_growth_work_condition_coverage(common,body.state,body.catalog,state,catalog,shared,*body.elapsed,*body.scale,body.catalog.identity,*body.action_registrations,body.actual_now,body.interaction,body.interaction_catalog));
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
// 逐项反读完整学习的时间、身体与技能原源；任何投影不一致均拒绝。
Status validate_complete_study_result_sources(
    const CommonStore &common, const GrowthState &state,
    const GrowthCatalog &catalog, const CommonCatalog &shared,
    const GrowthAccess &access, const StableId &id, const StudyFact &fact,
    const GrowthCompleteValidationSources &exact) {
  try {
    const auto &body = exact.condition;
    if (&body.common != &common || body.growth != &state ||
        body.growth_catalog != &catalog || body.shared != &shared ||
        !body.elapsed || !body.scale || !catalog.complete_study_sources)
      fail("CompleteGrowthValidationCandidateRequired", id);
    take(validate_study_time_bindings(common, state, catalog, body.catalog,
                                      *body.elapsed, *body.scale,
                                      body.actual_now, access));
    const auto *time = state.lesson_capabilities.study_time_bindings.find(id);
    if (!time || !fact.study_service_binding || !growth::study_segmented(fact))
      fail("CompleteStudyExactSourceRequired", id);
    const auto &reg = registration(body, fact);
    GrowthActionTimeRead timing(common, state, catalog, shared, *body.elapsed,
                                *body.scale, body.catalog.identity,
                                body.actual_now, {}, body.interaction,
                                body.interaction_catalog);
    const auto proof = take(timing.read_action_time_owner(reg, id));
    const auto *elapsed = find_fixed_elapsed(*body.elapsed, id);
    if (!elapsed)
      fail("CompleteStudyElapsedRequired", id);
    take(validate_fixed_elapsed_value(*elapsed, body.actual_now, *body.scale));
    take(timing.validate_retained_fixed_action_sources(reg, proof, *elapsed));
    if (fact.start_tick != time->start_at.half_day_tick ||
        fact.lessons.size() > static_cast<std::size_t>(fact.terms.duration) ||
        fact.xp_settlement_before)
      fail("CompleteStudyOriginalProgressInvalid", id);
    for (std::size_t n = 0; n < fact.lessons.size(); ++n) {
      auto source = take(read_study_lesson_permanent_contribution(
          body, state, {id, static_cast<Integer>(n + 1)}));
      if (source.lesson->segment->adjustment)
        take(read_study_adjustment_permanent_contribution(
            common, state, catalog, {id, static_cast<Integer>(n + 1)},
            body.actual_now));
    }
    // Strict known-net check remains; full six-family qualification will merge
    // additional actual writers. Until those providers exist, never bless an
    // unexplained current XP increase as a complete new profile.
    const auto net = take(read_verified_growth_skill_net_v2(
        body, state, fact.terms.student_id, fact.terms.skill_id));
    const bool pause_field =
        std::find(fact.field_order.begin(), fact.field_order.end(), "pauses") !=
        fact.field_order.end();
    if (bool(fact.pauses) != pause_field)
      fail("InvalidStudyPause", id);
    if (fact.pauses) {
      if (fact.pauses->empty() || !growth::study_pause_supported(fact) ||
          fact.pauses->size() != time->pause_intervals.size())
        fail("InvalidStudyPause", id);
      Integer previous_lessons = 0, previous_credit = 0;
      for (std::size_t n = 0; n < fact.pauses->size(); ++n) {
        const auto &p = (*fact.pauses)[n];
        const auto &actual = time->pause_intervals[n];
        if (p.start_tick != actual.opened_at.half_day_tick ||
            p.after_lessons < previous_lessons ||
            p.after_lessons >= fact.terms.duration ||
            p.after_lessons != provided_before(state, id, actual.opened_at) ||
            (p.end_tick >= 0) != bool(actual.closed_at))
          fail("StudyExactPauseOriginalMismatch", id);
        previous_lessons = p.after_lessons;
        if (!actual.closed_at) {
          if (n + 1 != fact.pauses->size() || fact.phase != "started" ||
              p.end_tick != -1 || p.student_fatigue != -1 ||
              p.teacher_fatigue != -1 || p.student_xp != -1 || p.credited_xp)
            fail("StudyExactOpenPauseInvalid", id);
          continue;
        }
        if (!actual.closure_source_ref ||
            p.end_tick != actual.closed_at->half_day_tick || !p.credited_xp ||
            *p.credited_xp < previous_credit ||
            *p.credited_xp > growth::study_xp_gained(fact) ||
            p.student_xp < fact.terms.xp_before ||
            p.student_xp > net.current_xp)
          fail("StudyExactClosedPauseInvalid", id);
        previous_credit = *p.credited_xp;
        const auto &closed =
            study_boundary(state, *actual.closure_source_ref, id);
        const auto &student =
            capability(closed, fact.terms.student_id, "study");
        const auto &teacher =
            capability(closed, fact.terms.teacher_id, "provide_teaching");
        const auto s = take(read_character_condition_at_head(
            body, student.condition_head_snapshot, *actual.closed_at));
        const auto t = take(read_character_condition_at_head(
            body, teacher.condition_head_snapshot, *actual.closed_at));
        if (p.student_fatigue != s.body.fatigue ||
            p.teacher_fatigue != t.body.fatigue)
          fail("StudyExactPauseBodySourceMismatch", id);
      }
    }
    const auto count = static_cast<Integer>(fact.lessons.size());
    const auto *action = common.action(id);
    const auto *payload = state.actions.find(id);
    const auto *student = common.state().actors.find(fact.terms.student_id);
    const auto *teacher = common.actor(fact.terms.teacher_id);
    if (!student || !teacher)
      fail("StudyOriginalParticipantMissing", id);
    if (fact.phase == "started") {
      if (fact.completed_tick != -1 || fact.success || !fact.code.empty() ||
          time->terminal_time || !action || !payload ||
          action->kind != reg.action_kind ||
          action->actor_id != growth::study_initiator(fact.terms) ||
          action->start_tick != fact.start_tick ||
          action->remaining_ticks != fact.terms.duration - count ||
          action->remaining_ticks < 1)
        fail("StudyExactActiveProgressInvalid", id);
      const auto *terms = std::get_if<StudyTerms>(&payload->quote.data);
      const bool paused = growth::study_paused(fact);
      if (!terms || !growth::study_equal(*terms, fact.terms) ||
          action->participants !=
              (paused ? std::vector<StableId>{}
                      : growth::study_participants(fact.terms)))
        fail("StudyFactMismatch", id);
      if (fact.terms.study_teaching_agreement &&
          (!payload->quote.context ||
           !growth::context_equal(
               *payload->quote.context,
               fact.terms.study_teaching_agreement->request_context) ||
           payload->request.kind != "teach_study"))
        fail("StudyTeachingActionMismatch", id);
      const LifeFixedServiceBoundary *latest = nullptr;
      for (const auto &row : state.fixed_service_boundaries.rows())
        if (row.second.action_ref == id &&
            (!latest || row.second.boundary_ordinal > latest->boundary_ordinal))
          latest = &row.second;
      if (!latest)
        fail("StudyActualSkillInputRequired", id);
      auto adopted =
          take(read_growth_skill_qualification_before_fixed_boundary_v2(
              body, state, fact.terms.student_id, fact.terms.skill_id,
              latest->boundary_id));
      if (adopted.minimum_xp != net.current_xp)
        fail("StudyUnadoptedSkillChange", id);
      const auto fee = growth::original_study_lesson_cost(fact.terms.xp_before);
      if (growth::reserved(common.wallet(fact.terms.payer_wallet_id), id) !=
          fact.terms.fee - fee * count)
        fail("StudyReservationMismatch", id);
      if (paused && (student->action_id == id || teacher->action_id == id))
        fail("StudyOccupationMissing", id);
      for (const auto &participant : action->participants)
        if (!common.actor(participant) ||
            common.actor(participant)->action_id != id)
          fail("StudyOccupationMissing", participant);
    } else if (fact.phase == "completed") {
      if (action || payload || !time->terminal_time ||
          fact.completed_tick != time->terminal_time->at.half_day_tick ||
          growth::reserved(common.wallet(fact.terms.payer_wallet_id), id) != -1)
        fail("StudyExactCompletionInvalid", id);
      take(timing.validate_adopted_terminal_source(reg, id));
      if (fact.success) {
        if (!fact.code.empty() || count != fact.terms.duration ||
            fact.study_service_terminal)
          fail("StudyExactCompletionInvalid", id);
      } else if (fact.study_service_terminal) {
        if (!exact.deaths || !exact.death_read_stamp)
          fail("StudyActualTerminationReaderRequired", id);
        const StudyPendingReleaseProof *pending = nullptr;
        if (exact.pending_study_releases)
          for (const auto &value : *exact.pending_study_releases)
            if (value.action_id() && *value.action_id() == id) {
              if (pending)
                fail("DuplicateStudyPendingReleaseProof", id);
              pending = &value;
            }
        bool unsealed = false;
        for (const auto &released :
             fact.study_service_terminal->reservation_release_results)
          if (released.original_release_commit_ref.result_id.empty())
            unsealed = true;
        if (pending && unsealed)
          take(read_study_service_termination_in_capture(
              body, state, access, {id}, *exact.death_read_stamp, *exact.deaths,
              *pending));
        else
          take(read_study_service_termination(
              common, state, catalog, body.catalog, access, {id},
              body.actual_now, *exact.death_read_stamp, *exact.deaths));
      } else
        fail("StudyAcceptedActualTerminationRequired", id);
    } else
      fail("InvalidStudyFact", id);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
}
namespace hanmo::resident {
Status validate_complete_rest_result_sources(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const StableId& id,const RestFact& fact,const GrowthCompleteValidationSources& exact){try{
 const auto& body=exact.condition;if(&body.common!=&common||body.growth!=&state||body.growth_catalog!=&catalog||body.shared!=&shared||!body.elapsed||!body.scale||!body.action_registrations||!body.rest_estates||!catalog.complete_exact_period_sources)fail("CompleteLongRestSourcesRequired",id);
 const ActionTimeRegistration* reg=nullptr;for(const auto& candidate:*body.action_registrations)if(candidate.owner_domain=="growth"&&candidate.action_kind=="long_rest"){if(reg)fail("DuplicateLongRestRegistration",id);reg=&candidate;}if(!reg)fail("LongRestRegistrationRequired",id);
 if(id.rfind("action:",0)!=0||!common.actor(fact.actor_id)||fact.duration<1||fact.duration>720||fact.elapsed<0||fact.elapsed>fact.duration||fact.consumed<0||fact.start_tick<0||fact.actual_periods.size()!=static_cast<std::size_t>(fact.elapsed))fail("InvalidLongRestExactResult",id);
 GrowthActionTimeRead timing(common,state,catalog,shared,*body.elapsed,*body.scale,body.catalog.identity,body.actual_now,{},body.interaction,body.interaction_catalog);auto proof=take(timing.read_action_time_owner(*reg,id));const auto* elapsed=find_fixed_elapsed(*body.elapsed,id);if(!elapsed)fail("LongRestElapsedRequired",id);take(validate_fixed_elapsed_value(*elapsed,body.actual_now,*body.scale));take(timing.validate_retained_fixed_action_sources(*reg,proof,*elapsed));const auto started=take(timing.read_action_time_source(proof.started_source));if(fact.start_tick!=started.half_day_tick)fail("LongRestStartProjectionMismatch",id);
 if(fact.phase=="active"){const auto* action=common.action(id);const auto* payload=state.actions.find(id);if(!action||!payload||proof.phase==ActionTimeOwnerPhase::terminal||fact.completed_tick!=-1||!fact.stop_reason.empty()||action->actor_id!=fact.actor_id||action->kind!="long_rest"||action->participants!=std::vector<StableId>{fact.actor_id}||action->remaining_ticks!=fact.duration-fact.elapsed||payload->request.estate_id!=fact.estate_id||payload->request.duration!=fact.duration)fail("LongRestActiveProjectionMismatch",id);}
 else if(fact.phase=="completed"){if(common.action(id)||state.actions.find(id)||!proof.ended_source||proof.phase!=ActionTimeOwnerPhase::terminal||(fact.stop_reason!="duration"&&fact.stop_reason!="cancelled"&&fact.stop_reason!="RestFoodShortage"&&fact.stop_reason!="RestInterrupted"&&fact.stop_reason!="RestNeedsQuiet"))fail("LongRestTerminalSourceRequired",id);const auto ended=take(timing.read_action_time_source(*proof.ended_source));if(ended.half_day_tick!=fact.completed_tick)fail("LongRestEndProjectionMismatch",id);take(timing.validate_adopted_terminal_source(*reg,id));if(fact.stop_reason=="duration"&&fact.elapsed!=fact.duration)fail("LongRestIncompleteDuration",id);}
 else fail("InvalidRestPhase",id);
 LongRestPeriodSources periods{common,state,catalog,shared,*body.elapsed,*body.scale,body.catalog.identity,*body.action_registrations,*body.rest_estates,body.actual_now,&body};take(validate_long_rest_actual_periods(periods));take(validate_long_rest_condition_coverage(body));return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}
