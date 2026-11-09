#include "growth_fixed_terminal.hpp"
#include "growth_fixed_spatial_inputs.hpp"
#include "growth_internal.hpp"
#include "growth_quiet_source.hpp"
#include "growth_trade_terminal_read.hpp"
#include "growth_trade_grant_read.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "study_permanent_contribution_read.hpp"
#include "study_service_sources.hpp"
#include <limits>
namespace hanmo::resident {
struct PreparedGrowthFixedTerminalInputs::State {
  const CommonStore *common = nullptr;
  const GrowthState *growth = nullptr;
  WorldIdentity identity;
  std::shared_ptr<const ActorConditionCaptureIdentity> capture;
  ActorConditionSavepointBoundary savepoint;
  LifeFixedServiceBoundary terminal;
  std::optional<GrowthMarketExactSources> market_exact;
  // 只在当前prepared候选存原写前像，不加入持久正文或第二份XP历史。
  std::optional<Integer> trade_xp_before;
  bool full_gate = false, consumed = false;
};
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
bool same(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool same(const ActorConditionHead &a, const ActorConditionHead &b) {
  return a.actor_id == b.actor_id &&
         a.condition_revision == b.condition_revision &&
         a.initial_condition_seed_ref.seed_key ==
             b.initial_condition_seed_ref.seed_key &&
         bool(a.latest_receipt_ref) == bool(b.latest_receipt_ref) &&
         (!a.latest_receipt_ref ||
          a.latest_receipt_ref->result_id == b.latest_receipt_ref->result_id);
}
const LifeFixedServiceBoundary &last(const GrowthState &growth,
                                     const StableId &action) {
  const LifeFixedServiceBoundary *found = nullptr;
  for (const auto &row : growth.fixed_service_boundaries.rows())
    if (row.second.action_ref == action &&
        (!found || row.second.boundary_ordinal > found->boundary_ordinal))
      found = &row.second;
  if (!found)
    fail("GrowthFixedOriginalStartRequired", action);
  return *found;
}
LifeFixedOutcomeRef outcome(const GrowthState &growth, const StableId &id,
                            bool &success) {
  LifeFixedOutcomeRef out;
  out.id = id;
  if (const auto *raw = growth.facts.find(id)) {
    if (const auto *value = std::get_if<TrainingFact>(raw)) {
      if (value->phase != "completed")
        fail("GrowthActualTerminalResultRequired", id);
      out.kind = LifeFixedOutcomeRef::Kind::training;
      success = value->success;
    } else if (const auto *value = std::get_if<StudyFact>(raw)) {
      if (value->phase != "completed")
        fail("GrowthActualTerminalResultRequired", id);
      out.kind = LifeFixedOutcomeRef::Kind::study;
      success = value->success;
    } else if (const auto *value = std::get_if<RestFact>(raw)) {
      if (value->phase != "completed")
        fail("GrowthActualTerminalResultRequired", id);
      out.kind = LifeFixedOutcomeRef::Kind::rest;
      success = value->stop_reason == "duration";
    } else
      fail("GrowthActualTerminalResultRequired", id);
  } else if (const auto *value = growth.life_activities.find(id)) {
    if (value->view.phase == LifeActivityPhase::active)
      fail("GrowthActualTerminalResultRequired", id);
    out.kind = LifeFixedOutcomeRef::Kind::life_activity;
    success = value->view.success;
  } else
    fail("GrowthActualTerminalResultRequired", id);
  return out;
}
} // namespace
Outcome<PreparedGrowthFixedTerminalInputs> prepare_growth_fixed_terminal_inputs(
    CommonStore &common, const GrowthState &growth,
    const CharacterConditionSources &body, const GrowthFixedStepPermit &gate,
    const GrowthMarketExactSources *market_exact) {
  try {
    if (&body.common != &common || body.growth != &growth || !body.shared ||
        !body.fixed_spatial)
      fail("GrowthFixedTerminalSameCandidateRequired", gate.action_id());
    const auto *action = common.action(gate.action_id());
    if (!action)
      fail("GrowthFixedTerminalActualActionRequired", gate.action_id());
    take(validate_growth_fixed_step_permit(gate, common, growth, *action));
    if (take(compare_world_moments(gate.at(), body.actual_now)))
      fail("GrowthFixedTerminalActualClockRequired", gate.action_id());
    const auto &previous = last(growth, action->id);
    if (previous.kind == LifeFixedBoundaryKind::completed ||
        previous.kind == LifeFixedBoundaryKind::stopped ||
        previous.kind == LifeFixedBoundaryKind::suspended ||
        previous.boundary_ordinal == std::numeric_limits<Integer>::max())
      fail("GrowthFixedTerminalOriginalPhaseMismatch", action->id);
    // 精确标量终局必须从原active状态进入；已有尾不能借晚重放覆盖。
    if (action->kind == "trade") {
      const auto *life = growth.life_activities.find(action->id);
      if (!body.growth_catalog ||
          !body.growth_catalog->complete_study_sources || !life ||
          life->view.kind != LifeActivityKind::trade ||
          life->view.phase != LifeActivityPhase::active ||
          !std::holds_alternative<TradeTerms>(life->terms) ||
          life->selected_trade_settlement || life->trade_terminal_time)
        fail("GrowthTradeActualTerminalStartRequired", action->id);
    }
    auto capture = common.actor_condition_capture_identity();
    if (!capture)
      fail("GrowthFixedTerminalCaptureRequired", action->id);
    auto p = checkpoint_make_shared<PreparedGrowthFixedTerminalInputs::State>();
    p->common = &common;
    p->growth = &growth;
    p->identity = common.state().identity;
    p->capture = std::move(capture);
    p->savepoint = take(common.actor_condition_savepoint_boundary());
    p->full_gate = gate.final_gate();
    if (action->kind == "trade" &&
        body.growth_catalog->complete_capability_application_chain) {
      const auto *actor = common.state().actors.find(action->actor_id);
      if (!actor || actor->skills.commerce < 0 ||
          actor->skills.commerce > 1200 ||
          growth.trade_grants.find(action->id))
        fail("GrowthTradeGrantActualPreimageRequired", action->id);
      p->trade_xp_before = actor->skills.commerce;
    }
    if (market_exact)
      p->market_exact.emplace(*market_exact);
    auto &next = p->terminal;
    next.action_ref = action->id;
    next.action_kind = action->kind;
    next.owner_ref = previous.owner_ref;
    next.previous_boundary_ref = previous.boundary_id;
    next.boundary_ordinal = previous.boundary_ordinal + 1;
    next.occurred_at = body.actual_now;
    next.input_snapshot = previous.input_snapshot;
    // No effect has run. Every selected input must still be the actual
    // predecessor; a missing earlier InputChange is a rejected candidate, never
    // backfilled here.
    for (auto &person : next.input_snapshot.actors) {
      const auto *current =
          body.state.character_condition_extensions.actor_condition_heads.find(
              person.actor_id);
      if (!current || person.capability_frames.empty())
        fail("GrowthFixedTerminalBodyHeadRequired", person.actor_id);
      const auto *required =
          take(read_growth_fixed_required_skill(body, growth, next, person));
      std::optional<StableId> purpose_skill;
      for (auto &frozen : person.capability_frames) {
        if (!same(*current, frozen.condition_head_snapshot))
          fail("GrowthFixedTerminalUnadoptedConditionChange", person.actor_id);
        if (frozen.purpose_ref.id == "study" ||
            frozen.purpose_ref.id == "provide_teaching") {
          if (!required)
            fail("GrowthFixedTerminalSkillMaskMismatch", person.actor_id);
          purpose_skill = *required;
        } else
          purpose_skill.reset();
        auto actual = take(read_character_capability_at_head(
            body, *current, frozen.purpose_ref, purpose_skill,
            body.actual_now));
        frozen = take(freeze_character_capability(actual));
      }
      const auto *actor = common.state().actors.find(person.actor_id);
      if (!actor || actor->action_id != action->id)
        fail("GrowthFixedTerminalActualParticipationRequired", person.actor_id);
      if (required) {
        if (person.skill_qualification_views.size() != 1 ||
            person.skill_qualification_views.front().skill_id != *required ||
            person.skill_qualification_views.front().minimum_xp !=
                growth_skill_xp(actor->skills, *required))
          fail("GrowthFixedTerminalUnadoptedSkillChange", person.actor_id);
      } else if (!person.skill_qualification_views.empty())
        fail("GrowthFixedTerminalSkillMaskMismatch", person.actor_id);
      for (auto &value : person.skill_qualification_views) {
        value.source_identity = common.state().identity;
        value.observed_at = body.actual_now;
      }
      const auto actual = take(read_actual_actor_spatial_location(
          common, body.fixed_spatial->movement, body.fixed_spatial->original,
          body.fixed_spatial->battle, person.actor_id));
 if(std::holds_alternative<FieldBattleSpatialView>(actual.actual_anchor))fail("GrowthFixedTerminalUnadoptedSpatialChange",person.actor_id);
      const auto *now =
          std::get_if<SettlementLocation>(&actual.location_snapshot);
      const auto *frozen =
          std::get_if<SettlementLocation>(&person.location_snapshot);
      if (!now || !frozen || now->settlement_id != frozen->settlement_id ||
          now->place_id != frozen->place_id ||
          take(read_actual_transport_participation_presence(
              body.fixed_spatial->carriers, body.fixed_spatial->original,
              person.actor_id)))
        fail("GrowthFixedTerminalUnadoptedSpatialChange", person.actor_id);
    }
    if (next.action_kind == "long_rest") {
      const auto *raw = growth.facts.find(next.action_ref);
      const auto *rest = raw ? std::get_if<RestFact>(raw) : nullptr;
      auto &quiet =
          next.input_snapshot.specific.growth_fact.original_quiet_or_rest_basis;
      if (!rest || !quiet)
        fail("GrowthLongRestQuietSourceRequired", next.action_ref);
      take(refresh_growth_long_rest_quiet_projection(body, rest->actor_id,
                                                     *quiet));
      if (!quiet->is_quiet)
        fail("GrowthLongRestNotQuiet", next.action_ref);
    }
    PreparedGrowthFixedTerminalInputs result;
    result.state_ = std::move(p);
    return {true, std::move(result), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<StableId> complete_growth_fixed_terminal_in_candidate(
    CommonStore &common, GrowthState &growth,
    const CharacterConditionSources &body, ActionElapsedStore &elapsed,
    const ActionTimeRegistration &registration, const GrowthStep &applied,
    PreparedGrowthFixedTerminalInputs &prepared,
    const std::function<Status(const StableId &)> &release) {
  try {
    auto p = prepared.state_;
    if (!p || p->consumed || p->common != &common || p->growth != &growth ||
        !same(p->identity, common.state().identity) ||
        p->capture != common.actor_condition_capture_identity() ||
        body.growth != &growth || body.elapsed != &elapsed || !body.scale ||
        !body.growth_catalog || !body.shared || !release)
      fail("GrowthFixedTerminalCaptureExpired");
    take(common.validate_actor_condition_savepoint_boundary(p->savepoint));
    auto &next = p->terminal;
    if (take(compare_world_moments(next.occurred_at, body.actual_now)) ||
        !applied.completed || registration.action_kind != next.action_kind ||
        registration.owner_domain != "growth" ||
        last(growth, next.action_ref).boundary_id !=
            *next.previous_boundary_ref)
      fail("GrowthFixedTerminalOriginalApplicationMismatch", next.action_ref);
    bool success = false;
    auto result = outcome(growth, next.action_ref, success);
    if (success && !p->full_gate)
      fail("GrowthFixedSuccessfulResultBeforeFullService", next.action_ref);
    // 原业务已完成一次成长；此处只冻结同prepared真实时刻，不再支付或转货。
    if (next.action_kind == "trade") {
      auto *life = growth.life_activities.find(next.action_ref);
      if (!body.growth_catalog->complete_study_sources || !life ||
          life->view.kind != LifeActivityKind::trade ||
          !std::holds_alternative<TradeTerms>(life->terms) ||
          life->view.phase != LifeActivityPhase::completed || !success ||
          !life->view.code.empty() ||
          life->view.completed_tick != body.actual_now.half_day_tick ||
          life->selected_trade_settlement || life->trade_terminal_time)
        fail("GrowthTradeActualCompletionRequired", next.action_ref);
      life->trade_terminal_time = TradeTerminalTime{2, body.actual_now};
      // 原业务grant必须来自本次prepared前像和同一人物的实际写后值。
      if (body.growth_catalog->complete_capability_application_chain) {
        const auto *grant = growth.trade_grants.find(next.action_ref);
        const auto *actor = common.state().actors.find(life->view.actor_id);
        if (!p->trade_xp_before || !grant || !actor ||
            grant->actor_id != actor->id ||
            grant->xp_before != *p->trade_xp_before ||
            grant->xp_after != actor->skills.commerce ||
            grant->capability_application)
          fail("GrowthTradeGrantActualApplicationMismatch", next.action_ref);
      }
    }

    next.kind = success ? LifeFixedBoundaryKind::completed
                        : LifeFixedBoundaryKind::stopped;
    LifeOriginalTerminalApplication cause;
    cause.disposition =
        success ? LifeOriginalTerminalApplication::Disposition::completed
                : LifeOriginalTerminalApplication::Disposition::stopped;
    cause.actual_result_refs.push_back(result);
    if (success) {
      const auto &snapshot = next.input_snapshot;
      const auto *rule =
          next.owner_ref.kind == 0 ? &snapshot.specific.growth_fact
                                          .original_training_or_study_rule_ref
          : snapshot.specific.life_activity.activity_ref
              ? &*snapshot.specific.life_activity.activity_ref
          : next.action_kind == "trade" && snapshot.locked_rule_refs.size() == 1
              ? &snapshot.locked_rule_refs.front()
              : nullptr;
      if (!rule)
        fail("GrowthFixedOriginalCompletionRuleRequired", next.action_ref);
      cause.trigger = LifeFixedCompletionGate{next.action_ref, *rule, {}};
    } else
      cause.trigger = LifeOriginalResultTrigger{result};
    next.cause = std::move(cause);
    next.original_outcome_ref = result;
    take(release(next.action_ref));
    if (common.action(next.action_ref) || growth.actions.find(next.action_ref))
      fail("GrowthFixedOriginalReleaseIncomplete", next.action_ref);
    if (next.action_kind == "trade") {
      take(read_growth_trade_terminal_v2(
          common, growth, *body.growth_catalog, *body.shared,
          {"growth", "life_activity", next.action_ref}, 2, body.actual_now));
      if (body.growth_catalog->complete_capability_application_chain) {
        const auto grant = take(read_growth_trade_grant_v1(
            common, growth, *body.growth_catalog, *body.shared,
            {next.action_ref}, body.actual_now));
        if (take(compare_world_moments(grant.terminal->ended_at, body.actual_now)))
          fail("GrowthTradeGrantActualMomentMismatch", next.action_ref);
      }
    }
    next.boundary_id = take(common.next_id("growth_fixed_service_boundary"));
    const auto id = next.boundary_id;
    for (auto &person : next.input_snapshot.actors)
      for (auto &value : person.skill_qualification_views)
        value.actual_boundary_ref = {
            {"growth", "growth_fixed_service_boundary", id}, 1};
    if (!growth.fixed_service_boundaries.insert(id, std::move(next)))
      fail("DuplicateGrowthFixedBoundary", id);
    p->consumed = true;
    if (growth::study_action_kind(registration.action_kind))
      take(bind_study_time_after_actual_boundary(common, growth, id));
    take(adopt_growth_fixed_boundary_in_candidate(
        common, growth, *body.growth_catalog, *body.shared, elapsed,
        *body.scale, body.catalog.identity, registration, id, body.actual_now,
        {}, body.interaction, body.interaction_catalog,
        p->market_exact ? &*p->market_exact : nullptr));
    return {true, id, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<StableId> adopt_growth_fixed_period_inputs_in_candidate(
    CommonStore &common, GrowthState &growth,
    const CharacterConditionSources &body, ActionElapsedStore &elapsed,
    const ActionTimeRegistration &registration,
    const GrowthFixedStepPermit &gate,
    const GrowthFixedStudyApplication &applied,
    PreparedGrowthFixedTerminalInputs &prepared) {
  try {
    auto p = prepared.state_;
    if (!p || p->consumed || p->common != &common || p->growth != &growth ||
        !same(p->identity, common.state().identity) ||
        p->capture != common.actor_condition_capture_identity() ||
        body.growth != &growth || body.elapsed != &elapsed || !body.scale ||
        !body.growth_catalog || !body.shared)
      fail("GrowthFixedInputCaptureExpired");
    take(common.validate_actor_condition_savepoint_boundary(p->savepoint));
    const auto *raw = growth.facts.find(gate.action_id());
    const auto *fact = raw ? std::get_if<StudyFact>(raw) : nullptr;
    const auto *rest = raw ? std::get_if<RestFact>(raw) : nullptr;
    if ((fact ? (fact->phase != "started" ||
                 !growth::study_action_kind(registration.action_kind))
              : (!rest || rest->phase != "active" ||
                 registration.action_kind != "long_rest")) ||
        applied.result.completed ||
        registration.action_kind != p->terminal.action_kind ||
        registration.owner_domain != "growth" ||
        p->terminal.action_ref != gate.action_id() ||
        last(growth, gate.action_id()).boundary_id !=
            *p->terminal.previous_boundary_ref ||
        take(compare_world_moments(gate.at(), body.actual_now)))
      fail("GrowthFixedActualStudyPeriodRequired", gate.action_id());
    const StudyLessonResultRef lesson{gate.action_id(), gate.ordinal()};
    std::optional<StudyLessonPermanentContributionView> net;
    if (fact) {
      net =
          take(read_study_lesson_permanent_contribution(body, growth, lesson));
      if (take(compare_world_moments(net->effective_at, body.actual_now)))
        fail("GrowthFixedActualStudyPeriodSourcesRequired", gate.action_id());
    } else if (rest->actual_periods.empty() ||
               rest->actual_periods.back().ordinal != gate.ordinal() ||
               take(compare_world_moments(
                   rest->actual_periods.back().applied_at, body.actual_now)))
      fail("GrowthFixedActualRestPeriodSourcesRequired", gate.action_id());
    if (applied.actual_condition_writes.empty())
      fail("GrowthFixedActualPeriodWritesRequired", gate.action_id());
    LifeFixedServiceBoundary next = std::move(p->terminal);
    StableId last_id;
    auto append = [&](LifeFixedInputChange change,
                      const ActorFoundationRuleRef &rule,
                      LifeFixedBoundaryKind kind =
                          LifeFixedBoundaryKind::input_rebound) {
      next.boundary_id = take(common.next_id("growth_fixed_service_boundary"));
      const auto id = next.boundary_id;
      next.kind = kind;
      next.original_outcome_ref.reset();
      next.cause = LifeActualInputChange{std::move(change), rule};
      for (auto &person : next.input_snapshot.actors)
        for (auto &value : person.skill_qualification_views)
          value.actual_boundary_ref = {
              {"growth", "growth_fixed_service_boundary", id}, 1};
      if (!growth.fixed_service_boundaries.insert(id, std::move(next)))
        fail("DuplicateGrowthFixedBoundary", id);
      if (fact)
        take(bind_study_time_after_actual_boundary(common, growth, id));
      take(adopt_growth_fixed_boundary_in_candidate(
          common, growth, *body.growth_catalog, *body.shared, elapsed,
          *body.scale, body.catalog.identity, registration, id, body.actual_now,
          {}, body.interaction, body.interaction_catalog,
          p->market_exact ? &*p->market_exact : nullptr));
      const auto *row = growth.fixed_service_boundaries.find(id);
      if (!row || row->boundary_ordinal == std::numeric_limits<Integer>::max())
        fail("GrowthFixedInputOrdinalOverflow", id);
      next = *row;
      next.boundary_id.clear();
      next.previous_boundary_ref = id;
      ++next.boundary_ordinal;
      last_id = id;
    };
    if (net && net->xp_gained > 0) {
      bool changed = false;
      ActorFoundationRuleRef selected;
      for (auto &person : next.input_snapshot.actors)
        if (person.actor_id == fact->terms.student_id) {
          for (auto &value : person.skill_qualification_views)
            if (value.skill_id == fact->terms.skill_id) {
              if (value.minimum_xp != net->xp_before)
                fail("GrowthFixedStudyInputBeforeMismatch", person.actor_id);
              value.minimum_xp = net->xp_after;
              value.minimum_level = growth_skill_level(value.minimum_xp);
              value.contribution_refs.emplace_back(lesson);
              changed = true;
            }
          if (person.capability_frames.empty())
            fail("GrowthFixedStudyCapabilityRequired", person.actor_id);
          selected = person.capability_frames.front().result.rule_ref;
        }
      if (!changed)
        fail("GrowthFixedStudyActualSkillInputMissing", gate.action_id());
      append(LifeGrowthInputChange{lesson}, selected);
    }
    for (const auto &binding : applied.actual_condition_writes) {
      const auto *receipt =
          body.state.character_condition_extensions.applied_receipts.find(
              binding.receipt_ref.result_id);
      if (!receipt)
        fail("GrowthFixedAppliedConditionMissing",
             binding.receipt_ref.result_id);
      if (fact) {
        const auto *source =
            std::get_if<WorkOrStudyConditionCause>(&receipt->cause);
        if (!source ||
            source->kind != WorkOrStudyConditionCause::Kind::study_lesson ||
            source->action_id != gate.action_id() ||
            source->ordinal != gate.ordinal())
          fail("GrowthFixedStudyConditionSourceMismatch",
               binding.receipt_ref.result_id);
      } else {
        const auto *source =
            std::get_if<OriginalRestProvidedCause>(&receipt->cause);
        const auto *key =
            std::get_if<RestConditionBusinessKey>(&receipt->business_key);
        if (!source || source->rest_kind != "long_rest" ||
            source->action_id != gate.action_id() || !key ||
            key->completion_ordinal != gate.ordinal())
          fail("GrowthFixedRestConditionSourceMismatch",
               binding.receipt_ref.result_id);
      }

      bool changed = false, allowed = true;
      ActorFoundationRuleRef selected;
      for (auto &person : next.input_snapshot.actors)
        if (person.actor_id == receipt->actor_id) {
          const auto *skill = take(
              read_growth_fixed_required_skill(body, growth, next, person));
          for (auto &frozen : person.capability_frames) {
            std::optional<StableId> required;
            if (frozen.purpose_ref.id == "study" ||
                frozen.purpose_ref.id == "provide_teaching") {
              if (!skill)
                fail("GrowthFixedStudyCapabilitySkillMissing", person.actor_id);
              required = *skill;
            }
            frozen = take(freeze_character_capability_after_bound_condition(
                body, frozen, required, binding));
            allowed = allowed && frozen.result.allowed;
            selected = frozen.result.rule_ref;
            changed = true;
          }
        }
      if (!changed) {
        if (rest && body.growth_catalog->complete_exact_rest_mouth_sources) {
          const auto &period = rest->actual_periods.back();
          std::size_t count = 0;
          for (std::size_t n = 0; n < period.mouth_actor_ids.size(); ++n)
            if (period.mouth_actor_ids[n] == receipt->actor_id &&
                period.condition_receipt_refs[n].result_id ==
                    binding.receipt_ref.result_id)
              ++count;
          if (count != 1)
            fail("GrowthFixedRestMouthConditionSourceMismatch",
                 receipt->actor_id);
          continue;
        }
        fail("GrowthFixedPeriodConditionActorMissing", receipt->actor_id);
      }
      auto kind = LifeFixedBoundaryKind::input_rebound;
      if (!allowed && fact && !growth::study_paused(*fact)) {
        take(growth::pause_study_in_candidate(common, growth, gate.action_id(),
                                              body.actual_now));
        kind = LifeFixedBoundaryKind::suspended;
      } else if (!allowed && !fact)
        fail("GrowthFixedRestActualStopRequired", gate.action_id());
      append(LifeConditionInputChange{binding.receipt_ref}, selected, kind);
    }
    if (last_id.empty())
      fail("GrowthFixedActualPeriodAdoptionRequired", gate.action_id());
    p->consumed = true;
    return {true, std::move(last_id), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
