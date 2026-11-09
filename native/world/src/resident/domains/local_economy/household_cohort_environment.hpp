// Actual one-person household service coverage. Selection/allocation,
// population and position remain the original P02/S1 sources; this leaf stores
// no attendance.
#pragma once
#include "household_buyer_execution.hpp"
#include "household_purchase_contract.hpp"
#include "population_selection_source.hpp"
#include "population_change_time.hpp"
#include "resident/domains/travel/population_spatial_history.hpp"
namespace hanmo::resident {
struct HouseholdCohortEnvironmentContext {
  const CommonStore &common;
  const LocalEconomyState &economy;
  const LocalEconomyCatalog &economy_catalog;
  const PopulationTransportState &population;
  const HouseholdPurchaseState &state;
  const HouseholdPurchaseCatalog &catalog;
  const SpatialCarrierState &carriers;
  const MovementExactState &movement;
  const TransportRegistrationState &registrations;
  const SpatialCarrierSources &spatial;
  const CurrentMomentRead &clock;
  const PopulationChangeTimeContext *population_history = nullptr;
  const PrivateShopHouseholdCustodyContext *private_market_custody = nullptr;
};
Outcome<HouseholdPurchaseCoverage>
read_cohort_household_actual_coverage(const HouseholdCohortEnvironmentContext &,
                                      const HouseholdPurchase &,
                                      const MomentSpan &);
Status validate_cohort_household_actual_segment(
    const HouseholdCohortEnvironmentContext &, const HouseholdPurchase &,
    const HouseholdServiceSegment &);
} // namespace hanmo::resident
