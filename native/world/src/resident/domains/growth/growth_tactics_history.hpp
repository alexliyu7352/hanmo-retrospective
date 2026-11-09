// 兵法资格独立具名出口；只复用原永久课程历史，不放宽既有陆战三兵种入口。
#pragma once
#include "growth_naval_history.hpp"
namespace hanmo::resident {
using GrowthTacticsHistoryContext=GrowthNavalHistoryContext;
using GrowthTacticsSkillObservation=GrowthNavalSkillObservation;
// 原Family/Creation/Birth及完整Growth须先验；缺任何可解释XP来源即拒，不能取今日值填历史。
// strict phase只允许S2原contact_install：actions_step后、clock_commit前；返回身份仍为候选当前from。
Outcome<GrowthTacticsSkillObservation> read_growth_tactics_skill(const CommonStore&,const GrowthTacticsHistoryContext&,const GrowthState&,const StableId& actor,const DynamicStepInput* contact_phase=nullptr);
// 只认实际military_rout_entry:/military_rout_entry/military头；S2先核原entry载荷tick。
// 同tick自动课程在前、零时迟答在后，因果集合按Common事实实序唯一排列。
Outcome<GrowthTacticsSkillObservation> read_growth_tactics_skill_before_entry(const CommonStore&,const GrowthTacticsHistoryContext&,const GrowthState&,const StableId& actor,Integer entry_tick,const StableId& actual_entry_fact,const DynamicStepInput* contact_phase=nullptr);
}
