// Player limits affect actual private-food consumption, never the household's nutritional requirement.
#pragma once
#include "resident/domains/family/household_control.hpp"
#include "../local_economy_model.hpp"
namespace hanmo::resident {
struct HouseholdProvisionManagementCatalog {
    Integer version=1;
    std::vector<StableId> authorization_ids;
};
enum class HouseholdProvisionConfigurationKind { configure,cancel_pending };
struct ConfigureHouseholdProvisionCommand {
    StableId actor_id,authorization_id;
    HouseholdProvisionConfigurationKind kind=HouseholdProvisionConfigurationKind::configure;
    // null restores the original due amount; zero intentionally supplies no food.
    std::optional<Integer> consumption_limit;
};
struct HouseholdProvisionConfigurationQuote {
    ConfigureHouseholdProvisionCommand command;
    HouseholdProvisionControlProof authority;
    Integer requested_tick=0,requested_revision=0,effective_tick=0,first_due_tick=0;
    StableId previous_configuration_id,pending_configuration_id;
};
struct HouseholdProvisionConfiguration {
    StableId id,authorization_id,actor_id,previous_configuration_id;
    HouseholdProvisionControlProof authority;
    Integer requested_tick=0,requested_revision=0,effective_tick=0,first_due_tick=0;
    std::optional<Integer> consumption_limit;
};
struct HouseholdProvisionConfigurationResolution {
    StableId id,configuration_id,kind,actor_id;
    HouseholdProvisionControlProof authority;
    Integer tick=0,source_revision=0;
};
struct HouseholdProvisionManagementState {
    Integer version=1;
    OrderedTable<HouseholdProvisionConfiguration> configurations;
    OrderedTable<HouseholdProvisionConfigurationResolution> resolutions;
    OrderedTable<StableId> current_by_authorization;
};
struct HouseholdProvisionManagementEffects { std::vector<StableId> fact_ids; };
// Read-only phase input. The canonical obligation, binding and RationReceipt remain in LocalEconomyState.
struct HouseholdProvisionManagementContext {
    const HouseholdProvisionManagementCatalog& catalog;
    const HouseholdProvisionManagementState& state;
};
Status validate_household_provision_management_catalog(const CommonStore&,const LocalEconomyCatalog&,const HouseholdProvisionManagementCatalog&);
Outcome<HouseholdProvisionConfigurationQuote> preview_configure_household_provision(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const HouseholdProvisionManagementCatalog&,const HouseholdProvisionManagementState&,const HouseholdProvisionControlRead&,const ConfigureHouseholdProvisionCommand&);
Outcome<HouseholdProvisionManagementEffects> apply_configure_household_provision_in_candidate(CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const HouseholdProvisionManagementCatalog&,HouseholdProvisionManagementState&,const HouseholdProvisionControlRead&,const ConfigureHouseholdProvisionCommand&);
Outcome<HouseholdProvisionManagementEffects> apply_configure_household_provision(CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const HouseholdProvisionManagementCatalog&,HouseholdProvisionManagementState&,const HouseholdProvisionControlRead&,const ConfigureHouseholdProvisionCommand&);
Status validate_household_provision_management(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const HouseholdProvisionManagementCatalog&,const HouseholdProvisionManagementState&,const HouseholdProvisionControlRead&);
// Used at the existing ration consumption point: actual=min(original requirement, available food, this limit).
Outcome<Integer> household_provision_consumption_limit(const LocalEconomyState&,const LocalEconomyCatalog&,const HouseholdProvisionManagementContext&,const RecurringObligation&,Integer due_tick,Integer original_requirement);
Outcome<ConfigureHouseholdProvisionCommand> decode_configure_household_provision(const BoundaryValue&);
BoundaryValue encode_configure_household_provision(const ConfigureHouseholdProvisionCommand&);
BoundaryValue encode_household_provision_configuration_quote(const HouseholdProvisionConfigurationQuote&);
// 同原字段writer，只发当前object字段，不构造中间树。
class ProjectionWriter;
Status emit_household_provision_management_effects_fields(ProjectionWriter&,const HouseholdProvisionManagementEffects&);
BoundaryValue encode_household_provision_management_effects(const HouseholdProvisionManagementEffects&);
Outcome<HouseholdProvisionManagementEffects> decode_household_provision_management_effects(const BoundaryValue&);
Status visit_household_provision_management_effects(CodecVisitor&,HouseholdProvisionManagementEffects&);
}
