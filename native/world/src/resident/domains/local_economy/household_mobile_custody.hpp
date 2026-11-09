// P05/3 adopts an existing portable inventory in the original planned result.
// Its physical source stays S1; its title remains the original Common owner.
#pragma once
#include "household_purchase_contract.hpp"
#include "resident/domains/travel/mobile_inventory_adoption.hpp"
namespace hanmo::resident {
struct FamilyState;struct MilitaryState;struct NavalState;struct NavalCatalog;struct EstateCatalog;
struct HouseholdMobileCustodyContext {
 const MobileInventoryAdoptionContext& spatial;
 const FamilyState& family;const MilitaryState& military;
 const NavalState& naval;const NavalCatalog& naval_catalog;const EstateCatalog& estate_catalog;
};
struct HouseholdMobileCustodyRequest {StableId inventory_id;};
Outcome<HouseholdMobileCustodyAdoption> capture_household_mobile_custody(
 const HouseholdPurchaseContext&,const HouseholdMobileCustodyContext&,
 const HouseholdPurchasePlan&,const HouseholdMobileCustodyRequest&,const SourceRef& actual_plan_source);
Status validate_household_mobile_custody(const HouseholdPurchaseContext&,
 const HouseholdMobileCustodyContext&,const HouseholdPurchasePlan&);
bool same_household_mobile_custody(const std::optional<HouseholdMobileCustodyAdoption>&,
 const std::optional<HouseholdMobileCustodyAdoption>&);
}
