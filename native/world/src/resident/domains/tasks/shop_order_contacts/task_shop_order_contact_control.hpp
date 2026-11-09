#pragma once
#include "task_shop_order_contact_state.hpp"
namespace hanmo::resident {
struct FamilyState;struct FamilyLegacyState;
Outcome<StableId> task_shop_order_contact_player_before(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer,const StableId& actual_start_fact);
}
