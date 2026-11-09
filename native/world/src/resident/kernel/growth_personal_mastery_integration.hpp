// 只借同World的不可变Installed初态与原Family/Growth链；无跨调用借用。
#pragma once
#include "resident/domains/growth/growth_personal_mastery_history.hpp"
namespace hanmo::resident {
class WorldBundle;
// 调用方须先验完整Family/Birth/Growth；本桥不递归validate，也不证明装备选择或疲劳。
Outcome<GrowthPersonalMasteryObservation> read_growth_personal_mastery_world(const WorldBundle&,const StableId& actor);
Outcome<GrowthPersonalMasteryObservation> read_growth_personal_mastery_before_world(const WorldBundle&,const StableId& actor,Integer tick,const StableId& actual_entry);
}
