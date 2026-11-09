// 永生服务只借同World候选；短scope明确收到原业务的实际局部账，不能缓存旧LE副本。
#pragma once
#include "resident/domains/commerce/private_shop_market_contract.hpp"
#include "resident/domains/growth/growth_market_capture.hpp"
#include "resident/domains/growth/growth_market_cause.hpp"
#include "resident/domains/growth/growth_market_fifo_read.hpp"
#include "resident/domains/local_economy/market_history/economic_market_cause.hpp"
namespace hanmo::resident {
struct WorldBundle;
class NativeEconomicFifoRead;
// World复制和两条NPC保存集必须连此状态一起保留/恢复；不进持久格式。
// 只在Common/Growth/LE/market完整回滚后恢复旧值，服务重绑绝不清零。
struct GrowthMarketCaptureState {
  Integer open_scopes = 0;
  bool unbound_business = false;
};
struct GrowthMarketHistoryWorldContext {
  CommonStore &common;
  const CommonCatalog &shared;
  const GrowthCatalog &growth_catalog;
  GrowthState &growth;
  const LocalEconomyCatalog &economy_catalog;
  LocalEconomyState &economy;
  const GrowthMarketHistoryCatalog &catalog;
  GrowthMarketHistoryState &state;
  GrowthMarketCaptureState &capture;
  const EconomicTakeoverExecutionState *takeover = nullptr;
  // 精确开工只借当前真实World；旧History1不消费此尾部，克隆必须重绑。
  const WorldBundle *exact_world = nullptr;
  // 真LE读取者由实际World域owner保活；临时LE候选另须逐根及phase对址。
  const NativeEconomicFifoRead *original_economic_fifo = nullptr;
};
// 每次使用显式绑定实际LE候选及真正phase；pure读不回完整World/Shop/政治。
class GrowthMarketHistoryWorldRead final
    : public MarketBusinessCauseRead,
      public PrivateShopMarketHistoryRead,
      public MarketOriginalFifoBusinessRead {
public:
  GrowthMarketHistoryWorldRead(const GrowthMarketHistoryWorldContext &,
                               const GrowthState &, const LocalEconomyState &,
                               const DynamicStepInput *phase = nullptr);
  Outcome<MarketBusinessCause>
  market_business_cause(MarketCauseKind, const StableId &) const override;
  GrowthMarketHistoryAccess access() const;
  // 物理2从原业务账独立反枚举，并按真实业务类型分派唯一冷核。
  Status
  visit_original_fifo_sources(std::span<const InventoryView *const>,
                              MarketOriginalFifoSourceSink &) const override;
  Status validate_original_fifo_business(
      const MarketMovementRecord &,
      std::span<const MarketOriginalStageReadView>) const override;
  Status validate() const;
  Outcome<GrowthMarketMidpoint>
  private_shop_market_history_current(const StableId &,
                                      const StableId &) const override;
  Outcome<GrowthMarketMidpoint>
  private_shop_market_before_result(const StableId &, const StableId &, Integer,
                                    const StableId &) const override;
  // 只认唯一紧邻且已完整核史的原trade回执，供同一命令继续把刚买批次入私店。
  Status private_shop_immediate_market_trade(const StableId &) const override;

private:
  // 浅值保存真实根引用，不能借工厂临时聚合包自身。
  GrowthMarketHistoryWorldContext world_;
  const GrowthState &growth_;
  const LocalEconomyState &economy_;
  const DynamicStepInput *phase_;
  Status validate_original_bindings() const;
  Outcome<std::vector<StableId>>
  source_ids(const std::vector<StableId> &) const;
};
class NativeEconomicMarketCaptureServices;
class GrowthMarketHistoryWorldAccess final
    : public MarketBusinessCaptureServices,
      public ProtectedMarketResourceObserver {
public:
  explicit GrowthMarketHistoryWorldAccess(GrowthMarketHistoryWorldContext);
  GrowthMarketHistoryWorldAccess(const GrowthMarketHistoryWorldAccess &) =
      delete;
  GrowthMarketHistoryWorldAccess &
  operator=(const GrowthMarketHistoryWorldAccess &) = delete;
  Status validate_binding() const;
  Status validate_for_publish() const;
  Status validate_for_publish(const NativeEconomicFifoRead &) const;
  GrowthMarketHistoryWorldRead
  read(const LocalEconomyState &,
       const DynamicStepInput *phase = nullptr) const;
  // 原实际scope提供真实LE来源；返回读包不改永生服务中的上下文。
  GrowthMarketHistoryWorldRead read(const LocalEconomyState &,
                                    const DynamicStepInput *,
                                    const NativeEconomicFifoRead &) const;
  Outcome<std::unique_ptr<MarketBusinessCaptureScope>> open_growth_trade(
      CommonStore &, const GrowthCatalog &, const ActionCoreView &,
      const GrowthQuote &,
      const GrowthExactStartContext *actual_start = nullptr) override;
  Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
  open_economic_business(CommonStore &, MarketCauseKind, const StableId &,
                         Integer, const DynamicStepInput &) override;
  Status inspect_market_recipe(const CommonStore &,
                               const ResourceRecipe &) const override;
  Status capture_market_recipe(const CommonStore &, const ResourceRecipe &,
                               const OrderedTable<InventoryView> &,
                               Integer) override;

private:
  class Scope;
  friend class Scope;
  friend class NativeEconomicMarketCaptureServices;
  Status validate_economic_sources(const NativeEconomicFifoRead &,
                                   const DynamicStepInput *) const;
  Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
  open_growth_trade_bound(CommonStore &, const GrowthCatalog &,
                          const ActionCoreView &, const GrowthQuote &,
                          const GrowthExactStartContext *,
                          std::shared_ptr<const NativeEconomicFifoRead>);
  Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
  open_economic_business_bound(CommonStore &, MarketCauseKind, const StableId &,
                               Integer, const DynamicStepInput &,
                               std::shared_ptr<const NativeEconomicFifoRead>);
  GrowthMarketHistoryWorldContext context_;
  ProtectedMarketAccounts protected_;
  Scope *active_ = nullptr;
};
// 只组合同候选真实原来源，shared_ptr 必须别名保活该来源的完整域 owner。
// 此短服务不替换 Common observer，不把一次 phase 借口写进永生 BundleServices。
class NativeEconomicMarketCaptureServices final
    : public MarketBusinessCaptureServices,
      public CheckpointAllocatedObject {
  GrowthMarketHistoryWorldAccess &original_;
  std::shared_ptr<const NativeEconomicFifoRead> sources_;
  NativeEconomicMarketCaptureServices(
      GrowthMarketHistoryWorldAccess &,
      std::shared_ptr<const NativeEconomicFifoRead>);

public:
  static Outcome<std::unique_ptr<NativeEconomicMarketCaptureServices>>
  create(GrowthMarketHistoryWorldAccess &,
         std::shared_ptr<const NativeEconomicFifoRead>);
  Status validate_for_publish() const;
  const NativeEconomicMaterialRead *
  original_economic_material_read() const noexcept override;
  Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
  open_growth_trade(CommonStore &, const GrowthCatalog &,
                    const ActionCoreView &, const GrowthQuote &,
                    const GrowthExactStartContext * = nullptr) override;
  Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
  open_economic_business(CommonStore &, MarketCauseKind, const StableId &,
                         Integer, const DynamicStepInput &) override;
};
} // namespace hanmo::resident
