#include "growth_action_time_read.hpp"
#include "growth_equal.hpp"
#include "growth_fixed_actor_input_source.hpp"
#include "growth_fixed_control_source.hpp"
#include "growth_fixed_input_changes.hpp"
#include "growth_fixed_start.hpp"
#include "growth_internal.hpp"
#include "growth_market_start_read.hpp"
#include "growth_trade_terminal_codec.hpp"
#include "growth_trade_terminal_read.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "hanmo/resident_world_api.hpp"
#include "resident/kernel/action_time_actor_history.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"
#include "resident/model/procurement_timing.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw Failure{common_failure(code, id, kind).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
template <class T, class F> Outcome<T> guard(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
// 原市场执行才持有交易开始时刻；FSL只能反核同刻，不能替换这个地址。
struct ScalarStartProof {
  const StableId *movement_id;
  GrowthScalarMarketStartView original;
};
ScalarStartProof
scalar_start(const CommonStore &common, const GrowthState &growth,
             const GrowthCatalog &catalog, const CommonCatalog &shared,
             const std::optional<GrowthMarketExactSources> &sources,
             const StableId &action, const WorldMoment &now,
             ExactArithmeticBudget budget) {
  if (!sources)
    fail("GrowthTradeMarketExactSourcesRequired", action);
  auto actual = take(read_growth_scalar_market_start_for_action_v2(
      common, shared, catalog, growth, sources->actor_foundation,
      sources->history_catalog, sources->history, action, now, budget));
  return {actual.movement_id, actual};
}
SourceRef boundary_source(const StableId &id) {
  return {"growth", "growth_fixed_service_boundary", id};
}
// 交易首尾沿原 owner proof 的真实时刻源；不把同刻 FSL 输入冒充市场或终局。
const MomentOwnerSource &
trade_endpoint_source(const ActionTimeOwnerProof &proof,
                      FixedTransitionKind kind) {
  const ElapsedTimeSource *source = nullptr;
  if (kind == FixedTransitionKind::started)
    source = &proof.started_source;
  else if (kind == FixedTransitionKind::terminal && proof.ended_source)
    source = &*proof.ended_source;
  const auto *actual =
      source ? std::get_if<MomentOwnerSource>(source) : nullptr;
  if (proof.action_kind != "trade" || !actual)
    fail("GrowthTradeOwnerTimeSourceRequired", proof.action_ref);
  return *actual;
}
bool same_catalog(const CatalogIdentity &a, const CatalogIdentity &b) {
  return a.content_id == b.content_id &&
         a.content_version == b.content_version &&
         a.content_digest == b.content_digest && a.ruleset_id == b.ruleset_id &&
         a.schema_sha256 == b.schema_sha256 &&
         a.pack_set_digest == b.pack_set_digest &&
         a.module_versions == b.module_versions &&
         a.artifact_sha256 == b.artifact_sha256;
}
bool fixed_kind(const StableId &k) {
  return k == "study_skill" || k == "teach_study" || k == "learn_skill" ||
         k == "practice_skill" || k == "teach_skill" || k == "service" ||
         k == "long_rest" || k == "trade";
}
void registration(const ActionTimeRegistration &r) {
  if (!fixed_kind(r.action_kind) || r.owner_domain != "growth" ||
      r.source_schema != 1 || r.mode != ActionTimeMode::fixed_duration ||
      r.remaining_meaning != ActionRemainingMeaning::service_estimate ||
      r.shared_gate_kind || !r.retained_wait_phases.empty())
    fail("GrowthActionTimeRegistrationMismatch", r.action_kind);
}
struct Execution {
  CheckpointMemoryCharge temporary_memory;
  StableId kind, actor;
  std::vector<StableId> participants;
  SourceRef source;
  Integer duration = 0, start_tick = 0, end_tick = -1;
  bool terminal = false, paused = false, success = false;
  StableId stop_code;
  LifeFixedOutcomeRef::Kind outcome = LifeFixedOutcomeRef::Kind::training;
  const TrainingFact *training = nullptr;
  const StudyFact *study = nullptr;
  const RestFact *rest = nullptr;
  const LifeActivityRecord *life = nullptr;
  const TradeTerminalTime *trade_terminal = nullptr;
};
void actors(const CommonStore &common, const Execution &e) {
  if (e.actor.empty())
    fail("GrowthTimeActorMissing");
  bool initiator = false;
  for (std::size_t n = 0; n < e.participants.size(); ++n) {
    const auto &id = e.participants[n];
    if (id.empty() || !common.actor(id))
      fail("GrowthTimeParticipantInvalid", id);
    for (std::size_t prior = 0; prior < n; ++prior)
      if (e.participants[prior] == id)
        fail("GrowthTimeParticipantInvalid", id);
    initiator = initiator || id == e.actor;
  }
  if (!initiator)
    fail("GrowthTimeInitiatorMissing", e.actor);
}
void header(const CommonStore &c, const StableId &id, const char *kind,
            const char *domain) {
  const auto *h = c.state().facts.find(id);
  if (!h || h->id != id || h->kind != kind || h->domain != domain)
    fail("GrowthTimeOriginalHeaderMismatch", id);
}
bool service_equal(const ServiceTerms &a, const ServiceTerms &b) {
  return a.duration == b.duration && a.cost == b.cost && a.wage == b.wage &&
         a.hp_gain == b.hp_gain && a.fatigue_delta == b.fatigue_delta &&
         a.wallet_id == b.wallet_id;
}
// 所有身份取原拥有表，已结束行也保留；无Activity时绝不拿Action缓存作终局来源。
Execution execution(const CommonStore &common, const GrowthState &state,
                    const GrowthCatalog &catalog, const CommonCatalog &shared,
                    const StableId &id, const WorldMoment &current) {
  take(compare_world_moments(current, current));
  if (current.half_day_tick < common.tick())
    fail("GrowthReadClockBeforeWorld", id);
  const auto *fact = state.facts.find(id);
  const auto *activity = state.life_activities.find(id);
  if (bool(fact) == bool(activity))
    fail("GrowthTimeOriginalOwnerAmbiguous", id);
  Execution e;
  // 生命周期令牌不计量字符串；原始生活活动仍须在复制前满足两人身份合同。
  if (activity && activity->view.participant_actor_ids.size() != 2)
    fail("GrowthTimeLifeIdentityInvalid", id);
  e.temporary_memory = CheckpointMemoryCharge(0);
  e.participants.reserve(2);
  if (fact) {
    if (const auto *f = std::get_if<TrainingFact>(fact)) {
      e.training = f;
      e.kind = f->action_kind;
      e.actor = f->actor_id;
      e.participants.push_back(e.actor);
      if (f->context && f->context->provider_actor_id != e.actor)
        e.participants.push_back(f->context->provider_actor_id);
      e.duration = f->training.duration;
      e.start_tick = f->start_tick;
      e.end_tick = f->completed_tick;
      e.terminal = f->phase == "completed";
      e.success = f->success;
      e.source = {"growth", "training", id};
      e.outcome = LifeFixedOutcomeRef::Kind::training;
      header(common, id, "training", "cultivation");
      const auto &t = f->training;
      if ((e.kind != "learn_skill" && e.kind != "practice_skill" &&
           e.kind != "teach_skill") ||
          (f->phase != "started" && !e.terminal) || t.student_id.empty() ||
          !common.actor(t.student_id) || t.skill_id.empty() ||
          !shared.places.find(t.place_id) || t.fee < 0 ||
          t.student_fatigue < 0 || t.student_fatigue > 180 ||
          t.teacher_fatigue < 0 || t.teacher_fatigue > 60 ||
          t.mastery_before < -1 || t.mastery_after < 0 || t.mastery_after > 100)
        fail("GrowthTimeTrainingTermsInvalid", id);
      if (e.kind == "practice_skill") {
        if (e.actor != t.student_id || !t.teacher_id.empty() ||
            e.duration > 8 || f->context)
          fail("GrowthTimePracticeTermsInvalid", id);
      } else if (t.teacher_id.empty() || t.teacher_id == t.student_id ||
                 !common.actor(t.teacher_id) || !f->context ||
                 e.actor !=
                     (e.kind == "teach_skill" ? t.teacher_id : t.student_id))
        fail("GrowthTimeTrainingParticipantsMismatch", id);
      if (e.kind == "learn_skill") {
        const auto *role = catalog.roles.find(f->context->role_id);
        const GrowthLesson *lesson = nullptr;
        if (role)
          for (const auto &row : role->lessons)
            if (row.skill_id == t.skill_id) {
              if (lesson)
                fail("DuplicateGrowthTimeLesson", id);
              lesson = &row;
            }
        if (!lesson || lesson->study_ticks != t.duration ||
            lesson->fee != t.fee || lesson->wallet_id != t.recipient_wallet_id)
          fail("GrowthTimeLessonRuleMismatch", id);
      } else if (e.kind == "teach_skill" && t.duration != 2)
        fail("GrowthTimeTeachingDurationMismatch", id);
    } else if (const auto *f = std::get_if<StudyFact>(fact)) {
      e.study = f;
      e.kind =
          f->terms.study_teaching_agreement ? "teach_study" : "study_skill";
      e.actor = growth::study_initiator(f->terms);
      e.participants.push_back(f->terms.study_teaching_agreement
                                   ? f->terms.teacher_id
                                   : f->terms.student_id);
      e.participants.push_back(f->terms.study_teaching_agreement
                                   ? f->terms.student_id
                                   : f->terms.teacher_id);
      e.duration = f->terms.duration;
      e.start_tick = f->start_tick;
      e.end_tick = f->completed_tick;
      e.terminal = f->phase == "completed";
      e.paused = !e.terminal && growth::study_paused(*f);
      e.success = f->success;
      e.stop_code = f->code;
      e.source = {"growth", "skill_study", id};
      e.outcome = LifeFixedOutcomeRef::Kind::study;
      header(common, id, "skill_study", "cultivation");
      const auto &t = f->terms;
      const auto *course =
          growth::course(catalog, t.context.role_id, t.skill_id);
      if ((f->phase != "started" && !e.terminal) ||
          f->actor_id != t.student_id || t.student_id == t.teacher_id ||
          t.context.provider_actor_id != t.teacher_id || !course ||
          course->mode != t.mode ||
          course->wallet_id != t.recipient_wallet_id ||
          course->settlement_policy != t.settlement_policy || t.duration < 2 ||
          t.duration > 6 || t.fee < 0 || t.xp_before < 0 || t.teacher_xp < 0 ||
          t.xp_limit < t.xp_before ||
          f->lessons.size() > static_cast<std::size_t>(t.duration))
        fail("GrowthTimeStudyTermsInvalid", id);
      for (const auto &lesson : f->lessons)
        if (lesson.fatigue_before < 0 || lesson.fatigue_before > 100 ||
            lesson.injury < 0 || lesson.xp < 0)
          fail("GrowthTimeLessonSourceInvalid", id);
    } else if (const auto *f = std::get_if<RestFact>(fact)) {
      e.rest = f;
      e.kind = "long_rest";
      e.actor = f->actor_id;
      e.participants.push_back(e.actor);
      e.duration = f->duration;
      e.start_tick = f->start_tick;
      e.end_tick = f->completed_tick;
      e.terminal = f->phase == "completed";
      e.success = e.terminal && f->stop_reason == "duration";
      e.stop_code = f->stop_reason;
      e.source = {"growth", "long_rest", id};
      e.outcome = LifeFixedOutcomeRef::Kind::rest;
      header(common, id, "long_rest", "services");
      if ((f->phase != "active" && !e.terminal) || f->estate_id.empty() ||
          !shared.items.find(f->food_item_id) || f->duration > 720 ||
          f->elapsed < 0 || f->elapsed > f->duration ||
          f->ration_per_person < 0 || f->consumed < 0 ||
          (e.success && f->elapsed != f->duration))
        fail("GrowthTimeRestTermsInvalid", id);
    } else
      fail("GrowthFactHasNoActionTime", id);
  } else {
    e.life = activity;
    const auto &v = activity->view;
    e.kind = v.kind == LifeActivityKind::service ? "service" : "trade";
    e.actor = v.actor_id;
    e.participants = v.participant_actor_ids;
    e.start_tick = v.start_tick;
    e.end_tick = v.completed_tick;
    e.terminal = v.phase != LifeActivityPhase::active;
    e.success = v.success;
    e.stop_code = v.code;
    e.source = {"growth", "life_activity", id};
    e.outcome = LifeFixedOutcomeRef::Kind::life_activity;
    header(common, id, "life_activity", "growth");
    if (v.fact_id != id || v.action_id != id ||
        activity->context.provider_actor_id != v.provider_actor_id ||
        (v.participant_actor_ids.size() != 2 ||
         v.participant_actor_ids[0] != v.actor_id ||
         v.participant_actor_ids[1] != v.provider_actor_id) ||
        static_cast<unsigned>(v.phase) > 2 || activity->cost_paid < 0 ||
        activity->wage_paid < 0)
      fail("GrowthTimeLifeIdentityInvalid", id);
    if (v.kind == LifeActivityKind::service) {
      const auto *terms = std::get_if<ServiceTerms>(&activity->terms);
      const auto *role = catalog.roles.find(activity->context.role_id);
      const auto *original = role ? role->activities.find(v.activity) : nullptr;
      if (!terms || !original || !service_equal(*terms, *original) ||
          activity->cost_paid != terms->cost ||
          activity->wage_paid !=
              (v.phase == LifeActivityPhase::completed ? terms->wage : 0))
        fail("GrowthTimeServiceTermsInvalid", id);
      e.duration = terms->duration;
    } else if (v.kind == LifeActivityKind::trade) {
      const auto *terms = std::get_if<TradeTerms>(&activity->terms);
      const auto *shop = terms ? catalog.shops.find(terms->shop_id) : nullptr;
      if (!terms || !shop || terms->quantity != v.quantity ||
          terms->total != v.amount || terms->item_id != v.item_id ||
          terms->side != v.activity || !shared.items.find(v.item_id) ||
          terms->quantity <= 0 || terms->total < 0)
        fail("GrowthTimeTradeTermsInvalid", id);
      e.duration = trade_action_ticks;
      take(growth_trade_terminal_codec::validate_activity(
          *activity, catalog.complete_study_sources));
      if (e.terminal) {
        const auto terminal = take(read_growth_trade_terminal_v2(
            common, state, catalog, shared, {"growth", "life_activity", id}, 2,
            current));
        e.trade_terminal = terminal.terminal;
      }
    } else
      fail("GrowthTimeLifeKindInvalid", id);
  }
  if (e.duration <= 0 || e.start_tick < 0 ||
      e.start_tick > current.half_day_tick ||
      (e.terminal
           ? (e.end_tick < e.start_tick || e.end_tick > current.half_day_tick)
           : e.end_tick != -1))
    fail("GrowthTimeOriginalPhaseInvalid", id);
  actors(common, e);
  const auto *action = common.action(id);
  const auto *payload = state.actions.find(id);
  if (e.terminal) {
    if (action || payload)
      fail("TerminalGrowthStillActive", id);
  } else {
    if (!action || !payload || action->actor_id != e.actor ||
        action->kind != e.kind ||
        (e.paused ? !action->participants.empty()
                  : action->participants != e.participants) ||
        action->start_tick != e.start_tick || payload->request.kind != e.kind ||
        payload->quote.duration != e.duration)
      fail("GrowthTimeActiveIdentityMismatch", id);
    if (e.training) {
      const auto *terms = std::get_if<TrainingTerms>(&payload->quote.data);
      if (!terms || !growth::training_equal(*terms, e.training->training))
        fail("GrowthTimeTrainingQuoteMismatch", id);
    }
    if (e.study) {
      const auto *terms = std::get_if<StudyTerms>(&payload->quote.data);
      if (!terms || !growth::study_equal(*terms, e.study->terms))
        fail("GrowthTimeStudyQuoteMismatch", id);
    }
  }
  return e;
}
using Chain = CheckpointVector<const LifeFixedServiceBoundary *>;
void owner_ref(const LifeFixedServiceBoundary &b, const Execution &e) {
  if (e.life) {
    if (b.owner_ref.kind != 1 || b.owner_ref.life_activity_id != b.action_ref ||
        b.owner_ref.terms_kind != (e.kind == "trade" ? 1 : 0) ||
        !b.owner_ref.action_id.empty() || b.input_snapshot.specific.kind != 1 ||
        b.input_snapshot.specific.life_activity.activity_id != b.action_ref ||
        b.input_snapshot.specific.life_activity.terms_kind !=
            b.owner_ref.terms_kind)
      fail("GrowthFixedLifeOwnerMismatch", b.boundary_id);
  } else {
    if (b.owner_ref.kind != 0 || b.owner_ref.action_id != b.action_ref ||
        !b.owner_ref.life_activity_id.empty() || b.owner_ref.terms_kind != 0 ||
        static_cast<Integer>(b.owner_ref.fact_kind) !=
            static_cast<Integer>(e.outcome) ||
        b.input_snapshot.specific.kind != 0 ||
        b.input_snapshot.specific.growth_fact.terms_action_ref != b.action_ref)
      fail("GrowthFixedFactOwnerMismatch", b.boundary_id);
  }
}
void rule(const LifeCatalogFieldRef &ref, const CatalogIdentity &identity) {
  if (!same_catalog(ref.catalog_identity, identity) ||
      ref.scenario_id.empty() || ref.entity_id.empty() ||
      ref.field_selector.empty())
    fail("GrowthFixedLockedRuleMismatch", ref.entity_id);
}
void inputs(const LifeFixedServiceBoundary &b, const Execution &e,
            const CatalogIdentity &identity, ExactArithmeticBudget budget) {
  owner_ref(b, e);
  const auto &actor_inputs = b.input_snapshot.actors;
  for (std::size_t n = 0; n < actor_inputs.size(); ++n) {
    const auto &actor = actor_inputs[n];
    for (std::size_t prior = 0; prior < n; ++prior)
      if (actor_inputs[prior].actor_id == actor.actor_id &&
          actor_inputs[prior].role == actor.role)
        fail("GrowthFixedActorInputIncomplete", b.boundary_id);
    if (actor.role.empty() || actor.origin_actor_key != actor.actor_id ||
        actor.origin_schema != 2 ||
        std::find(e.participants.begin(), e.participants.end(),
                  actor.actor_id) == e.participants.end() ||
        actor.capability_frames.empty() ||
        actor.actual_location_source_refs.empty() ||
        actor.participation_source_refs.empty() ||
        actor.occupation_source_refs.empty())
      fail("GrowthFixedActorInputIncomplete", b.boundary_id);
    for (const auto &frame : actor.capability_frames)
      if (frame.actor_id != actor.actor_id ||
          frame.condition_head_snapshot.actor_id != actor.actor_id ||
          take(compare_world_moments(frame.captured_at, b.occurred_at,
                                     budget)) ||
          take(compare_world_moments(frame.result.observed_at, b.occurred_at,
                                     budget)))
        fail("GrowthFixedCapabilityTimeMismatch", b.boundary_id);
  }
  for (const auto &id : e.participants)
    if (std::none_of(actor_inputs.begin(), actor_inputs.end(),
                     [&](const auto &actor) { return actor.actor_id == id; }))
      fail("GrowthFixedParticipantCoverageMismatch", b.boundary_id);
  if (b.input_snapshot.locked_rule_refs.empty())
    fail("GrowthFixedParticipantCoverageMismatch", b.boundary_id);
  for (const auto &r : b.input_snapshot.locked_rule_refs)
    rule(r, identity);
  if (e.life) {
    const auto &s = b.input_snapshot.specific.life_activity;
    if (e.kind == "service") {
      if (!s.service_role_ref || !s.activity_ref)
        fail("GrowthFixedServiceRuleMissing", b.boundary_id);
      rule(*s.service_role_ref, identity);
      rule(*s.activity_ref, identity);
      if (s.activity_ref->entity_id != e.life->context.role_id ||
          s.activity_ref->field_selector !=
              "activities." + e.life->view.activity)
        fail("GrowthFixedServiceRuleMismatch", b.boundary_id);
    } else if (!s.trade_settlement_ref || s.service_role_ref || s.activity_ref)
      fail("GrowthFixedTradeSourceRequired", b.boundary_id);
  } else
    rule(b.input_snapshot.specific.growth_fact
             .original_training_or_study_rule_ref,
         identity);
}
// 每个FSL输入只引用同笔实际市场开工，不允许同刻另一收据或另一个能力目的替代。
void scalar_inputs(const Chain &history, const ScalarStartProof &start,
                   const Execution &e, const CommonStore &common,
                   const CatalogIdentity &identity) {
  if (!start.movement_id || !start.original.activity ||
      e.duration != trade_action_ticks)
    fail("GrowthTradeOriginalDurationMismatch", e.source.id);
  for (const auto *boundary : history) {
    const auto &snapshot = boundary->input_snapshot;
    const auto &specific = snapshot.specific.life_activity;
    if (specific.terms_kind != 1 || specific.service_role_ref ||
        specific.activity_ref || specific.recovery_service_ref ||
        specific.original_quiet_or_rest_basis ||
        !specific.trade_settlement_ref ||
        specific.trade_settlement_ref->domain != "growth_market_history" ||
        specific.trade_settlement_ref->kind != "growth_market_movement" ||
        specific.trade_settlement_ref->id != *start.movement_id ||
        snapshot.locked_rule_refs.size() != 1 || snapshot.actors.size() != 2 ||
        !snapshot.facility_use_refs.empty())
      fail("GrowthTradeFixedInputMismatch", boundary->boundary_id);
    const auto &locked = snapshot.locked_rule_refs.front();
    if (!same_catalog(locked.catalog_identity, identity) ||
        locked.scenario_id != common.state().metadata.scenario_id ||
        locked.entity_id != "regional_life_v1" ||
        locked.field_selector != "trade_action_ticks")
      fail("GrowthTradeOriginalDurationRuleMismatch", boundary->boundary_id);
    if (boundary->kind == LifeFixedBoundaryKind::completed) {
      const auto *terminal =
          std::get_if<LifeOriginalTerminalApplication>(&boundary->cause);
      const auto *gate =
          terminal ? std::get_if<LifeFixedCompletionGate>(&terminal->trigger)
                   : nullptr;
      if (!gate || gate->last_actual_period_ref ||
          gate->original_required_rule_ref.scenario_id != locked.scenario_id ||
          gate->original_required_rule_ref.entity_id != locked.entity_id ||
          gate->original_required_rule_ref.field_selector !=
              locked.field_selector ||
          !same_catalog(gate->original_required_rule_ref.catalog_identity,
                        locked.catalog_identity))
        fail("GrowthTradeOriginalCompletionGateMismatch",
             boundary->boundary_id);
    }
    for (const auto &actor : snapshot.actors) {
      const bool initiator = actor.actor_id == e.actor;
      if (actor.actor_id !=
              (initiator ? e.actor : e.life->view.provider_actor_id) ||
          actor.role != (initiator ? "recipient" : "provider") ||
          actor.capability_frames.size() != 1 ||
          actor.capability_frames[0].purpose_ref.id !=
              "growth_trade_participant")
        fail("GrowthTradeOriginalParticipationMismatch", boundary->boundary_id);
    }
    if (snapshot.accepted_permission_refs.size() != 2 ||
        snapshot.resource_basis_refs.size() != 2)
      fail("GrowthTradeOriginalBasisMismatch", boundary->boundary_id);
    const auto &terms = snapshot.accepted_permission_refs[0];
    const auto *owned = std::get_if<LifeOwnedFieldRef>(&terms.original_ref);
    const auto &provider = snapshot.accepted_permission_refs[1];
    const auto *role = std::get_if<LifeCatalogFieldRef>(&provider.original_ref);
    if (terms.permission_kind != "trade_terms" || terms.source_schema != 1 ||
        !owned || !same_source_ref(owned->owner_ref, e.source) ||
        owned->field_selector != "terms" ||
        provider.permission_kind != "provider_role" ||
        provider.source_schema != 1 || !role ||
        !same_catalog(role->catalog_identity, identity) ||
        role->scenario_id != common.state().metadata.scenario_id ||
        role->entity_id != e.life->context.role_id ||
        role->field_selector != "role")
      fail("GrowthTradeOriginalPermissionMismatch", boundary->boundary_id);
    for (std::size_t n = 0; n < 2; ++n) {
      const auto &basis = snapshot.resource_basis_refs[n];
      const auto *ref = std::get_if<SourceRef>(&basis.original_source_ref);
      if (basis.purpose != (n == 0 ? LifeResourceBasisRef::Purpose::fee
                                   : LifeResourceBasisRef::Purpose::material) ||
          basis.source_schema != 2 || !ref ||
          !same_source_ref(*ref, *specific.trade_settlement_ref))
        fail("GrowthTradeOriginalResourceBasisMismatch", boundary->boundary_id);
    }
  }
}
void command(const CommonStore &common, const GrowthState &state,
             const InteractionState *interaction,
             const InteractionCatalog *interaction_catalog,
             const LifeActualCommand &c, const Execution &e,
             const LifeFixedServiceBoundary &b) {
  if (c.actor_id != e.actor || c.command_binding.request_seq < 0 ||
      c.command_binding.result_id.empty())
    fail("GrowthFixedCommandBindingInvalid", b.boundary_id);
  const auto &k = c.command_binding.command_kind;
  if (b.kind == LifeFixedBoundaryKind::started) {
    if (k != e.kind) {
      if (k != "dialogue_confirm")
        fail("GrowthFixedStartCommandMismatch", b.boundary_id);
      take(validate_growth_dialogue_confirmation(
          common, state, interaction, interaction_catalog,
          c.command_binding.result_id, c.command_binding.request_seq,
          c.actor_id, e.kind, b.action_ref, b.occurred_at));
    }
  } else if (b.kind == LifeFixedBoundaryKind::suspended) {
    if (k != "study_pause" || !e.study)
      fail("GrowthFixedPauseCommandMismatch", b.boundary_id);
  } else if (b.kind == LifeFixedBoundaryKind::resumed) {
    if (k != "study_resume" || !e.study)
      fail("GrowthFixedResumeCommandMismatch", b.boundary_id);
  } else if (k != "study_cancel" && k != "rest_cancel")
    fail("GrowthFixedStopCommandMismatch", b.boundary_id);
  const auto *actual = std::get_if<SourceRef>(&c.accepted_business_ref);
  if (!actual || !same_source_ref(*actual, e.source))
    fail("GrowthFixedAcceptedOwnerMismatch", b.boundary_id);
}
Chain chain(const CommonStore &common, const InteractionState *interaction,
            const InteractionCatalog *interaction_catalog,
            const GrowthState &state, const Execution &e, const StableId &id,
            const CatalogIdentity &identity, const WorldMoment &current,
            ExactArithmeticBudget budget) {
  Chain rows;
  for (const auto &row : state.fixed_service_boundaries.rows())
    if (row.second.action_ref == id) {
      if (row.first != row.second.boundary_id)
        fail("GrowthFixedBoundaryKeyMismatch", row.first);
      rows.push_back(&row.second);
    }
  std::sort(rows.begin(), rows.end(), [](const auto *a, const auto *z) {
    return a->boundary_ordinal < z->boundary_ordinal;
  });
  if (rows.empty())
    fail("GrowthExactStartSourceRequired", id);
  bool paused = false, ended = false;
  Integer ordinal = 0;
  const LifeFixedServiceBoundary *previous = nullptr;
  for (const auto *p : rows) {
    const auto &b = *p;
    if (b.source_schema != 1 || b.action_kind != e.kind ||
        b.boundary_ordinal != ordinal ||
        b.previous_boundary_ref !=
            (previous ? std::optional<StableId>{previous->boundary_id}
                      : std::nullopt) ||
        ended ||
        (previous && take(compare_world_moments(previous->occurred_at,
                                                b.occurred_at, budget)) > 0))
      fail("GrowthFixedBoundaryChainInvalid", b.boundary_id);
    take(compare_world_moments(b.occurred_at, b.occurred_at, budget));
    if (take(compare_world_moments(b.occurred_at, current, budget)) > 0)
      fail("GrowthFixedBoundaryInFuture", b.boundary_id);
    inputs(b, e, identity, budget);
    if (!previous) {
      if (b.kind != LifeFixedBoundaryKind::started ||
          b.occurred_at.half_day_tick != e.start_tick || b.original_outcome_ref)
        fail("GrowthFixedStartMismatch", id);
    } else if (b.kind == LifeFixedBoundaryKind::started)
      fail("DuplicateGrowthFixedStart", id);
    if (const auto *c = std::get_if<LifeActualCommand>(&b.cause))
      command(common, state, interaction, interaction_catalog, *c, e, b);
    else if (const auto *accepted =
                 std::get_if<LifeAcceptedExecution>(&b.cause)) {
      if (!valid_source_ref(accepted->accepted_source_ref) ||
          accepted->source_schema <= 0 ||
          accepted->execution_rule_ref.id.empty() ||
          accepted->actual_initiator_actor_id != e.actor)
        fail("GrowthFixedAcceptedExecutionInvalid", b.boundary_id);
    } else if (const auto *changed =
                   std::get_if<LifeActualInputChange>(&b.cause)) {
      take(validate_growth_fixed_input_change_reference(changed->change));
      if (changed->transition_rule_ref.id.empty() ||
          changed->transition_rule_ref.version <= 0 ||
          changed->transition_rule_ref.catalog_digest !=
              identity.content_digest)
        fail("GrowthFixedInputTransitionRuleRequired", b.boundary_id);
    }
    switch (b.kind) {
    case LifeFixedBoundaryKind::started:
      if (b.cause.index() > 1)
        fail("GrowthFixedStartCauseInvalid", id);
      break;
    case LifeFixedBoundaryKind::suspended:
      if (paused || b.cause.index() > 2)
        fail("GrowthFixedSuspensionInvalid", id);
      paused = true;
      break;
    case LifeFixedBoundaryKind::resumed:
      if (!paused || b.cause.index() > 2)
        fail("GrowthFixedResumeInvalid", id);
      paused = false;
      break;
    case LifeFixedBoundaryKind::input_rebound:
      if (b.cause.index() != 2)
        fail("GrowthFixedReboundInvalid", id);
      break;
    case LifeFixedBoundaryKind::completed:
    case LifeFixedBoundaryKind::stopped: {
      ended = true;
      const auto *terminal =
          std::get_if<LifeOriginalTerminalApplication>(&b.cause);
      if (!terminal || !e.terminal ||
          b.occurred_at.half_day_tick != e.end_tick ||
          (b.kind == LifeFixedBoundaryKind::completed) !=
              (terminal->disposition ==
               LifeOriginalTerminalApplication::Disposition::completed))
        fail("GrowthFixedTerminalSourceMismatch", id);
      // 终局持值独立来自原LifeActivity；FSL只核同一真实发生点，不能反向补值。
      if (e.trade_terminal &&
          take(compare_world_moments(e.trade_terminal->ended_at, b.occurred_at,
                                     budget)))
        fail("GrowthFixedTradeTerminalMomentMismatch", id);
      bool actual = false;
      for (const auto &r : terminal->actual_result_refs) {
        if (r.id != id || r.kind != e.outcome || r.period_ordinal ||
            !r.secondary_ref.empty() || actual)
          fail("GrowthFixedTerminalOutcomeMismatch", id);
        actual = true;
      }
      if (!actual || !b.original_outcome_ref ||
          b.original_outcome_ref->kind != e.outcome ||
          b.original_outcome_ref->id != id)
        fail("GrowthFixedTerminalOutcomeMissing", id);
      if (const auto *result =
              std::get_if<LifeOriginalResultTrigger>(&terminal->trigger)) {
        if (result->outcome.kind != e.outcome || result->outcome.id != id)
          fail("GrowthFixedTerminalTriggerMismatch", id);
      } else if (const auto *gate =
                     std::get_if<LifeFixedCompletionGate>(&terminal->trigger)) {
        if (b.kind != LifeFixedBoundaryKind::completed ||
            gate->action_ref != id)
          fail("GrowthFixedTerminalGateMismatch", id);
        rule(gate->original_required_rule_ref, identity);
      } else {
        const auto &stop = std::get<LifeActualStopTrigger>(terminal->trigger);
        if (b.kind != LifeFixedBoundaryKind::stopped ||
            stop.original_stop_rule_ref.id.empty())
          fail("GrowthFixedStopTriggerMismatch", id);
        if (const auto *c = std::get_if<LifeActualCommand>(&stop.source))
          command(common, state, interaction, interaction_catalog, *c, e, b);
        else
          take(validate_growth_fixed_input_change_reference(
              std::get<LifeFixedInputChange>(stop.source)));
      }
      break;
    }
    }
    if (ordinal == std::numeric_limits<Integer>::max())
      fail("GrowthFixedOrdinalOverflow", id);
    ++ordinal;
    previous = p;
  }
  if (ended != e.terminal || (!ended && paused != e.paused))
    fail("GrowthFixedOriginalPhaseMismatch", id);
  return rows;
}
FixedTransitionKind transition_kind(LifeFixedBoundaryKind k) {
  switch (k) {
  case LifeFixedBoundaryKind::started:
    return FixedTransitionKind::started;
  case LifeFixedBoundaryKind::suspended:
    return FixedTransitionKind::suspended;
  case LifeFixedBoundaryKind::resumed:
    return FixedTransitionKind::resumed;
  case LifeFixedBoundaryKind::input_rebound:
    return FixedTransitionKind::input_rebound;
  case LifeFixedBoundaryKind::completed:
  case LifeFixedBoundaryKind::stopped:
    return FixedTransitionKind::terminal;
  }
  fail("InvalidGrowthFixedBoundaryKind");
}
} // namespace
Status GrowthActionTimeRead::enumerate_action_time_owners(
    const ActionTimeRegistration &reg, ActionTimeOwnerIdSink &sink) const {
  return guard<Done>([&] {
    registration(reg);
    for (const auto &row : growth_.facts.rows()) {
      if (std::holds_alternative<BargainFact>(row.second))
        continue;
      auto e =
          execution(common_, growth_, catalog_, shared_, row.first, current_);
      if (e.kind == reg.action_kind)
        take(sink.append(row.first));
    }
    for (const auto &row : growth_.life_activities.rows()) {
      auto e =
          execution(common_, growth_, catalog_, shared_, row.first, current_);
      if (e.kind == reg.action_kind)
        take(sink.append(row.first));
    }
    return Done{};
  });
}
Outcome<ActionTimeOwnerProof>
GrowthActionTimeRead::read_action_time_owner(const ActionTimeRegistration &reg,
                                             const StableId &id) const {
  return guard<ActionTimeOwnerProof>([&] {
    registration(reg);
    auto e = execution(common_, growth_, catalog_, shared_, id, current_);
    if (e.kind != reg.action_kind)
      fail("GrowthTimeKindMismatch", id);
    auto history = chain(common_, interaction_, interaction_catalog_, growth_,
                         e, id, identity_, current_, budget_);
    // These views borrow the checked original inputs. The shared proof
    // constructor reserves its complete fresh payload before copying any owning
    // member.
    CheckpointVector<std::string_view> participants, gates;
    participants.reserve(e.participants.size());
    for (const auto &actor : e.participants)
      participants.emplace_back(actor);
    std::optional<ElapsedTimeSourceBuildView> ended;
    std::optional<std::string_view> terminal;
    if (e.terminal) {
      if (e.trade_terminal)
        ended = MomentOwnerSourceBuildView{{"growth", "life_activity", id}, 2};
      else
        ended = MomentOwnerSourceBuildView{{"growth",
                                            "growth_fixed_service_boundary",
                                            history.back()->boundary_id},
                                           1};
      terminal = id;
    }
    MomentOwnerSourceBuildView started{{"growth",
                                        "growth_fixed_service_boundary",
                                        history.front()->boundary_id},
                                       1};
    if (e.kind == "trade") {
      const auto original = scalar_start(common_, growth_, catalog_, shared_,
                                         market_exact_, id, current_, budget_);
      if (take(compare_world_moments(*original.original.started_at,
                                     history.front()->occurred_at, budget_)))
        fail("GrowthTradeStartBoundaryMismatch", id);
      scalar_inputs(history, original, e, common_, identity_);
      started = {{"growth_market_history", "growth_market_movement",
                  *original.movement_id},
                 2};
    }
    const ActionTimeOwnerProofBuildView view{
        id,
        e.kind,
        e.actor,
        {participants},
        {e.source.domain, e.source.kind, e.source.id},
        1,
        started,
        e.terminal ? ActionTimeOwnerPhase::terminal
        : e.paused ? ActionTimeOwnerPhase::suspended
                   : ActionTimeOwnerPhase::working,
        std::nullopt,
        ended,
        terminal,
        {gates},
        std::nullopt};
    return ActionTimeOwnerProof(view);
  });
}
Outcome<WorldMoment> GrowthActionTimeRead::read_action_time_source(
    const ElapsedTimeSource &source) const {
  return guard<WorldMoment>([&] {
    const auto *ref = std::get_if<MomentOwnerSource>(&source);
    if (ref && ref->source_schema == 2 &&
        ref->source_ref.domain == "growth_market_history" &&
        ref->source_ref.kind == "growth_market_movement") {
      if (!market_exact_)
        fail("GrowthTradeMarketExactSourcesRequired", ref->source_ref.id);
      return *take(read_growth_scalar_market_start_v2(
                       common_, shared_, catalog_, growth_,
                       market_exact_->actor_foundation,
                       market_exact_->history_catalog, market_exact_->history,
                       ref->source_ref, 2, current_, budget_))
                  .started_at;
    }
    // schema2终局只选原交易持值；这里不触中央累计或任何FSL查找。
    if (ref && ref->source_schema == 2 && ref->source_ref.domain == "growth" &&
        ref->source_ref.kind == "life_activity")
      return take(read_growth_trade_terminal_v2(
                      common_, growth_, catalog_, shared_, ref->source_ref,
                      ref->source_schema, current_, budget_))
          .terminal->ended_at;
    if (!ref || ref->source_schema != 1 || ref->source_ref.domain != "growth" ||
        ref->source_ref.kind != "growth_fixed_service_boundary")
      fail("GrowthExactTimeSourceRequired");
    const auto *row = growth_.fixed_service_boundaries.find(ref->source_ref.id);
    if (!row)
      fail("GrowthFixedBoundaryMissing", ref->source_ref.id);
    auto e = execution(common_, growth_, catalog_, shared_, row->action_ref,
                       current_);
    auto history = chain(common_, interaction_, interaction_catalog_, growth_,
                         e, row->action_ref, identity_, current_, budget_);
    if (std::find(history.begin(), history.end(), row) == history.end())
      fail("GrowthFixedBoundaryNotOwned", row->boundary_id);
    if (e.kind == "trade" && (row->kind == LifeFixedBoundaryKind::started ||
                              row->kind == LifeFixedBoundaryKind::completed ||
                              row->kind == LifeFixedBoundaryKind::stopped))
      fail("GrowthTradeTerminalOriginalSourceRequired", row->boundary_id);
    return row->occurred_at;
  });
}
Status GrowthActionTimeRead::validate_fixed_action_sources(
    const ActionTimeRegistration &reg, const ActionTimeOwnerProof &proof,
    const ActionElapsedState &elapsed,
    const std::vector<FixedServiceTransition> &transitions) const {
  return validate_fixed_sources(reg, proof, elapsed, transitions, false);
}
Status GrowthActionTimeRead::validate_retained_fixed_action_sources(
    const ActionTimeRegistration &reg, const ActionTimeOwnerProof &proof,
    const ActionElapsedState &elapsed) const {
  return validate_fixed_sources(reg, proof, elapsed,
                                elapsed_.fixed_service_transitions, true);
}
Status GrowthActionTimeRead::validate_fixed_sources(
    const ActionTimeRegistration &reg, const ActionTimeOwnerProof &proof,
    const ActionElapsedState &elapsed,
    const std::vector<FixedServiceTransition> &transitions,
    bool whole_store) const {
  return guard<Done>([&] {
    registration(reg);
    auto e = execution(common_, growth_, catalog_, shared_, proof.action_ref,
                       current_);
    auto history = chain(common_, interaction_, interaction_catalog_, growth_,
                         e, proof.action_ref, identity_, current_, budget_);
    if (e.kind != reg.action_kind || elapsed.action_ref != proof.action_ref ||
        elapsed.execution_source_ref != proof.action_ref ||
        elapsed.owner_cost_state_ref != proof.action_ref ||
        proof.actor_id != e.actor ||
        proof.participant_actor_ids != e.participants ||
        !same_source_ref(proof.original_execution_source, e.source))
      fail("GrowthFixedElapsedOwnerMismatch", proof.action_ref);
    std::optional<ActionTimeOwnerProof> trade_owner;
    if (e.kind == "trade") {
      const auto original =
          scalar_start(common_, growth_, catalog_, shared_, market_exact_,
                       proof.action_ref, current_, budget_);
      scalar_inputs(history, original, e, common_, identity_);
      const auto *start = std::get_if<MomentOwnerSource>(&proof.started_source);
      if (!start || start->source_schema != 2 ||
          start->source_ref.domain != "growth_market_history" ||
          start->source_ref.kind != "growth_market_movement" ||
          start->source_ref.id != *original.movement_id ||
          take(compare_world_moments(*original.original.started_at,
                                     history.front()->occurred_at, budget_)))
        fail("GrowthTradeOriginalStartSourceMismatch", proof.action_ref);
      trade_owner.emplace(take(read_action_time_owner(reg, proof.action_ref)));
    }
    auto required = take(fixed_duration_work_units(
        e.duration, scale_.units_per_half_day, budget_));
    if (take(compare_fractions(required.value, elapsed.required_units.value,
                               budget_)) ||
        take(compare_world_moments(elapsed.started_at,
                                   history.front()->occurred_at, budget_)) ||
        static_cast<std::size_t>(std::count_if(
            transitions.begin(), transitions.end(), [&](const auto &row) {
              return row.action_ref == proof.action_ref;
            })) != history.size())
      fail("GrowthFixedOriginalDurationMismatch", proof.action_ref);
    if (!whole_store && std::any_of(transitions.begin(), transitions.end(),
                                    [&](const auto &row) {
                                      return row.action_ref != proof.action_ref;
                                    }))
      fail("GrowthFixedUnexpectedActionTransition", proof.action_ref);
    for (std::size_t n = 0; n < history.size(); ++n) {
      const auto &b = *history[n];
      const FixedServiceTransition *t = nullptr;
      for (const auto &row : transitions)
        if (row.action_ref == proof.action_ref &&
            row.transition_ordinal == b.boundary_ordinal) {
          if (t)
            fail("DuplicateGrowthFixedTransition", b.boundary_id);
          t = &row;
        }
      if (!t || t->action_ref != b.action_ref ||
          t->kind != transition_kind(b.kind) ||
          t->execution_source_schema != 1 ||
          !same_source_ref(t->execution_source_ref, e.source) ||
          take(compare_world_moments(t->occurred_at, b.occurred_at, budget_)))
        fail("GrowthFixedTransitionSourceMismatch", b.boundary_id);
      SourceRef source;
      Integer schema = 0;
      std::visit(
          [&](const auto &cause) {
            using T = std::decay_t<decltype(cause)>;
            if constexpr (std::is_same_v<T, FixedOwnerAdopted> ||
                          std::is_same_v<T, FixedOwnerStateChanged>) {
              source = cause.owner_source_ref;
              schema = cause.owner_source_schema;
            } else if constexpr (std::is_same_v<T, FixedOwnerTerminated>) {
              source = cause.owner_result_ref;
              schema = cause.owner_result_schema;
            } else {
              source = cause.original_cause_ref;
              schema = cause.original_cause_schema;
            }
          },
          t->cause);
      if (trade_owner && (t->kind == FixedTransitionKind::started ||
                          t->kind == FixedTransitionKind::terminal)) {
        const auto &actual = trade_endpoint_source(*trade_owner, t->kind);
        if (schema != actual.source_schema ||
            !same_source_ref(source, actual.source_ref))
          fail("GrowthFixedTransitionOwnerMissing", b.boundary_id);
      } else if (schema != 1 ||
                 !same_source_ref(source, boundary_source(b.boundary_id)))
        fail("GrowthFixedTransitionOwnerMissing", b.boundary_id);
      if (t->kind == FixedTransitionKind::terminal) {
        if (!t->terminal_disposition ||
            (b.kind == LifeFixedBoundaryKind::completed) !=
                (*t->terminal_disposition ==
                 FixedTerminalDisposition::completed))
          fail("GrowthFixedTerminalDispositionMismatch", b.boundary_id);
      } else {
        bool input = false;
        for (const auto &item : t->input_basis_refs)
          if (const auto *p = std::get_if<FixedAppliedInput>(&item);
              p && p->source_schema == 1 &&
              same_source_ref(p->source_ref, boundary_source(b.boundary_id))) {
            if (input)
              fail("DuplicateGrowthFixedAppliedInput", b.boundary_id);
            input = true;
          }
        if (!input)
          fail("GrowthFixedAppliedInputMissing", b.boundary_id);
      }
    }
    if (e.terminal &&
        history.back()->kind == LifeFixedBoundaryKind::completed &&
        take(compare_fractions(elapsed.credited_units.value, required.value,
                               budget_)) != 0)
      fail("GrowthFixedCompletedBeforeActualService", proof.action_ref);
    return Done{};
  });
}
Outcome<StableId> GrowthActionTimeRead::read_original_business_rule(
    const ActionTimeRegistration &reg, const StableId &id) const {
  return guard<StableId>([&] {
    registration(reg);
    auto e = execution(common_, growth_, catalog_, shared_, id, current_);
    auto history = chain(common_, interaction_, interaction_catalog_, growth_,
                         e, id, identity_, current_, budget_);
    if (e.kind != reg.action_kind)
      fail("GrowthBusinessRuleActionMismatch", id);
    if (e.life) {
      if (!catalog_.regional_life)
        fail("GrowthRegionalLifeRuleRequired", id);
      return StableId{"regional_life_v1"};
    }
    const auto &definition = history.front()
                                 ->input_snapshot.specific.growth_fact
                                 .original_training_or_study_rule_ref;
    const auto expected_entity =
        e.study      ? e.study->terms.context.role_id
        : e.training ? (e.kind == "learn_skill" ? e.training->context->role_id
                                                : StableId{"cultivation"})
                     : e.rest->estate_id;
    const auto expected_field =
        e.study ? StableId{"courses."} + e.study->terms.skill_id
        : e.training
            ? (e.kind == "learn_skill"
                   ? StableId{"lessons."} + e.training->training.skill_id
                   : e.kind)
            : StableId{"long_rest"};
    if (definition.scenario_id != common_.state().metadata.scenario_id ||
        definition.entity_id != expected_entity ||
        definition.field_selector != expected_field)
      fail("GrowthOriginalBusinessRuleSourceMismatch", id);
    for (const auto *boundary : history) {
      const auto &value = boundary->input_snapshot.specific.growth_fact
                              .original_training_or_study_rule_ref;
      if (!same_catalog(value.catalog_identity, definition.catalog_identity) ||
          value.scenario_id != definition.scenario_id ||
          value.entity_id != definition.entity_id ||
          value.field_selector != definition.field_selector)
        fail("GrowthOriginalBusinessRuleChanged", id);
    }
    return definition.entity_id + "/" + definition.field_selector;
  });
}
Status GrowthActionTimeRead::validate_adopted_terminal_source(
    const ActionTimeRegistration &reg, const StableId &id) const {
  return guard<Done>([&] {
    auto proof = take(read_action_time_owner(reg, id));
    const auto *elapsed = find_fixed_elapsed(elapsed_, id);
    if (proof.phase != ActionTimeOwnerPhase::terminal || !proof.ended_source ||
        !elapsed || !std::holds_alternative<ElapsedTerminal>(elapsed->timing) ||
        std::get<ElapsedTerminal>(elapsed->timing).outcome_ref !=
            proof.terminal_result_ref)
      fail("GrowthAdoptedTerminalRequired", id);
    take(validate_fixed_elapsed_value(*elapsed, current_, scale_, budget_));
    take(validate_retained_fixed_action_sources(reg, proof, *elapsed));
    const ActionElapsedTerminalBinding *binding = nullptr;
    for (const auto &row : elapsed_.terminal_bindings)
      if (row.action_ref == id) {
        if (binding)
          fail("DuplicateGrowthTerminalTimeBinding", id);
        binding = &row;
      }
    if (!binding || binding->timing_kind != 0 ||
        binding->action_kind != proof.action_kind ||
        binding->actor_id != proof.actor_id ||
        binding->participant_actor_ids != proof.participant_actor_ids ||
        binding->terminal_result_ref != proof.terminal_result_ref ||
        !same_source_ref(binding->original_execution_source_ref,
                         proof.original_execution_source) ||
        take(compare_world_moments(
            take(read_action_time_source(binding->started_source_ref)),
            take(read_action_time_source(proof.started_source)), budget_)) ||
        take(compare_world_moments(
            take(read_action_time_source(binding->ended_source_ref)),
            take(read_action_time_source(*proof.ended_source)), budget_)))
      fail("GrowthTerminalTimeBindingMismatch", id);
    // 即使旧FSL终点恰好同刻，也不能冒充原交易持值的schema2地址。
    if (proof.action_kind == "trade") {
      const auto *started =
          std::get_if<MomentOwnerSource>(&binding->started_source_ref);
      const auto *actual =
          std::get_if<MomentOwnerSource>(&proof.started_source);
      if (!started || !actual || started->source_schema != 2 ||
          !same_source_ref(started->source_ref, actual->source_ref))
        fail("GrowthTradeStartBindingSourceMismatch", id);
      const auto *ended =
          std::get_if<MomentOwnerSource>(&binding->ended_source_ref);
      if (!ended || ended->source_schema != 2 ||
          ended->source_ref.domain != "growth" ||
          ended->source_ref.kind != "life_activity" ||
          ended->source_ref.id != id)
        fail("GrowthTradeTerminalBindingSourceMismatch", id);
    }
    return Done{};
  });
}
Status GrowthActionTimeRead::validate_shared_action_sources(
    const ActionTimeRegistration &, const ActionTimeOwnerProof &,
    const SharedWorkElapsedState &) const {
  return common_failure("GrowthFixedActionCannotUseSharedWork");
}
Outcome<RunStop> GrowthActionTimeRead::read_action_continuation_stop(
    const ActionTimeRegistration &reg, const ActionTimeOwnerProof &proof,
    const WorldMoment &current) const {
  return guard<RunStop>([&] {
    registration(reg);
    auto actual = take(read_action_time_owner(reg, proof.action_ref));
    auto e = execution(common_, growth_, catalog_, shared_, proof.action_ref,
                       current_);
    auto start = take(read_action_time_source(actual.started_source));
    if (take(compare_world_moments(start, current, budget_)) > 0)
      fail("GrowthContinuationBeforeStart", proof.action_ref);
    if (e.terminal) {
      auto end = take(read_action_time_source(*actual.ended_source));
      if (take(compare_world_moments(end, current, budget_)) > 0)
        fail("GrowthContinuationBeforeTerminal", proof.action_ref);
      if (e.success)
        return RunStop::completed;
      if (e.stop_code == "cancelled" || e.stop_code == "StudyCancelled" ||
          e.stop_code == "RestCancelled")
        return RunStop::cancelled;
      return RunStop::blocked;
    }
    if (e.paused)
      return RunStop::paused;
    if (e.study && e.study->challenge &&
        growth::challenge_pending(*e.study->challenge) &&
        e.study->challenge->frozen.mode == "manual")
      return RunStop::awaiting_input;
    return RunStop::awaiting_permit;
  });
}
Outcome<OriginalActionRemaining>
GrowthActionTimeRead::read_original_action_remaining(
    const ActionTimeRegistration &reg, const ActionTimeOwnerProof &proof,
    const WorldMoment &current) const {
  return guard<OriginalActionRemaining>([&] {
    registration(reg);
    auto actual = take(read_action_time_owner(reg, proof.action_ref));
    const auto *elapsed = find_fixed_elapsed(elapsed_, proof.action_ref);
    if (!elapsed)
      fail("GrowthFixedElapsedMissing", proof.action_ref);
    take(validate_fixed_elapsed_value(*elapsed, current, scale_, budget_));
    take(validate_retained_fixed_action_sources(reg, actual, *elapsed));
    auto remaining = take(subtract_work_units(
        elapsed->required_units, elapsed->credited_units, budget_));
    const auto duration = take(
        scale_units_to_duration(remaining, scale_.units_per_half_day, budget_));
    return OriginalActionRemaining{
        ActualServiceRemaining{ActualServiceRemainingBuildView{
            duration,
            {actual.original_execution_source.domain,
             actual.original_execution_source.kind,
             actual.original_execution_source.id},
            1}}};
  });
}
Outcome<StableId>
GrowthActionTimeRead::read_gate_result_id(const SharedWorkGateResult &) const {
  return {false, {}, common_failure("GrowthFixedActionHasNoSharedGate").error};
}
Outcome<SharedWorkGateResult>
GrowthActionTimeRead::read_original_gate_result(const StableId &id) const {
  return {
      false, {}, common_failure("GrowthFixedActionHasNoSharedGate", id).error};
}
Status GrowthActionTimeRead::validate_original_gate_result(
    const SharedWorkGateResult &) const {
  return common_failure("GrowthFixedActionHasNoSharedGate");
}
Outcome<GrowthOriginalTimeView>
read_growth_original_time(const GrowthActionTimeRead &reader,
                          const ActionTimeRegistration &reg,
                          const StableId &id) {
  return guard<GrowthOriginalTimeView>([&] {
    auto p = take(reader.read_action_time_owner(reg, id));
    GrowthOriginalTimeView out;
    out.action_id = id;
    out.action_kind = p.action_kind;
    out.actor_id = p.actor_id;
    out.started_at = take(reader.read_action_time_source(p.started_source));
    out.terminal = p.phase == ActionTimeOwnerPhase::terminal;
    out.execution_source = p.original_execution_source;
    if (p.ended_source) {
      out.ended_at = take(reader.read_action_time_source(*p.ended_source));
      out.terminal_source =
          std::get<MomentOwnerSource>(*p.ended_source).source_ref;
    }
    return out;
  });
}
// 只构造一次采用候选；原来源全部通过后再提交，失败仍由外层整候选撤回。
Outcome<StableId> adopt_growth_fixed_boundary_in_candidate(
    CommonStore &common, GrowthState &growth, const GrowthCatalog &catalog,
    const CommonCatalog &shared, ActionElapsedStore &store,
    const TimeScaleRef &scale, const CatalogIdentity &identity,
    const ActionTimeRegistration &reg, const StableId &boundary_id,
    const WorldMoment &actual_current, ExactArithmeticBudget budget,
    const InteractionState *interaction,
    const InteractionCatalog *interaction_catalog,
    const GrowthMarketExactSources *market_exact) {
  return guard<StableId>([&] {
    registration(reg);
    const auto *boundary = growth.fixed_service_boundaries.find(boundary_id);
    if (!boundary || boundary->boundary_id != boundary_id)
      fail("GrowthFixedBoundaryMissing", boundary_id);
    auto e = execution(common, growth, catalog, shared, boundary->action_ref,
                       actual_current);
    auto history =
        chain(common, interaction, interaction_catalog, growth, e,
              boundary->action_ref, identity, actual_current, budget);
    if (history.back() != boundary || e.kind != reg.action_kind)
      fail("GrowthFixedAdoptionNotCurrentBoundary", boundary_id);
    GrowthActionTimeRead reader(common, growth, catalog, shared, store, scale,
                                identity, actual_current, budget, interaction,
                                interaction_catalog, market_exact);
    const auto boundary_kind = transition_kind(boundary->kind);
    std::optional<ActionTimeOwnerProof> trade_owner;
    const MomentOwnerSource *trade_source = nullptr;
    if (e.kind == "trade" && (boundary_kind == FixedTransitionKind::started ||
                              boundary_kind == FixedTransitionKind::terminal)) {
      trade_owner.emplace(
          take(reader.read_action_time_owner(reg, boundary->action_ref)));
      trade_source = &trade_endpoint_source(*trade_owner, boundary_kind);
    }
    // 重入只能复验已经采用的同一实际source，绝不再分配另一transition。
    for (const auto &t : store.fixed_service_transitions) {
      bool same = false;
      if (trade_source) {
        // 同 action/序位/转换联合分支都须一致，不能凭同刻或某个来源碰巧相同。
        if (t.action_ref == boundary->action_ref && t.kind == boundary_kind &&
            t.transition_ordinal == boundary->boundary_ordinal) {
          if (boundary_kind == FixedTransitionKind::started) {
            if (const auto *cause = std::get_if<FixedOwnerAdopted>(&t.cause))
              same =
                  cause->owner_source_schema == trade_source->source_schema &&
                  same_source_ref(cause->owner_source_ref,
                                  trade_source->source_ref);
          } else if (const auto *cause =
                         std::get_if<FixedOwnerTerminated>(&t.cause))
            same = cause->owner_result_schema == trade_source->source_schema &&
                   same_source_ref(cause->owner_result_ref,
                                   trade_source->source_ref);
        }
      } else {
        std::visit(
            [&](const auto &cause) {
              using T = std::decay_t<decltype(cause)>;
              if constexpr (std::is_same_v<T, FixedOwnerAdopted> ||
                            std::is_same_v<T, FixedOwnerStateChanged>)
                same = cause.owner_source_schema == 1 &&
                       same_source_ref(cause.owner_source_ref,
                                       boundary_source(boundary_id));
              else if constexpr (std::is_same_v<T, FixedOwnerTerminated>)
                same = cause.owner_result_schema == 1 &&
                       same_source_ref(cause.owner_result_ref,
                                       boundary_source(boundary_id));
            },
            t.cause);
      }
      if (same) {
        const auto *entry = find_fixed_elapsed(store, boundary->action_ref);
        if (!entry)
          fail("GrowthFixedElapsedMissing", boundary_id);
        take(reader.validate_retained_fixed_action_sources(
            reg, take(reader.read_action_time_owner(reg, entry->action_ref)),
            *entry));
        if (t.kind == FixedTransitionKind::terminal)
          take(append_growth_terminal_publication_in_candidate(
              common, growth, reader, reg, entry->action_ref));
        return t.transition_id;
      }
    }
    FixedServiceTransition transition;
    transition.transition_id = take(common.next_id("fixed_service_transition"));
    transition.action_ref = boundary->action_ref;
    transition.transition_ordinal = boundary->boundary_ordinal;
    transition.kind = transition_kind(boundary->kind);
    transition.occurred_at = boundary->occurred_at;
    transition.execution_source_ref = e.source;
    transition.execution_source_schema = 1;
    const LifeCatalogFieldRef *definition =
        e.life
            ? (boundary->input_snapshot.specific.life_activity.activity_ref
                   ? &*boundary->input_snapshot.specific.life_activity
                           .activity_ref
                   : (boundary->input_snapshot.locked_rule_refs.empty()
                          ? nullptr
                          : &boundary->input_snapshot.locked_rule_refs.front()))
            : &boundary->input_snapshot.specific.growth_fact
                   .original_training_or_study_rule_ref;
    if (transition.kind != FixedTransitionKind::terminal) {
      transition.input_basis_refs.push_back(
          FixedAppliedInput{boundary_source(boundary_id), 1});
      for (const auto &r : boundary->input_snapshot.locked_rule_refs)
        transition.input_basis_refs.push_back(FixedLockedRuleInput{
            r.catalog_identity, r.entity_id + "/" + r.field_selector});
    }
    auto change = [&]() -> PreparedFixedServiceChange {
      if (transition.kind == FixedTransitionKind::started) {
        const auto *action = common.action(boundary->action_ref);
        if (!action || !definition)
          fail("GrowthFixedOriginalStartRequired", boundary_id);
        transition.cause =
            trade_source ? FixedOwnerAdopted{trade_source->source_ref,
                                             trade_source->source_schema}
                         : FixedOwnerAdopted{boundary_source(boundary_id), 1};
        auto elapsed = take(begin_fixed_elapsed(
            action->id, e.source.id,
            definition->entity_id + "/" + definition->field_selector,
            action->id, boundary->occurred_at, e.duration, scale, budget));
        return take(prepare_fixed_service_adoption(*action, elapsed, transition,
                                                   reg, scale, reader, budget));
      } else {
        const auto *entry = find_fixed_elapsed(store, boundary->action_ref);
        if (!entry)
          fail("GrowthFixedElapsedMissing", boundary_id);
        const FixedServiceTransition *previous = nullptr;
        for (const auto &row : store.fixed_service_transitions)
          if (row.action_ref == entry->action_ref &&
              (!previous ||
               row.transition_ordinal > previous->transition_ordinal))
            previous = &row;
        if (!previous ||
            previous->transition_ordinal ==
                std::numeric_limits<Integer>::max() ||
            previous->transition_ordinal + 1 != transition.transition_ordinal)
          fail("GrowthFixedTransitionPredecessorMissing", boundary_id);
        transition.previous_transition_ref = previous->transition_id;
        transition.previous_input_basis_refs = previous->input_basis_refs;
        transition.credited_at = entry->credited_units;
        if (std::holds_alternative<ElapsedRunning>(entry->timing) &&
            take(compare_world_moments(entry->last_settled_at,
                                       boundary->occurred_at, budget)) != 0)
          transition.credited_at =
              take(prepare_fixed_elapsed_slice(
                       *entry, {entry->last_settled_at, boundary->occurred_at},
                       scale, budget))
                  .credited_units;
        std::optional<StableId> suspension;
        if (transition.kind == FixedTransitionKind::terminal) {
          transition.cause =
              trade_source
                  ? FixedOwnerTerminated{trade_source->source_ref,
                                         trade_source->source_schema}
                  : FixedOwnerTerminated{boundary_source(boundary_id), 1};
          transition.terminal_disposition =
              boundary->kind == LifeFixedBoundaryKind::completed
                  ? FixedTerminalDisposition::completed
              : (e.stop_code == "cancelled" ||
                 e.stop_code == "StudyCancelled" ||
                 e.stop_code == "RestCancelled")
                  ? FixedTerminalDisposition::cancelled
                  : FixedTerminalDisposition::interrupted;
        } else {
          StableId rule_id;
          if (const auto *command =
                  std::get_if<LifeActualCommand>(&boundary->cause))
            rule_id = command->command_binding.command_kind;
          else if (const auto *accepted =
                       std::get_if<LifeAcceptedExecution>(&boundary->cause))
            rule_id = accepted->execution_rule_ref.id;
          else if (const auto *changed =
                       std::get_if<LifeActualInputChange>(&boundary->cause))
            rule_id = changed->transition_rule_ref.id;
          if (rule_id.empty())
            fail("GrowthFixedTransitionRuleMissing", boundary_id);
          transition.cause = FixedOwnerStateChanged{
              boundary_source(boundary_id), 1, std::move(rule_id)};
          if (transition.kind == FixedTransitionKind::suspended)
            suspension = boundary_id;
        }
        return take(prepare_fixed_service_change(store, boundary->action_ref,
                                                 transition, suspension, reg,
                                                 scale, reader, budget));
      }
    }();
    const auto id = transition.transition_id;
    take(apply_fixed_service_change_in_candidate(store, std::move(change), reg,
                                                 scale, reader, budget));
    if (transition.kind == FixedTransitionKind::terminal)
      take(append_growth_terminal_publication_in_candidate(
          common, growth, reader, reg, boundary->action_ref));
    return id;
  });
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<std::vector<ActionTimeRegistration>>
make_growth_action_time_registrations(Integer version) {
  if (version != 1 && version != 2 && version != 3 && version != 4)
    return {false,
            {},
            common_failure("UnsupportedGrowthExactPhaseVersion", {},
                           ErrorKind::unsupported_capability)
                .error};
  std::vector<ActionTimeRegistration> out;
  out.reserve(8);
  for (const char *kind :
       {"learn_skill", "practice_skill", "teach_skill", "study_skill",
        "teach_study", "service", "trade", "long_rest"}) {
    ActionTimeRegistration r;
    r.action_kind = kind;
    r.owner_domain = "growth";
    r.profile_id = "growth_fixed_service_time2";
    r.profile_version = version;
    r.source_schema = 1;
    r.mode = ActionTimeMode::fixed_duration;
    r.remaining_meaning = ActionRemainingMeaning::service_estimate;
    out.push_back(std::move(r));
  }
  auto valid = validate_action_time_registry(out);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident

namespace hanmo::resident {
// 只从原业务表枚举，ActionElapsed用于反向核验，不以今日Action缺失证明过去空闲。
Status GrowthActionTimeRead::visit_actor_history_with_sources(
    const CharacterConditionSources &body, const ActionTimeRegistration &reg,
    const StableId &actor, ActionTimeActorHistorySink &sink) const {
  return guard<Done>([&] {
    registration(reg);
    if (&body.common != &common_ || body.growth != &growth_ ||
        body.growth_catalog != &catalog_ || body.shared != &shared_ ||
        body.elapsed != &elapsed_ || body.scale != &scale_ ||
        !same_catalog(body.catalog.identity, identity_) ||
        take(compare_world_moments(body.actual_now, current_, budget_)))
      fail("GrowthActorHistorySameSourcesRequired", actor);
    if (!common_.actor(actor))
      fail("GrowthActorHistoryActorMissing", actor);
    auto visit = [&](const StableId &id) {
      auto e = execution(common_, growth_, catalog_, shared_, id, current_);
      if (e.kind != reg.action_kind)
        return;
      // 先报告原业务独立枚举的身份，其他 actor 的身体/Knowledge 不在此展开。
      take(sink.original_action_owner(id));
      if (std::find(e.participants.begin(), e.participants.end(), actor) ==
          e.participants.end())
        return;
      auto history = chain(common_, interaction_, interaction_catalog_, growth_,
                           e, id, identity_, current_, budget_);
      auto proof = take(read_action_time_owner(reg, id));
      const auto *elapsed = find_fixed_elapsed(elapsed_, id);
      if (!elapsed)
        fail("GrowthFixedElapsedMissing", id);
      take(validate_fixed_elapsed_value(*elapsed, current_, scale_, budget_));
      take(validate_retained_fixed_action_sources(reg, proof, *elapsed));
      const FactRecord *terminal_header = nullptr;
      if (e.terminal) {
        take(validate_adopted_terminal_source(reg, id));
        const GrowthTerminalPublication *publication = nullptr;
        for (const auto &row : growth_.terminal_publications.rows())
          if (row.second.action_id == id) {
            if (publication)
              fail("DuplicateGrowthTerminalPublication", id);
            publication = &row.second;
          }
        if (!publication)
          fail("GrowthTerminalPublicationRequired", id);
        auto original = take(read_growth_terminal_publication(
            common_, growth_, *this, reg, publication->publication_id));
        if (original.publication != publication)
          fail("GrowthTerminalPublicationSourceMismatch", id);
        terminal_header =
            common_.state().facts.find(publication->publication_id);
      }
      const LifeFixedServiceBoundary *acquired = nullptr;
      const FactRecord *acquired_header = nullptr;
      const CommitResult *acquired_command = nullptr;
      for (const auto *boundary : history) {
        take(validate_growth_fixed_boundary_actor_inputs(body, growth_,
                                                         *boundary));
        const LifeActualCommand *command =
            std::get_if<LifeActualCommand>(&boundary->cause);
        const LifeFixedInputChange *changed = nullptr;
        if (const auto *value =
                std::get_if<LifeActualInputChange>(&boundary->cause))
          changed = &value->change;
        if (const auto *terminal =
                std::get_if<LifeOriginalTerminalApplication>(&boundary->cause))
          if (const auto *stop =
                  std::get_if<LifeActualStopTrigger>(&terminal->trigger)) {
            command = std::get_if<LifeActualCommand>(&stop->source);
            changed = std::get_if<LifeFixedInputChange>(&stop->source);
          }
        const CommitResult *applied_command = nullptr;
        if (command)
          applied_command = take(read_growth_fixed_control_command_source(
              body, growth_, *boundary));
        else if (changed)
          take(validate_growth_fixed_input_change_source(
              body, growth_, boundary->boundary_id));
        else if (std::holds_alternative<LifeAcceptedExecution>(boundary->cause))
          fail("GrowthActorHistoryAcceptedSourceReaderRequired",
               boundary->boundary_id, ErrorKind::unsupported_capability);
        switch (boundary->kind) {
        case LifeFixedBoundaryKind::started:
          if (acquired)
            fail("GrowthActorHistoryDuplicateAcquisition", id);
          acquired = boundary;
          acquired_command = applied_command;
          // 原 start_growth_impl
          // 先核每位participant已被真实占用，才append本业务头。
          // 后续F绑定与seal仍在同候选；这里只证明个人占用，不冒S0采用时序。
          acquired_header = common_.state().facts.find(id);
          if (!acquired_header)
            fail("GrowthTimeOriginalHeaderMismatch", id);
          break;
        case LifeFixedBoundaryKind::resumed:
          if (acquired)
            fail("GrowthActorHistoryDuplicateAcquisition", id);
          acquired = boundary;
          acquired_command = applied_command;
          acquired_header = nullptr;
          break;
        case LifeFixedBoundaryKind::suspended:
          if (!acquired)
            fail("GrowthActorHistoryReleaseWithoutAcquisition", id);
          take(sink.original_actor_occupancy(
              {id, acquired->occurred_at, acquired_header,
               &boundary->occurred_at, nullptr, acquired_command,
               applied_command}));
          acquired = nullptr;
          acquired_header = nullptr;
          acquired_command = nullptr;
          break;
        case LifeFixedBoundaryKind::input_rebound:
          break;
        case LifeFixedBoundaryKind::completed:
        case LifeFixedBoundaryKind::stopped:
          if (!terminal_header)
            fail("GrowthTerminalPublicationHeaderMismatch", id);
          if (acquired) {
            take(sink.original_actor_occupancy(
                {id, acquired->occurred_at, acquired_header,
                 &boundary->occurred_at, terminal_header, acquired_command,
                 applied_command}));
            acquired = nullptr;
            acquired_header = nullptr;
            acquired_command = nullptr;
          }
          break;
        }
      }
      if (acquired)
        take(sink.original_actor_occupancy({id, acquired->occurred_at,
                                            acquired_header, nullptr, nullptr,
                                            acquired_command, nullptr}));
    };
    // 原活动头及保留F转换反向覆盖本域正文，删掉一份正文不能伪装为空历史。
    for (const auto &row : common_.state().actions.rows())
      if (row.second.kind == reg.action_kind) {
        auto actual =
            execution(common_, growth_, catalog_, shared_, row.first, current_);
        if (actual.kind != reg.action_kind || actual.terminal)
          fail("GrowthActorHistoryActiveOwnerMismatch", row.first);
      }
    for (const auto &row : elapsed_.fixed_service_transitions)
      if (row.execution_source_ref.domain == "growth") {
        auto actual = execution(common_, growth_, catalog_, shared_,
                                row.action_ref, current_);
        if (actual.kind == reg.action_kind &&
            !same_source_ref(actual.source, row.execution_source_ref))
          fail("GrowthActorHistoryTransitionOwnerMismatch", row.action_ref);
      }
    for (const auto &row : growth_.facts.rows())
      if (!std::holds_alternative<BargainFact>(row.second))
        visit(row.first);
    for (const auto &row : growth_.life_activities.rows())
      visit(row.first);
    return Done{};
  });
}
} // namespace hanmo::resident
