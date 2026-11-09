// 从原舟战/伤亡事实派生行旅终止，不新增Naval存档字段或伪造Travel成功回执。
#pragma once
#include "naval_battle_state.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct NavalJourneyTermination {
 StableId action_id,actor_id,battle_id,source_fact_id,casualty_fact_id,reason;Integer tick=0;
};
// 核直接Common头、原接敌行旅、实际伤亡与中断列表；完整准入仍须原Naval战术历史校验。
Outcome<std::vector<NavalJourneyTermination>> query_naval_journey_terminations(
 const CommonStore&,const MilitaryState&,const NavalState&,const StableId& source_fact_id);
// 精确冷绑定给定原源/action/actor，不根据latest、当前available或当前位置猜完成。
Outcome<std::optional<NavalJourneyTermination>> query_naval_journey_termination(
 const CommonStore&,const MilitaryState&,const NavalState&,const StableId& source_fact_id,
 const StableId& action_id,const StableId& actor_id);
}
