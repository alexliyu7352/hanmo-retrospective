// The original buyer environment, composed from S4 condition, P02 population,
// S1 actual location/handling and the original market sources in one snapshot.
#pragma once
#include "household_named_environment.hpp"
#include "household_cohort_environment.hpp"
#include "household_cargo_handling.hpp"
#include "market_service_reconciliation.hpp"
#include "household_transport_read.hpp"
#include "resident/domains/travel/inventory_spatial_history.hpp"
namespace hanmo::resident {
struct NativeHouseholdEnvironmentContext {
  const HouseholdNamedEnvironmentContext &named;
  const HouseholdCohortEnvironmentContext &cohort;
  const HouseholdProvisionControlRead &controls;
  const FacilityClaimsCatalog &claim_catalog;
  const HouseholdRetailReadContext &retail;
  const TimeScaleRef &scale;
  const EconomicTransportExpenseState &transport_expenses;
  const CargoHandlingState &handling;
  const TravelCatalog &travel;
  const GroupMovementExecutionState &group_executions;
  const MarketServiceCapacityContext &market;
  const InventorySpatialHistoryContext &inventory_history;
  Integer selected_household_version = 2;
  const HouseholdMobileCustodyContext *mobile_custody = nullptr;
  const PrivateShopHouseholdCustodyContext *private_market_custody = nullptr;
};
class NativeHouseholdPurchaseEnvironmentRead final
    : public HouseholdPurchaseEnvironmentRead {
  const NativeHouseholdEnvironmentContext &x_;
  HouseholdPurchaseContext context() const;

public:
  explicit NativeHouseholdPurchaseEnvironmentRead(
      const NativeHouseholdEnvironmentContext &x)
      : x_(x) {}
  Outcome<HouseholdPurchasePresence>
  read_purchase_presence_for_service(const HouseholdPurchasePlan &,
                                     const HouseholdMarketServiceDefinition &,
                                     const WorldMoment &) const override;
  Outcome<HouseholdPurchasePresence>
  read_purchase_presence(const HouseholdPurchasePlan &, const StableId &,
                         const WorldMoment &) const override;
  Outcome<HouseholdPurchaseCoverage>
  read_purchase_service_coverage(const HouseholdPurchase &,
                                 const MomentSpan &) const override;
  Status validate_purchase_service_segment(
      const HouseholdPurchase &,
      const HouseholdServiceSegment &) const override;
  Outcome<bool>
  read_local_purchase_handover(const HouseholdPurchase &, const StableId &,
                               const WorldMoment &) const override;
  Outcome<HouseholdPurchaseDeliverySource>
  read_purchase_delivery(const HouseholdPurchase &,
                         const SourceRef &) const override;
  Status validate_named_purchase_service(const NamedHouseholdBuyer &,
                                         const HouseholdPurchasePlan &,
                                         const WorldMoment &) const override;
  Outcome<std::vector<FixedServiceInputRef>> read_named_service_adoption_inputs(
      const HouseholdPurchase &, const HouseholdServiceEvent2 &) const override;
  Status validate_named_service_adoption_inputs(
      const HouseholdPurchase &, const HouseholdServiceEvent2 &,
      const std::vector<FixedServiceInputRef> &) const override;
  Outcome<FacilityClaimCloseReason>
  read_named_service_interruption(const HouseholdPurchase &, const SourceRef &,
                                  const WorldMoment &, bool) const override;
  Outcome<std::vector<SourceRef>>
  read_named_service_participation(const HouseholdPurchase &,
                                   const MomentSpan &) const override;
  Status
  validate_named_service_continuation(const HouseholdPurchase &,
                                      const WorldMoment &) const override;
  Status validate_purchase_phase_cause(const HouseholdPurchase &,
                                       HouseholdPurchasePhase,
                                       const SourceRef &,
                                       const WorldMoment &) const override;
};
} // namespace hanmo::resident
