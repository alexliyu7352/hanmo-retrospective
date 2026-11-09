#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "provision_management.hpp"
#include "../views/local_economy_view.hpp"
namespace hanmo::resident {
// Derived lawful source IDs only; never persisted and never a duplicate food ledger.
struct HouseholdProvisionAvailability {bool may_configure=false,may_cancel=false;StableId configure_code,cancel_code;};
struct HouseholdProvisionViewIndex {
    CheckpointMemoryCharge text_memory; // 先于字段构造，后于所有字段销毁。
    HouseholdProvisionViewIndex()=default;HouseholdProvisionViewIndex(const HouseholdProvisionViewIndex&)=default;HouseholdProvisionViewIndex(HouseholdProvisionViewIndex&&)noexcept=default;
    HouseholdProvisionViewIndex& operator=(HouseholdProvisionViewIndex value)noexcept{this->~HouseholdProvisionViewIndex();new(this)HouseholdProvisionViewIndex(std::move(value));return *this;}
    WorldIdentity identity; StableId observer_actor_id,place_id;
    CheckpointVector<StableId> authorization_ids;
    CheckpointMap<StableId,StableId> latest_ration_by_authorization;
    CheckpointMap<StableId,HouseholdProvisionAvailability> availability;
    std::optional<RuleError> build_error;
};
struct HouseholdProvisionPlanView {
    StableId id; Integer effective_tick=0,first_due_tick=0;
    std::optional<Integer> consumption_limit;
};
struct HouseholdProvisionManagementRow {
    StableId authorization_id,household_group_id,cohort_id,item_id;
    Integer headcount=0,available_food=0,next_due_tick=0,through_tick=0;
    std::optional<HouseholdProvisionPlanView> current_plan,pending_plan;
    std::optional<RationReceipt> latest_ration;
    bool may_configure=false,may_cancel=false; StableId configure_code,cancel_code;
};
struct HouseholdProvisionManagementPage {
    WorldIdentity identity; StableId observer_actor_id,place_id;
    std::vector<HouseholdProvisionManagementRow> rows;
    std::optional<LocalEconomyCursor> next_cursor;
};
Outcome<HouseholdProvisionViewIndex> build_household_provision_view_index(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const HouseholdProvisionManagementCatalog&,const HouseholdProvisionManagementState&,const HouseholdProvisionControlRead&);
Outcome<HouseholdProvisionManagementPage> query_household_provision_management(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const HouseholdProvisionManagementCatalog&,const HouseholdProvisionManagementState&,const HouseholdProvisionControlRead&,const HouseholdProvisionViewIndex&,const LocalEconomyQuery&);
Outcome<LocalEconomyQuery> decode_household_provision_management_query(const BoundaryValue&,const StableId& observer);
BoundaryValue encode_household_provision_management_page(const HouseholdProvisionManagementPage&);
}
