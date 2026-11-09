// Replays original allocation owners; no second current labor table is stored.
#pragma once
#include "household_purchase_contract.hpp"
#include "population_change_time.hpp"
#include "public_market_staffing.hpp"
#include "resident/domains/organization/organization_calendar_admission.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 原真reader借用，或同World静置选择证明；不是返回空成功的provider。
struct PopulationLaborTransportSources {
  const PopulationTransportSourceRead *original;
  const WorldBundle *stationary_world = nullptr;
  PopulationLaborTransportSources(const PopulationTransportSourceRead &source)
      : original(&source) {}
  PopulationLaborTransportSources(const PopulationTransportSourceRead *source,
                                  const WorldBundle &world)
      : original(source), stationary_world(&world) {}
};
struct PopulationLaborHistoryContext {
  const PopulationChangeTimeContext &original;
  const LocalEconomyCatalog &catalog;
  PopulationLaborTransportSources transport_sources;
  const OrganizationCatalog &organization_catalog;
  const OrganizationCalendarAdmissionRead &calendar;
  const HouseholdPurchaseState *household_purchases = nullptr;
  const PublicMarketStaffingContext *staffing = nullptr;
};
Status validate_population_labor_transport_sources(
    const PopulationLaborHistoryContext &);
struct OriginalLaborAllocationRead {
  std::optional<LaborAllocation> allocation;
  std::vector<SourceRef> actual_sources;
};
// Cut is an existing Common original source. It selects the actual source
// order, not ID lexical order or the current allocation. All retained history
// and the final original table are checked even when the requested cut is old.
Outcome<OriginalLaborAllocationRead>
read_original_labor_allocation_before_source(
    const PopulationLaborHistoryContext &, const StableId &allocation,
    const SourceRef &actual_cut);
Outcome<OriginalLaborAllocationRead>
read_original_labor_allocation_after_source(
    const PopulationLaborHistoryContext &, const StableId &allocation,
    const SourceRef &actual_cut);
Status validate_population_labor_history(const PopulationLaborHistoryContext &);
} // namespace hanmo::resident
