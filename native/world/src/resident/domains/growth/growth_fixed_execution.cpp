#include "growth_fixed_execution.hpp"
#include "growth_internal.hpp"
#include <limits>
namespace hanmo::resident {
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
template <class T, class F> Outcome<T> guarded(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
struct Gate {
  Integer ordinal = 0, previous = 0, required = 0;
};
Gate original_gate(const GrowthState &growth, const ActionCoreView &action) {
  const auto *payload = growth.actions.find(action.id);
  if (!payload || payload->request.kind != action.kind ||
      payload->quote.duration < 1)
    fail("GrowthFixedOriginalActionRequired", action.id);
  Gate gate;
  gate.required = payload->quote.duration;
  gate.ordinal = gate.required;
  if (growth::study_action_kind(action.kind)) {
    const auto *raw = growth.facts.find(action.id);
    const auto *study = raw ? std::get_if<StudyFact>(raw) : nullptr;
    if (!study || study->phase != "started" || growth::study_paused(*study) ||
        study->terms.duration != gate.required ||
        study->lessons.size() >= static_cast<std::size_t>(gate.required))
      fail("GrowthFixedStudyGateUnavailable", action.id);
    gate.previous = static_cast<Integer>(study->lessons.size());
    gate.ordinal = gate.previous + 1;
  } else if (action.kind == "long_rest") {
    const auto *raw = growth.facts.find(action.id);
    const auto *rest = raw ? std::get_if<RestFact>(raw) : nullptr;
    if (!rest || rest->phase != "active" || rest->duration != gate.required ||
        rest->elapsed < 0 || rest->elapsed >= gate.required)
      fail("GrowthFixedRestGateUnavailable", action.id);
    gate.previous = rest->elapsed;
    gate.ordinal = gate.previous + 1;
  } else if (action.kind != "learn_skill" && action.kind != "practice_skill" &&
             action.kind != "teach_skill" && action.kind != "service" &&
             action.kind != "trade")
    fail("GrowthFixedKindUnavailable", action.kind);
  return gate;
}
void identity(const ActionTimeRegistration &reg, const ActionCoreView &action) {
  if (reg.owner_domain != "growth" ||
      reg.mode != ActionTimeMode::fixed_duration ||
      reg.action_kind != action.kind || reg.source_schema != 1)
    fail("GrowthFixedRegistrationMismatch", action.id);
}
const ActionCoreView &active(const CommonStore &common, const StableId &id) {
  const auto *action = common.action(id);
  if (!action)
    fail("GrowthFixedActionInactive", id);
  return *action;
}
void sources(const GrowthActionTimeRead &reader,
             const ActionTimeRegistration &reg,
             const ActionElapsedState &entry) {
  auto proof = take(reader.read_action_time_owner(reg, entry.action_ref));
  if (proof.phase != ActionTimeOwnerPhase::working)
    fail("GrowthFixedActionNotWorking", entry.action_ref);
  take(reader.validate_retained_fixed_action_sources(reg, proof, entry));
}
WorldMoment gate_at(const ActionElapsedState &entry, const Gate &gate,
                    const WorldMoment &current, const TimeScaleRef &scale,
                    ExactArithmeticBudget budget) {
  if (!std::holds_alternative<ElapsedRunning>(entry.timing))
    fail("GrowthFixedActionNotRunning", entry.action_ref);
  auto target = take(fixed_duration_work_units(
      gate.ordinal, scale.units_per_half_day, budget));
  if (take(compare_fractions(entry.credited_units.value, target.value,
                             budget)) > 0)
    fail("UnconsumedGrowthFixedGate", entry.action_ref);
  if (take(compare_world_moments(entry.last_settled_at, current, budget)))
    fail("UnsettledGrowthFixedGate", entry.action_ref);
  auto remaining =
      take(subtract_work_units(target, entry.credited_units, budget));
  return take(
      add_world_duration(current,
                         take(scale_units_to_duration(
                             remaining, scale.units_per_half_day, budget)),
                         budget));
}
} // namespace
Outcome<std::optional<WorldMoment>> predict_growth_fixed_step(
    const CommonStore &common, const GrowthState &growth,
    const GrowthCatalog &catalog, const CommonCatalog &shared,
    const ActionElapsedStore &elapsed, const TimeScaleRef &scale,
    const CatalogIdentity &identity_value, const ActionTimeRegistration &reg,
    const StableId &id, const WorldMoment &current,
    ExactArithmeticBudget budget, const InteractionState *interaction,
    const InteractionCatalog *interaction_catalog,
    const GrowthMarketExactSources *market_exact) {
  return guarded<std::optional<WorldMoment>>(
      [&]() -> std::optional<WorldMoment> {
        const auto *entry = find_fixed_elapsed(elapsed, id);
        if (!entry)
          fail("GrowthFixedElapsedMissing", id);
        GrowthActionTimeRead reader(
            common, growth, catalog, shared, elapsed, scale, identity_value,
            current, budget, interaction, interaction_catalog, market_exact);
        auto proof = take(reader.read_action_time_owner(reg, id));
        take(reader.validate_retained_fixed_action_sources(reg, proof, *entry));
        if (proof.phase == ActionTimeOwnerPhase::terminal ||
            proof.phase == ActionTimeOwnerPhase::suspended)
          return std::nullopt;
        const auto &action = active(common, id);
        identity(reg, action);
        const auto gate = original_gate(growth, action);
        if (take(compare_fractions(entry->credited_units.value,
                                   entry->required_units.value, budget)) == 0)
          take(validate_fixed_elapsed_at_completion_due(*entry, current, scale,
                                                        budget));
        else
          take(validate_fixed_elapsed_value(*entry, current, scale, budget));
        return gate_at(*entry, gate, current, scale, budget);
      });
}
Status provide_growth_fixed_interval_in_candidate(
    CommonStore &common, const GrowthState &growth,
    const GrowthCatalog &catalog, const CommonCatalog &shared,
    ActionElapsedStore &elapsed, const TimeScaleRef &scale,
    const CatalogIdentity &identity_value,
    const std::vector<ActionTimeRegistration> &registrations,
    const MomentSpan &span, ExactArithmeticBudget budget,
    const InteractionState *interaction,
    const InteractionCatalog *interaction_catalog,
    const GrowthMarketExactSources *market_exact) {
  return guarded<Done>([&] {
    take(make_moment_span(span.from, span.to, budget));
    for (const auto &row : growth.actions.rows()) {
      const auto &action = active(common, row.first);
      const ActionTimeRegistration *reg = nullptr;
      for (const auto &candidate : registrations)
        if (candidate.owner_domain == "growth" &&
            candidate.action_kind == action.kind) {
          if (reg)
            fail("DuplicateGrowthFixedRegistration", action.kind);
          reg = &candidate;
        }
      if (!reg)
        fail("GrowthFixedRegistrationMissing", action.kind);
      auto predicted = take(predict_growth_fixed_step(
          common, growth, catalog, shared, elapsed, scale, identity_value, *reg,
          row.first, span.from, budget, interaction, interaction_catalog,
          market_exact));
      if (!predicted)
        continue;
      if (take(compare_world_moments(span.to, *predicted, budget)) > 0)
        fail("GrowthFixedIntervalCrossesOriginalGate", row.first);
      auto *entry = find_fixed_elapsed(elapsed, row.first);
      if (!entry)
        fail("GrowthFixedElapsedMissing", row.first);
      auto actual =
          take(prepare_fixed_elapsed_slice(*entry, span, scale, budget));
      entry->credited_units = std::move(actual.credited_units);
      entry->last_settled_at = std::move(actual.last_settled_at);
      // Original per-lesson/long-rest writers still consume the due period
      // against their unconsumed ordinal. Other partial spans publish only S0's
      // true ceil.
      const bool period_due =
          (growth::study_action_kind(action.kind) ||
           action.kind == "long_rest") &&
          take(compare_world_moments(span.to, *predicted, budget)) == 0;
      if (!period_due) {
        const auto projected =
            take(fixed_elapsed_remaining_ticks(*entry, scale, budget));
        if (action.remaining_ticks != projected) {
          ActionMutation update;
          update.action = action;
          update.remaining_ticks = projected;
          take(common.apply_action(update));
        }
      }
    }
    return Done{};
  });
}
Outcome<GrowthFixedStepPermit> prepare_growth_fixed_step(
    const CommonStore &common, const GrowthState &growth,
    const GrowthCatalog &catalog, const CommonCatalog &shared,
    const ActionElapsedStore &elapsed, const TimeScaleRef &scale,
    const CatalogIdentity &identity_value, const ActionTimeRegistration &reg,
    const StableId &id, const WorldMoment &current,
    ExactArithmeticBudget budget, const InteractionState *interaction,
    const InteractionCatalog *interaction_catalog,
    const GrowthMarketExactSources *market_exact) {
  return guarded<GrowthFixedStepPermit>([&] {
    auto predicted = take(predict_growth_fixed_step(
        common, growth, catalog, shared, elapsed, scale, identity_value, reg,
        id, current, budget, interaction, interaction_catalog, market_exact));
    if (!predicted || take(compare_world_moments(*predicted, current, budget)))
      fail("GrowthFixedOriginalGateNotDue", id);
    const auto &action = active(common, id);
    auto gate = original_gate(growth, action);
    const auto *entry = find_fixed_elapsed(elapsed, id);
    auto previous = gate.previous
                        ? take(fixed_duration_work_units(
                              gate.previous, scale.units_per_half_day, budget))
                        : WorkUnits{};
    auto target = take(fixed_duration_work_units(
        gate.ordinal, scale.units_per_half_day, budget));
    if (take(compare_fractions(entry->credited_units.value, target.value,
                               budget)))
      fail("GrowthFixedOriginalGateCreditMismatch", id);
    auto from = take(read_fixed_service_credit_boundary(
        elapsed, id, previous, current, scale, budget));
    auto at = take(read_fixed_service_credit_boundary(elapsed, id, target,
                                                      current, scale, budget));
    if (take(compare_world_moments(at, current, budget)))
      fail("GrowthFixedGateWasNotConsumedAtBoundary", id);
    auto provided =
        take(gate.ordinal == gate.required
                 ? read_fixed_service_intervals_at_completion_due(
                       elapsed, id, from, current, current, scale, budget)
                 : read_fixed_service_intervals(elapsed, id, from, current,
                                                current, scale, budget));
    auto delta = take(subtract_work_units(provided.credited_after,
                                          provided.credited_before, budget));
    auto expected = take(fixed_duration_work_units(
        gate.ordinal - gate.previous, scale.units_per_half_day, budget));
    if (!provided.complete || provided.intervals.empty() ||
        take(compare_fractions(delta.value, expected.value, budget)))
      fail("GrowthFixedActualServiceWindowIncomplete", id);
    GrowthFixedStepPermit out;
    out.common_ = &common;
    out.growth_ = &growth;
    out.elapsed_ = &elapsed;
    out.identity_ = common.state().identity;
    out.action_id_ = id;
    out.ordinal_ = gate.ordinal;
    out.required_half_days_ = gate.required;
    out.at_ = current;
    out.from_ = std::move(from);
    out.provided_ = std::move(provided);
    out.final_ = gate.ordinal == gate.required;
    return out;
  });
}
Status validate_growth_fixed_step_permit(const GrowthFixedStepPermit &permit,
                                         const CommonStore &common,
                                         const GrowthState &growth,
                                         const ActionCoreView &action) {
  return guarded<Done>([&] {
    const auto &a = permit.identity_;
    const auto &b = common.state().identity;
    if (permit.common_ != &common || permit.growth_ != &growth ||
        !permit.elapsed_ || permit.action_id_ != action.id ||
        a.world_id != b.world_id || a.execution_epoch != b.execution_epoch ||
        a.revision != b.revision || a.tick != b.tick)
      fail("ExpiredGrowthFixedStepPermit", action.id);
    const auto &live = active(common, action.id);
    if (live.kind != action.kind || live.actor_id != action.actor_id ||
        live.participants != action.participants ||
        live.remaining_ticks != action.remaining_ticks)
      fail("GrowthFixedStepActionChanged", action.id);
    auto gate = original_gate(growth, action);
    if (gate.ordinal != permit.ordinal_ ||
        gate.required != permit.required_half_days_ ||
        permit.final_ != (gate.ordinal == gate.required))
      fail("GrowthFixedStepAlreadyConsumed", action.id);
    const auto *entry = find_fixed_elapsed(*permit.elapsed_, action.id);
    if (!entry || !std::holds_alternative<ElapsedRunning>(entry->timing) ||
        take(compare_world_moments(entry->last_settled_at, permit.at_)))
      fail("GrowthFixedStepTimeChanged", action.id);
    return Done{};
  });
}
} // namespace hanmo::resident
