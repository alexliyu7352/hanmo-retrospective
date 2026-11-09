#include "private_shop_market_stage.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
const PrivateShopOperationSourceContext &
shop_operations(const NativeHouseholdFoundationLease &h) {
  const auto *original = h.operations();
  if (!original)
    fail("NativePrivateShopMarketOriginalOperationRequired");
  return *original;
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
template <class T, class F> Outcome<T> guard(F &&f) {
  try {
    return {true, f(), {}};
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  }
}
Integer next_period(const PrivateShopOperationSourceContext &x,
                    const StableId &id) {
  bool periods[2]{};
  for (const auto &row : x.sources.labor_applied_sources)
    if (row.operation_ref == id) {
      const auto ordinal = row.work_period_ref.period_ordinal;
      if (ordinal < 0 || ordinal > 1 || periods[ordinal])
        fail("NativePrivateShopLaborPeriodMismatch", id);
      take(read_original_private_shop_labor(
          x, {private_shop_labor_applied_source(row.id), 1}));
      periods[ordinal] = true;
    }
  if (periods[1] && !periods[0])
    fail("NativePrivateShopLaborPeriodGap", id);
  return periods[1] ? 2 : periods[0] ? 1 : 0;
}
ActorActionWorkBinding work_binding(const StableId &id) {
  return {id, "private_shop_operate",
          private_shop_operation_started_source(id)};
}
} // namespace
Outcome<std::vector<ExactEventPrediction>>
predict_native_private_shop_service_periods(
    const NativeHouseholdFoundationLease &household, const WorldMoment &current,
    const WorldMoment &until) {
  return guard<std::vector<ExactEventPrediction>>([&] {
    take(household.validate_bindings());
    if (!household.operations())
      return std::vector<ExactEventPrediction>{};
    const auto &x = shop_operations(household);
    const auto budget = x.spatial.budget;
    if (take(compare_world_moments(current, until, budget)) > 0 ||
        take(compare_world_moments(current, take(x.clock.read_current_moment()),
                                   budget)))
      fail("NativePrivateShopPredictionWindowMismatch");
    std::vector<ExactEventPrediction> events;
    NativePrivateShopActionTimeRead owner(x);
    const auto &registration =
        private_shop_operation_action_time_registration();
    for (const auto &accepted : x.sources.accepted_operations) {
      const auto &id = accepted.operation_ref;
      take(read_original_private_shop_operation_acceptance(
          x, {private_shop_operation_started_source(id), 2}));
      const auto *operation = x.shops.state.operations.find(id);
      const auto *entry = find_fixed_elapsed(x.elapsed, id);
      if (!operation || !entry)
        fail("NativePrivateShopOriginalServiceRequired", id);
      if (!operation->result_fact_id.empty())
        continue;
      if (!household_retail_operation_selected(x.shops, id))
        fail("NativePrivateShopHouseholdRetailSelectionRequired", id);
      const auto ordinal = next_period(x, id);
      if (ordinal == 2)
        fail("NativePrivateShopLaborTerminalRequired", id);
      const auto proof = take(owner.read_action_time_owner(registration, id));
      const auto target = take(fixed_duration_work_units(
          ordinal + 1, x.scale.units_per_half_day, budget));
      const auto credited = take(
          compare_fractions(entry->credited_units.value, target.value, budget));
      WorldMoment due;
      if (credited >= 0) {
        // 已供工量只反读原 FWP；漏过的历史期不得拿当前时刻补记。
        const auto period = take(read_fixed_service_work_period2(
            x.elapsed, work_binding(id), ordinal, 2, current, x.scale, budget));
        due = period.ref.until;
        if (take(compare_world_moments(due, current, budget)))
          fail("NativePrivateShopUnconsumedLaborPeriod", id);
      } else {
        take(validate_fixed_elapsed_value(*entry, current, x.scale, budget));
        if (std::holds_alternative<ElapsedSuspended>(entry->timing))
          continue;
        if (!std::holds_alternative<ElapsedRunning>(entry->timing) ||
            proof.phase != ActionTimeOwnerPhase::working)
          fail("NativePrivateShopActualRunningServiceRequired", id);
        const auto remaining =
            take(subtract_work_units(target, entry->credited_units, budget));
        const auto duration = take(scale_units_to_duration(
            remaining, x.scale.units_per_half_day, budget));
        due = take(add_world_duration(current, duration, budget));
        if (take(compare_world_moments(
                due, std::get<ElapsedRunning>(entry->timing).completion_due_at,
                budget)) > 0)
          fail("NativePrivateShopLaborCrossesCompletion", id);
      }
      if (take(compare_world_moments(due, until, budget)) <= 0)
        events.push_back({due, ExactStage::action_completions,
                          private_shop_operation_started_source(id), 2,
                          ordinal});
    }
    return events;
  });
}
Outcome<ExactEventApplied>
apply_native_private_shop_service_period_in_candidate(
    NativePrivateShopMarketCommandContext &x,
    const ExactEventPrediction &prediction) {
  return guard<ExactEventApplied>([&] {
    const auto at =
        take(shop_operations(x.household).clock.read_current_moment());
    if (prediction.stage != ExactStage::action_completions ||
        prediction.source_schema != 2 ||
        prediction.actual_source_basis.domain != private_shop_module ||
        prediction.actual_source_basis.kind !=
            "private_shop_operating_started" ||
        take(compare_world_moments(
            prediction.at, at, shop_operations(x.household).spatial.budget)))
      fail("NativePrivateShopActualLaborPredictionRequired");
    const auto due =
        take(predict_native_private_shop_service_periods(x.household, at, at));
    bool found = false;
    for (const auto &event : due)
      if (same_source_ref(event.actual_source_basis,
                          prediction.actual_source_basis) &&
          event.dependency_revision == prediction.dependency_revision) {
        if (found)
          fail("NativePrivateShopLaborPredictionAmbiguous");
        found = true;
      }
    if (!found)
      fail("NativePrivateShopLaborPredictionStale");
    auto &w = x.world;
    if (!w.catalog || !w.catalog->exact_time ||
        !exact_complete_selected(*w.catalog->exact_time) || !w.action_elapsed ||
        !w.household_purchases || !w.household_retail_settlements ||
        !w.private_shop_operation_sources ||
        !w.private_shop_operation_capabilities ||
        !x.household.bound_to(w.common, w.local_economy, *w.household_purchases,
                              w.private_shops, *w.household_retail_settlements,
                              *w.action_elapsed) ||
        &shop_operations(x.household).sources !=
            &*w.private_shop_operation_sources ||
        &shop_operations(x.household).capabilities !=
            &*w.private_shop_operation_capabilities ||
        !w.common.actor_condition_capture_identity() ||
        !w.common.resource_execution_capture_identity())
      fail("NativePrivateShopLaborCandidateMismatch");
    if (take(
            compare_world_moments(at, take(w.actual_phase_moment()),
                                  shop_operations(x.household).spatial.budget)))
      fail("NativePrivateShopLaborMomentMismatch");
    const auto &operation = prediction.actual_source_basis.id;
    const auto labor = take(apply_private_shop_labor_period_in_candidate(
        w.common, w.local_economy, w.private_shops,
        *w.private_shop_operation_sources,
        *w.private_shop_operation_capabilities, shop_operations(x.household),
        operation, prediction.dependency_revision));
    const MomentOwnerSource actual_labor{
        private_shop_labor_applied_source(labor), 1};
    if (prediction.dependency_revision == 0)
      return ExactEventApplied{actual_labor.source_ref, 1};
    const auto completed = take(complete_private_shop_operation_in_candidate(
        w.common, w.local_economy, w.private_shops,
        *w.private_shop_operation_sources, *w.action_elapsed,
        shop_operations(x.household), operation, actual_labor));
    const MomentOwnerSource terminal{
        {private_shop_module, "private_shop_operating_completed", completed},
        2};
    take(reconcile_native_private_shop_market_operation_change_in_candidate(
        x, operation, terminal));
    ActionCompletion completion;
    completion.completed = true;
    if (!x.phase_writes.advance.completed_actions.insert(operation,
                                                         std::move(completion)))
      fail("NativePrivateShopDuplicateActionCompletion", operation);
    return ExactEventApplied{terminal.source_ref, terminal.source_schema};
  });
}
} // namespace hanmo::resident
