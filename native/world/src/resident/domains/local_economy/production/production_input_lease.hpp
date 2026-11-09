// Same-world lifetime for the concrete original production input readers.
// The enclosing World lease still owns all cross-domain providers and the
// clock.
#pragma once
#include "production_input_source.hpp"
#include "production_population_change.hpp"
namespace hanmo::resident {
struct NativeEconomicProductionInputDependencies {
  PopulationChangeTimeContext population;
  OrganizationTrainingSourceContext training;
  EconomicCalendarReadContext calendar;
  ProductionFacilityHistoryContext conditions;
  std::optional<PoliticalTakeoverContext> takeover;
  const LocalEconomyCatalog &catalog;
  const EconomicWorkCatalog &work_catalog;
  PopulationLaborTransportSources transport;
  const SpatialCarrierState &carriers;
  const MovementExactState &movement;
  const SpatialCarrierSources &spatial;
  const TaskState &tasks;
  const HouseholdPurchaseState *purchases = nullptr;
  const PublicMarketStaffingContext *staffing = nullptr;
  // 只在实际未选接管时借同World原根，不能借成功标记替代反覆盖。
  const WorldBundle *unselected_takeover_world = nullptr;
};
// All members are borrowed contexts or concrete pure readers. There is no
// duplicated population, production, money, capacity, or mutable cursor here.
// Non-movable because the contained readers borrow the preceding members.
class NativeEconomicProductionInputLease final {
  NativeEconomicProductionInputDependencies dependencies_;
  PopulationLaborHistoryContext labor_history_;
  OriginalProductionSupplyContext supply_;
  OriginalProductionPopulationChangeContext population_changes_;
  EconomicProductionAdoptionContext adoption_context_;
  NativeEconomicProductionAdoptionRead adoptions_;
  std::optional<NativePoliticalProductionTakeoverRead> takeovers_;
  OriginalProductionCapacityContext capacity_;
  OriginalProductionLaborContext labor_;
  NativeProductionInputContext input_context_;
  NativeProductionInputSourceRead inputs_;

public:
  explicit NativeEconomicProductionInputLease(
      NativeEconomicProductionInputDependencies);
  NativeEconomicProductionInputLease(
      const NativeEconomicProductionInputLease &) = delete;
  NativeEconomicProductionInputLease &
  operator=(const NativeEconomicProductionInputLease &) = delete;
  NativeEconomicProductionInputLease(NativeEconomicProductionInputLease &&) =
      delete;
  NativeEconomicProductionInputLease &
  operator=(NativeEconomicProductionInputLease &&) = delete;
  // The central factory calls this before exposing a read/write binding. It
  // checks snapshot and selected dependencies, never creates missing originals.
  Status validate_bindings() const;
  const NativeProductionInputSourceRead &inputs() const { return inputs_; }
  const NativeEconomicProductionAdoptionRead &adoptions() const {
    return adoptions_;
  }
  const NativePoliticalProductionTakeoverRead *takeovers() const {
    return takeovers_ ? &*takeovers_ : nullptr;
  }
  const OriginalProductionPopulationChangeContext &population_changes() const {
    return population_changes_;
  }
  const PopulationLaborHistoryContext &labor_history() const {
    return labor_history_;
  }
  const NativeProductionInputContext &original_inputs() const {
    return input_context_;
  }
};
} // namespace hanmo::resident
