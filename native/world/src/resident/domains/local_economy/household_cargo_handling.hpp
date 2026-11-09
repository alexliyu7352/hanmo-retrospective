#pragma once
#include "household_purchase_cargo.hpp"
#include "resident/domains/travel/cargo_handling.hpp"
#include "resident/domains/travel/group_transport_registration.hpp"
namespace hanmo::resident {
// The original S1 four tables remain the only handling execution/timing state.
struct HouseholdCargoHandlingContext {
 const HouseholdPurchaseContext& household;const CargoHandlingState& handling;
 const SpatialCarrierState& carriers;const MovementExactState& movement;
 const TransportRegistrationState& registrations;const SpatialCarrierSources& spatial;
 const TravelCatalog& travel;const CurrentMomentRead& clock;
};
Outcome<HouseholdPurchaseDeliverySource> read_household_cargo_handling_delivery(const HouseholdCargoHandlingContext&,const HouseholdPurchase&,const SourceRef& actual_result);
Status validate_household_cargo_handling_result(const HouseholdCargoHandlingContext&,const CargoHandlingTerms&,const CargoHandlingExecution&,const CargoHandlingResult&);
// Called from CargoHandlingBusinessWriter after the same S1 actual WorkResult.
// It performs the unique Common cargo leg, returning the original S5 references
// for S1 to install its Result before the complete candidate can be published.
Outcome<CargoHandlingBusinessTransfer> complete_household_cargo_handling_transfer_in_candidate(CommonStore&,HouseholdPurchaseState&,const HouseholdCargoHandlingContext&,const CargoHandlingTerms&,const CargoHandlingExecution&,const CargoHandlingWorkResult&,const SourceRef& pending_result,const WorldMoment&);
}
