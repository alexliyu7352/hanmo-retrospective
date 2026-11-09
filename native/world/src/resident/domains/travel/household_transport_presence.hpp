// 家庭成交/搬运只读真实人物或人口绑定；拥有同一Owner不证明库存同地。
#pragma once
#include "group_transport_registration.hpp"
#include "../local_economy/household_purchase_types.hpp"
namespace hanmo::resident {
struct HouseholdSpatialPresence {
    StableId plan_id,place_id;HouseholdBuyer executor;WorldMoment observed_at;
    MovementAnchor actual_anchor,custody_anchor;SourceRef actual_presence_source;
    StorageReachability custody_to_destination;
};
Outcome<HouseholdSpatialPresence> read_household_purchase_presence(const CommonStore&,const LocalEconomyState&,const PopulationTransportState&,const SpatialCarrierState&,const MovementExactState&,const SpatialCarrierSources&,const HouseholdPurchasePlan& actual_plan,const StableId& actual_place_id,const StableId& custody_inventory_id,const StableId& destination_inventory_id,const SourceRef& actual_handling_rule,const WorldMoment&,ExactArithmeticBudget budget={});
}
