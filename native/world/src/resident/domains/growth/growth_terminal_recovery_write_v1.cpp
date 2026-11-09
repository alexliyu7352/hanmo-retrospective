#include "growth_terminal_recovery_write_v1.hpp"
#include "growth_model.hpp"
#include "growth_fixed_execution.hpp"
#include "growth_recovery_qualification.hpp"
#include "resident/domains/actor_foundation/character_condition_replay.hpp"
#include "resident/domains/actor_foundation/condition_application_fact.hpp"
#include "resident/domains/actor_foundation/injury_progress_replay_v1.hpp"
#include "resident/domains/actor_foundation/injury_source_links_v1.hpp"
#include "resident/domains/actor_foundation/injury_sources_v1.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
struct PreparedTerminalRecoveryV1::State {
  CheckpointMemoryCharge copied_fields;
  const CommonStore *common = nullptr;
  const GrowthState *growth = nullptr;
  const AuthoredActorFoundationState *foundation = nullptr;
  CharacterInjurySourcesStateV1 *injury = nullptr;
  std::shared_ptr<const ActorConditionCaptureIdentity> capture;
  ActorConditionSavepointBoundary savepoint;
  WorldIdentity identity;
  WorldMoment at;
  StableId action, actor;
  ActorConditionHead head;
  FrozenBodyScalars before;
  GrowthRecoveryCredit credit;
  Integer hp_gain = 0, fatigue_delta = 0;
  std::size_t draft_offset = 0;
  bool consumed = false;
};
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
bool same(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
} // namespace
Outcome<PreparedTerminalRecoveryV1>
prepare_terminal_recovery_v1(CommonStore &common,
                             AuthoredActorFoundationState &foundation,
                             CharacterInjurySourcesStateV1 &injury,
                             const CharacterInjuryReplaySourcesV1 &context,
                             const GrowthFixedStepPermit &gate,
                             const GrowthRecoveryOccupationRead &occupation) {
  try {
    const auto &body = context.conditions;
    if (&body.common != &common || &body.state != &foundation ||
        &context.injury != &injury || !body.growth || !body.growth_catalog ||
        !body.shared || !body.injury_recovery_catalog || !body.elapsed ||
        !body.scale)
      fail("TerminalRecoverySameCandidateRequired", gate.action_id());
    const auto *action = common.action(gate.action_id());
    if (!action || action->kind != "service" || !gate.final_gate())
      fail("TerminalRecoveryActualFullServiceRequired", gate.action_id());
    take(
        validate_growth_fixed_step_permit(gate, common, *body.growth, *action));
    if (take(compare_world_moments(gate.at(), body.actual_now)))
      fail("TerminalRecoveryActualMomentRequired", action->id);
    take(validate_growth_recovery_catalog(*body.shared, *body.growth_catalog,
                                          *body.injury_recovery_catalog));
    const auto *activity = body.growth->life_activities.find(action->id);
    const auto *terms =
        activity ? std::get_if<ServiceTerms>(&activity->terms) : nullptr;
    const auto *method = activity
                             ? body.injury_recovery_catalog->service_roles.find(
                                   activity->context.role_id)
                             : nullptr;
    if (!activity || !terms || !method ||
        activity->view.kind != LifeActivityKind::service ||
        activity->view.activity != "rest" ||
        activity->view.phase != LifeActivityPhase::active ||
        activity->view.actor_id != action->actor_id || terms->hp_gain < 0 ||
        terms->hp_gain > 100 || terms->fatigue_delta < -100 ||
        terms->fatigue_delta > 100)
      fail("TerminalRecoveryActualMethodRequired", action->id);


    auto qualification_memory = CheckpointMemoryCharge(0);
    GrowthRecoveryService qualified_service{activity->view.actor_id,
                                            activity->view.provider_actor_id,
                                            activity->context.role_id,
                                            activity->context.place_id,
                                            *method,
                                            activity->view.start_tick,
                                            terms->duration,
                                            {}};
    take(validate_growth_recovery_qualification_now(
        common, *body.shared, *body.growth_catalog,
        *body.injury_recovery_catalog, *body.growth, occupation,
        qualified_service));
    const auto *actor = common.state().actors.find(action->actor_id);
    const auto *head =
        foundation.character_condition_extensions.actor_condition_heads.find(
            action->actor_id);
    if (!actor || !actor->alive || !head ||
        head->condition_revision == std::numeric_limits<Integer>::max())
      fail("TerminalRecoveryActualActorRequired", action->id);
    const auto verified = take(
        read_character_condition_verified_prefix(body, *head, body.actual_now));
    if (verified.body.hp != actor->hp ||
        verified.body.fatigue != actor->fatigue ||
        verified.body.injury != actor->injury ||
        verified.body.alive != actor->alive)
      fail("TerminalRecoveryBeforeBodyMismatch", action->id);
    const auto progress = take(read_character_injury_progress_at_head_v1(
        context, *head, body.actual_now));


    auto completion_memory = CheckpointMemoryCharge(0);
    const ActionElapsedEntryRef entry{action->id};
    const auto actual = take(read_fixed_service_completion(
        *body.elapsed, entry, body.actual_now, *body.scale));
    if (take(compare_world_moments(actual.completed_at, body.actual_now)))
      fail("TerminalRecoveryActualFullServiceRequired", action->id);
    const auto work =
        take(subtract_work_units(actual.actual_service.credited_after,
                                 actual.actual_service.credited_before));
    const auto duration =
        take(scale_units_to_duration(work, body.scale->units_per_half_day));
    const auto halves = take(floor_fraction(duration.value));
    if (halves < 1 ||
        take(compare_fractions(work.value,
                               take(fixed_duration_work_units(
                                        halves, body.scale->units_per_half_day))
                                   .value)))
      fail("TerminalRecoveryWholeServiceRequired", action->id);
    auto p = std::make_shared<PreparedTerminalRecoveryV1::State>();
    p->credit = take(apply_growth_terminal_recovery_points_v1(
        progress, growth_recovery_service_points(*method), halves));
    for (const auto &row : injury.applications.rows()) {
      const auto *old = std::get_if<CharacterTreatmentActionSourceV1>(
          &row.second.original_source);
      const auto *rest =
          std::get_if<OriginalRestProvidedCause>(&row.second.original_source);
      if ((old && old->action_id == action->id) ||
          (rest && rest->action_id == action->id))
        fail("TerminalRecoveryAlreadyApplied", action->id);
    }
    p->capture = common.actor_condition_capture_identity();
    if (!p->capture)
      fail("TerminalRecoveryCaptureRequired", action->id);
    p->savepoint = take(common.actor_condition_savepoint_boundary());


    p->copied_fields = CheckpointMemoryCharge(0);
    p->common = &common;
    p->growth = body.growth;
    p->foundation = &foundation;
    p->injury = &injury;
    p->action = action->id;
    p->actor = action->actor_id;
    p->head = *head;
    p->identity = common.state().identity;
    p->at = body.actual_now;
    p->before = {actor->alive, actor->hp, actor->fatigue, actor->injury, {}};
    p->hp_gain = terms->hp_gain;
    p->fatigue_delta = terms->fatigue_delta;
    p->draft_offset = common.actor_condition_execution_drafts().size();
    PreparedTerminalRecoveryV1 out;
    out.state_ = std::move(p);
    return {true, std::move(out), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<GrowthRecoveryCredit> read_terminal_recovery_service_write_v1(
    const PreparedTerminalRecoveryV1 &prepared, const CommonStore &common,
    const GrowthState &growth, const ActionCoreView &action) {
  try {
    const auto p = prepared.state_;
    if (!p || p->consumed || p->common != &common || p->growth != &growth ||
        p->action != action.id || p->actor != action.actor_id ||
        p->capture != common.actor_condition_capture_identity() ||
        !same(p->identity, common.state().identity) ||
        common.actor_condition_execution_drafts().size() != p->draft_offset ||
        action.kind != "service")
      fail("TerminalRecoveryPreparedWriteExpired", action.id);
    take(common.validate_actor_condition_savepoint_boundary(p->savepoint));
    const auto *live = common.action(action.id);
    if (!live || live->actor_id != p->actor || live->kind != "service" ||
        live->remaining_ticks != 0)
      fail("TerminalRecoveryOriginalActionNotCompleted", action.id);
    const auto *actor = common.state().actors.find(p->actor);
    const auto *activity = growth.life_activities.find(p->action);
    const auto *terms =
        activity ? std::get_if<ServiceTerms>(&activity->terms) : nullptr;
    if (!actor || !terms || activity->view.phase != LifeActivityPhase::active ||
        terms->hp_gain != p->hp_gain ||
        terms->fatigue_delta != p->fatigue_delta || actor->hp != p->before.hp ||
        actor->fatigue != p->before.fatigue ||
        actor->injury != p->before.injury || actor->alive != p->before.alive)
      fail("TerminalRecoveryPreparedBodyChanged", action.id);
    return {true, p->credit, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<BoundActorConditionWrite> bind_terminal_recovery_condition_v1(
    PreparedTerminalRecoveryV1 &prepared, CommonStore &common,
    AuthoredActorFoundationState &foundation,
    CharacterInjurySourcesStateV1 &injury,
    const CharacterInjuryReplaySourcesV1 &context) {
  try {
    const auto p = prepared.state_;
    const auto &body = context.conditions;
    if (!p || p->consumed || p->common != &common ||
        p->foundation != &foundation || p->injury != &injury ||
        &body.common != &common || &body.state != &foundation ||
        &context.injury != &injury || p->growth != body.growth ||
        p->capture != common.actor_condition_capture_identity() ||
        !same(p->identity, common.state().identity) ||
        take(compare_world_moments(p->at, body.actual_now)))
      fail("TerminalRecoveryBindExpired");
    take(common.validate_actor_condition_savepoint_boundary(p->savepoint));
    const auto *activity = body.growth->life_activities.find(p->action);
    const auto *terms =
        activity ? std::get_if<ServiceTerms>(&activity->terms) : nullptr;
    if (!activity || !terms ||
        activity->view.phase != LifeActivityPhase::completed ||
        !activity->view.success || !activity->view.code.empty() ||
        activity->view.completed_tick != p->at.half_day_tick ||
        activity->wage_paid != terms->wage || terms->hp_gain != p->hp_gain ||
        terms->fatigue_delta != p->fatigue_delta)
      fail("TerminalRecoveryOriginalCompletionRequired", p->action);
    const auto &drafts = common.actor_condition_execution_drafts();
    if (drafts.size() != p->draft_offset + 1)
      fail("TerminalRecoveryActualWriteCountMismatch", p->action);
    const auto &draft = drafts[p->draft_offset];
    const auto *actor = common.state().actors.find(p->actor);
    auto *head =
        foundation.character_condition_extensions.actor_condition_heads.find(
            p->actor);
    if (!actor || !head ||
        head->condition_revision != p->head.condition_revision ||
        bool(head->latest_receipt_ref) != bool(p->head.latest_receipt_ref) ||
        (head->latest_receipt_ref &&
         head->latest_receipt_ref->result_id !=
             p->head.latest_receipt_ref->result_id) ||
        draft.kind != ActorConditionExecutionDraft::Kind::mutation ||
        draft.actor_id != p->actor || !draft.before || !draft.hp_written ||
        !draft.fatigue_written || !draft.injury_written ||
        draft.alive_written || !same(draft.identity, p->identity) ||
        draft.sequence != Integer(p->draft_offset + 1) ||
        draft.before->hp != p->before.hp ||
        draft.before->fatigue != p->before.fatigue ||
        draft.before->injury != p->before.injury ||
        draft.before->alive != p->before.alive || draft.after.hp != actor->hp ||
        draft.after.fatigue != actor->fatigue ||
        draft.after.injury != actor->injury ||
        draft.after.alive != actor->alive || actor->alive != p->before.alive ||
        actor->injury != p->credit.after.injury ||
        actor->hp != std::clamp<Integer>(
                         p->before.hp + p->hp_gain, 0,
                         growth_injury_hp_limit(p->credit.after.injury)) ||
        actor->fatigue !=
            std::clamp<Integer>(p->before.fatigue + p->fatigue_delta, 0, 100) ||
        draft.next_instance_before != draft.next_instance_after)
      fail("TerminalRecoveryActualBodyMismatch", p->action);
    ConditionReceiptV2 receipt;
    receipt.result_id = take(common.next_id("condition"));
    receipt.actor_id = p->actor;
    receipt.cause = CharacterInjurySourceRefV1{receipt.result_id};
    receipt.business_key =
        CharacterInjuryConditionBusinessKeyV1{{receipt.result_id}, p->actor};
    // source1唯一编译终局给点规则；不是用rest_rule冒普通休养减一级。
    receipt.rule_ref = {"character_terminal_recovery_credit", 1,
                        body.catalog.identity.content_digest};
    receipt.effective_at.exact_moment = p->at;
    receipt.condition_revision_before = head->condition_revision;
    receipt.condition_revision_after = head->condition_revision + 1;
    receipt.scalar_changes = {
        {ConditionScalarChange::Field::hp, p->before.hp, actor->hp},
        {ConditionScalarChange::Field::fatigue, p->before.fatigue,
         actor->fatigue},
        {ConditionScalarChange::Field::injury, p->before.injury,
         actor->injury}};
    CharacterInjuryApplicationV1 application{
        p->actor, CharacterTreatmentActionSourceV1{p->action, 1}, p->credit};
    const auto id = receipt.result_id;
    if (!injury.applications.insert(id, std::move(application)) ||
        !foundation.character_condition_extensions.applied_receipts.insert(
            id, std::move(receipt)))
      fail("TerminalRecoveryApplicationCollision", id);
    head->condition_revision += 1;
    head->latest_receipt_ref = ConditionReceiptRefV2{id};
    BoundActorConditionWrite actual{{id}, p->capture, draft.sequence};
    take(append_condition_application_fact_in_candidate(common, foundation,
                                                        body.catalog, actual));
    take(validate_character_injury_bound_source_link_v1(
        common, foundation, body.catalog, injury, actual));
    p->consumed = true;
    return {true, std::move(actual), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
