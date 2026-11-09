// 交割现金沿 Common 原双腿与原 frame 校验；金额来自已接受同版条款，不造余额。
#include "production_asset_transfer_cash.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
#include "resident/storage/selected_resource_execution.hpp"

namespace hanmo::resident {
namespace {
// 本 adapter 只把原 money recipe 的顺序与当前实际 capture 对齐；
// 扣款/冻结/守恒正文仍由原 read_original_wallet_delta 唯一校验。
class TransferLegs final : public resource_recipe::MoneyTransferLegSink {
  const CommonStore &common_;
  const ResourceExecutionDraft &capture_;
  std::size_t next_ = 0;

public:
  TransferLegs(const CommonStore &common, const ResourceExecutionDraft &capture)
      : common_(common), capture_(capture) {}
  Status wallet_delta(const StableId &wallet, Integer amount,
                      bool spendable) override {
    if (next_ >= capture_.operations.size())
      return common_failure("ProductionAssetTransferCashLegMissing", wallet);
    const auto *request =
        std::get_if<WalletDelta>(&capture_.recipe.operations[next_]);
    const auto &executed = capture_.operations[next_];
    if (!request || request->wallet_id != wallet || request->delta != amount ||
        request->require_spendable != spendable ||
        executed.operation_index != next_ ||
        executed.next_instance_before != capture_.next_instance_before ||
        executed.next_instance_after != capture_.next_instance_after)
      return common_failure("ProductionAssetTransferCashLegMismatch", wallet);
    auto original = read_original_wallet_delta(*request, executed);
    if (!original.ok)
      return {false, {}, original.error};
    const auto *actual = common_.wallet(wallet);
    if (!actual || !same_original_wallet_frame(*actual, original.value->after))
      return common_failure("ProductionAssetTransferCashFrameMismatch", wallet);
    ++next_;
    return common_success();
  }
  bool complete() const { return next_ == capture_.operations.size(); }
};
} // namespace
Status validate_production_asset_transfer_cash_capture(
    const CommonStore &common, const ProductionAssetTransferTerms &terms,
    const ProductionAssetTitleTransfer &transfer,
    const ResourceExecutionDraft &capture, const CurrentMomentRead &clock) {
  const auto &actual = common.resource_execution_drafts();
  const auto &world = common.state().identity;
  if (!common.resource_execution_capture_identity() || actual.empty() ||
      &actual.back() != &capture ||
      capture.identity.world_id != world.world_id ||
      capture.identity.execution_epoch != world.execution_epoch ||
      capture.identity.revision != world.revision ||
      capture.identity.tick != world.tick || capture.operations.size() != 2 ||
      capture.recipe.operations.size() != 2 ||
      capture.next_instance_before != capture.next_instance_after ||
      transfer.transfer_id.empty() || transfer.offer_id.empty() ||
      transfer.acceptance_id.empty() || terms.price < 0 ||
      terms.payer_wallet_id.empty() || terms.payee_wallet_id.empty() ||
      terms.payer_wallet_id == terms.payee_wallet_id ||
      transfer.actual_cash.from_wallet_id != terms.payer_wallet_id ||
      transfer.actual_cash.to_wallet_id != terms.payee_wallet_id ||
      transfer.actual_cash.quantity != terms.price)
    return common_failure("ProductionAssetActualTransferCaptureRequired",
                          transfer.transfer_id);
  auto now = clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same = compare_world_moments(transfer.applied_at, now.value);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return common_failure("ProductionAssetTransferCashMomentMismatch",
                          transfer.transfer_id);
  TransferLegs legs(common, capture);
  auto result = resource_recipe::visit_original_money_transfer(
      terms.payer_wallet_id, terms.payee_wallet_id, terms.price,
      resource_recipe::MoneyTransferPolicy::spendable, legs);
  if (!result.ok)
    return result;
  if (!legs.complete())
    return common_failure("ProductionAssetUnexpectedTransferOperation",
                          transfer.transfer_id);
  return common_success();
}
} // namespace hanmo::resident
