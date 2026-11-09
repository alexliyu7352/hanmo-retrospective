#pragma once
#include "resident/domains/local_economy/household/provision_management_view.hpp"
namespace hanmo::resident {
struct WorldBundle;
bool household_provision_management_enabled(const WorldBundle&);
Outcome<HouseholdProvisionConfigurationQuote> prepare_household_provision_configuration(const WorldBundle&,const StableId& actor,const ConfigureHouseholdProvisionCommand&);
Outcome<HouseholdProvisionManagementEffects> apply_household_provision_configuration(WorldBundle&,const StableId& actor,const ConfigureHouseholdProvisionCommand&);
Status validate_household_provision_world(const WorldBundle&);
Status validate_household_provision_configuration_receipts(const WorldBundle&);
HouseholdProvisionViewIndex rebuild_household_provision_index(const WorldBundle&);
Outcome<BoundaryValue> query_household_provision_world(const WorldBundle&,const HouseholdProvisionViewIndex&,const BoundaryValue&);
}
