#include "private_shop_market_commands.hpp"
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
bool same_rule(const ActorFoundationRuleRef &a,
               const ActorFoundationRuleRef &b) {
  return a.id == b.id && a.version == b.version &&
         a.catalog_digest == b.catalog_digest;
}
WorldMoment binding(const NativePrivateShopMarketCommandContext &x) {
  auto &w = x.world;
  const auto &shop = shop_operations(x.household);
  const auto &service = x.household.service();
  const auto &market = service.market();
  if (!w.catalog || !w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time) || !w.action_elapsed ||
      !w.facility_claims || !w.household_purchases ||
      !w.household_retail_settlements || !w.private_shop_operation_sources ||
      !w.private_shop_operation_capabilities ||
      !x.household.bound_to(w.common, w.local_economy, *w.household_purchases,
                            w.private_shops, *w.household_retail_settlements,
                            *w.action_elapsed) ||
      &shop.sources != &*w.private_shop_operation_sources ||
      &shop.capabilities != &*w.private_shop_operation_capabilities ||
      &market.claims != &*w.facility_claims ||
      &shop.markets != w.catalog->household_purchases.get() ||
      &shop.shops.catalog != w.catalog->private_shops.get() ||
      &service.market_original_context().identity != &w.catalog->identity)
    fail("NativePrivateShopMarketCandidateMismatch");
  if (!w.common.actor_condition_capture_identity() ||
      !w.common.resource_execution_capture_identity())
    fail("NativePrivateShopMarketActualCaptureRequired");
  take(x.household.validate_bindings());
  const auto at = take(shop.clock.read_current_moment());
  if (take(compare_world_moments(at, market.now, shop.spatial.budget)) ||
      take(compare_world_moments(at, take(w.actual_phase_moment()),
                                 shop.spatial.budget)))
    fail("NativePrivateShopMarketMomentMismatch");
  return at;
}
const MarketServiceDefinition &
definition_for_shop(const PrivateShopOperationSourceContext &x,
                    const StableId &shop_id) {
  const auto *shop = x.shops.state.shops.find(shop_id);
  if (!shop)
    fail("NativePrivateShopMarketShopRequired", shop_id);
  if (!household_retail_selected(x.shops, shop_id))
    fail("NativePrivateShopHouseholdRetailSelectionRequired", shop_id);
  const MarketServiceDefinition *found = nullptr;
  for (const auto &d : x.markets.market_service_definitions)
    if (d.slot_ref.provider_domain == "commerce" &&
        d.slot_ref.slot_kind == "market_service" &&
        d.asset_source_ref.domain == "commerce" &&
        d.asset_source_ref.kind == "initial_shop_premises" &&
        d.asset_source_ref.id == shop->terms.premises_id &&
        d.place_ref == shop->terms.place_id) {
      if (found)
        fail("NativePrivateShopMarketDefinitionAmbiguous", shop_id);
      found = &d;
    }
  if (!found)
    fail("NativePrivateShopMarketDefinitionRequired", shop_id);
  return *found;
}
// 实际变化先由原 reader 核准，再调用原 S0 全 slot 优先序与一次 finalizer。
// 这些借用 callback 只存本栈，绝不注册成另一套世界 provider。
void reconcile(NativePrivateShopMarketCommandContext &x,
               const StableId &operation, const MomentOwnerSource &actual) {
  const auto at = binding(x);
  auto &w = x.world;
  const auto &shop = shop_operations(x.household);
  const auto &service = x.household.service();
  const auto &market = service.market();
  const auto *o = w.private_shops.operations.find(operation);
  if (!o || actual.source_schema != 2)
    fail("NativePrivateShopMarketOriginalOperationRequired", operation);
  const auto &d = definition_for_shop(shop, o->shop_id);
  const auto *accepted = take(read_original_private_shop_operation_acceptance(
      shop, {private_shop_operation_started_source(operation), 2}));
  if (!same_rule(d.definition_ref, accepted->market_definition_ref))
    fail("NativePrivateShopMarketAcceptedDefinitionMismatch", operation);
  if (actual.source_ref.kind == "private_shop_operating_started") {
    if (!same_source_ref(actual.source_ref,
                         private_shop_operation_started_source(operation)))
      fail("NativePrivateShopMarketOriginalStartRequired", operation);
  } else if (actual.source_ref.domain == "actor_foundation" &&
             actual.source_ref.kind == "condition_receipt") {
    bool adopted = false;
    for (const auto &t : shop.elapsed.fixed_service_transitions)
      if (t.action_ref == operation)
        if (const auto *cause = std::get_if<FixedEligibilityChanged>(&t.cause))
          if (cause->original_cause_schema == actual.source_schema &&
              same_source_ref(cause->original_cause_ref, actual.source_ref)) {
            if (adopted)
              fail("NativePrivateShopMarketConditionAmbiguous", operation);
            adopted = true;
          }
    if (!adopted)
      fail("NativePrivateShopMarketAppliedConditionRequired", operation);
    take(service.private_market_originals()->read_private_market_service_change(
        d, {actual.source_ref, actual.source_schema}));
  } else {
    const auto *terminal =
        take(read_original_private_shop_operation_terminal(shop, actual));
    if (terminal->operation_ref != operation)
      fail("NativePrivateShopMarketOriginalTerminalMismatch", operation);
  }
  FacilityCapacityReconciliationRequest request{
      d.slot_ref, {actual.source_ref, actual.source_schema}, at};
  // 同一原原因不得二次处置 holder；正式命令重放由原 Common 缓存返回。
  for (const auto &s :
       market.purchases.market_service_sources.private_sources) {
    const auto original =
        take(market.originals.read_original_market_change_source(
            s.slot_ref, s.change_cause));
    if (same_source_ref(original.source_ref, actual.source_ref))
      fail("NativePrivateShopMarketChangeAlreadyAdopted", actual.source_ref.id);
  }
  FacilityCapacityReconciliationRegistration registration;
  registration.provider_domain = "commerce";
  registration.profile_id = "named_household_market_service";
  registration.profile_version = 1;
  registration.validate_request =
      [&x, &market](const WorldBundle &world,
                    const FacilityCapacityReconciliationRequest &r) {
        if (&world != &x.world)
          return common_failure("NativePrivateShopMarketWorldMismatch");
        return validate_market_service_reconciliation_request(market, r);
      };
  registration.finalize =
      [&x, &market](WorldBundle &world,
                    const FacilityCapacityReconciliationRequest &r,
                    const std::vector<FacilityClaimMutationReceipt> &owners,
                    ExactPhaseWriteContext &writes)
      -> Outcome<std::optional<FacilityClaimMutationReceipt>> {
    if (&world != &x.world || &writes != &x.phase_writes)
      return {false,
              {},
              common_failure("NativePrivateShopMarketWorldMismatch").error};
    return finalize_market_service_capacity_in_candidate(
        world.common, *world.household_purchases, market, r, owners);
  };
  take(reconcile_facility_capacity_in_candidate(
      w, *w.facility_claims, service.household().claim_catalog, request,
      registration, x.holder_handlers, market.holders, service.capacity(),
      x.phase_writes, shop.spatial.budget));
  take(service.private_market_originals()->validate_capacity_source_coverage(
      *w.household_purchases));
}
} // namespace

Status reconcile_native_private_shop_market_operation_change_in_candidate(
    NativePrivateShopMarketCommandContext &x, const StableId &operation,
    const MomentOwnerSource &actual_change) {
  return guard<Done>([&] {
    reconcile(x, operation, actual_change);
    return Done{};
  });
}

Outcome<StableId> start_native_private_shop_market_operation_in_candidate(
    NativePrivateShopMarketCommandContext &x,
    const PrivateShopOperationQuote &quote) {
  return guard<StableId>([&] {
    binding(x);
    auto &w = x.world;
    const auto &shop = shop_operations(x.household);
    const auto &d = definition_for_shop(shop, quote.command.shop_id);
    auto started = take(start_private_shop_operation_in_candidate(
        w.common, w.local_economy, w.private_shops,
        *w.private_shop_operation_sources,
        *w.private_shop_operation_capabilities, *w.action_elapsed, shop, quote,
        d.definition_ref));
    reconcile(x, started, {private_shop_operation_started_source(started), 2});
    return started;
  });
}
Outcome<PendingPrivateShopCancellation>
cancel_native_private_shop_market_operation_in_candidate(
    NativePrivateShopMarketCommandContext &x, const StableId &actor,
    const StableId &operation) {
  return guard<PendingPrivateShopCancellation>([&] {
    binding(x);
    return take(cancel_private_shop_exact_operation_in_candidate(
        x.world.common, x.world.local_economy, x.world.private_shops,
        shop_operations(x.household), actor, operation));
  });
}
Status bind_native_private_shop_market_cancellation_in_candidate(
    NativePrivateShopMarketCommandContext &x,
    PendingPrivateShopCancellation &&pending,
    const MomentOwnerSource &actual_command_seal) {
  return guard<Done>([&] {
    binding(x);
    auto &w = x.world;
    const auto result =
        take(read_pending_private_shop_cancellation_result(pending));
    const PrivateShopOperation *operation = nullptr;
    for (const auto &row : w.private_shops.operations.rows())
      if (row.second.result_fact_id == result) {
        if (operation)
          fail("NativePrivateShopMarketTerminalAmbiguous", result);
        operation = &row.second;
      }
    if (!operation)
      fail("NativePrivateShopMarketTerminalRequired", result);
    take(bind_private_shop_cancellation_in_candidate(
        w.common, *w.private_shop_operation_sources,
        *w.private_shop_operation_capabilities, *w.action_elapsed,
        shop_operations(x.household), std::move(pending), actual_command_seal));
    reconcile(
        x, operation->id,
        {{private_shop_module, "private_shop_operating_interrupted", result},
         2});
    return Done{};
  });
}
} // namespace hanmo::resident
