// P1 原拥有 lease 到 Complete 的窄作用域；只借同 World 原来源，不重开组合根。
#pragma once
#include "native_complete_world_registry.hpp"
namespace hanmo::resident {
struct NativeCompleteWorldCoreSources;
struct EconomicFacilityInstallationReadContext;
struct PoliticalFiscalTime2Context;
struct PoliticalTakeoverContext;
struct GovernanceProgramReadContext;
class NativeEconomicProductionInputLease;
// 未选来源才允许空；已选的真实目录和状态仍由原 P1 factory 逐项反验。
struct NativePoliticalWorldLateRead {
  const EconomicFacilityInstallationReadContext *installation = nullptr;
  const GovernanceProgramReadContext *governance = nullptr;
  const PopulationMilitaryReadContext *population_military = nullptr;
  const PublicMarketStaffingContext *staffing = nullptr;
  const PoliticalTreatyExactReadBinding *treaty_exact = nullptr;
  // 只借原宏观 producer 的真实 phase，不从精确 M 制造整数窗。
  const DynamicStepInput *actual_phase = nullptr;
};
// 生产输入必须由原经济 owner 持有；本域仅复制借用 context，不制造零产出来源。
struct NativePoliticalWorldLateWrite {
  NativePoliticalWorldLateRead read;
  const NativeEconomicProductionInputLease &production_inputs;
  EconomicProductionContext &production;
  PublicMarketStaffingEffectsWrite *staffing_effects = nullptr;
  PoliticalTreatyExactWriteBinding *treaty_exact = nullptr;
};
// 只供完整 World 冷验消费真实尾：同根同钟后仍走原业务全验，不代回执/执行账。
Status validate_native_political_world_business_sources(
    const WorldBundle &, const PoliticalExactReadBinding &);
class NativePoliticalWorldScope final : public NativeCompleteWorldSourceScope {
  struct Sources;
  std::unique_ptr<Sources> sources_;
  explicit NativePoliticalWorldScope(std::unique_ptr<Sources>);

public:
  ~NativePoliticalWorldScope();
  // 外层 retaining owner 必须保 core、late 原来源及真实 phase 活到本 scope
  // 析构。 本构造只核绑定和原 P1 source；不调用 Complete getter，不发布候选。
  static Outcome<std::shared_ptr<const NativePoliticalWorldScope>>
  create_read(const WorldBundle &, const NativeCompleteWorldCoreSources &,
              const WorldMoment &, NativePoliticalWorldLateRead);
  static Outcome<std::shared_ptr<NativePoliticalWorldScope>>
  create_write(WorldBundle &, const NativeCompleteWorldCoreSources &,
               const WorldMoment &, NativePoliticalWorldLateWrite);
  // 同一不可移动 scope 保活，供原生产 owner 借入，不重新获取 Complete。
  // 返回基础原 lease 加真实业务源尾的同一视图，alias 须保活本 scope。
  const PoliticalExactReadLease &political_read_lease() const noexcept;
  const PoliticalTakeoverContext *takeover_context() const noexcept;
  const PoliticalFiscalTime2Context *fiscal_context() const noexcept;
  bool bound_to(const WorldBundle &) const noexcept override;
  NativeCompleteWorldRoute route() const noexcept override;
  const WorldMoment &actual_at() const noexcept override;
  const NativeCompleteWorldSourceExports &sources() const noexcept override;
};
} // namespace hanmo::resident
