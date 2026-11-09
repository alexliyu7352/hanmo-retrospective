// 围城工程资格具名出口：准备在本轮课程之前，不能借原接敌阶段的同tick取值规则。
#pragma once
#include "growth_naval_history.hpp"
namespace hanmo::resident {
using GrowthSiegeEngineeringHistoryContext=GrowthNavalHistoryContext;
using GrowthSiegeEngineeringSkillObservation=GrowthNavalSkillObservation;
// 同候选完整Family/Creation/Birth/Growth先验。稳定预览取当前；strict phase只供siege_step原resolve前。
// phase须from=Common.tick、to=from+1，返回观察to但经验截止from，含from已实际完成的零时结算。
Outcome<GrowthSiegeEngineeringSkillObservation> read_growth_siege_engineering_skill(const CommonStore&,const GrowthSiegeEngineeringHistoryContext&,const GrowthState&,const StableId& actor,const DynamicStepInput* siege_phase=nullptr);
// 只认military_siege_engineering:/military_siege_engineering_prepared/military实际头，调用者先核payload.tick。
// 所有settled_tick==prepared_tick的自动课时和零时调整均在准备之后，永不倒填；未知当前XP差额仍拒。
// 因果集合按Common真实头序唯一排列；此口不授军官身份、围城授权或控制权。
Outcome<GrowthSiegeEngineeringSkillObservation> read_growth_siege_engineering_skill_before_preparation(const CommonStore&,const GrowthSiegeEngineeringHistoryContext&,const GrowthState&,const StableId& actor,Integer prepared_tick,const StableId& actual_preparation,const DynamicStepInput* siege_phase=nullptr);
}
