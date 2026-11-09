// 家庭只是唯一Owner/原HouseholdGroup；具名与匿名执行者不互相伪装。
#pragma once
#include "population_transport.hpp"
#include "resident/domains/family/household_control.hpp"
#include "resident/kernel/work_execution.hpp"
#include "resident/domains/travel/mobile_inventory_model.hpp"
namespace hanmo::resident {
struct NamedHouseholdBuyer { StableId actor_id;std::optional<SourceRef> accepted_service_ref; };
struct CohortHouseholdBuyer { StableId cohort_id,population_selection_ref,labor_allocation_id;Integer workers=1;SourceRef accepted_household_policy_ref; };
using HouseholdBuyer=std::variant<NamedHouseholdBuyer,CohortHouseholdBuyer>;
struct HouseholdItemCap { StableId item_id;Integer quantity_cap=0; };
struct HouseholdPurchasePolicy { StableId id,household_id,controller_actor_id;Integer revision=1,accepted_tick=0,valid_from_tick=0,valid_until_tick=0;HouseholdProvisionControlProof controller_authority_ref;std::vector<HouseholdItemCap> allowed_items;Integer merchandise_cap_qian=0,transport_cap_qian=0,total_cap_qian=0;std::vector<StableId> allowed_market_refs,allowed_destination_inventories;SourceRef acceptance_source_ref;bool allow_cohort_buyer=false;std::optional<SourceRef> closed_source_ref; };
struct HouseholdMobileCustodyAdoption {SourceRef plan_source;MobileInventoryAdoptionView actual_view;};
struct HouseholdPurchasePlan { StableId plan_id,household_id;SourceRef need_or_policy_ref;HouseholdProvisionControlProof controller_authority_ref;std::vector<HouseholdItemCap> allowed_items;Integer spend_cap_qian=0;StableId known_market_ref,destination_inventory;HouseholdBuyer execution_choice;std::optional<SourceRef> approach_travel_ref;Integer latest_start_tick=0,delivery_deadline=0,terms_version=1;SourceRef accepted_policy_ref;Integer accepted_policy_revision=0;std::optional<HouseholdMobileCustodyAdoption> mobile_custody_adoption; };
struct PublicHouseholdRetailSeller { StableId market_shop_id; };
struct PrivateHouseholdRetailSeller { StableId shop_id,operation_id; };
using HouseholdRetailSeller=std::variant<PublicHouseholdRetailSeller,PrivateHouseholdRetailSeller>;
enum class HouseholdPurchasePhase { planned,servicing,awaiting_dispatch,in_transit,blocked,received,settling,cancelled,lost };
}
