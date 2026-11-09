// 原业务专用短寿命捕获服务；World适配器持有同候选Common/原业务账，不能保存到checkpoint。
#pragma once
#include "growth_market_history.hpp"
#include <cstdint>
#include <memory>
#include <span>
namespace hanmo::resident {
struct GrowthState;
struct GrowthQuote;
struct LocalEconomyState;
struct DynamicStepInput;
struct GrowthExactStartContext;
class NativeEconomicMaterialRead;
// 只由原 LE 数量循环记录真实 op 与收据槽；不从数量或最终库存反猜 recipe 槽。
struct EconomicMarketItemOperationBorrow {
  std::size_t operation_index = 0;
  std::uint32_t receipt_ordinal = 0, need_slot_ordinal = 0;
};
// 服务实现必须在每次apply的栈期间才授Common scope；所有失败使整候选失效。
// 析构时尚有已触市场且未绑定原事实的草稿，适配器必须标记发布失败，不能静默丢弃。
class MarketBusinessCaptureScope {
public:
  virtual ~MarketBusinessCaptureScope() = default;
  // 只由已核同World真实History2的原scalar
  // scope选择；旧scope不获得selected权限。
  virtual bool original_scalar_fifo_selected() const noexcept { return false; }
  // 仅真实LE History2 scope开启；物料源缺失不回退旧ConsumeLots。
  virtual bool original_economic_fifo_selected() const noexcept {
    return false;
  }
  // actual_legs由原业务实际recipe/收据计算，包含未选库存；适配器按静态保护集合筛选。
  // 实际钱货仍只执行一次Common::apply_resources，成功capture才接纳整个recipe草稿。
  virtual Status apply(MarketCaptureStage, const ResourceRecipe &,
                       const std::vector<MarketQuantityLeg> &actual_legs) = 0;
  // 旧1保原入口；选中2的具体scope必须覆盖并消费真实原循环元数据。
  virtual Status apply_original_economic_stage(
      MarketCaptureStage stage, const ResourceRecipe &recipe,
      const std::vector<MarketQuantityLeg> &legs,
      std::span<const EconomicMarketItemOperationBorrow>) {
    if (original_economic_fifo_selected())
      return common_failure("GrowthMarketEconomicActualCaptureRequired");
    return apply(stage, recipe, legs);
  }
  // 只有原头与原typed payload都成功后才能绑定；借这里明确传入的实际域候选。
  virtual Status commit_trade(const GrowthState &,
                              const StableId &exact_trade_fact_id) = 0;
  virtual Status commit_economy(const LocalEconomyState &,
                                const DynamicStepInput &,
                                const StableId &exact_fact_id) = 0;
};
class MarketBusinessCaptureServices {
public:
  virtual ~MarketBusinessCaptureServices() = default;
  virtual const NativeEconomicMaterialRead *
  original_economic_material_read() const noexcept {
    return nullptr;
  }
  // 合法未触选中市场可返回空scope，后续原recipe仍经Common全局guard。
  virtual Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
  open_growth_trade(CommonStore &, const GrowthCatalog &,
                    const ActionCoreView &, const GrowthQuote &,
                    const GrowthExactStartContext *actual_start = nullptr) = 0;
  // 生产key=原unit_id，口粮key=原obligation_id，due_tick是原到期轮，不能拿新事实ID替代。
  virtual Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
  open_economic_business(CommonStore &, MarketCauseKind,
                         const StableId &original_business_key,
                         Integer business_due_tick,
                         const DynamicStepInput &) = 0;
};
// 给中央observer用的整recipe纯捕获：所有库存都通过才返回一个完整草稿；失败没有局部accept。
Outcome<std::optional<MarketCaptureStep>> capture_protected_market_recipe(
    const CommonStore &, const ProtectedMarketAccounts &,
    const ResourceRecipe &, const MarketRecipeAuthorization *active_scope,
    MarketCaptureStage, const OrderedTable<InventoryView> &staged_inventories,
    Integer next_instance_after);
} // namespace hanmo::resident
