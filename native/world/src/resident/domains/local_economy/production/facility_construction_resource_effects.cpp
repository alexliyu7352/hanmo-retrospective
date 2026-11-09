// 施工成本冷/热共用原 Selected/钱包正文，不重放 writer 或从现值推历史。
#include "facility_construction_resource_effects.hpp"
#include "facility_construction_model.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
void take(Status valid) {
  if (!valid.ok)
    throw Failure{std::move(valid.error)};
}
// 只比较原批完整值与实际扣量；原 Selected 核负责未选集及所有流向守恒。
void material(const ConstructionConsumedMaterial &result,
              const CargoSelection &selection, const InventoryView &before) {
  const LotView *original = nullptr;
  for (const auto &lot : before.lots)
    if (lot.id == selection.lot_id)
      original = &lot;
  if (!original || original->item_instance_id ||
      result.actual.inventory_id != selection.inventory_id ||
      result.actual.lot_id != selection.lot_id ||
      result.actual.quantity != selection.quantity ||
      result.item_id != original->item_id || result.original_owner.extra ||
      !owner_equal(result.original_owner, original->owner) ||
      result.original_owner.field_order != original->owner.field_order ||
      result.properties_ref.has_value() !=
          original->lot_properties_ref.has_value() ||
      (result.properties_ref &&
       result.properties_ref->properties_id !=
           original->lot_properties_ref->properties_id))
    fail("ConstructionOriginalConsumedLotMismatch", selection.lot_id);
}
// 原钱腿顺序和 spendable 仍由唯一 builder 定义；这里只比真正保留的帧。
class OriginalMoneyLegs final : public resource_recipe::MoneyTransferLegSink {
  const ConstructionMoneyCostEffects &effects_;
  std::size_t index_ = 0;

public:
  explicit OriginalMoneyLegs(const ConstructionMoneyCostEffects &effects)
      : effects_(effects) {}
  Status wallet_delta(const StableId &id, Integer delta,
                      bool spendable) override {
    if (index_ >= 2)
      return common_failure("ConstructionUnexpectedMoneyLeg", id);
    const auto &effect = index_++ ? effects_.credit : effects_.debit;
    if (effect.request.wallet_id != id || effect.request.delta != delta ||
        effect.request.require_spendable != spendable)
      return common_failure("ConstructionOriginalMoneyLegMismatch", id);
    return validate_original_wallet_delta(effect.request, effect.before,
                                          effect.after);
  }
  bool complete() const { return index_ == 2; }
};
} // namespace
Status validate_facility_construction_start_cost_effects(
    const ConstructionTerms &terms, const ConstructionStartCost &result) {
  try {
    const auto &effects = result.actual_resources;
    if (result.receipt_id.empty() || result.construction_id.empty() ||
        result.cash_paid < 0 || result.cash_paid != terms.money_cost ||
        result.payee_wallet_id != terms.payee_wallet_id ||
        result.consumed.empty() || effects.materials.empty() ||
        bool(effects.payment) != (result.cash_paid > 0))
      fail("ConstructionOriginalResourceEffectsRequired", result.receipt_id);
    std::size_t consumed = 0;
    Integer next = effects.materials.front().next_instance_before;
    for (std::size_t i = 0; i < effects.materials.size(); ++i) {
      const auto &effect = effects.materials[i];
      const auto &request = effect.request;
      if (request.reservation != ReservationRef{} ||
          effect.next_instance_before != next ||
          effect.next_instance_after != next)
        fail("ConstructionUnreservedStartMaterialRequired", result.receipt_id);
      take(validate_selected_resource_evidence(request,
                                               {{},
                                                effect.inventories,
                                                effect.flows,
                                                effect.next_instance_before,
                                                effect.next_instance_after}));
      for (std::size_t prior = 0; prior < i; ++prior)
        if (effects.materials[prior].request.inventory_id ==
            request.inventory_id)
          fail("ConstructionDuplicateMaterialInventory", request.inventory_id);
      // 原 Consume 核已证明恰一个真实 before/after 库帧与所有有序流。
      const auto &frame = effect.inventories.front();
      for (const auto &selected : request.selections) {
        if (consumed >= result.consumed.size())
          fail("ConstructionActualMaterialCoverageMismatch", result.receipt_id);
        material(result.consumed[consumed++], selected, *frame.before);
      }
    }
    if (consumed != result.consumed.size())
      fail("ConstructionActualMaterialCoverageMismatch", result.receipt_id);
    for (const auto &required : terms.materials) {
      Integer quantity = 0;
      for (const auto &row : result.consumed)
        if (row.item_id == required.item_id) {
          if (row.actual.quantity <= 0 ||
              quantity >
                  std::numeric_limits<Integer>::max() - row.actual.quantity)
            fail("ConstructionMaterialQuantityOverflow", required.item_id);
          quantity += row.actual.quantity;
        }
      if (quantity != required.quantity)
        fail("ConstructionActualMaterialCoverageMismatch", required.item_id);
    }
    for (const auto &row : result.consumed) {
      bool expected = false;
      for (const auto &required : terms.materials)
        expected |= required.item_id == row.item_id;
      if (!expected)
        fail("ConstructionUnexpectedMaterial", row.item_id);
    }
    if (result.cash_paid == 0) {
      if (!result.payer_wallet_id.empty() || !result.payee_wallet_id.empty())
        fail("ConstructionUnexpectedMoneyLeg", result.receipt_id);
    } else {
      if (result.payer_wallet_id.empty() ||
          result.payer_wallet_id == result.payee_wallet_id)
        fail("ConstructionOriginalPayerRequired", result.receipt_id);
      OriginalMoneyLegs legs(*effects.payment);
      take(resource_recipe::visit_original_money_transfer(
          result.payer_wallet_id, result.payee_wallet_id, result.cash_paid,
          resource_recipe::MoneyTransferPolicy::spendable, legs));
      if (!legs.complete())
        fail("ConstructionOriginalMoneyLegMissing", result.receipt_id);
    }
    return common_success();
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  }
}
} // namespace hanmo::resident
