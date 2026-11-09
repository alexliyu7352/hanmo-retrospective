// The original plan selects a person, not a permanent cohort identity. Follow
// the one real materialization; never allocate a second buyer on arrival.
#pragma once
#include "household_purchase_types.hpp"
namespace hanmo::resident {
struct TransportRegistrationState;
struct HouseholdPurchaseAcceptedTerms;
struct HouseholdBuyerExecutionContext {
 const CommonStore& common;
 const LocalEconomyState& economy;
 const PopulationTransportState& population;
 const TransportRegistrationState* transport_registrations=nullptr;
};
// before_source is an actual Common adoption header, exclusive. It makes a
// same-M later registration unable to change an already accepted executor.
Outcome<HouseholdBuyer> read_household_buyer_for_adoption(
 const HouseholdBuyerExecutionContext&,const HouseholdPurchasePlan&,
 const WorldMoment&,const SourceRef* before_source=nullptr);
Status validate_household_buyer_adoption(const HouseholdBuyerExecutionContext&,
 const HouseholdPurchasePlan& original,const HouseholdPurchaseAcceptedTerms&);
// Replays the original finite reservation and its real registration/release.
// Current truncated allocation fields cannot erase earlier legitimate work.
Status validate_household_buyer_labor_span(const HouseholdBuyerExecutionContext&,
 const HouseholdPurchasePlan& original,const HouseholdPurchaseAcceptedTerms&,
 const MomentSpan&);
}
