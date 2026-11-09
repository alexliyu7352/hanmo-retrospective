// 原scalar贸易的有限FIFO适配；权限/价格仍归原报价，钱货只由Common一次执行。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/storage/resource_execution_draft.hpp"
#include <span>
namespace hanmo::resident {
// 保原容器顺序、物品、Owner和空预约过滤，性质批不得跳过；不执行recipe。
class GrowthScalarFifoRecipe {
public:
  GrowthScalarFifoRecipe() = default;
  GrowthScalarFifoRecipe(const GrowthScalarFifoRecipe &) = delete;
  GrowthScalarFifoRecipe &operator=(const GrowthScalarFifoRecipe &) = delete;
  GrowthScalarFifoRecipe(GrowthScalarFifoRecipe &&) = default;
  GrowthScalarFifoRecipe &operator=(GrowthScalarFifoRecipe &&) = delete;
  const ResourceRecipe *recipe() const noexcept {
    return recipe_ ? &*recipe_ : nullptr;
  }

private:
  explicit GrowthScalarFifoRecipe(CheckpointMemoryCharge memory)
      : copied_fields_(std::move(memory)) {}
  // 全部真实普通字段先析构，最后才退还同job的实际预留。
  CheckpointMemoryCharge copied_fields_;
  std::optional<ResourceRecipe> recipe_;
  friend Outcome<GrowthScalarFifoRecipe>
  prepare_growth_scalar_fifo_recipe(const CommonStore &, const GrowthCatalog &,
                                    const ActionCoreView &,
                                    const GrowthQuote &);
};
Outcome<GrowthScalarFifoRecipe>
prepare_growth_scalar_fifo_recipe(const CommonStore &, const GrowthCatalog &,
                                  const ActionCoreView &, const GrowthQuote &);
// 热prepare与冷证据共同调用同一原FIFO扫描，不从净量反选批次。
Status validate_growth_scalar_fifo_order(const InventoryView &,
                                         const TradeTerms &, const Owner &,
                                         std::span<const CargoSelection>);
struct GrowthMarketHistoryCatalog;
struct ProtectedMarketAccounts;
// 供同World保护observer在原staging前只读核精确原recipe；不宽放其它selected写者。
Status inspect_growth_scalar_fifo_market_recipe(
    const CommonStore &, const GrowthCatalog &,
    const GrowthMarketHistoryCatalog &, const ProtectedMarketAccounts &,
    const ActionCoreView &, const GrowthQuote &, const ResourceRecipe &);
// 只借本Common当前capture中的一次真实recipe；无调用者提交的前后快照。
struct GrowthScalarFifoCaptureView {
  const ResourceExecutionDraft *execution = nullptr;
  const TransferSelectedLots *transfer = nullptr;
  const ResourceInventoryExecutionDraft *source = nullptr;
  const ResourceInventoryExecutionDraft *destination = nullptr;
  const ResourceWalletExecutionDraft *payer = nullptr;
  const ResourceWalletExecutionDraft *payee = nullptr;
};
// 核完整未动集、FIFO、实际flow、双钱包与操作顺序；不再转货/收费或生成事实。
Outcome<GrowthScalarFifoCaptureView>
read_growth_scalar_fifo_capture(const CommonStore &, const GrowthCatalog &,
                                const ActionCoreView &, const GrowthQuote &,
                                std::size_t actual_recipe_index);
} // namespace hanmo::resident
