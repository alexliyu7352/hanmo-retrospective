// E21.1: reconstruct the unreceived cargo from the original retail and actual
// handover lot flows. No inventory balance or transport position is retained here.
#pragma once
#include "household_purchase_contract.hpp"
namespace hanmo::resident {
struct HouseholdPurchaseCargoView {
 StableId purchase_id;Integer purchased_quantity=0,received_quantity=0;
 std::vector<CargoSelection> unreceived_cargo;
 std::vector<SourceRef> actual_handover_sources;
};
Outcome<HouseholdPurchaseCargoView> read_household_purchase_cargo(const HouseholdPurchaseContext&,const StableId& purchase_id);
// The caller first verifies this real owner adoption source and Moment. Later
// handovers cannot rewrite the cargo actually accepted by an earlier journey.
Outcome<HouseholdPurchaseCargoView> read_household_purchase_cargo_at_adoption(
 const HouseholdPurchaseContext&,const StableId& purchase_id,const WorldMoment&,
 const SourceRef& actual_adoption_source);
Status validate_household_purchase_cargo(const HouseholdPurchaseContext&,const HouseholdPurchase&);
}
