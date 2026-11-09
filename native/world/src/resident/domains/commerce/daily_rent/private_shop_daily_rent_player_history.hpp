// 完整Family/Legacy先验后的日租具名前界；跨域实例序与事实序必须指向同一侧。
#pragma once
#include "private_shop_daily_rent.hpp"
namespace hanmo::resident {
struct FamilyState;
struct FamilyLegacyState;
Outcome<StableId> read_private_shop_rent_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& actual_rent_fact);
}
