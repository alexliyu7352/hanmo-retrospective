// 热门把唯一冷证据逐字绑定本次真实捕获；资源规则只走原共用正文。
#include "facility_construction_cost_capture.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <tuple>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> result) {
  if (!result.ok)
    throw Failure{std::move(result.error)};
  return std::move(result.value);
}
// 只比实际原请求的所有成员；不是另一份 Consume 算法。
bool same(const ConsumeSelectedLots &a, const ConsumeSelectedLots &b) {
  if (a.inventory_id != b.inventory_id || a.reservation != b.reservation ||
      a.selections.size() != b.selections.size() ||
      a.owners.size() != b.owners.size())
    return false;
  for (std::size_t i = 0; i < a.selections.size(); ++i) {
    const auto &x = a.selections[i];
    const auto &y = b.selections[i];
    if (std::tie(x.inventory_id, x.lot_id, x.quantity) !=
        std::tie(y.inventory_id, y.lot_id, y.quantity))
      return false;
  }
  for (std::size_t i = 0; i < a.owners.size(); ++i)
    if (!owner_equal(a.owners[i], b.owners[i]) ||
        a.owners[i].extra != b.owners[i].extra ||
        a.owners[i].field_order != b.owners[i].field_order)
      return false;
  return true;
}
// 冷保留帧必须等于实际 capture，而非另一组也满足守恒的伪帧。
void material(const CommonStore &common,
              const ConstructionSelectedConsumptionEffect &saved,
              const ConsumeSelectedLots &request,
              const ResourceOperationExecutionDraft &actual) {
  if (!same(saved.request, request) ||
      saved.next_instance_before != actual.next_instance_before ||
      saved.next_instance_after != actual.next_instance_after ||
      saved.inventories.size() != actual.inventories.size() ||
      saved.flows.size() != actual.lot_flows.size())
    fail("ConstructionOriginalMaterialCaptureMismatch", request.inventory_id);
  for (std::size_t i = 0; i < saved.inventories.size(); ++i) {
    const auto &a = saved.inventories[i];
    const auto &b = actual.inventories[i];
    const auto *current = common.inventory(b.inventory_id);
    if (a.inventory_id != b.inventory_id || bool(a.before) != bool(b.before) ||
        (a.before && !same_original_inventory_frame(*a.before, *b.before)) ||
        !same_original_inventory_frame(a.after, b.after) || !current ||
        !same_original_inventory_frame(*current, b.after))
      fail("ConstructionActualInventoryFrameMismatch", request.inventory_id);
  }
  for (std::size_t i = 0; i < saved.flows.size(); ++i) {
    const auto &a = saved.flows[i];
    const auto &b = actual.lot_flows[i];
    if (std::tie(a.kind, a.source_inventory_id, a.source_lot_id,
                 a.destination_inventory_id, a.destination_lot_id, a.item_id,
                 a.quantity) !=
        std::tie(b.kind, b.source_inventory_id, b.source_lot_id,
                 b.destination_inventory_id, b.destination_lot_id, b.item_id,
                 b.quantity))
      fail("ConstructionOriginalMaterialFlowMismatch", request.inventory_id);
  }
}
void money(const CommonStore &common,
           const ConstructionWalletDeltaEffect &saved,
           const WalletDelta &request,
           const ResourceOperationExecutionDraft &actual) {
  if (saved.request.wallet_id != request.wallet_id ||
      saved.request.delta != request.delta ||
      saved.request.require_spendable != request.require_spendable)
    fail("ConstructionOriginalMoneyLegMismatch", request.wallet_id);
  const auto *frame = take(read_original_wallet_delta(request, actual));
  const auto *current = common.wallet(request.wallet_id);
  if (!frame || !frame->before ||
      !same_original_wallet_frame(saved.before, *frame->before) ||
      !same_original_wallet_frame(saved.after, frame->after) || !current ||
      !same_original_wallet_frame(*current, frame->after))
    fail("ConstructionActualWalletFrameMismatch", request.wallet_id);
}
} // namespace
Status validate_facility_construction_start_cost_capture(
    const CommonStore &common, const ConstructionTerms &terms,
    const ConstructionStartCost &result, const ResourceExecutionDraft &capture,
    const CurrentMomentRead &clock) {
  try {
    const auto &identity = common.state().identity;
    const auto &all = common.resource_execution_drafts();
    if (!common.resource_execution_capture_identity() || all.empty() ||
        &all.back() != &capture ||
        capture.identity.world_id != identity.world_id ||
        capture.identity.execution_epoch != identity.execution_epoch ||
        capture.identity.revision != identity.revision ||
        capture.identity.tick != identity.tick || capture.operations.empty() ||
        capture.operations.size() != capture.recipe.operations.size() ||
        capture.next_instance_before != capture.next_instance_after ||
        take(compare_world_moments(result.occurred_at,
                                   take(clock.read_current_moment()))))
      fail("ConstructionOriginalResourceCaptureRequired", result.receipt_id);
    take(validate_facility_construction_start_cost_effects(terms, result));
    const auto &effects = result.actual_resources;
    if (effects.materials.size() > capture.operations.size() ||
        capture.operations.size() - effects.materials.size() !=
            (effects.payment ? 2u : 0u))
      fail("ConstructionUnexpectedResourceOperation", result.receipt_id);
    for (std::size_t i = 0; i < capture.operations.size(); ++i) {
      const auto &actual = capture.operations[i];
      if (actual.operation_index != i ||
          actual.next_instance_before != capture.next_instance_before ||
          actual.next_instance_after != capture.next_instance_after)
        fail("ConstructionOriginalCaptureOrderMismatch", result.receipt_id);
      if (i < effects.materials.size()) {
        const auto *request =
            std::get_if<ConsumeSelectedLots>(&capture.recipe.operations[i]);
        if (!request)
          fail("ConstructionOriginalMaterialCaptureMismatch",
               result.receipt_id);
        take(validate_selected_resource_execution(capture.recipe.operations[i],
                                                  actual));
        material(common, effects.materials[i], *request, actual);
      } else {
        const auto *request =
            std::get_if<WalletDelta>(&capture.recipe.operations[i]);
        if (!request)
          fail("ConstructionOriginalMoneyLegMismatch", result.receipt_id);
        money(common,
              i == effects.materials.size() ? effects.payment->debit
                                            : effects.payment->credit,
              *request, actual);
      }
    }
    return common_success();
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  }
}
} // namespace hanmo::resident
