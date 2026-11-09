#include "growth_condition_input_consumer.hpp"
#include "growth_pending_started.hpp"
#include "growth_action_time_read.hpp"
#include "growth_internal.hpp"
#include "growth_life_results.hpp"
#include "growth_quiet_source.hpp"
#include "growth_trade_terminal_read.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/domains/military/named_military_custody_history_model.hpp"
#include "resident/domains/military/named_military_custody_placement.hpp"
#include "resident/domains/travel/military_action_placement.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
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
const LifeFixedServiceBoundary &latest(const GrowthState &growth,
                                       const StableId &action) {
  const LifeFixedServiceBoundary *result = nullptr;
  for (const auto &row : growth.fixed_service_boundaries.rows())
    if (row.second.action_ref == action &&
        (!result || row.second.boundary_ordinal > result->boundary_ordinal))
      result = &row.second;
  if (!result)
    fail("GrowthInputOriginalStartRequired", action);
  return *result;
}
const ActionTimeRegistration &
registration(const CharacterConditionSources &body, const StableId &kind) {
  const ActionTimeRegistration *result = nullptr;
  for (const auto &row : *body.action_registrations)
    if (row.owner_domain == "growth" && row.action_kind == kind) {
      if (result)
        fail("DuplicateGrowthFixedRegistration", kind);
      result = &row;
    }
  if (!result)
    fail("GrowthFixedRegistrationMissing", kind);
  return *result;
}
class Release final : public StudyOriginalActionReleaseWrite {
  const std::function<Status(const StableId &)> &release_;

public:
  explicit Release(const std::function<Status(const StableId &)> &r)
      : release_(r) {}
  Status release_original_study_action(const StableId &id) override {
    return release_(id);
  }
};
struct ActualChange {
  const StableId *actor = nullptr;
  LifeFixedInputChange source;
  const BoundActorConditionWrite *body = nullptr;
  const CapacityTransitionV2 *capacity = nullptr;
  std::optional<DeathRef> death;
  const ActorSpatialHeadChange *spatial = nullptr;
  const GrowthQuietHeadChange *quiet = nullptr;
};
// 只接原军事释放自身的实际落点头；分类不代替原行、人物、版本和时刻证明。
// 完整释放期限/选址反覆盖仍由外层 Military validator 负责，不在这里递归验整图。
void verify_named_custody_position_change(
    const CharacterConditionSources &body, const SourceRef &source,
    const StableId &actor, const ActorSpatialHeadChange &event) {
  const auto *spatial = body.fixed_spatial;
  if (!spatial || !spatial->custody_releases ||
      !spatial->custody_releases->bound_to(body.common, spatial->movement))
    fail("NamedMilitaryCustodyPlacementOriginalReadRequired", actor);
  const auto *actual = take(spatial->custody_releases->read_named_custody_placement(
      source, actor));
  if (!actual || actual->id != source.id || actual->actor_id != actor ||
      event.immutable_source_head.size() != 1 ||
      event.immutable_source_head.front().schema != 1 ||
      !same_source_ref(event.immutable_source_head.front().source, source) ||
      take(compare_world_moments(actual->released_at, event.occurred_at)) ||
      !take(same_spatial_anchor(actual->released_anchor, event.actual_anchor)))
    fail("GrowthNamedCustodyOriginalPositionMismatch", source.id);
}
bool already(const LifeFixedServiceBoundary &boundary,
             const ActualChange &change) {
  const LifeFixedInputChange *actual = nullptr;
  if (const auto *c = std::get_if<LifeActualInputChange>(&boundary.cause))
    actual = &c->change;
  else if (const auto *t =
               std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))
    if (const auto *stop = std::get_if<LifeActualStopTrigger>(&t->trigger))
      actual = std::get_if<LifeFixedInputChange>(&stop->source);
  if (!actual || actual->index() != change.source.index())
    return false;
  if (const auto *a = std::get_if<LifeConditionInputChange>(actual))
    return a->condition_receipt_ref.result_id ==
           change.body->receipt_ref.result_id;
  if (const auto *a = std::get_if<LifeDeathInputChange>(actual))
    return a->death_ref.id == change.death->id;
  if (const auto *a = std::get_if<LifeActualPositionInputChange>(actual)) {
    const auto *source =
        std::get_if<LifeActualPositionInputChange>(&change.source);
    return source && a->source_schema == source->source_schema &&
           same_source_ref(a->original_source_ref, source->original_source_ref);
  }
  if (const auto *a = std::get_if<LifeOccupationInputChange>(actual)) {
    const auto *source = std::get_if<LifeOccupationInputChange>(&change.source);
    return source && a->source_schema == source->source_schema &&
           same_source_ref(a->original_source_ref, source->original_source_ref);
  }
  return false;
}
// Re-entry at the end of one live slice sees the same immutable draft list.
// Earlier writes are skipped only after every service which actually read that
// predecessor has its own matching adoption (or a proven start/terminal cut).
void require_historical_change_consumed(const CommonStore &common,
                                        const GrowthState &growth,
                                        const CharacterConditionSources &body,
                                        const ActualChange &change,
                                        const WorldMoment &occurred) {
  const auto *receipt =
      change.body
          ? body.state.character_condition_extensions.applied_receipts.find(
                change.body->receipt_ref.result_id)
          : nullptr;
  for (const auto &row : growth.fixed_service_boundaries.rows()) {
    const auto &start = row.second;
    if (start.kind != LifeFixedBoundaryKind::started)
      continue;
    const LifeFixedActorInputs *initial = nullptr;
    for (const auto &person : start.input_snapshot.actors)
      if (person.actor_id == *change.actor) {
        if (initial && person.capability_frames.empty())
          fail("GrowthInputBodyFrameRequired", start.action_ref);
        initial = &person;
      }
    if (!initial)
      continue;
    const auto &last = latest(growth, start.action_ref);
    const bool terminal = last.kind == LifeFixedBoundaryKind::completed ||
                          last.kind == LifeFixedBoundaryKind::stopped;
    if (receipt) {
      if (initial->capability_frames.empty())
        fail("GrowthInputBodyFrameRequired", start.action_ref);
      if (initial->capability_frames.front()
              .condition_head_snapshot.condition_revision >=
          receipt->condition_revision_after)
        continue;
      if (terminal) {
        const LifeFixedActorInputs *final = nullptr;
        for (const auto &person : last.input_snapshot.actors)
          if (person.actor_id == *change.actor)
            final = &person;
        if (!final || final->capability_frames.empty())
          fail("GrowthInputBodyFrameRequired", last.boundary_id);
        if (final->capability_frames.front()
                    .condition_head_snapshot.condition_revision <
                receipt->condition_revision_after &&
            take(compare_world_moments(occurred, last.occurred_at)) >= 0)
          continue;
      }
    } else if (change.capacity) {
      const auto begins =
          take(compare_world_moments(occurred, start.occurred_at));
      if (begins < 0)
        continue;
      const auto &order = common.state().facts.order();
      const auto cause =
          std::find(order.begin(), order.end(),
                    take(read_original_availability_cause_position(
                             body, *change.capacity))
                        .source->id);
      if (cause == order.end())
        fail("GrowthAvailabilityInputOriginalCutRequired", start.action_ref);
      if (begins == 0) {
        const auto began =
            std::find(order.begin(), order.end(), start.action_ref);
        if (began == order.end())
          fail("GrowthAvailabilityInputOriginalCutRequired", start.action_ref);
        if (cause < began)
          continue;
      }
      if (terminal) {
        const auto ended =
            take(compare_world_moments(occurred, last.occurred_at));
        if (ended > 0)
          continue;
        if (ended == 0) {
          const GrowthTerminalPublication *publication = nullptr;
          for (const auto &entry : growth.terminal_publications.rows())
            if (entry.second.action_id == start.action_ref) {
              if (publication)
                fail("GrowthFixedTerminalPublicationAmbiguous",
                     start.action_ref);
              publication = &entry.second;
            }
          if (!publication)
            fail("GrowthFixedTerminalPublicationRequired", start.action_ref);
          const auto end = std::find(order.begin(), order.end(),
                                     publication->publication_id);
          if (end == order.end())
            fail("GrowthAvailabilityInputOriginalCutRequired",
                 start.action_ref);
          if (cause > end)
            continue;
        }
      }
    } else
      fail("GrowthHistoricalInputActualSourceRequired", start.action_ref);
    std::size_t count = 0;
    for (const auto &candidate : growth.fixed_service_boundaries.rows())
      if (candidate.second.action_ref == start.action_ref &&
          already(candidate.second, change)) {
        if (take(compare_world_moments(candidate.second.occurred_at, occurred)))
          fail("GrowthInputHistoricalAdoptionMomentMismatch", candidate.first);
        ++count;
      }
    if (count != 1)
      fail("GrowthEarlierInputNotConsumed", start.action_ref);
  }
}
GrowthEffect
stop_original(CommonStore &common, GrowthState &growth,
              const CharacterConditionSources &body,
              const GrowthFixedCaptureServices &services, const StableId &id,
              const ActualChange &change,
              const std::function<Status(const StableId &)> &release,
              GrowthConditionInputEffects &output) {
  const auto *action = common.action(id);
  if (!action)
    fail("GrowthInputOriginalActionRequired", id);
  GrowthEffect effect;
  effect.kind = action->kind;
  effect.action_id = id;
  effect.success = false;
  if (auto *raw = growth.facts.find(id)) {
    if (auto *study = std::get_if<StudyFact>(raw)) {
      if (!change.death || !body.deaths || !body.life_sources)
        fail("StudyPermanentCapacitySourceRequired", id);
      std::vector<ResourceKey> resources{
          {ResourceKey::Kind::actor, *change.actor}};
      std::vector<DomainReadKey> domains{
          {"actor_foundation", "condition_head", *change.actor}};
      std::vector<StableId> sources{change.body->receipt_ref.result_id,
                                    change.death->id};
      auto stamp = take(services.reads.capture(resources, domains, sources));
      if (!stamp)
        fail("StudyDeathActualReadStampRequired", id);
      FamilyVerifiedDeathRead deaths(common, *body.deaths, *body.life_sources,
                                     services.authority, services.versions);
      Release actual_release(release);
      StudyVerifiedParticipantDeath cause;
      cause.actor_id = *change.actor;
      cause.role = study->terms.teacher_id == *change.actor
                       ? StudyParticipantRole::teacher
                       : StudyParticipantRole::student;
      cause.death_ref = *change.death;
      output.pending_study_releases.push_back(
          take(stop_study_for_verified_death_in_capture(
              common, growth, body, services.original, id, cause, *change.body,
              *stamp, deaths, actual_release)));
      effect.code = study->code;
      effect.skill_id = study->terms.skill_id;
      effect.xp_gained = growth::study_xp_gained(*study);
      effect.completed_lessons = static_cast<Integer>(study->lessons.size());
      effect.fee_paid =
          (growth::original_study_lesson_cost(study->terms.xp_before)) *
          effect.completed_lessons;
      effect.unprovided_released = study->terms.fee - effect.fee_paid;
      effect.challenge = study->challenge;
      return effect;
    }
    if (auto *training = std::get_if<TrainingFact>(raw)) {
      if (training->phase != "started")
        fail("GrowthInputOriginalTrainingRequired", id);
      training->phase = "completed";
      training->completed_tick = body.actual_now.half_day_tick;
      training->success = false;
      effect.code = "TrainingInterrupted";
      effect.skill_id = training->training.skill_id;
      effect.mastery = training->training.mastery_before;
    } else if (auto *rest = std::get_if<RestFact>(raw)) {
      if (rest->phase != "active")
        fail("GrowthInputOriginalRestRequired", id);
      rest->phase = "completed";
      rest->completed_tick = body.actual_now.half_day_tick;
      rest->stop_reason = change.quiet && !change.quiet->after.quiet
                              ? "RestNeedsQuiet"
                              : "RestInterrupted";
      rest->last_mouths.clear();
      effect.code = rest->stop_reason;
      effect.elapsed = rest->elapsed;
    } else
      fail("GrowthInputOriginalResultRequired", id);
  } else {
    auto *life = growth.life_activities.find(id);
    if (!life || life->view.phase != LifeActivityPhase::active)
      fail("GrowthInputOriginalLifeRequired", id);
    const bool trade = life->view.kind == LifeActivityKind::trade;
    if (trade && (!body.growth_catalog->complete_study_sources ||
                  !std::holds_alternative<TradeTerms>(life->terms) ||
                  life->selected_trade_settlement || life->trade_terminal_time))
      fail("GrowthTradeActualCancellationRequired", id);
    effect.code = "ActorUnavailable";
    take(cancel_life_activity(common, growth, id, body.actual_now.half_day_tick,
                              effect.code));
    // 原停止原因已被同capture采用；只记实际终点，不退回开工的钱货。
    if (trade)
      life->trade_terminal_time = TradeTerminalTime{2, body.actual_now};
  }
  take(release(id));
  if (const auto *life = growth.life_activities.find(id);
      life && life->view.kind == LifeActivityKind::trade)
    take(read_growth_trade_terminal_v2(
        common, growth, *body.growth_catalog, *body.shared,
        {"growth", "life_activity", id}, 2, body.actual_now));
  return effect;
}
// 原输入逐行动消费；仅不可伪造的未变新开工可等真实封存，旧行动仍走原守卫。
void apply_one(CommonStore &common, GrowthState &growth,
               const CharacterConditionSources &body,
               ActionElapsedStore &elapsed,
               const GrowthFixedCaptureServices &services,
               const ActualChange &change, const StableId &id,
               const std::function<Status(const StableId &)> &release,
               GrowthConditionInputEffects &output,
               const GrowthPendingStartedProof *pending) {
  const auto *action = common.action(id);
  if (!action || !growth.actions.find(id))
    return;
  // 只允许真实同候选且输入未变的新开工等待原封存，旧来源仍走原守卫。
  if (pending && take(pending->permits(common, growth, body, id)))
    return;
  const auto &old = latest(growth, id);
  bool reads = false;
  for (const auto &input : old.input_snapshot.actors)
    if (input.actor_id == *change.actor)
      reads = true;
  if (!reads)
    return;
  for (const auto &row : growth.fixed_service_boundaries.rows())
    if (row.second.action_ref == id && already(row.second, change))
      return;
  // Original completion applies its own fatigue after taking the terminal input
  // cut. That receipt is an output, never an input to that already-ended
  // service.
  if (old.kind == LifeFixedBoundaryKind::completed ||
      old.kind == LifeFixedBoundaryKind::stopped)
    return;
  if (change.capacity) {
    const LifeFixedServiceBoundary *start = nullptr;
    for (const auto &row : growth.fixed_service_boundaries.rows())
      if (row.second.action_ref == id &&
          row.second.kind == LifeFixedBoundaryKind::started)
        start = &row.second;
    if (!start)
      fail("GrowthInputOriginalStartRequired", id);
    const auto relative = take(compare_world_moments(
        change.capacity->effective_at.exact_moment, start->occurred_at));
    if (relative < 0)
      return;
    if (relative == 0) {
      const auto &order = common.state().facts.order();
      const auto began = std::find(order.begin(), order.end(), id),
                 cause =
                     std::find(order.begin(), order.end(),
                               take(read_original_availability_cause_position(
                                        body, *change.capacity))
                                   .source->id);
      if (began == order.end() || cause == order.end())
        fail("GrowthAvailabilityInputOriginalCutRequired", id);
      if (cause < began)
        return;
    }
  }
  const ConditionReceiptV2 *receipt =
      change.body
          ? body.state.character_condition_extensions.applied_receipts.find(
                change.body->receipt_ref.result_id)
          : nullptr;
  if (receipt) {
    for (const auto &person : old.input_snapshot.actors)
      if (person.actor_id == *change.actor) {
        if (person.capability_frames.empty())
          fail("GrowthInputBodyFrameRequired", id);
        if (person.capability_frames.front()
                .condition_head_snapshot.condition_revision >=
            receipt->condition_revision_after) {
          const LifeFixedServiceBoundary *start = nullptr;
          for (const auto &row : growth.fixed_service_boundaries.rows())
            if (row.second.action_ref == id &&
                row.second.kind == LifeFixedBoundaryKind::started)
              start = &row.second;
          bool initial = false;
          if (start)
            for (const auto &original : start->input_snapshot.actors)
              if (original.actor_id == *change.actor &&
                  !original.capability_frames.empty() &&
                  original.capability_frames.front()
                          .condition_head_snapshot.condition_revision >=
                      receipt->condition_revision_after)
                initial = true;
          if (initial)
            return;
          fail("GrowthInputMissingActualConditionAdoption", id);
        }
      }
  }
  if (old.boundary_ordinal == std::numeric_limits<Integer>::max())
    fail("GrowthFixedOrdinalOverflow", id);
  LifeFixedServiceBoundary next;
  next.action_ref = id;
  next.action_kind = action->kind;
  next.owner_ref = old.owner_ref;
  next.previous_boundary_ref = old.boundary_id;
  next.boundary_ordinal = old.boundary_ordinal + 1;
  next.occurred_at = body.actual_now;
  next.input_snapshot = old.input_snapshot;
  next.kind = LifeFixedBoundaryKind::input_rebound;
  ActorFoundationRuleRef rule;
  bool allowed = true;
  for (auto &person : next.input_snapshot.actors) {
    const auto *skill =
        take(read_growth_fixed_required_skill(body, growth, next, person));
    for (auto &frame : person.capability_frames) {
      std::optional<StableId> required;
      if (frame.purpose_ref.id == "study" ||
          frame.purpose_ref.id == "provide_teaching") {
        if (!skill)
          fail("GrowthInputOriginalSkillRequired", id);
        required = *skill;
      }
      if (change.body && person.actor_id == *change.actor)
        frame = take(freeze_character_capability_after_bound_condition(
            body, frame, required, *change.body));
      else
        frame = take(
            freeze_character_capability(take(read_character_capability_at_head(
                body, frame.condition_head_snapshot, frame.purpose_ref,
                required, body.actual_now))));
      if (person.actor_id == *change.actor) {
        allowed = allowed && frame.result.allowed;
        rule = frame.result.rule_ref;
      }
    }
    for (auto &q : person.skill_qualification_views) {
      q.source_identity = common.state().identity;
      q.observed_at = body.actual_now;
    }
  }
  if (change.capacity) {
    allowed = change.capacity->to;
    rule = change.capacity->purpose;
  }
  if (change.spatial) {
    if (!body.fixed_spatial || change.spatial->immutable_source_head.empty() ||
        take(compare_world_moments(change.spatial->occurred_at,
                                   body.actual_now)))
      fail("GrowthPositionInputActualMomentRequired", id);
    bool affected = false;
    for (auto &person : next.input_snapshot.actors)
      if (person.actor_id == *change.actor) {
        person.actual_location_source_refs.clear();
        for (const auto &source : change.spatial->immutable_source_head) {
          if (take(actor_spatial_history_source_schema(source.source)) !=
              source.schema)
            fail("GrowthFixedSpatialSchemaMismatch", source.source.id);
          person.actual_location_source_refs.emplace_back(source.source);
        }
        affected = true;
      }
    if (!affected)
      fail("GrowthPositionInputActorNotRead", id);
    rule = growth_fixed_original_site_rule(body.catalog.identity);
    // This input change only reads the original location predicate. Existing
    // body predicates remain unchanged and do not invent a second source here.
    allowed = true;
    for (const auto &person : next.input_snapshot.actors)
      allowed =
          allowed && take(read_growth_fixed_actor_at_original_site(
                         body, *body.fixed_spatial, growth, next, person));
  }
  if (change.quiet) {
    auto &quiet =
        next.input_snapshot.specific.growth_fact.original_quiet_or_rest_basis;
    if (next.action_kind != "long_rest" || !quiet ||
        !body.catalog.original_long_rest_rule ||
        take(compare_world_moments(change.quiet->occurred_at, body.actual_now)))
      fail("GrowthQuietInputActualMomentRequired", id);
    quiet->actual_conflict_source_refs.clear();
    for (const auto &source : change.quiet->after.original_sources)
      quiet->actual_conflict_source_refs.emplace_back(source);
    quiet->is_quiet = change.quiet->after.quiet;
    quiet->observed_at = body.actual_now;
    allowed = quiet->is_quiet;
    rule = body.catalog.original_long_rest_rule->rule_ref;
  }
  if (rule.id.empty())
    fail("GrowthInputActualRuleRequired", id);
  if (!body.fixed_spatial)
    fail("GrowthFixedSpatialSourcesRequired", id);
  take(
      refresh_growth_fixed_spatial_projection(body, *body.fixed_spatial, next));
  if (next.action_kind == "long_rest") {
    auto *raw = growth.facts.find(next.action_ref);
    auto *rest = raw ? std::get_if<RestFact>(raw) : nullptr;
    auto &quiet =
        next.input_snapshot.specific.growth_fact.original_quiet_or_rest_basis;
    if (!rest || !quiet)
      fail("GrowthLongRestQuietSourceRequired", next.action_ref);
    take(refresh_growth_long_rest_quiet_projection(body, rest->actor_id,
                                                   *quiet));
  }
  const auto *raw = growth.facts.find(id);
  const auto *study = raw ? std::get_if<StudyFact>(raw) : nullptr;
  const bool paused = study && growth::study_paused(*study);
  bool stopped = false;
  if (change.death || (!allowed && !study))
    stopped = true;
  else if (!allowed && !paused) {
    take(growth::pause_study_in_candidate(common, growth, id, body.actual_now));
    next.kind = LifeFixedBoundaryKind::suspended;
    for (const auto &person : growth::study_participants(study->terms))
      output.suspended_actors.push_back(person);
  }
  std::optional<GrowthEffect> original_result;
  if (stopped) {
    original_result = stop_original(common, growth, body, services, id, change,
                                    release, output);
    if (common.action(id) || growth.actions.find(id))
      fail("GrowthInputOriginalReleaseIncomplete", id);
    next.kind = LifeFixedBoundaryKind::stopped;
    LifeOriginalTerminalApplication terminal;
    terminal.disposition =
        LifeOriginalTerminalApplication::Disposition::stopped;
    LifeActualStopTrigger trigger;
    trigger.original_stop_rule_ref = rule;
    trigger.source = change.source;
    terminal.trigger = std::move(trigger);
    LifeFixedOutcomeRef result;
    result.id = id;
    result.kind = next.owner_ref.kind == 1
                      ? LifeFixedOutcomeRef::Kind::life_activity
                  : next.owner_ref.fact_kind == GrowthFixedFactKind::training
                      ? LifeFixedOutcomeRef::Kind::training
                  : next.owner_ref.fact_kind == GrowthFixedFactKind::study
                      ? LifeFixedOutcomeRef::Kind::study
                      : LifeFixedOutcomeRef::Kind::rest;
    terminal.actual_result_refs.push_back(result);
    next.original_outcome_ref = result;
    next.cause = std::move(terminal);
  } else
    next.cause = LifeActualInputChange{change.source, rule};
  next.boundary_id = take(common.next_id("growth_fixed_service_boundary"));
  const auto boundary = next.boundary_id;
  for (auto &person : next.input_snapshot.actors)
    for (auto &q : person.skill_qualification_views)
      q.actual_boundary_ref = {
          {"growth", "growth_fixed_service_boundary", boundary}, 1};
  const auto &reg = registration(body, next.action_kind);
  if (!growth.fixed_service_boundaries.insert(boundary, std::move(next)))
    fail("DuplicateGrowthFixedBoundary", boundary);
  if (study)
    take(bind_study_time_after_actual_boundary(common, growth, boundary));
  take(adopt_growth_fixed_boundary_in_candidate(
      common, growth, *body.growth_catalog, *body.shared, elapsed, *body.scale,
      body.catalog.identity, reg, boundary, body.actual_now, {},
      body.interaction, body.interaction_catalog, services.market_exact));
  output.adopted_boundary_ids.push_back(boundary);
  if (original_result)
    output.completed_actions.emplace_back(id, std::move(*original_result));
}
} // namespace
Outcome<GrowthConditionInputEffects>
consume_growth_condition_inputs_in_candidate(
    CommonStore &common, GrowthState &growth,
    const CharacterConditionSources &body, ActionElapsedStore &elapsed,
    const GrowthFixedCaptureServices &services,
    const std::vector<BoundActorConditionWrite> &writes,
    const std::vector<BoundActorAvailabilityWrite> &availability,
    const std::function<Status(const StableId &)> &release,
    const GrowthPendingStartedProof *pending) {
  try {
    if (&body.common != &common || body.growth != &growth ||
        body.elapsed != &elapsed || !body.growth_catalog || !body.shared ||
        !body.scale || !body.action_registrations || !release)
      fail("GrowthConditionInputSameCandidateRequired");
    const auto capture = common.actor_condition_capture_identity();
    if (!capture)
      fail("GrowthConditionInputCaptureRequired");
    GrowthConditionInputEffects output;
    const auto &drafts = common.actor_condition_execution_drafts();
    // Use the actual successful Common mutation order, including zero-net
    // writes.
    for (std::size_t n = 0; n < drafts.size(); ++n) {
      const auto seq = static_cast<Integer>(n + 1);
      const BoundActorConditionWrite *scalar = nullptr;
      const BoundActorAvailabilityWrite *capacity = nullptr;
      for (const auto &value : writes)
        if (value.capture_sequence == seq) {
          if (scalar || value.capture_identity != capture)
            fail("GrowthConditionInputCaptureMismatch",
                 value.receipt_ref.result_id);
          scalar = &value;
        }
      for (const auto &value : availability)
        if (value.capture_sequence == seq) {
          if (capacity || value.capture_identity != capture)
            fail("GrowthConditionInputCaptureMismatch", value.transition_id);
          capacity = &value;
        }
      if (!scalar && !capacity)
        continue;
      if (scalar && capacity)
        fail("GrowthConditionInputCaptureAmbiguous", drafts[n].actor_id);
      ActualChange change;
      if (scalar) {
        const auto *receipt =
            body.state.character_condition_extensions.applied_receipts.find(
                scalar->receipt_ref.result_id);
        if (!receipt || receipt->actor_id != drafts[n].actor_id ||
            receipt->effective_at.kind != ActorFoundationAt::Kind::exact_moment)
          fail("GrowthConditionInputActualMomentRequired",
               scalar->receipt_ref.result_id);
        change.actor = &receipt->actor_id;
        change.body = scalar;
        if (receipt->life_transition_ref) {
          if (!body.deaths || !body.life_sources)
            fail("GrowthConditionInputDeathSourcesRequired",
                 receipt->result_id);
          change.death = DeathRef{"family_life", "family_death",
                                  *receipt->life_transition_ref};
          auto death = take(
              read_verified_death4(common, *body.deaths, *body.life_sources,
                                   *change.death, body.actual_now));
          if (death.actor_id != *change.actor)
            fail("GrowthConditionInputDeathMismatch", receipt->result_id);
          change.source = LifeDeathInputChange{*change.death};
        } else
          change.source = LifeConditionInputChange{scalar->receipt_ref};
      } else {
        auto original = take(read_verified_military_availability_transition(
            body, capacity->transition_id));
        const auto &draft = drafts[n];
        if (draft.kind != ActorConditionExecutionDraft::Kind::mutation ||
            !draft.before || draft.actor_id != original->actor_id ||
            !draft.available_written ||
            draft.before->available != original->from ||
            draft.after.available != original->to)
          fail("GrowthAvailabilityInputActualCaptureRequired",
               capacity->transition_id);
        change.actor = &original->actor_id;
        change.capacity = original;
        change.source = LifeOccupationInputChange{
            military_availability_transition_source(original->transition_id),
            2};
      }
      const auto &occurred =
          change.body
              ? body.state.character_condition_extensions.applied_receipts
                    .find(change.body->receipt_ref.result_id)
                    ->effective_at.exact_moment
              : change.capacity->effective_at.exact_moment;
      const auto relative =
          take(compare_world_moments(occurred, body.actual_now));
      if (relative > 0)
        fail("GrowthInputFutureCaptureWrite", *change.actor);
      if (relative < 0) {
        require_historical_change_consumed(common, growth, body, change,
                                           occurred);
        continue;
      }
      // Results live in different tables from the active action index. Original
      // release may erase actions without invalidating this source traversal.
      for (const auto &row : growth.facts.rows())
        apply_one(common, growth, body, elapsed, services, change, row.first,
                  release, output, pending);
      for (const auto &row : growth.life_activities.rows())
        apply_one(common, growth, body, elapsed, services, change, row.first,
                  release, output, pending);
    }
    return {true, std::move(output), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<GrowthConditionInputEffects> consume_growth_spatial_inputs_in_candidate(
    CommonStore &common, GrowthState &growth,
    const CharacterConditionSources &body, ActionElapsedStore &elapsed,
    const GrowthFixedCaptureServices &services,
    const std::function<Status(const StableId &)> &release,
    const GrowthPendingStartedProof *pending) {
  try {
    if (&body.common != &common || body.growth != &growth ||
        body.elapsed != &elapsed || !body.growth_catalog ||
        !body.growth_catalog->complete_exact_period_sources || !body.shared ||
        !body.scale || !body.action_registrations || !body.fixed_spatial ||
        !release || !common.actor_condition_capture_identity())
      fail("GrowthSpatialInputSameCaptureRequired");
    GrowthConditionInputEffects output;
    // The finite real source headers are already appended by S1/S2. Internal
    // FSL boundaries have no Common header; a real terminal publication may
    // append one while we apply, so retain only this original prefix length,
    // never references into its possibly reallocating vector.
    const auto prefix = common.state().facts.order().size();
    for (std::size_t n = 0; n < prefix; ++n) {
      const auto &header_id = common.state().facts.order()[n];
      const auto *header = common.state().facts.find(header_id);
      if (!header ||
          (header->domain != "travel" && header->domain != "military"))
        continue;


      auto memory = CheckpointMemoryCharge(0);
      const SourceRef source{header->domain, header->kind, header->id};
      const bool custody_release = is_named_military_custody_placement_source(source);
      // Field 原退出 evidence 的真正采用头是 travel/group_contact_member_release，
      // 原 S1 walk 已按该因果序输出；不把裸 military/field_battle_exit 重复作触发。
      if (source.domain == "military" &&
          !is_military_action_placement_source_kind(source) && !custody_release)
        continue;
      auto process = [&](const StableId &action) {
        if (!common.action(action) || !growth.actions.find(action))
          return;
        if (pending && take(pending->permits(common, growth, body, action)))
          return;
        const auto &prior = latest(growth, action);
        if (prior.kind == LifeFixedBoundaryKind::completed ||
            prior.kind == LifeFixedBoundaryKind::stopped)
          return;
        // Read only. At most one canonical S1 event for this actual header and
        // actor. Adopt after the scan so the source-table insertion cannot
        // invalidate prior.
        std::optional<ActorSpatialHeadChange> selected;
        StableId affected;
        for (const auto &person : prior.input_snapshot.actors) {
          auto events = take(read_growth_fixed_spatial_head_changes(
              body, *body.fixed_spatial, prior, person));
          for (auto &event : events)
            for (const auto &actual : event.immutable_source_head)
              if (same_source_ref(actual.source, source)) {
                if (selected)
                  fail("GrowthSpatialInputEventAmbiguous", source.id);
                if (custody_release)
                  verify_named_custody_position_change(body, source, person.actor_id, event);
                if (take(compare_world_moments(event.occurred_at,
                                               body.actual_now)))
                  fail("GrowthSpatialInputMissedActualBoundary", source.id);
                affected = person.actor_id;
                selected = std::move(event);
                break;
              }
        }
        if (!selected)
          return;
        if (selected->immutable_source_head.size() != 1)
          fail("GrowthSpatialInputSingleOwnerRequired", source.id);
        ActualChange change;
        change.actor = &affected;
        change.spatial = &*selected;
        change.source = LifeActualPositionInputChange{
            selected->immutable_source_head.front().source,
            selected->immutable_source_head.front().schema};
        apply_one(common, growth, body, elapsed, services, change, action,
                  release, output, pending);
      };
      for (const auto &row : growth.facts.rows())
        process(row.first);
      for (const auto &row : growth.life_activities.rows())
        process(row.first);
    }
    return {true, std::move(output), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<GrowthConditionInputEffects> consume_growth_quiet_inputs_in_candidate(
    CommonStore &common, GrowthState &growth,
    const CharacterConditionSources &body, ActionElapsedStore &elapsed,
    const GrowthFixedCaptureServices &services,
    const std::function<Status(const StableId &)> &release,
    const GrowthPendingStartedProof *pending) {
  try {
    if (&body.common != &common || body.growth != &growth ||
        body.elapsed != &elapsed || !body.growth_catalog ||
        !body.growth_catalog->complete_exact_command_cuts || !body.shared ||
        !body.scale || !body.action_registrations || !body.fixed_quiet ||
        !body.fixed_quiet->bound_to(common) || !release ||
        !common.actor_condition_capture_identity())
      fail("GrowthQuietInputSameCaptureRequired");
    GrowthConditionInputEffects output;
    for (const auto &row : growth.facts.rows()) {
      const auto *rest = std::get_if<RestFact>(&row.second);
      if (!rest)
        continue;
      for (;;) {
        if (!common.action(row.first) || !growth.actions.find(row.first))
          break;
        if (pending && take(pending->permits(common, growth, body, row.first)))
          break;
        const auto &prior = latest(growth, row.first);
        if (prior.kind == LifeFixedBoundaryKind::completed ||
            prior.kind == LifeFixedBoundaryKind::stopped)
          break;
        const auto &quiet = prior.input_snapshot.specific.growth_fact
                                .original_quiet_or_rest_basis;
        if (!quiet)
          fail("GrowthLongRestQuietSourceRequired", row.first);
        auto actual = take(read_growth_long_rest_next_quiet_change(
            body, rest->actor_id, *quiet));
        if (!actual)
          break;
        if (take(compare_world_moments(actual->occurred_at, body.actual_now)))
          fail("GrowthQuietEarlierInputNotConsumed", actual->actual_source.id);
        ActualChange change;
        change.actor = &rest->actor_id;
        change.quiet = &*actual;
        change.source = LifeOccupationInputChange{actual->actual_source,
                                                  actual->source_schema};
        apply_one(common, growth, body, elapsed, services, change, row.first,
                  release, output, pending);
      }
    }
    return {true, std::move(output), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
