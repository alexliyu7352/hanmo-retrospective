// History2 的 LE 原业务核，只借已解析的历史帧与真正原收据。
#pragma once
#include "economic_material_read.hpp"
#include "resident/domains/family/family_life_history4.hpp"
#include "resident/domains/growth/growth_market_fifo_read.hpp"
#include "resident/domains/local_economy/disruptions/production_disruptions.hpp"
#include "resident/domains/local_economy/economic_work_sources.hpp"
#include "resident/domains/local_economy/household/provision_management.hpp"
namespace hanmo::resident {
struct EconomicFifoReadContext {
  const CommonStore &common;
  const CommonCatalog &shared;
  const CatalogIdentity &identity;
  const LocalEconomyState &economy;
  const LocalEconomyCatalog &catalog;
  const NativeEconomicMaterialRead &materials;
  // World先核原LE、人口和选中S0来源全史；此处只补原业务FIFO，不递归全验市场。
  // 原production_cycles/3未选才可空，未选仍拒Common孤头。
  const EconomicWorkValidationContext *work = nullptr;
  const EconomicTakeoverExecutionState *takeover = nullptr;
  const DynamicStepInput *phase = nullptr;
  const ProductionDisruptionCatalog *disruption_catalog = nullptr;
  const ProductionDisruptionState *disruptions = nullptr;
  const HouseholdProvisionManagementContext *household_management = nullptr;
  const FamilyLifeHistoryRead4 *life4 = nullptr;
};
class NativeEconomicFifoRead final : public MarketOriginalFifoBusinessRead {
  EconomicFifoReadContext x_;

public:
  explicit NativeEconomicFifoRead(EconomicFifoReadContext x) : x_(x) {}
  const EconomicFifoReadContext &context() const noexcept { return x_; }
  Status validate_bindings() const;
  Status
  visit_original_fifo_sources(std::span<const InventoryView *const>,
                              MarketOriginalFifoSourceSink &) const override;
  Status validate_original_fifo_business(
      const MarketMovementRecord &,
      std::span<const MarketOriginalStageReadView>) const override;
  // 包括真实可用量扫描中未选、过期及独特件候选的原 source pin。
  Status
  visit_original_material_pins(const MarketMovementRecord &,
                               std::span<const MarketOriginalStageReadView>,
                               LotPropertiesSourcePinSink &) const;
};
} // namespace hanmo::resident
